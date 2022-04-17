/* 
 *
 */
#ifndef __UTIL_RESOLVER_H__
#define __UTIL_RESOLVER_H__

/**
 * @brief Asynchronous DNS resolver
 */
#include <utilDns.h>


BASE_BEGIN_DECL


/**
 * @defgroup BASE_DNS_RESOLVER DNS Asynchronous/Caching Resolution Engine
 * @ingroup BASE_DNS
 * @{
 *
 * This module manages the host/server resolution by performing asynchronous
 * DNS queries and caching the results in the cache. It uses -UTIL 
 * low-level DNS parsing functions (see @ref BASE_DNS) and currently supports
 * several types of DNS resource records such as A record (typical query with
 * gethostbyname()) and SRV record.
 *
 * \section BASE_DNS_RESOLVER_FEATURES Features
 *
 * \subsection BASE_DNS_RESOLVER_FEATURES_ASYNC Asynchronous Query and Query Aggregation
 * 
 * The DNS queries are performed asychronously, with timeout setting 
 * configured on per resolver instance basis. Application can issue multiple
 * asynchronous queries simultaneously. Subsequent queries to the same resource
 * (name and DNS resource type) while existing query is still pending will be
 * merged into one query, so that only one DNS request packet is issued.
 * 
 * \subsection BASE_DNS_RESOLVER_FEATURES_RETRANSMISSION Query Retransmission
 *
 * Asynchronous query will be retransmitted if no response is received
 * within the preconfigured time. Once maximum retransmission count is
 * exceeded and no response is received, the query will time out and the
 * callback will be called when error status.
 *
 * \subsection BASE_DNS_RESOLVER_FEATURES_CACHING Response Caching with TTL
 *
 * The resolver instance caches the results returned by nameservers, to
 * enhance the performance by minimizing the message round-trip to the server.
 * The TTL of the cached resposne is calculated from minimum TTL value found 
 * across all resource record (RR) TTL in the response and further more it can
 * be limited to some preconfigured maximum TTL in the resolver. 
 *
 * Response caching can be  disabled by setting the maximum TTL value of the 
 * resolver to zero.
 *
 * \subsection BASE_DNS_RESOLVER_FEATURES_PARALLEL Parallel and Backup Name Servers
 *
 * When the resolver is configured with multiple nameservers, initially the
 * queries will be issued to multiple name servers simultaneously to probe
 * which servers are not active. Once the probing stage is done, subsequent 
 * queries will be directed to only one ACTIVE server which provides the best
 * response time.
 *
 * Name servers are probed periodically to see which nameservers are active
 * and which are down. This probing is done when a query is sent, thus no
 * timer is needed to maintain this. Also probing will be done in parallel
 * so that there would be no additional delay for the query.
 *
 *
 * \subsection BASE_DNS_RESOLVER_FEATURES_REC Supported Resource Records
 *
 * The low-level DNS parsing utility (see @ref BASE_DNS) supports parsing of
 * the following DNS resource records (RR):
 *  - DNS A record
 *  - DNS SRV record
 *  - DNS PTR record
 *  - DNS NS record
 *  - DNS CNAME record
 *
 * For other types of record, application can parse the raw resource 
 * record data (rdata) from the parsed DNS packet (#bdns_parsed_packet).
 *
 *
 * \section BASE_DNS_RESOLVER_USING Using the Resolver
 *
 * To use the resolver, application first creates the resolver instance by
 * calling #bdns_resolver_create(). If application already has its own
 * timer and ioqueue instances, it can instruct the resolver to use these
 * instances so that application does not need to poll the resolver 
 * periodically to process events. If application does not specify the
 * timer and ioqueue instance for the resolver, an internal timer and
 * ioqueue will be created by the resolver. And since the resolver does not
 * create it's own thread, application MUST poll the resolver periodically
 * by calling #bdns_resolver_handle_events() to allow events (network and 
 * timer) to be processed.
 *
 * Next, application MUST configure the nameservers to be used by the
 * resolver, by calling #bdns_resolver_set_ns().
 *
 * Application performs asynchronous query by submitting the query with
 * #bdns_resolver_start_query(). Once the query completes (either 
 * successfully or times out), the callback will be called.
 *
 * Application can cancel a pending query by calling #bdns_resolver_cancel_query().
 *
 * Resolver must be destroyed by calling #bdns_resolver_destroy() to
 * release all resources back to the system.
 *
 *
 * \section BASE_DNS_RESOLVER_LIMITATIONS Resolver Limitations
 *
 * Current implementation mainly suffers from a growing memory problem,
 * which mainly is caused by the response caching. Although there is only
 * one cache entry per {query, name} combination, these cache entry will
 * never get deleted since there is no timer is created to invalidate these
 * entries. So the more unique names being queried by application, there more
 * enties will be created in the response cache.
 *
 * Note that a single response entry will occupy about 600-700 bytes of 
 * pool memory (the BASE_DNS_RESOLVER_RES_BUF_SIZE value plus internal
 * structure). 
 *
 * Application can work around this problem by doing one of these:
 *  - disable caching by setting BASE_DNS_RESOLVER_MAX_TTL and 
 *    BASE_DNS_RESOLVER_INVALID_TTL to zero.
 *  - periodically query #bdns_resolver_get_cached_count() and destroy-
 *    recreate the resolver to recycle the memory used by the resolver.
 *
 * Note that future improvement may solve this problem by introducing 
 * expiration timer to the cached entries.
 *
 *
 * \section BASE_DNS_RESOLVER_REFERENCE Reference
 *
 * The -UTIL resolver was built from the information in the following
 * standards:
 *  - <A HREF="http://www.faqs.org/rfcs/rfc1035.html">
 *    RFC 1035: "Domain names - implementation and specification"</A>
 *  - <A HREF="http://www.faqs.org/rfcs/rfc2782.html">
 *    RFC 2782: "A DNS RR for specifying the location of services (DNS SRV)"
 *    </A>
 */



/**
 * Opaque data type for DNS resolver object.
 */
typedef struct bdns_resolver bdns_resolver;

/**
 * Opaque data type for asynchronous DNS query object.
 */
typedef struct bdns_async_query bdns_async_query;

/**
 * Type of asynchronous callback which will be called when the asynchronous
 * query completes.
 *
 * @param user_data	The user data set by application when creating the
 *			asynchronous query.
 * @param status	Status of the DNS resolution.
 * @param response	The response packet received from the server. This
 *			argument may be NULL when status is not BASE_SUCCESS.
 */
typedef void bdns_callback(void *user_data,
			     bstatus_t status,
			     bdns_parsed_packet *response);


/**
 * This structure describes resolver settings.
 */
typedef struct bdns_settings
{
    unsigned	options;	/**< Options flags.			    */
    unsigned	qretr_delay;	/**< Query retransmit delay in msec.	    */
    unsigned	qretr_count;	/**< Query maximum retransmission count.    */
    unsigned	cache_max_ttl;	/**< Maximum TTL for cached responses. If the
				     value is zero, caching is disabled.    */
    unsigned	good_ns_ttl;	/**< See #BASE_DNS_RESOLVER_GOOD_NS_TTL	    */
    unsigned	bad_ns_ttl;	/**< See #BASE_DNS_RESOLVER_BAD_NS_TTL	    */
} bdns_settings;


/**
 * This structure represents DNS A record, as the result of parsing
 * DNS response packet using #bdns_parse_a_response().
 */
typedef struct bdns_a_record
{
    /** The target name being queried.   */
    bstr_t		name;

    /** If target name corresponds to a CNAME entry, the alias contains
     *  the value of the CNAME entry, otherwise it will be empty.
     */
    bstr_t		alias;

    /** Number of IP addresses. */
    unsigned		addr_count;

    /** IP addresses of the host found in the response */
    bin_addr		addr[BASE_DNS_MAX_IP_IN_A_REC];

    /** Internal buffer for hostname and alias. */
    char		buf_[128];

} bdns_a_record;


/**
 * This structure represents DNS address record, i.e: DNS A and DNS AAAA
 * records, as the result of parsing DNS response packet using
 * #bdns_parse_addr_response().
 */
typedef struct bdns_addr_record
{
    /** The target name being queried.   */
    bstr_t		name;

    /** If target name corresponds to a CNAME entry, the alias contains
     *  the value of the CNAME entry, otherwise it will be empty.
     */
    bstr_t		alias;

    /** Number of IP addresses. */
    unsigned		addr_count;

    /** IP addresses of the host found in the response */
    struct {

	/** IP address family */
	int		af;
	
	/** IP address */
	union {
	    /** IPv4 address */
	    bin_addr	v4;

	    /** IPv6 address */
	    bin6_addr	v6;
	} ip;

    } addr[BASE_DNS_MAX_IP_IN_A_REC];

    /** Internal buffer for hostname and alias. */
    char		buf_[128];

} bdns_addr_record;


/**
 * Set default values to the DNS settings.
 *
 * @param s	    The DNS settings to be initialized.
 */
void bdns_settings_default(bdns_settings *s);


/**
 * Create DNS resolver instance. After the resolver is created, application
 * MUST configure the nameservers with #bdns_resolver_set_ns().
 *
 * When creating the resolver, application may specify both timer heap
 * and ioqueue instance, so that it doesn't need to poll the resolver
 * periodically.
 *
 * @param pf	     Pool factory where the memory pool will be created from.
 * @param name	     Optional resolver name to identify the instance in 
 *		     the log.
 * @param options    Optional options, must be zero for now.
 * @param timer	     Optional timer heap instance to be used by the resolver.
 *		     If timer heap is not specified, an internal timer will be
 *		     created, and application would need to poll the resolver
 *		     periodically.
 * @param ioqueue    Optional I/O Queue instance to be used by the resolver.
 *		     If ioqueue is not specified, an internal one will be
 *		     created, and application would need to poll the resolver
 *		     periodically.
 * @param p_resolver Pointer to receive the resolver instance.
 *
 * @return	     BASE_SUCCESS on success, or the appropriate error code,
 */
bstatus_t bdns_resolver_create(bpool_factory *pf,
					    const char *name,
					    unsigned options,
					    btimer_heap_t *timer,
					    bioqueue_t *ioqueue,
					    bdns_resolver **p_resolver);


/**
 * Update the name servers for the DNS resolver. The name servers MUST be
 * configured before any resolution can be done. The order of nameservers
 * specifies their priority; the first name server will be tried first
 * before the next in the list.
 *
 * @param resolver  The resolver instance.
 * @param count     Number of name servers in the array.
 * @param servers   Array of name server IP addresses or hostnames. If
 *		    hostname is specified, the hostname must be resolvable
 *		    with bgethostbyname().
 * @param ports	    Optional array of ports. If this argument is NULL,
 *		    the nameserver will use default port.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code,
 */
bstatus_t bdns_resolver_set_ns(bdns_resolver *resolver,
					    unsigned count,
					    const bstr_t servers[],
					    const buint16_t ports[]);


/**
 * Get the resolver current settings.
 *
 * @param resolver  The resolver instance.
 * @param st	    Buffer to be filled up with resolver settings.
 *
 * @return	    The query timeout setting, in seconds.
 */
bstatus_t bdns_resolver_get_settings(bdns_resolver *resolver,
						  bdns_settings *st);


/**
 * Modify the resolver settings. Application should initialize the settings
 * by retrieving current settings first before applying new settings, to
 * ensure that all fields are initialized properly.
 *
 * @param resolver  The resolver instance.
 * @param st	    The resolver settings.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code,
 */
bstatus_t bdns_resolver_set_settings(bdns_resolver *resolver,
						  const bdns_settings *st);


/**
 * Poll for events from the resolver. This function MUST be called 
 * periodically when the resolver is using it's own timer or ioqueue
 * (in other words, when NULL is specified as either \a timer or
 * \a ioqueue argument in #bdns_resolver_create()).
 *
 * @param resolver  The resolver instance.
 * @param timeout   Maximum time to wait for event occurence. If this
 *		    argument is NULL, this function will wait forever
 *		    until events occur.
 */
void bdns_resolver_handle_events(bdns_resolver *resolver,
					    const btime_val *timeout);


/**
 * Destroy DNS resolver instance.
 *
 * @param resolver  The resolver object to be destryed
 * @param notify    If non-zero, all pending asynchronous queries will be
 *		    cancelled and its callback will be called. If FALSE,
 *		    then no callback will be called.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code,
 */
bstatus_t bdns_resolver_destroy(bdns_resolver *resolver,
					     bbool_t notify);


/**
 * Create and start asynchronous DNS query for a single resource. Depending
 * on whether response cache is available, this function will either start
 * an asynchronous DNS query or call the callback immediately.
 *
 * If response is not available in the cache, an asynchronous query will be
 * started, and callback will be called at some time later when the query
 * completes. If \a p_query argument is not NULL, it will be filled with
 * the asynchronous query object.
 *
 * If response is available in the cache, the callback will be called 
 * immediately before this function returns. In this case, if \a p_query
 * argument is not NULL, the value will be set to NULL since no new query
 * is started.
 *
 * @param resolver  The resolver object.
 * @param name	    The name to be resolved.
 * @param type	    The type of resource (see #bdns_type constants).
 * @param options   Optional options, must be zero for now.
 * @param cb	    Callback to be called when the query completes,
 *		    either successfully or with failure.
 * @param user_data Arbitrary user data to be associated with the query,
 *		    and which will be given back in the callback.
 * @param p_query   Optional pointer to receive the query object, if one
 *		    was started. If this pointer is specified, a NULL may
 *		    be returned if response cache is available immediately.
 *
 * @return	    BASE_SUCCESS if either an asynchronous query has been 
 *		    started successfully or response cache is available and
 *		    the user callback has been called.
 */
bstatus_t bdns_resolver_start_query(bdns_resolver *resolver,
						 const bstr_t *name,
						 int type,
						 unsigned options,
						 bdns_callback *cb,
						 void *user_data,
						 bdns_async_query **p_query);

/**
 * Cancel a pending query.
 *
 * @param query	    The pending asynchronous query to be cancelled.
 * @param notify    If non-zero, the callback will be called with failure
 *		    status to notify that the query has been cancelled.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code,
 */
bstatus_t bdns_resolver_cancel_query(bdns_async_query *query,
						  bbool_t notify);

/**
 * A utility function to parse a DNS response containing A records into 
 * DNS A record.
 *
 * @param pkt	    The DNS response packet.
 * @param rec	    The structure to be initialized with the parsed
 *		    DNS A record from the packet.
 *
 * @return	    BASE_SUCCESS if response can be parsed successfully.
 */
bstatus_t bdns_parse_a_response(const bdns_parsed_packet *pkt,
					     bdns_a_record *rec);


/**
 * A utility function to parse a DNS response containing AAAA records into 
 * DNS AAAA record.
 *
 * @param pkt	    The DNS response packet.
 * @param rec	    The structure to be initialized with the parsed
 *		    DNS AAAA record from the packet.
 *
 * @return	    BASE_SUCCESS if response can be parsed successfully.
 */
bstatus_t bdns_parse_addr_response(
					    const bdns_parsed_packet *pkt,
					    bdns_addr_record *rec);


/**
 * Put the specified DNS packet into DNS cache. This function is mainly used
 * for testing the resolver, however it can also be used to inject entries
 * into the resolver.
 *
 * The packet MUST contain either answer section or query section so that
 * it can be indexed.
 *
 * @param resolver  The resolver instance.
 * @param pkt	    DNS packet to be added to the DNS cache. If the packet
 *		    matches existing entry, it will update the entry.
 * @param set_ttl   If the value is BASE_FALSE, the entry will not expire 
 *		    (so use with care). Otherwise cache expiration will be
 *		    calculated based on the TTL of the answeres.
 *
 * @return	    BASE_SUCCESS on success, or the appropriate error code.
 */
bstatus_t bdns_resolver_add_entry(bdns_resolver *resolver,
					       const bdns_parsed_packet *pkt,
					       bbool_t set_ttl);

/**
 * Get the total number of response in the response cache.
 *
 * @param resolver  The resolver instance.
 *
 * @return	    Current number of entries being stored in the response
 *		    cache.
 */
unsigned bdns_resolver_get_cached_count(bdns_resolver *resolver);


/**
 * Dump resolver state to the log.
 *
 * @param resolver  The resolver instance.
 * @param detail    Will print detailed entries.
 */
void bdns_resolver_dump(bdns_resolver *resolver,
				   bbool_t detail);


BASE_END_DECL


#endif

