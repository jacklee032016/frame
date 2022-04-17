/* 
 *
 */
#include <baseSock.h>
#include <baseAssert.h>
#include <baseCtype.h>
#include <baseErrno.h>
#include <baseIpHelper.h>
#include <baseOs.h>
#include <baseAddrResolv.h>
#include <baseRand.h>
#include <baseString.h>
#include <compat/socket.h>
#include <baseLog.h>

/*
 * Convert address string with numbers and dots to binary IP address.
 */ 
bin_addr binet_addr(const bstr_t *cp)
{
    bin_addr addr;

    binet_aton(cp, &addr);
    return addr;
}

/*
 * Convert address string with numbers and dots to binary IP address.
 */ 
bin_addr binet_addr2(const char *cp)
{
    bstr_t str = bstr((char*)cp);
    return binet_addr(&str);
}

/*
 * Get text representation.
 */
char* binet_ntop2( int af, const void *src,
			     char *dst, int size)
{
    bstatus_t status;

    status = binet_ntop(af, src, dst, size);
    return (status==BASE_SUCCESS)? dst : NULL;
}

/*
 * Print socket address.
 */
char* bsockaddr_print( const bsockaddr_t *addr,
				 char *buf, int size,
				 unsigned flags)
{
    enum {
	WITH_PORT = 1,
	WITH_BRACKETS = 2
    };

    char txt[BASE_INET6_ADDRSTRLEN];
    char port[32];
    const baddr_hdr *h = (const baddr_hdr*)addr;
    char *bquote, *equote;
    bstatus_t status;

    status = binet_ntop(h->sa_family, bsockaddr_get_addr(addr),
			  txt, sizeof(txt));
    if (status != BASE_SUCCESS)
	return "";

    if (h->sa_family != BASE_AF_INET6 || (flags & WITH_BRACKETS)==0) {
	bquote = ""; equote = "";
    } else {
	bquote = "["; equote = "]";
    }

    if (flags & WITH_PORT) {
	bansi_snprintf(port, sizeof(port), ":%d",
			 bsockaddr_get_port(addr));
    } else {
	port[0] = '\0';
    }

    bansi_snprintf(buf, size, "%s%s%s%s",
		     bquote, txt, equote, port);

    return buf;
}

/*
 * Set the IP address of an IP socket address from string address, 
 * with resolving the host if necessary. The string address may be in a
 * standard numbers and dots notation or may be a hostname. If hostname
 * is specified, then the function will resolve the host into the IP
 * address.
 */
bstatus_t bsockaddr_in_set_str_addr( bsockaddr_in *addr,
					         const bstr_t *str_addr)
{
    BASE_CHECK_STACK();

    BASE_ASSERT_RETURN(!str_addr || str_addr->slen < BASE_MAX_HOSTNAME, 
                     (addr->sin_addr.s_addr=BASE_INADDR_NONE, BASE_EINVAL));

    BASE_SOCKADDR_RESET_LEN(addr);
    addr->sin_family = BASE_AF_INET;
    bbzero(addr->sin_zero, sizeof(addr->sin_zero));

    if (str_addr && str_addr->slen) {
	addr->sin_addr = binet_addr(str_addr);
	if (addr->sin_addr.s_addr == BASE_INADDR_NONE) {
    	    baddrinfo ai;
	    unsigned count = 1;
	    bstatus_t status;

	    status = bgetaddrinfo(bAF_INET(), str_addr, &count, &ai);
	    if (status==BASE_SUCCESS) {
		bmemcpy(&addr->sin_addr, &ai.ai_addr.ipv4.sin_addr,
			  sizeof(addr->sin_addr));
	    } else {
		return status;
	    }
	}

    } else {
	addr->sin_addr.s_addr = 0;
    }

    return BASE_SUCCESS;
}

/* Set address from a name */
bstatus_t bsockaddr_set_str_addr(int af,
					     bsockaddr *addr,
					     const bstr_t *str_addr)
{
    bstatus_t status;

    if (af == BASE_AF_INET) {
	return bsockaddr_in_set_str_addr(&addr->ipv4, str_addr);
    }

    BASE_ASSERT_RETURN(af==BASE_AF_INET6, BASE_EAFNOTSUP);

    /* IPv6 specific */

    addr->ipv6.sin6_family = BASE_AF_INET6;
    BASE_SOCKADDR_RESET_LEN(addr);

    if (str_addr && str_addr->slen) {
#if defined(BASE_SOCKADDR_USE_GETADDRINFO) && BASE_SOCKADDR_USE_GETADDRINFO!=0
	if (1) {
#else
	status = binet_pton(BASE_AF_INET6, str_addr, &addr->ipv6.sin6_addr);
	if (status != BASE_SUCCESS) {
#endif
    	    baddrinfo ai;
	    unsigned count = 1;

	    status = bgetaddrinfo(BASE_AF_INET6, str_addr, &count, &ai);
	    if (status==BASE_SUCCESS) {
		bmemcpy(&addr->ipv6.sin6_addr, &ai.ai_addr.ipv6.sin6_addr,
			  sizeof(addr->ipv6.sin6_addr));
		addr->ipv6.sin6_scope_id = ai.ai_addr.ipv6.sin6_scope_id;
	    }
	}
    } else {
	status = BASE_SUCCESS;
    }

    return status;
}


/*
 * Set the IP address and port of an IP socket address.
 * The string address may be in a standard numbers and dots notation or 
 * may be a hostname. If hostname is specified, then the function will 
 * resolve the host into the IP address.
 */
bstatus_t bsockaddr_in_init( bsockaddr_in *addr,
				         const bstr_t *str_addr,
					 buint16_t port)
{
    BASE_ASSERT_RETURN(addr, (addr->sin_addr.s_addr=BASE_INADDR_NONE, BASE_EINVAL));

    BASE_SOCKADDR_RESET_LEN(addr);
    addr->sin_family = BASE_AF_INET;
    bbzero(addr->sin_zero, sizeof(addr->sin_zero));
    bsockaddr_in_set_port(addr, port);
    return bsockaddr_in_set_str_addr(addr, str_addr);
}

/*
 * Initialize IP socket address based on the address and port info.
 */
bstatus_t bsockaddr_init(int af, 
				     bsockaddr *addr,
				     const bstr_t *cp,
				     buint16_t port)
{
    bstatus_t status;

    if (af == BASE_AF_INET) {
	return bsockaddr_in_init(&addr->ipv4, cp, port);
    }

    /* IPv6 specific */
    BASE_ASSERT_RETURN(af==BASE_AF_INET6, BASE_EAFNOTSUP);

    bbzero(addr, sizeof(bsockaddr_in6));
    addr->addr.sa_family = BASE_AF_INET6;
    
    status = bsockaddr_set_str_addr(af, addr, cp);
    if (status != BASE_SUCCESS)
	return status;

    addr->ipv6.sin6_port = bhtons(port);
    return BASE_SUCCESS;
}

/*
 * Compare two socket addresses.
 */
int bsockaddr_cmp( const bsockaddr_t *addr1,
			     const bsockaddr_t *addr2)
{
    const bsockaddr *a1 = (const bsockaddr*) addr1;
    const bsockaddr *a2 = (const bsockaddr*) addr2;
    int port1, port2;
    int result;

    /* Compare address family */
    if (a1->addr.sa_family < a2->addr.sa_family)
	return -1;
    else if (a1->addr.sa_family > a2->addr.sa_family)
	return 1;

    /* Compare addresses */
    result = bmemcmp(bsockaddr_get_addr(a1),
		       bsockaddr_get_addr(a2),
		       bsockaddr_get_addr_len(a1));
    if (result != 0)
	return result;

    /* Compare port number */
    port1 = bsockaddr_get_port(a1);
    port2 = bsockaddr_get_port(a2);

    if (port1 < port2)
	return -1;
    else if (port1 > port2)
	return 1;

    /* TODO:
     *	Do we need to compare flow label and scope id in IPv6? 
     */
    
    /* Looks equal */
    return 0;
}

/*
 * Get first IP address associated with the hostname.
 */
bin_addr bgethostaddr(void)
{
    bsockaddr_in addr;
    const bstr_t *hostname = bgethostname();

    bsockaddr_in_set_str_addr(&addr, hostname);
    return addr.sin_addr;
}

/*
 * Get port number of a bsockaddr_in
 */
buint16_t bsockaddr_in_get_port(const bsockaddr_in *addr)
{
    return bntohs(addr->sin_port);
}

/*
 * Get the address part
 */
void* bsockaddr_get_addr(const bsockaddr_t *addr)
{
    const bsockaddr *a = (const bsockaddr*)addr;

    BASE_ASSERT_RETURN(a->addr.sa_family == BASE_AF_INET ||
		     a->addr.sa_family == BASE_AF_INET6, NULL);

    if (a->addr.sa_family == BASE_AF_INET6)
	return (void*) &a->ipv6.sin6_addr;
    else
	return (void*) &a->ipv4.sin_addr;
}

/*
 * Check if sockaddr contains a non-zero address
 */
bbool_t bsockaddr_has_addr(const bsockaddr_t *addr)
{
    const bsockaddr *a = (const bsockaddr*)addr;

    /* It's probably not wise to raise assertion here if
     * the address doesn't contain a valid address family, and
     * just return BASE_FALSE instead.
     * 
     * The reason is because application may need to distinguish 
     * these three conditions with sockaddr:
     *	a) sockaddr is not initialized. This is by convention
     *	   indicated by sa_family==0.
     *	b) sockaddr is initialized with zero address. This is
     *	   indicated with the address field having zero address.
     *	c) sockaddr is initialized with valid address/port.
     *
     * If we enable this assertion, then application will loose
     * the capability to specify condition a), since it will be
     * forced to always initialize sockaddr (even with zero address).
     * This may break some parts of upper layer libraries.
     */
    //BASE_ASSERT_RETURN(a->addr.sa_family == BASE_AF_INET ||
    //		     a->addr.sa_family == BASE_AF_INET6, BASE_FALSE);

    if (a->addr.sa_family!=BASE_AF_INET && a->addr.sa_family!=BASE_AF_INET6) {
	return BASE_FALSE;
    } else if (a->addr.sa_family == BASE_AF_INET6) {
	buint8_t zero[24];
	bbzero(zero, sizeof(zero));
	return bmemcmp(a->ipv6.sin6_addr.s6_addr, zero, 
			 sizeof(bin6_addr)) != 0;
    } else
	return a->ipv4.sin_addr.s_addr != BASE_INADDR_ANY;
}

/*
 * Get port number
 */
buint16_t bsockaddr_get_port(const bsockaddr_t *addr)
{
    const bsockaddr *a = (const bsockaddr*) addr;

    BASE_ASSERT_RETURN(a->addr.sa_family == BASE_AF_INET ||
		     a->addr.sa_family == BASE_AF_INET6, (buint16_t)0xFFFF);

    return bntohs((buint16_t)(a->addr.sa_family == BASE_AF_INET6 ?
				    a->ipv6.sin6_port : a->ipv4.sin_port));
}

/*
 * Get the length of the address part.
 */
unsigned bsockaddr_get_addr_len(const bsockaddr_t *addr)
{
    const bsockaddr *a = (const bsockaddr*) addr;
    BASE_ASSERT_RETURN(a->addr.sa_family == BASE_AF_INET ||
		     a->addr.sa_family == BASE_AF_INET6, 0);
    return a->addr.sa_family == BASE_AF_INET6 ?
	    sizeof(bin6_addr) : sizeof(bin_addr);
}

/*
 * Get socket address length.
 */
unsigned bsockaddr_get_len(const bsockaddr_t *addr)
{
    const bsockaddr *a = (const bsockaddr*) addr;
    BASE_ASSERT_RETURN(a->addr.sa_family == BASE_AF_INET ||
		     a->addr.sa_family == BASE_AF_INET6, 0);
    return a->addr.sa_family == BASE_AF_INET6 ?
	    sizeof(bsockaddr_in6) : sizeof(bsockaddr_in);
}

/*
 * Copy only the address part (sin_addr/sin6_addr) of a socket address.
 */
void bsockaddr_copy_addr( bsockaddr *dst,
				    const bsockaddr *src)
{
    /* Destination sockaddr might not be initialized */
    const char *srcbuf = (char*)bsockaddr_get_addr(src);
    char *dstbuf = ((char*)dst) + (srcbuf - (char*)src);
    bmemcpy(dstbuf, srcbuf, bsockaddr_get_addr_len(src));
}

/*
 * Copy socket address.
 */
void bsockaddr_cp(bsockaddr_t *dst, const bsockaddr_t *src)
{
    bmemcpy(dst, src, bsockaddr_get_len(src));
}

/*
 * Synthesize address.
 */
bstatus_t bsockaddr_synthesize(int dst_af,
				           bsockaddr_t *dst,
				           const bsockaddr_t *src)
{
    char ip_addr_buf[BASE_INET6_ADDRSTRLEN];
    unsigned int count = 1;
    baddrinfo ai[1];
    bstr_t ip_addr;
    bstatus_t status;

    /* Validate arguments */
    BASE_ASSERT_RETURN(src && dst, BASE_EINVAL);

    if (dst_af == ((const bsockaddr *)src)->addr.sa_family) {
        bsockaddr_cp(dst, src);
        return BASE_SUCCESS;
    }

    bsockaddr_print(src, ip_addr_buf, sizeof(ip_addr_buf), 0);
    ip_addr = bstr(ip_addr_buf);
    
    /* Try to synthesize address using bgetaddrinfo(). */
    status = bgetaddrinfo(dst_af, &ip_addr, &count, ai); 
    if (status == BASE_SUCCESS && count > 0) {
    	bsockaddr_cp(dst, &ai[0].ai_addr);
    	bsockaddr_set_port(dst, bsockaddr_get_port(src));
    }
    
    return status;
}

/*
 * Set port number of bsockaddr_in
 */
void bsockaddr_in_set_port(bsockaddr_in *addr, 
				     buint16_t hostport)
{
    addr->sin_port = bhtons(hostport);
}

/*
 * Set port number of bsockaddr
 */
bstatus_t bsockaddr_set_port(bsockaddr *addr, 
					 buint16_t hostport)
{
    int af = addr->addr.sa_family;

    BASE_ASSERT_RETURN(af==BASE_AF_INET || af==BASE_AF_INET6, BASE_EINVAL);

    if (af == BASE_AF_INET6)
	addr->ipv6.sin6_port = bhtons(hostport);
    else
	addr->ipv4.sin_port = bhtons(hostport);

    return BASE_SUCCESS;
}

/*
 * Get IPv4 address
 */
bin_addr bsockaddr_in_get_addr(const bsockaddr_in *addr)
{
    bin_addr in_addr;
    in_addr.s_addr = bntohl(addr->sin_addr.s_addr);
    return in_addr;
}

/*
 * Set IPv4 address
 */
void bsockaddr_in_set_addr(bsockaddr_in *addr,
				     buint32_t hostaddr)
{
    addr->sin_addr.s_addr = bhtonl(hostaddr);
}

/*
 * Parse address
 */
bstatus_t bsockaddr_parse2(int af, unsigned options,
				       const bstr_t *str,
				       bstr_t *p_hostpart,
				       buint16_t *p_port,
				       int *raf)
{
    const char *end = str->ptr + str->slen;
    const char *last_colon_pos = NULL;
    unsigned colon_cnt = 0;
    const char *p;

    BASE_ASSERT_RETURN((af==BASE_AF_INET || af==BASE_AF_INET6 || af==BASE_AF_UNSPEC) &&
		     options==0 &&
		     str!=NULL, BASE_EINVAL);

    /* Special handling for empty input */
    if (str->slen==0 || str->ptr==NULL) {
	if (p_hostpart)
	    p_hostpart->slen = 0;
	if (p_port)
	    *p_port = 0;
	if (raf)
	    *raf = BASE_AF_INET;
	return BASE_SUCCESS;
    }

    /* Count the colon and get the last colon */
    for (p=str->ptr; p!=end; ++p) {
	if (*p == ':') {
	    ++colon_cnt;
	    last_colon_pos = p;
	}
    }

    /* Deduce address family if it's not given */
    if (af == BASE_AF_UNSPEC) {
	if (colon_cnt > 1)
	    af = BASE_AF_INET6;
	else
	    af = BASE_AF_INET;
    } else if (af == BASE_AF_INET && colon_cnt > 1)
	return BASE_EINVAL;

    if (raf)
	*raf = af;

    if (af == BASE_AF_INET) {
	/* Parse as IPv4. Supported formats:
	 *  - "10.0.0.1:80"
	 *  - "10.0.0.1"
	 *  - "10.0.0.1:"
	 *  - ":80"
	 *  - ":"
	 */
	bstr_t hostpart;
	unsigned long port;

	hostpart.ptr = (char*)str->ptr;

	if (last_colon_pos) {
	    bstr_t port_part;
	    int i;

	    hostpart.slen = last_colon_pos - str->ptr;

	    port_part.ptr = (char*)last_colon_pos + 1;
	    port_part.slen = end - port_part.ptr;

	    /* Make sure port number is valid */
	    for (i=0; i<port_part.slen; ++i) {
		if (!bisdigit(port_part.ptr[i]))
		    return BASE_EINVAL;
	    }
	    port = bstrtoul(&port_part);
	    if (port > 65535)
		return BASE_EINVAL;
	} else {
	    hostpart.slen = str->slen;
	    port = 0;
	}

	if (p_hostpart)
	    *p_hostpart = hostpart;
	if (p_port)
	    *p_port = (buint16_t)port;

	return BASE_SUCCESS;

    } else if (af == BASE_AF_INET6) {

	/* Parse as IPv6. Supported formats:
	 *  - "fe::01:80"  ==> note: port number is zero in this case, not 80!
	 *  - "[fe::01]:80"
	 *  - "fe::01"
	 *  - "fe::01:"
	 *  - "[fe::01]"
	 *  - "[fe::01]:"
	 *  - "[::]:80"
	 *  - ":::80"
	 *  - "[::]"
	 *  - "[::]:"
	 *  - ":::"
	 *  - "::"
	 */
	bstr_t hostpart, port_part;

	if (*str->ptr == '[') {
	    char *end_bracket;
	    int i;
	    unsigned long port;

	    if (last_colon_pos == NULL)
		return BASE_EINVAL;

	    end_bracket = bstrchr(str, ']');
	    if (end_bracket == NULL)
		return BASE_EINVAL;

	    hostpart.ptr = (char*)str->ptr + 1;
	    hostpart.slen = end_bracket - hostpart.ptr;

	    if (last_colon_pos < end_bracket) {
		port_part.ptr = NULL;
		port_part.slen = 0;
	    } else {
		port_part.ptr = (char*)last_colon_pos + 1;
		port_part.slen = end - port_part.ptr;
	    }

	    /* Make sure port number is valid */
	    for (i=0; i<port_part.slen; ++i) {
		if (!bisdigit(port_part.ptr[i]))
		    return BASE_EINVAL;
	    }
	    port = bstrtoul(&port_part);
	    if (port > 65535)
		return BASE_EINVAL;

	    if (p_hostpart)
		*p_hostpart = hostpart;
	    if (p_port)
		*p_port = (buint16_t)port;

	    return BASE_SUCCESS;

	} else {
	    /* Treat everything as part of the IPv6 IP address */
	    if (p_hostpart)
		*p_hostpart = *str;
	    if (p_port)
		*p_port = 0;

	    return BASE_SUCCESS;
	}

    } else {
	return BASE_EAFNOTSUP;
    }

}

/*
 * Parse address
 */
bstatus_t bsockaddr_parse( int af, unsigned options,
				       const bstr_t *str,
				       bsockaddr *addr)
{
    bstr_t hostpart;
    buint16_t port;
    bstatus_t status;

    BASE_ASSERT_RETURN(addr, BASE_EINVAL);
    BASE_ASSERT_RETURN(af==BASE_AF_UNSPEC ||
		     af==BASE_AF_INET ||
		     af==BASE_AF_INET6, BASE_EINVAL);
    BASE_ASSERT_RETURN(options == 0, BASE_EINVAL);

    status = bsockaddr_parse2(af, options, str, &hostpart, &port, &af);
    if (status != BASE_SUCCESS)
	return status;
    
#if !defined(BASE_HAS_IPV6) || !BASE_HAS_IPV6
    if (af==BASE_AF_INET6)
	return BASE_EIPV6NOTSUP;
#endif

    status = bsockaddr_init(af, addr, &hostpart, port);
#if defined(BASE_HAS_IPV6) && BASE_HAS_IPV6
    if (status != BASE_SUCCESS && af == BASE_AF_INET6) {
	/* Parsing does not yield valid address. Try to treat the last 
	 * portion after the colon as port number.
	 */
	const char *last_colon_pos=NULL, *p;
	const char *end = str->ptr + str->slen;
	unsigned long long_port;
	bstr_t port_part;
	int i;

	/* Parse as IPv6:port */
	for (p=str->ptr; p!=end; ++p) {
	    if (*p == ':')
		last_colon_pos = p;
	}

	if (last_colon_pos == NULL)
	    return status;

	hostpart.ptr = (char*)str->ptr;
	hostpart.slen = last_colon_pos - str->ptr;

	port_part.ptr = (char*)last_colon_pos + 1;
	port_part.slen = end - port_part.ptr;

	/* Make sure port number is valid */
	for (i=0; i<port_part.slen; ++i) {
	    if (!bisdigit(port_part.ptr[i]))
		return status;
	}
	long_port = bstrtoul(&port_part);
	if (long_port > 65535)
	    return status;

	port = (buint16_t)long_port;

	status = bsockaddr_init(BASE_AF_INET6, addr, &hostpart, port);
    }
#endif
    
    return status;
}

/* Resolve the IP address of local machine */
bstatus_t bgethostip(int af, bsockaddr *addr)
{
    unsigned i, count, cand_cnt;
    enum {
	CAND_CNT = 8,

	/* Weighting to be applied to found addresses */
	WEIGHT_HOSTNAME	= 1,	/* hostname IP is not always valid! */
	WEIGHT_DEF_ROUTE = 2,
	WEIGHT_INTERFACE = 1,
	WEIGHT_LOOPBACK = -5,
	WEIGHT_LINK_LOCAL = -4,
	WEIGHT_DISABLED = -50,

	MIN_WEIGHT = WEIGHT_DISABLED+1	/* minimum weight to use */
    };
    /* candidates: */
    bsockaddr cand_addr[CAND_CNT];
    int		cand_weight[CAND_CNT];
    int	        selected_cand;
    char	strip[BASE_INET6_ADDRSTRLEN+10];
    /* Special IPv4 addresses. */
    struct spec_ipv4_t
    {
	buint32_t addr;
	buint32_t mask;
	int	    weight;
    } spec_ipv4[] =
    {
	/* 127.0.0.0/8, loopback addr will be used if there is no other
	 * addresses.
	 */
	{ 0x7f000000, 0xFF000000, WEIGHT_LOOPBACK },

	/* 0.0.0.0/8, special IP that doesn't seem to be practically useful */
	{ 0x00000000, 0xFF000000, WEIGHT_DISABLED },

	/* 169.254.0.0/16, a zeroconf/link-local address, which has higher
	 * priority than loopback and will be used if there is no other
	 * valid addresses.
	 */
	{ 0xa9fe0000, 0xFFFF0000, WEIGHT_LINK_LOCAL }
    };
    /* Special IPv6 addresses */
    struct spec_ipv6_t
    {
	buint8_t addr[16];
	buint8_t mask[16];
	int	   weight;
    } spec_ipv6[] =
    {
	/* Loopback address, ::1/128 */
	{ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	  {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	   0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
	  WEIGHT_LOOPBACK
	},

	/* Link local, fe80::/10 */
	{ {0xfe,0x80,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	  {0xff,0xc0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	  WEIGHT_LINK_LOCAL
	},

	/* Disabled, ::/128 */
	{ {0x0,0x0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	{ 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
	  WEIGHT_DISABLED
	}
    };
    baddrinfo ai;
    bstatus_t status;

    /* May not be used if MTRACE is disabled */
    BASE_UNUSED_ARG(strip);

#ifdef _MSC_VER
    /* Get rid of "uninitialized he variable" with MS compilers */
    bbzero(&ai, sizeof(ai));
#endif

    cand_cnt = 0;
    bbzero(cand_addr, sizeof(cand_addr));
    bbzero(cand_weight, sizeof(cand_weight));
    for (i=0; i<BASE_ARRAY_SIZE(cand_addr); ++i) {
	cand_addr[i].addr.sa_family = (buint16_t)af;
	BASE_SOCKADDR_RESET_LEN(&cand_addr[i]);
    }

    addr->addr.sa_family = (buint16_t)af;
    BASE_SOCKADDR_RESET_LEN(addr);

#if !defined(BASE_GETHOSTIP_DISABLE_LOCAL_RESOLUTION) || \
    BASE_GETHOSTIP_DISABLE_LOCAL_RESOLUTION == 0
    /* Get hostname's IP address */
    {
	const bstr_t *hostname = bgethostname();
	count = 1;

	if (hostname->slen > 0)
	    status = bgetaddrinfo(af, hostname, &count, &ai);
	else
	    status = BASE_ERESOLVE;

	if (status == BASE_SUCCESS) {
    	    bassert(ai.ai_addr.addr.sa_family == (buint16_t)af);
    	    bsockaddr_copy_addr(&cand_addr[cand_cnt], &ai.ai_addr);
	    bsockaddr_set_port(&cand_addr[cand_cnt], 0);
	    cand_weight[cand_cnt] += WEIGHT_HOSTNAME;
	    ++cand_cnt;

	    MTRACE( "hostname IP is %s",  bsockaddr_print(&ai.ai_addr, strip, sizeof(strip), 3));
	}
    }
#else
    BASE_UNUSED_ARG(ai);
#endif

    /* Get default interface (interface for default route) */
    if (cand_cnt < BASE_ARRAY_SIZE(cand_addr)) {
	status = bgetdefaultipinterface(af, addr);
	if (status == BASE_SUCCESS) {
	    MTRACE( "default IP is %s",  bsockaddr_print(addr, strip, sizeof(strip), 3));

	    bsockaddr_set_port(addr, 0);
	    for (i=0; i<cand_cnt; ++i) {
		if (bsockaddr_cmp(&cand_addr[i], addr)==0)
		    break;
	    }

	    cand_weight[i] += WEIGHT_DEF_ROUTE;
	    if (i >= cand_cnt) {
		bsockaddr_copy_addr(&cand_addr[i], addr);
		++cand_cnt;
	    }
	}
    }


    /* Enumerate IP interfaces */
    if (cand_cnt < BASE_ARRAY_SIZE(cand_addr)) {
	unsigned start_if = cand_cnt;
	count = BASE_ARRAY_SIZE(cand_addr) - start_if;

	status = benum_ip_interface(af, &count, &cand_addr[start_if]);
	if (status == BASE_SUCCESS && count) {
	    /* Clear the port number */
	    for (i=0; i<count; ++i)
		bsockaddr_set_port(&cand_addr[start_if+i], 0);

	    /* For each candidate that we found so far (that is the hostname
	     * address and default interface address, check if they're found
	     * in the interface list. If found, add the weight, and if not,
	     * decrease the weight.
	     */
	    for (i=0; i<cand_cnt; ++i) {
		unsigned j;
		for (j=0; j<count; ++j) {
		    if (bsockaddr_cmp(&cand_addr[i], 
					&cand_addr[start_if+j])==0)
			break;
		}

		if (j == count) {
		    /* Not found */
		    cand_weight[i] -= WEIGHT_INTERFACE;
		} else {
		    cand_weight[i] += WEIGHT_INTERFACE;
		}
	    }

	    /* Add remaining interface to candidate list. */
	    for (i=0; i<count; ++i) {
		unsigned j;
		for (j=0; j<cand_cnt; ++j) {
		    if (bsockaddr_cmp(&cand_addr[start_if+i], 
					&cand_addr[j])==0)
			break;
		}

		if (j == cand_cnt) {
		    bsockaddr_copy_addr(&cand_addr[cand_cnt], 
					  &cand_addr[start_if+i]);
		    cand_weight[cand_cnt] += WEIGHT_INTERFACE;
		    ++cand_cnt;
		}
	    }
	}
    }

    /* Apply weight adjustment for special IPv4/IPv6 addresses
     * See http://trac.extsip.org/repos/ticket/1046
     */
    if (af == BASE_AF_INET) {
	for (i=0; i<cand_cnt; ++i) {
	    unsigned j;
	    for (j=0; j<BASE_ARRAY_SIZE(spec_ipv4); ++j) {
		    buint32_t a = bntohl(cand_addr[i].ipv4.sin_addr.s_addr);
		    buint32_t pa = spec_ipv4[j].addr;
		    buint32_t pm = spec_ipv4[j].mask;

		    if ((a & pm) == pa) {
			cand_weight[i] += spec_ipv4[j].weight;
			break;
		    }
	    }
	}
    } else if (af == BASE_AF_INET6) {
	for (i=0; i<BASE_ARRAY_SIZE(spec_ipv6); ++i) {
		unsigned j;
		for (j=0; j<cand_cnt; ++j) {
		    buint8_t *a = cand_addr[j].ipv6.sin6_addr.s6_addr;
		    buint8_t am[16];
		    buint8_t *pa = spec_ipv6[i].addr;
		    buint8_t *pm = spec_ipv6[i].mask;
		    unsigned k;

		    for (k=0; k<16; ++k) {
			am[k] = (buint8_t)((a[k] & pm[k]) & 0xFF);
		    }

		    if (bmemcmp(am, pa, 16)==0) {
			cand_weight[j] += spec_ipv6[i].weight;
		    }
		}
	}
    } else {
	return BASE_EAFNOTSUP;
    }

    /* Enumerate candidates to get the best IP address to choose */
    selected_cand = -1;
    for (i=0; i<cand_cnt; ++i) {
	MTRACE("Checking candidate IP %s, weight=%d", bsockaddr_print(&cand_addr[i], strip, sizeof(strip), 3), cand_weight[i]);

	if (cand_weight[i] < MIN_WEIGHT) {
	    continue;
	}

	if (selected_cand == -1)
	    selected_cand = i;
	else if (cand_weight[i] > cand_weight[selected_cand])
	    selected_cand = i;
    }

    /* If else fails, returns loopback interface as the last resort */
    if (selected_cand == -1) {
	if (af==BASE_AF_INET) {
	    addr->ipv4.sin_addr.s_addr = bhtonl (0x7f000001);
	} else {
	    bin6_addr *s6_addr;

	    s6_addr = (bin6_addr*) bsockaddr_get_addr(addr);
	    bbzero(s6_addr, sizeof(bin6_addr));
	    s6_addr->s6_addr[15] = 1;
	}
	MTRACE("Loopback IP %s returned", bsockaddr_print(addr, strip, sizeof(strip), 3));
    } else {
	bsockaddr_copy_addr(addr, &cand_addr[selected_cand]);
	MTRACE( "Candidate %s selected", bsockaddr_print(addr, strip, sizeof(strip), 3));
    }

    return BASE_SUCCESS;
}

/* Get IP interface for sending to the specified destination */
bstatus_t bgetipinterface(int af,
                                      const bstr_t *dst,
                                      bsockaddr *itf_addr,
                                      bbool_t allow_resolve,
                                      bsockaddr *p_dst_addr)
{
    bsockaddr dst_addr;
    bsock_t fd;
    int len;
    buint8_t zero[64];
    bstatus_t status;

    bsockaddr_init(af, &dst_addr, NULL, 53);
    status = binet_pton(af, dst, bsockaddr_get_addr(&dst_addr));
    if (status != BASE_SUCCESS) {
	/* "dst" is not an IP address. */
	if (allow_resolve) {
	    status = bsockaddr_init(af, &dst_addr, dst, 53);
	} else {
	    bstr_t cp;

	    if (af == BASE_AF_INET) {
		cp = bstr("1.1.1.1");
	    } else {
		cp = bstr("1::1");
	    }
	    status = bsockaddr_init(af, &dst_addr, &cp, 53);
	}

	if (status != BASE_SUCCESS)
	    return status;
    }

    /* Create UDP socket and connect() to the destination IP */
    status = bsock_socket(af, bSOCK_DGRAM(), 0, &fd);
    if (status != BASE_SUCCESS) {
	return status;
    }

    status = bsock_connect(fd, &dst_addr, bsockaddr_get_len(&dst_addr));
    if (status != BASE_SUCCESS) {
	bsock_close(fd);
	return status;
    }

    len = sizeof(*itf_addr);
    status = bsock_getsockname(fd, itf_addr, &len);
    if (status != BASE_SUCCESS) {
	bsock_close(fd);
	return status;
    }

    bsock_close(fd);

    /* Check that the address returned is not zero */
    bbzero(zero, sizeof(zero));
    if (bmemcmp(bsockaddr_get_addr(itf_addr), zero,
		  bsockaddr_get_addr_len(itf_addr))==0)
    {
	return BASE_ENOTFOUND;
    }

    if (p_dst_addr)
	*p_dst_addr = dst_addr;

    return BASE_SUCCESS;
}

/* Get the default IP interface */
bstatus_t bgetdefaultipinterface(int af, bsockaddr *addr)
{
    bstr_t cp;

    if (af == BASE_AF_INET) {
	cp = bstr("1.1.1.1");
    } else {
	cp = bstr("1::1");
    }

    return bgetipinterface(af, &cp, addr, BASE_FALSE, NULL);
}


/*
 * Bind socket at random port.
 */
bstatus_t bsock_bind_random(  bsock_t sockfd,
				          const bsockaddr_t *addr,
				          buint16_t port_range,
				          buint16_t max_try)
{
    bsockaddr bind_addr;
    int addr_len;
    buint16_t bport;
    bstatus_t status = BASE_SUCCESS;

    BASE_CHECK_STACK();

    BASE_ASSERT_RETURN(addr, BASE_EINVAL);

    bsockaddr_cp(&bind_addr, addr);
    addr_len = bsockaddr_get_len(addr);
    bport = bsockaddr_get_port(addr);

    if (bport == 0 || port_range == 0) {
	return bsock_bind(sockfd, &bind_addr, addr_len);
    }

    for (; max_try; --max_try) {
	buint16_t port;
	port = (buint16_t)(bport + brand() % (port_range + 1));
	bsockaddr_set_port(&bind_addr, port);
	status = bsock_bind(sockfd, &bind_addr, addr_len);
	if (status == BASE_SUCCESS)
	    break;
    }

    return status;
}


/*
 * Adjust socket send/receive buffer size.
 */
bstatus_t bsock_setsockopt_sobuf( bsock_t sockfd,
					      buint16_t optname,
					      bbool_t auto_retry,
					      unsigned *buf_size)
{
    bstatus_t status;
    int try_size, cur_size, i, step, size_len;
    enum { MAX_TRY = 20 };

    BASE_CHECK_STACK();

    BASE_ASSERT_RETURN(sockfd != BASE_INVALID_SOCKET &&
		     buf_size &&
		     *buf_size > 0 &&
		     (optname == bSO_RCVBUF() ||
		      optname == bSO_SNDBUF()),
		     BASE_EINVAL);

    size_len = sizeof(cur_size);
    status = bsock_getsockopt(sockfd, bSOL_SOCKET(), optname,
				&cur_size, &size_len);
    if (status != BASE_SUCCESS)
	return status;

    try_size = *buf_size;
    step = (try_size - cur_size) / MAX_TRY;
    if (step < 4096)
	step = 4096;

    for (i = 0; i < (MAX_TRY-1); ++i) {
	if (try_size <= cur_size) {
	    /* Done, return current size */
	    *buf_size = cur_size;
	    break;
	}

	status = bsock_setsockopt(sockfd, bSOL_SOCKET(), optname,
				    &try_size, sizeof(try_size));
	if (status == BASE_SUCCESS) {
	    status = bsock_getsockopt(sockfd, bSOL_SOCKET(), optname,
					&cur_size, &size_len);
	    if (status != BASE_SUCCESS) {
		/* Ops! No info about current size, just return last try size
		 * and quit.
		 */
		*buf_size = try_size;
		break;
	    }
	}

	if (!auto_retry)
	    break;

	try_size -= step;
    }

    return status;
}


char * baddr_str_print( const bstr_t *host_str, int port, 
				  char *buf, int size, unsigned flag)
{
    enum {
	WITH_PORT = 1
    };
    char *bquote, *equote;
    int af = bAF_UNSPEC();    
    bin6_addr dummy6;

    /* Check if this is an IPv6 address */
    if (binet_pton(bAF_INET6(), host_str, &dummy6) == BASE_SUCCESS)
	af = bAF_INET6();

    if (af == bAF_INET6()) {
	bquote = "[";
	equote = "]";    
    } else {
	bquote = "";
	equote = "";    
    } 

    if (flag & WITH_PORT) {
	bansi_snprintf(buf, size, "%s%.*s%s:%d",
			 bquote, (int)host_str->slen, host_str->ptr, equote, 
			 port);
    } else {
	bansi_snprintf(buf, size, "%s%.*s%s",
			 bquote, (int)host_str->slen, host_str->ptr, equote);
    }
    return buf;
}


/* Only need to implement these in DLL build */
#if defined(BASE_DLL)

buint16_t bAF_UNSPEC(void)
{
    return BASE_AF_UNSPEC;
}

buint16_t bAF_UNIX(void)
{
    return BASE_AF_UNIX;
}

buint16_t bAF_INET(void)
{
    return BASE_AF_INET;
}

buint16_t bAF_INET6(void)
{
    return BASE_AF_INET6;
}

buint16_t bAF_PACKET(void)
{
    return BASE_AF_PACKET;
}

buint16_t bAF_IRDA(void)
{
    return BASE_AF_IRDA;
}

int bSOCK_STREAM(void)
{
    return BASE_SOCK_STREAM;
}

int bSOCK_DGRAM(void)
{
    return BASE_SOCK_DGRAM;
}

int bSOCK_RAW(void)
{
    return BASE_SOCK_RAW;
}

int bSOCK_RDM(void)
{
    return BASE_SOCK_RDM;
}

buint16_t bSOL_SOCKET(void)
{
    return BASE_SOL_SOCKET;
}

buint16_t bSOL_IP(void)
{
    return BASE_SOL_IP;
}

buint16_t bSOL_TCP(void)
{
    return BASE_SOL_TCP;
}

buint16_t bSOL_UDP(void)
{
    return BASE_SOL_UDP;
}

buint16_t bSOL_IPV6(void)
{
    return BASE_SOL_IPV6;
}

int bIP_TOS(void)
{
    return BASE_IP_TOS;
}

int bIPTOS_LOWDELAY(void)
{
    return BASE_IPTOS_LOWDELAY;
}

int bIPTOS_THROUGHPUT(void)
{
    return BASE_IPTOS_THROUGHPUT;
}

int bIPTOS_RELIABILITY(void)
{
    return BASE_IPTOS_RELIABILITY;
}

int bIPTOS_MINCOST(void)
{
    return BASE_IPTOS_MINCOST;
}

int bIPV6_TCLASS(void)
{
    return BASE_IPV6_TCLASS;
}

buint16_t bSO_TYPE(void)
{
    return BASE_SO_TYPE;
}

buint16_t bSO_RCVBUF(void)
{
    return BASE_SO_RCVBUF;
}

buint16_t bSO_SNDBUF(void)
{
    return BASE_SO_SNDBUF;
}

buint16_t bTCP_NODELAY(void)
{
    return BASE_TCP_NODELAY;
}

buint16_t bSO_REUSEADDR(void)
{
    return BASE_SO_REUSEADDR;
}

buint16_t bSO_NOSIGPIPE(void)
{
    return BASE_SO_NOSIGPIPE;
}

buint16_t bSO_PRIORITY(void)
{
    return BASE_SO_PRIORITY;
}

buint16_t bIP_MULTICAST_IF(void)
{
    return BASE_IP_MULTICAST_IF;
}

buint16_t bIP_MULTICAST_TTL(void)
{
    return BASE_IP_MULTICAST_TTL;
}

buint16_t bIP_MULTICAST_LOOP(void)
{
    return BASE_IP_MULTICAST_LOOP;
}

buint16_t bIP_ADD_MEMBERSHIP(void)
{
    return BASE_IP_ADD_MEMBERSHIP;
}

buint16_t bIP_DROP_MEMBERSHIP(void)
{
    return BASE_IP_DROP_MEMBERSHIP;
}

int bMSG_OOB(void)
{
    return BASE_MSG_OOB;
}

int bMSG_PEEK(void)
{
    return BASE_MSG_PEEK;
}

int bMSG_DONTROUTE(void)
{
    return BASE_MSG_DONTROUTE;
}

#endif	/* BASE_DLL */

