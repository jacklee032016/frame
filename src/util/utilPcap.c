/* 
 *
 */
#include <utilPcap.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseFileIo.h>
#include <baseLog.h>
#include <basePool.h>
#include <baseSock.h>
#include <baseString.h>


#pragma pack(1)

typedef struct bpcap_hdr 
{
	buint32_t magic_number;   /* magic number */
	buint16_t version_major;  /* major version number */
	buint16_t version_minor;  /* minor version number */
	bint32_t  thiszone;       /* GMT to local correction */
	buint32_t sigfigs;        /* accuracy of timestamps */
	buint32_t snaplen;        /* max length of captured packets, in octets */
	buint32_t network;        /* data link type */
} bpcap_hdr;

typedef struct bpcap_rec_hdr 
{
	buint32_t ts_sec;         /* timestamp seconds */
	buint32_t ts_usec;        /* timestamp microseconds */
	buint32_t incl_len;       /* number of octets of packet saved in file */
	buint32_t orig_len;       /* actual length of packet */
} bpcap_rec_hdr;

#if 0
/* gcc insisted on aligning this struct to 32bit on ARM */
typedef struct bpcap_eth_hdr 
{
    buint8_t  dest[6];
    buint8_t  src[6];
    buint8_t  len[2];
} bpcap_eth_hdr;
#else
typedef buint8_t bpcap_eth_hdr[14];
#endif

typedef struct bpcap_ip_hdr 
{
    buint8_t	v_ihl;
    buint8_t	tos;
    buint16_t	len;
    buint16_t	id;
    buint16_t	flags_fragment;
    buint8_t	ttl;
    buint8_t	proto;
    buint16_t	csum;
    buint32_t	ip_src;
    buint32_t	ip_dst;
} bpcap_ip_hdr;

/* Implementation of pcap file */
struct bpcap_file
{
    char	    objName[BASE_MAX_OBJ_NAME];
    bOsHandle_t   fd;
    bbool_t	    swap;
    bpcap_hdr	    hdr;
    bpcap_filter  filter;
};

#pragma pack()

/* Init default filter */
void bpcap_filter_default(bpcap_filter *filter)
{
    bbzero(filter, sizeof(*filter));
}

/* Open pcap file */
bstatus_t bpcap_open(bpool_t *pool,
				 const char *path,
				 bpcap_file **p_file)
{
    bpcap_file *file;
    bssize_t sz;
    bstatus_t status;

    BASE_ASSERT_RETURN(pool && path && p_file, BASE_EINVAL);

    /* More sanity checks */
    MTRACE( "sizeof(bpcap_eth_hdr)=%d", sizeof(bpcap_eth_hdr));
    BASE_ASSERT_RETURN(sizeof(bpcap_eth_hdr)==14, BASE_EBUG);
    MTRACE( "sizeof(bpcap_ip_hdr)=%d",   sizeof(bpcap_ip_hdr));
    BASE_ASSERT_RETURN(sizeof(bpcap_ip_hdr)==20, BASE_EBUG);
    MTRACE( "sizeof(bpcap_udp_hdr)=%d",   sizeof(bpcap_udp_hdr));
    BASE_ASSERT_RETURN(sizeof(bpcap_udp_hdr)==8, BASE_EBUG);
    
    file = BASE_POOL_ZALLOC_T(pool, bpcap_file);

    bansi_strcpy(file->objName, "pcap");

    status = bfile_open(pool, path, BASE_O_RDONLY, &file->fd);
    if (status != BASE_SUCCESS)
	return status;

    /* Read file pcap header */
    sz = sizeof(file->hdr);
    status = bfile_read(file->fd, &file->hdr, &sz);
    if (status != BASE_SUCCESS) {
	bfile_close(file->fd);
	return status;
    }

    /* Check magic number */
    if (file->hdr.magic_number == 0xa1b2c3d4) {
	file->swap = BASE_FALSE;
    } else if (file->hdr.magic_number == 0xd4c3b2a1) {
	file->swap = BASE_TRUE;
	file->hdr.network = bntohl(file->hdr.network);
    } else {
	/* Not PCAP file */
	bfile_close(file->fd);
	return BASE_EINVALIDOP;
    }

    MTRACE("PCAP file %s opened", path);
    
    *p_file = file;
    return BASE_SUCCESS;
}

/* Close pcap file */
bstatus_t bpcap_close(bpcap_file *file)
{
    BASE_ASSERT_RETURN(file, BASE_EINVAL);
    MTRACE("PCAP file closed");
    return bfile_close(file->fd);
}

/* Setup filter */
bstatus_t bpcap_set_filter(bpcap_file *file,
				       const bpcap_filter *fil)
{
    BASE_ASSERT_RETURN(file && fil, BASE_EINVAL);
    bmemcpy(&file->filter, fil, sizeof(bpcap_filter));
    return BASE_SUCCESS;
}

/* Read file */
static bstatus_t read_file(bpcap_file *file,
			     void *buf,
			     bssize_t *sz)
{
    bstatus_t status;
    status = bfile_read(file->fd, buf, sz);
    if (status != BASE_SUCCESS)
	return status;
    if (*sz == 0)
	return BASE_EEOF;
    return BASE_SUCCESS;
}

static bstatus_t skip(bOsHandle_t fd, boff_t bytes)
{
    bstatus_t status;
    status = bfile_setpos(fd, bytes, BASE_SEEK_CUR);
    if (status != BASE_SUCCESS)
	return status; 
    return BASE_SUCCESS;
}


#define SKIP_PKT()  \
	if (rec_incl > sz_read) { \
	    status = skip(file->fd, rec_incl-sz_read);\
	    if (status != BASE_SUCCESS) \
		return status; \
	}

/* Read UDP packet */
bstatus_t bpcap_read_udp(bpcap_file *file,
				     bpcap_udp_hdr *udp_hdr,
				     buint8_t *udp_payload,
				     bsize_t *udp_payload_size)
{
    BASE_ASSERT_RETURN(file && udp_payload && udp_payload_size, BASE_EINVAL);
    BASE_ASSERT_RETURN(*udp_payload_size, BASE_EINVAL);

    /* Check data link type in PCAP file header */
    if ((file->filter.link && 
	    file->hdr.network != (buint32_t)file->filter.link) ||
	file->hdr.network != BASE_PCAP_LINK_TYPE_ETH)
    {
	/* Link header other than Ethernet is not supported for now */
	return BASE_ENOTSUP;
    }

    /* Loop until we have the packet */
    for (;;) {
	union {
	    bpcap_rec_hdr rec;
	    bpcap_eth_hdr eth;
	    bpcap_ip_hdr ip;
	    bpcap_udp_hdr udp;
	} tmp;
	unsigned rec_incl;
	bssize_t sz;
	bsize_t sz_read = 0;
	char addr[BASE_INET_ADDRSTRLEN];
	bstatus_t status;

	MTRACE( "Reading packet..");
	bbzero(&addr, sizeof(addr));

	/* Read PCAP packet header */
	sz = sizeof(tmp.rec);
	status = read_file(file, &tmp.rec, &sz); 
	if (status != BASE_SUCCESS) {
	    MTRACE("read_file() error: %d", status);
	    return status;
	}

	rec_incl = tmp.rec.incl_len;

	/* Swap byte ordering */
	if (file->swap) {
	    tmp.rec.incl_len = bntohl(tmp.rec.incl_len);
	    tmp.rec.orig_len = bntohl(tmp.rec.orig_len);
	    tmp.rec.ts_sec = bntohl(tmp.rec.ts_sec);
	    tmp.rec.ts_usec = bntohl(tmp.rec.ts_usec);
	}

	/* Read link layer header */
	switch (file->hdr.network) {
	case BASE_PCAP_LINK_TYPE_ETH:
	    sz = sizeof(tmp.eth);
	    status = read_file(file, &tmp.eth, &sz);
	    break;
	default:
	    MTRACE( "Error: link layer not Ethernet");
	    return BASE_ENOTSUP;
	}

	if (status != BASE_SUCCESS) {
	    MTRACE( "Error reading Eth header: %d", status);
	    return status;
	}

	sz_read += sz;
	    
	/* Read IP header */
	sz = sizeof(tmp.ip);
	status = read_file(file, &tmp.ip, &sz);
	if (status != BASE_SUCCESS) {
	    MTRACE("Error reading IP header: %d", status);
	    return status;
	}

	sz_read += sz;

	/* Skip if IP source mismatch */
	if (file->filter.ip_src && tmp.ip.ip_src != file->filter.ip_src) {
	    MTRACE( "IP source %s mismatch, skipping",    binet_ntop2(bAF_INET(), (bin_addr*)&tmp.ip.ip_src,
		    		  addr, sizeof(addr)));
	    SKIP_PKT();
	    continue;
	}

	/* Skip if IP destination mismatch */
	if (file->filter.ip_dst && tmp.ip.ip_dst != file->filter.ip_dst) {
	    MTRACE("IP detination %s mismatch, skipping", 
		    binet_ntop2(bAF_INET(), (bin_addr*)&tmp.ip.ip_dst,
		    		  addr, sizeof(addr)));
	    SKIP_PKT();
	    continue;
	}

	/* Skip if proto mismatch */
	if (file->filter.proto && tmp.ip.proto != file->filter.proto) {
	    MTRACE( "IP proto %d mismatch, skipping", tmp.ip.proto);
	    SKIP_PKT();
	    continue;
	}

	/* Read transport layer header */
	switch (tmp.ip.proto) {
	case BASE_PCAP_PROTO_TYPE_UDP:
	    sz = sizeof(tmp.udp);
	    status = read_file(file, &tmp.udp, &sz);
	    if (status != BASE_SUCCESS) {
		MTRACE( "Error reading UDP header: %d",status);
		return status;
	    }

	    sz_read += sz;

	    /* Skip if source port mismatch */
	    if (file->filter.src_port && 
	        tmp.udp.src_port != file->filter.src_port) 
	    {
		MTRACE("UDP src port %d mismatch, skipping", bntohs(tmp.udp.src_port));
		SKIP_PKT();
		continue;
	    }

	    /* Skip if destination port mismatch */
	    if (file->filter.dst_port && 
		tmp.udp.dst_port != file->filter.dst_port) 
	    {
		MTRACE( "UDP dst port %d mismatch, skipping", bntohs(tmp.udp.dst_port));
		SKIP_PKT();
		continue;
	    }

	    /* Copy UDP header if caller wants it */
	    if (udp_hdr) {
		bmemcpy(udp_hdr, &tmp.udp, sizeof(*udp_hdr));
	    }

	    /* Calculate payload size */
	    sz = bntohs(tmp.udp.len) - sizeof(tmp.udp);
	    break;
	default:
	    MTRACE( "Not UDP, skipping");
	    SKIP_PKT();
	    continue;
	}

	/* Check if payload fits the buffer */
	if (sz > (bssize_t)*udp_payload_size) {
	    MTRACE(   "Error: packet too large (%d bytes required)", sz);
	    SKIP_PKT();
	    return BASE_ETOOSMALL;
	}

	/* Read the payload */
	status = read_file(file, udp_payload, &sz);
	if (status != BASE_SUCCESS) {
	    MTRACE( "Error reading payload: %d", status);
	    return status;
	}

	sz_read += sz;

	*udp_payload_size = sz;

	// Some layers may have trailer, e.g: link eth2.
	/* Check that we've read all the packets */
	//BASE_ASSERT_RETURN(sz_read == rec_incl, BASE_EBUG);

	/* Skip trailer */
	while (sz_read < rec_incl) {
	    sz = rec_incl - sz_read;
	    status = read_file(file, &tmp.eth, &sz);
	    if (status != BASE_SUCCESS) {
		MTRACE( "Error reading trailer: %d", status);
		return status;
	    }
	    sz_read += sz;
	}

	return BASE_SUCCESS;
    }

    /* Does not reach here */
}


