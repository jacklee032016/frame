/*
 *
 */
#ifndef __UTIL_SRV_RESOLVER_H__
#define __UTIL_SRV_RESOLVER_H__

/**
 * @brief DNS SRV resolver
 */
#include <utilResolver.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_DNS_SRV_RESOLVER DNS SRV Resolution Helper
 * @ingroup BASE_DNS
 * @{
 *
 * \section BASE_DNS_SRV_RESOLVER_INTRO DNS SRV Resolution Helper
 *
 * This module provides an even higher layer of abstraction for the DNS
 * resolution framework, to resolve DNS SRV names.
 *
 * The #bdns_srv_resolve() function will asynchronously resolve the server
 * name into IP address(es) with a single function call. If the SRV name
 * contains multiple names, then each will be resolved with individual
 * DNS A resolution to get the IP addresses. Upon successful completion, 
 * application callback will be called with each IP address of the
 * target selected based on the load-balancing and fail-over criteria
 * below.
 *
 * When the resolver fails to resolve the name using DNS SRV resolution
 * (for example when the DNS SRV record is not present in the DNS server),
 * the resolver will fallback to using DNS A record resolution to resolve
 * the name.
 *
 * \subsection BASE_DNS_SRV_RESOLVER_FAILOVER_LOADBALANCE Load-Balancing and Fail-Over
 *
 * When multiple targets are returned in the DNS SRV response, server entries
 * are selected based on the following rule (which is described in RFC 2782):
 *  - targets will be sorted based on the priority first.
 *  - for targets with the same priority, #bdns_srv_resolve() will select
 *    only one target according to its weight. To select this one target,
 *    the function associates running-sum for all targets, and generates 
 *    a random number between zero and the total running-sum (inclusive).
 *    The target selected is the first target with running-sum greater than
 *    or equal to this random number.
 *
 * The above procedure will select one target for each priority, allowing
 * application to fail-over to the next target when the previous target fails.
 * These targets are returned in the #bdns_srv_record structure 
 * argument of the callback. 
 *
 * \section BASE_DNS_SRV_RESOLVER_REFERENCE Reference
 *
 * Reference:
 *  - <A HREF="http://www.ietf.org/rfc/rfc2782.txt">RFC 2782</A>: 
 *	A DNS RR for specifying the location of services (DNS SRV)
 */

/**
 * Flags to be specified when starting the DNS SRV query.
 */
typedef enum bdns_srv_option
{
    /**
     * Specify if the resolver should fallback with DNS A
     * resolution when the SRV resolution fails. This option may
     * be specified together with BASE_DNS_SRV_FALLBACK_AAAA to
     * make the resolver fallback to both DNS A and DNS AAAA
     * resolutions if SRV resolution fails.
     */
    BASE_DNS_SRV_FALLBACK_A	= 1,

    /**
     * Specify if the resolver should fallback with DNS AAAA
     * resolution when the SRV resolution fails. This option may
     * be specified together with BASE_DNS_SRV_FALLBACK_AAAA to
     * make the resolver fallback to both DNS A and DNS AAAA
     * resolutions if SRV resolution fails.
     */
    BASE_DNS_SRV_FALLBACK_AAAA	= 2,

    /**
     * Specify if the resolver should try to resolve with DNS AAAA
     * resolution of each targets in the DNS SRV record. If this
     * option is not specified, the SRV resolver will query the
     * DNS A record for the target instead.
     */
    BASE_DNS_SRV_RESOLVE_AAAA	= 4,

    /**
     * Specify if the resolver should try to resolve with DNS AAAA
     * resolution only (i.e: without DNS A resolution) for each targets
     * in the DNS SRV record.
     */
    BASE_DNS_SRV_RESOLVE_AAAA_ONLY = 8

} bdns_srv_option;


/**
 * This structure represents DNS SRV records as the result of DNS SRV
 * resolution using #bdns_srv_resolve().
 */
typedef struct bdns_srv_record
{
    /** Number of address records. */
    unsigned	count;

    /** Address records. */
    struct
    {
	/** Server priority (the lower the higher the priority). */
	unsigned		priority;

	/** Server weight (the higher the more load it can handle). */
	unsigned		weight;

	/** Port number. */
	buint16_t		port;

	/** The host address. */
	bdns_addr_record	server;

    } entry[BASE_DNS_SRV_MAX_ADDR];

} bdns_srv_record;


/** Opaque declaration for DNS SRV query */
typedef struct bdns_srv_async_query bdns_srv_async_query;

/**
 * Type of callback function to receive notification from the resolver
 * when the resolution process completes.
 */
typedef void bdns_srv_resolver_cb(void *user_data,
				    bstatus_t status,
				    const bdns_srv_record *rec);


/**
 * Start DNS SRV resolution for the specified name. The full name of the
 * entry will be concatenated from \a res_name and \a domain_name fragments.
 *
 * @param domain_name	The domain name part of the name.
 * @param res_name	The full service name, including the transport name
 *			and with all the leading underscore characters and
 *			ending dot (e.g. "_sip._udp.", "_stun._udp.").
 * @param def_port	The port number to be assigned to the resolved address
 *			when the DNS SRV resolution fails and the name is 
 *			resolved with DNS A resolution.
 * @param pool		Memory pool used to allocate memory for the query.
 * @param resolver	The resolver instance.
 * @param option	Option flags, which can be constructed from
 *			#bdns_srv_option bitmask. Note that this argument
 *			was called "fallback_a" in pjsip version 0.8.0 and
 *			older, but the new option should be backward 
 *			compatible with existing applications. If application
 *			specifies BASE_TRUE as "fallback_a" value, it will
 *			correspond to BASE_DNS_SRV_FALLBACK_A option.
 * @param token		Arbitrary data to be associated with this query when
 *			the calback is called.
 * @param cb		Pointer to callback function to receive the
 *			notification when the resolution process completes.
 * @param p_query	Optional pointer to receive the query object, if one
 *			was started. If this pointer is specified, a NULL may
 *			be returned if response cache is available immediately.
 *
 * @return		BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bdns_srv_resolve(const bstr_t *domain_name,
					const bstr_t *res_name,
					unsigned def_port,
					bpool_t *pool,
					bdns_resolver *resolver,
					unsigned option,
					void *token,
					bdns_srv_resolver_cb *cb,
					bdns_srv_async_query **p_query);


/**
 * Cancel an outstanding DNS SRV query.
 *
 * @param query	    The pending asynchronous query to be cancelled.
 * @param notify    If non-zero, the callback will be called with failure
 *		    status to notify that the query has been cancelled.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code,
 */
bstatus_t bdns_srv_cancel_query(bdns_srv_async_query *query,
					     bbool_t notify);

BASE_END_DECL


#endif

