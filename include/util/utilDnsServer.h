/* 
 *
 */
#ifndef __UTIL_DNS_SERVER_H__
#define __UTIL_DNS_SERVER_H__

/**
 * @brief Simple DNS server
 */
#include <utilTypes.h>
#include <utilDns.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_DNS_SERVER Simple DNS Server
 * @ingroup BASE_DNS
 * @{
 * This contains a simple but fully working DNS server implementation, 
 * mostly for testing purposes. It supports serving various DNS resource 
 * records such as SRV, CNAME, A, and AAAA.
 */

/**
 * Opaque structure to hold DNS server instance.
 */
typedef struct bdns_server bdns_server;

/**
 * Create the DNS server instance. The instance will run immediately.
 *
 * @param pf	    The pool factory to create memory pools.
 * @param ioqueue   Ioqueue instance where the server socket will be
 *		    registered to.
 * @param af	    Address family of the server socket (valid values
 *		    are bAF_INET() for IPv4 and bAF_INET6() for IPv6).
 * @param port	    The UDP port to listen.
 * @param flags	    Flags, currently must be zero.
 * @param p_srv	    Pointer to receive the DNS server instance.
 *
 * @return	    BASE_SUCCESS if server has been created successfully,
 *		    otherwise the function will return the appropriate
 *		    error code.
 */
bstatus_t bdns_server_create(bpool_factory *pf,
				          bioqueue_t *ioqueue,
					  int af,
					  unsigned port,
					  unsigned flags,
				          bdns_server **p_srv);

/**
 * Destroy DNS server instance.
 *
 * @param srv	    The DNS server instance.
 *
 * @return	    BASE_SUCCESS on success or the appropriate error code.
 */
bstatus_t bdns_server_destroy(bdns_server *srv);


/**
 * Add generic resource record entries to the server.
 *
 * @param srv	    The DNS server instance.
 * @param count	    Number of records to be added.
 * @param rr	    Array of records to be added.
 *
 * @return	    BASE_SUCCESS on success or the appropriate error code.
 */
bstatus_t bdns_server_add_rec(bdns_server *srv,
					   unsigned count,
					   const bdns_parsed_rr rr[]);

/**
 * Remove the specified record from the server.
 *
 * @param srv	    The DNS server instance.
 * @param dns_class The resource's DNS class. Valid value is BASE_DNS_CLASS_IN.
 * @param type	    The resource type.
 * @param name	    The resource name to be removed.
 *
 * @return	    BASE_SUCCESS on success or the appropriate error code.
 */
bstatus_t bdns_server_del_rec(bdns_server *srv,
					   int dns_class,
					   bdns_type type,
					   const bstr_t *name);


BASE_END_DECL

#endif

