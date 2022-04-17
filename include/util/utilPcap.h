/* 
 *
 */
#ifndef __UTIL_PCAP_H__
#define __UTIL_PCAP_H__

/**
 * @brief Simple PCAP file reader
 */

#include <_baseTypes.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_PCAP Simple PCAP file reader
 * @ingroup BASE_FILE_FMT
 * @{
 * This module describes simple utility to read PCAP file. It is not intended
 * to support all PCAP features (that's what libpcap is for!), but it can
 * be useful for example to playback or stream PCAP contents.
 */

/**
 * Enumeration to describe supported data link types.
 */
typedef enum bpcap_link_type
{
    /** Ethernet data link */
    BASE_PCAP_LINK_TYPE_ETH   = 1

} bpcap_link_type;


/**
 * Enumeration to describe supported protocol types.
 */
typedef enum bpcap_proto_type
{
    /** UDP protocol */
    BASE_PCAP_PROTO_TYPE_UDP  = 17

} bpcap_proto_type;


/**
 * This describes UDP header, which may optionally be returned in
 * #bpcap_read_udp() function. All fields are in network byte order.
 */
typedef struct bpcap_udp_hdr
{
    buint16_t	src_port;   /**< Source port.	    */
    buint16_t	dst_port;   /**< Destination port   */
    buint16_t	len;	    /**< Length.	    */
    buint16_t	csum;	    /**< Checksum.	    */
} bpcap_udp_hdr;


/**
 * This structure describes the filter to be used when reading packets from
 * a PCAP file. When a filter is configured, only packets matching all the
 * filter specifications will be read from PCAP file.
 */
typedef struct bpcap_filter
{
    /**
     * Select data link type, or zero to include any supported data links.
     */
    bpcap_link_type	link;

    /**
     * Select protocol, or zero to include all supported protocols.
     */
    bpcap_proto_type	proto;

    /**
     * Specify source IP address of the packets, or zero to include packets
     * from any IP addresses. Note that IP address here must be in
     * network byte order.
     */
    buint32_t		ip_src;

    /**
     * Specify destination IP address of the packets, or zero to include packets
     * destined to any IP addresses. Note that IP address here must be in
     * network byte order.
     */
    buint32_t		ip_dst;

    /**
     * Specify source port of the packets, or zero to include packets with
     * any source port number. Note that the port number must be in network
     * byte order.
     */
    buint16_t		src_port;

    /**
     * Specify destination port of the packets, or zero to include packets with
     * any destination port number. Note that the port number must be in network
     * byte order.
     */
    buint16_t		dst_port;

} bpcap_filter;


/** Opaque declaration for PCAP file */
typedef struct bpcap_file bpcap_file;


/**
 * Initialize filter with default values. The default value is to allow
 * any packets.
 *
 * @param filter    Filter to be initialized.
 */
void bpcap_filter_default(bpcap_filter *filter);

/**
 * Open PCAP file.
 *
 * @param pool	    Pool to allocate memory.
 * @param path	    File/path name.
 * @param p_file    Pointer to receive PCAP file handle.
 *
 * @return	    BASE_SUCCESS if file can be opened successfully.
 */
bstatus_t bpcap_open(bpool_t *pool,
				  const char *path,
				  bpcap_file **p_file);

/**
 * Close PCAP file.
 *
 * @param file	    PCAP file handle.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bpcap_close(bpcap_file *file);

/**
 * Configure filter for reading the file. When filter is configured,
 * only packets matching all the filter settings will be returned.
 *
 * @param file	    PCAP file handle.
 * @param filter    The filter.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bpcap_set_filter(bpcap_file *file,
				        const bpcap_filter *filter);

/**
 * Read UDP payload from the next packet in the PCAP file. Optionally it
 * can return the UDP header, if caller supplies it.
 *
 * @param file		    PCAP file handle.
 * @param udp_hdr	    Optional buffer to receive UDP header.
 * @param udp_payload	    Buffer to receive the UDP payload.
 * @param udp_payload_size  On input, specify the size of the buffer.
 *			    On output, it will be filled with the actual size
 *			    of the payload as read from the packet.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bpcap_read_udp(bpcap_file *file,
				      bpcap_udp_hdr *udp_hdr,
				      buint8_t *udp_payload,
				      bsize_t *udp_payload_size);

BASE_END_DECL

#endif

