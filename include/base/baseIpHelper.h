/* 
 *
 */
#ifndef __BASE_IP_ROUTE_H__
#define __BASE_IP_ROUTE_H__

/**
 * @brief IP helper API
 */

#include <baseSock.h>

BASE_BEGIN_DECL

/**
 * @defgroup bip_helper IP Interface and Routing Helper
 * @ingroup BASE_IO
 * @{
 *
 * This module provides functions to query local host's IP interface and 
 * routing table.
 */

/**
 * This structure describes IP routing entry.
 */
typedef union bip_route_entry
{
	/** IP routing entry for IP version 4 routing */
	struct
	{
		bin_addr	if_addr;    /**< Local interface IP address.	*/
		bin_addr	dst_addr;   /**< Destination IP address.	*/
		bin_addr	mask;	    /**< Destination mask.		*/
	} ipv4;
} bip_route_entry;


/**
 * Enumerate the local IP interfaces currently active in the host.
 *
 * @param af	    Family of the address to be retrieved. Application
 *		    may specify bAF_UNSPEC() to retrieve all addresses,
 *		    or bAF_INET() or bAF_INET6() to retrieve interfaces
 *		    with specific address family.
 * @param count	    On input, specify the number of entries. On output,
 *		    it will be filled with the actual number of entries.
 * @param ifs	    Array of socket addresses, which address part will
 *		    be filled with the interface address. The address
 *		    family part will be initialized with the address
 *		    family of the IP address.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t benum_ip_interface(int af, unsigned *count, bsockaddr ifs[]);


/**
 * Enumerate the IP routing table for this host.
 *
 * @param count	    On input, specify the number of routes entries. On output,
 *		    it will be filled with the actual number of route entries.
 * @param routes    Array of IP routing entries.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t benum_ip_route(unsigned *count, bip_route_entry routes[]);

BASE_END_DECL


#endif

