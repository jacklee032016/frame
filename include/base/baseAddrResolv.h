/* 
 *
 */
#ifndef __BASE_ADDR_RESOLV_H__
#define __BASE_ADDR_RESOLV_H__

/**
 * @brief IP address resolution.
 */

#include <baseSock.h>

BASE_BEGIN_DECL

/**
 * @defgroup baddr_resolve Network Address Resolution
 * @ingroup BASE_IO
 * @{
 *
 * This module provides function to resolve Internet address of the
 * specified host name. To resolve a particular host name, application
 * can just call #bgethostbyname().
 *
 * Example:
 * <pre>
 *   ...
 *   bhostent he;
 *   bstatus_t rc;
 *   bstr_t host = bstr("host.example.com");
 *   
 *   rc = bgethostbyname( &host, &he);
 *   if (rc != BASE_SUCCESS) {
 *      char errbuf[80];
 *      extStrError( rc, errbuf, sizeof(errbuf));
 *      BASE_CRIT("Unable to resolve host, error=%s", errbuf);
 *      return rc;
 *   }
 *
 *   // process address...
 *   addr.sin_addr.s_addr = *(buint32_t*)he.h_addr;
 *   ...
 * </pre>
 *
 * It's pretty simple really...
 */

/** This structure describes an Internet host address. */
typedef struct bhostent
{
    char    *h_name;		/**< The official name of the host. */
    char   **h_aliases;		/**< Aliases list. */
    int	     h_addrtype;	/**< Host address type. */
    int	     h_length;		/**< Length of address. */
    char   **h_addr_list;	/**< List of addresses. */
} bhostent;

/** Shortcut to h_addr_list[0] */
#define h_addr h_addr_list[0]

/** 
 * This structure describes address information bgetaddrinfo().
 */
typedef struct baddrinfo
{
    char	 ai_canonname[BASE_MAX_HOSTNAME]; /**< Canonical name for host*/
    bsockaddr  ai_addr;			/**< Binary address.	    */
} baddrinfo;


/**
 * This function fills the structure of type bhostent for a given host name.
 * For host resolution function that also works with IPv6, please see
 * #bgetaddrinfo().
 *
 * @param name	    Host name to resolve. Specifying IPv4 address here
 *		    may fail on some platforms (e.g. Windows)
 * @param he	    The bhostent structure to be filled. Note that
 *		    the pointers in this structure points to temporary
 *		    variables which value will be reset upon subsequent
 *		    invocation.
 *
 * @return	    BASE_SUCCESS, or the appropriate error codes.
 */ 
bstatus_t bgethostbyname(const bstr_t *name, bhostent *he);


/**
 * Resolve the primary IP address of local host. 
 *
 * @param af	    The desired address family to query. Valid values
 *		    are bAF_INET() or bAF_INET6().
 * @param addr      On successful resolution, the address family and address
 *		    part of this socket address will be filled up with the host
 *		    IP address, in network byte order. Other parts of the socket
 *		    address are untouched.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bgethostip(int af, bsockaddr *addr);


/**
 * Get the interface IP address to send data to the specified destination.
 *
 * @param af	    The desired address family to query. Valid values
 *		    are bAF_INET() or bAF_INET6().
 * @param dst	    The destination host.
 * @param itf_addr  On successful resolution, the address family and address
 *		    part of this socket address will be filled up with the host
 *		    IP address, in network byte order. Other parts of the socket
 *		    address should be ignored.
 * @param allow_resolve   If \a dst may contain hostname (instead of IP
 * 		    address), specify whether hostname resolution should
 * 	            be performed. If not, default interface address will
 *  		    be returned.
 * @param p_dst_addr If not NULL, it will be filled with the IP address of
 * 		    the destination host.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bgetipinterface(int af,
                                       const bstr_t *dst,
                                       bsockaddr *itf_addr,
                                       bbool_t allow_resolve,
                                       bsockaddr *p_dst_addr);

/**
 * Get the IP address of the default interface. Default interface is the
 * interface of the default route.
 *
 * @param af	    The desired address family to query. Valid values
 *		    are bAF_INET() or bAF_INET6().
 * @param addr      On successful resolution, the address family and address
 *		    part of this socket address will be filled up with the host
 *		    IP address, in network byte order. Other parts of the socket
 *		    address are untouched.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bgetdefaultipinterface(int af, bsockaddr *addr);


/**
 * This function translates the name of a service location (for example, 
 * a host name) and returns a set of addresses and associated information
 * to be used in creating a socket with which to address the specified 
 * service.
 *
 * @param af	    The desired address family to query. Valid values
 *		    are bAF_INET(), bAF_INET6(), or bAF_UNSPEC().
 * @param name	    Descriptive name or an address string, such as host name.
 * @param count	    On input, it specifies the number of elements in
 *		    \a ai array. On output, this will be set with the
 *		    number of address informations found for the
 *		    specified name.
 * @param ai	    Array of address info to be filled with the information about the host.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bgetaddrinfo(int af, const bstr_t *name, unsigned *count, baddrinfo ai[]);

BASE_END_DECL

#endif

