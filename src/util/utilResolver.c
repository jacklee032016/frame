/* 
 *
 */
#include <utilResolver.h>
#include <utilErrno.h>
#include <compat/socket.h>
#include <baseAssert.h>
#include <baseCtype.h>
#include <baseExcept.h>
#include <baseHash.h>
#include <baseIoQueue.h>
#include <baseLog.h>
#include <baseOs.h>
#include <basePool.h>
#include <basePoolBuf.h>
#include <baseRand.h>
#include <baseString.h>
#include <baseSock.h>
#include <baseTimer.h>


/* Check that maximum DNS nameservers is not too large. 
 * This has got todo with the datatype to index the nameserver in the query.
 */
#if BASE_DNS_RESOLVER_MAX_NS > 256
#   error "BASE_DNS_RESOLVER_MAX_NS is too large (max=256)"
#endif


#define RES_HASH_TABLE_SIZE 127		/**< Hash table size (must be 2^n-1 */
#define PORT		    53		/**< Default NS port.		    */
#define Q_HASH_TABLE_SIZE   127		/**< Query hash table size	    */
#define TIMER_SIZE	    127		/**< Initial number of timers.	    */
#define MAX_FD		    3		/**< Maximum internal sockets.	    */

#define RES_BUF_SZ	    BASE_DNS_RESOLVER_RES_BUF_SIZE
#define UDPSZ		    BASE_DNS_RESOLVER_MAX_UDP_SIZE
#define TMP_SZ		    BASE_DNS_RESOLVER_TMP_BUF_SIZE


/* Nameserver state */
enum ns_state
{
    STATE_PROBING,
    STATE_ACTIVE,
    STATE_BAD,
};

static const char *state_names[3] =
{
    "Probing",
    "Active",
    "Bad"
};


/* 
 * Each nameserver entry.
 * A name server is identified by its socket address (IP and port).
 * Each NS will have a flag to indicate whether it's properly functioning.
 */
struct nameserver
{
    bsockaddr     addr;		/**< Server address.		    */

    enum ns_state   state;		/**< Nameserver state.		    */
    btime_val	    state_expiry;	/**< Time set next state.	    */
    btime_val	    rt_delay;		/**< Response time.		    */
    

    /* For calculating rt_delay: */
    buint16_t	    q_id;		/**< Query ID.			    */
    btime_val	    sent_time;		/**< Time this query is sent.	    */
};


/* Child query list head 
 * See comments on bdns_async_query below.
 */
struct query_head
{
    BASE_DECL_LIST_MEMBER(bdns_async_query);
};


/* Key to look for outstanding query and/or cached response */
struct res_key
{
    buint16_t		     qtype;		    /**< Query type.	    */
    char		     name[BASE_MAX_HOSTNAME]; /**< Name being queried */
};


/* 
 * This represents each asynchronous query entry.
 * This entry will be put in two hash tables, the first one keyed on the DNS 
 * transaction ID to match response with the query, and the second one keyed
 * on "res_key" structure above to match a new request against outstanding 
 * requests.
 *
 * An asynchronous entry may have child entries; child entries are subsequent
 * queries to the same resource while there is pending query on the same
 * DNS resource name and type. When a query has child entries, once the
 * response is received (or error occurs), the response will trigger callback
 * invocations for all childs entries.
 *
 * Note: when application cancels the query, the callback member will be
 *       set to NULL, but for simplicity, the query will be let running.
 */
struct bdns_async_query
{
    BASE_DECL_LIST_MEMBER(bdns_async_query);	/**< List member.	    */

    bdns_resolver	*resolver;	/**< The resolver instance.	    */
    buint16_t		 id;		/**< Transaction ID.		    */

    unsigned		 transmit_cnt;	/**< Number of transmissions.	    */

    struct res_key	 key;		/**< Key to index this query.	    */
    bhash_entry_buf	 hbufid;	/**< Hash buffer 1		    */
    bhash_entry_buf	 hbufkey;	/**< Hash buffer 2		    */
    btimer_entry	 timer_entry;	/**< Timer to manage timeouts	    */
    unsigned		 options;	/**< Query options.		    */
    void		*user_data;	/**< Application data.		    */
    bdns_callback	*cb;		/**< Callback to be called.	    */
    struct query_head	 child_head;	/**< Child queries list head.	    */
};


/* This structure is used to keep cached response entry.
 * The cache is a hash table keyed on "res_key" structure above.
 */
struct cached_res
{
    BASE_DECL_LIST_MEMBER(struct cached_res);

    bpool_t		    *pool;	    /**< Cache's pool.		    */
    struct res_key	     key;	    /**< Resource key.		    */
    bhash_entry_buf	     hbuf;	    /**< Hash buffer		    */
    btime_val		     expiry_time;   /**< Expiration time.	    */
    bdns_parsed_packet    *pkt;	    /**< The response packet.	    */
    unsigned		     ref_cnt;	    /**< Reference counter.	    */
};


/* Resolver entry */
struct bdns_resolver
{
    bstr_t		 name;		/**< Resolver instance name for id. */

    /* Internals */
    bpool_t		*pool;		/**< Internal pool.		    */
    bgrp_lock_t	*grp_lock;	/**< Group lock protection.	    */
    bbool_t		 own_timer;	/**< Do we own timer?		    */
    btimer_heap_t	*timer;		/**< Timer instance.		    */
    bbool_t		 own_ioqueue;	/**< Do we own ioqueue?		    */
    bioqueue_t	*ioqueue;	/**< Ioqueue instance.		    */
    char		 tmp_pool[TMP_SZ];/**< Temporary pool buffer.	    */

    /* Socket */
    bsock_t		 udp_sock;	/**< UDP socket.		    */
    bioqueue_key_t	*udp_key;	/**< UDP socket ioqueue key.	    */
    unsigned char	 udp_rx_pkt[UDPSZ];/**< UDP receive buffer.	    */
    unsigned char	 udp_tx_pkt[UDPSZ];/**< UDP transmit buffer.	    */
    bioqueue_op_key_t	 udp_op_rx_key;	/**< UDP read operation key.	    */
    bioqueue_op_key_t	 udp_op_tx_key;	/**< UDP write operation key.	    */
    bsockaddr		 udp_src_addr;	/**< Source address of packet	    */
    int			 udp_addr_len;	/**< Source address length.	    */

#if BASE_HAS_IPV6
    /* IPv6 socket */
    bsock_t		 udp6_sock;	/**< UDP socket.		    */
    bioqueue_key_t	*udp6_key;	/**< UDP socket ioqueue key.	    */
    unsigned char	 udp6_rx_pkt[UDPSZ];/**< UDP receive buffer.	    */
    //unsigned char	 udp6_tx_pkt[UDPSZ];/**< UDP transmit buffer.	    */
    bioqueue_op_key_t	 udp6_op_rx_key;/**< UDP read operation key.	    */
    bioqueue_op_key_t	 udp6_op_tx_key;/**< UDP write operation key.	    */
    bsockaddr		 udp6_src_addr;	/**< Source address of packet	    */
    int			 udp6_addr_len;	/**< Source address length.	    */
#endif

    /* Settings */
    bdns_settings	 settings;	/**< Resolver settings.		    */

    /* Nameservers */
    unsigned		 ns_count;	/**< Number of name servers.	    */
    struct nameserver	 ns[BASE_DNS_RESOLVER_MAX_NS];	/**< Array of NS.   */

    /* Last DNS transaction ID used. */
    buint16_t		 last_id;

    /* Hash table for cached response */
    bhash_table_t	*hrescache;	/**< Cached response in hash table  */

    /* Pending asynchronous query, hashed by transaction ID. */
    bhash_table_t	*hquerybyid;

    /* Pending asynchronous query, hashed by "res_key" */
    bhash_table_t	*hquerybyres;

    /* Query entries free list */
    struct query_head	 query_free_nodes;
};


/* Callback from ioqueue when packet is received */
static void on_read_complete(bioqueue_key_t *key, 
                             bioqueue_op_key_t *op_key, 
                             bssize_t bytes_read);

/* Callback to be called when query has timed out */
static void on_timeout( btimer_heap_t *timer_heap, struct _btimer_entry *entry);

/* Select which nameserver to use */
static bstatus_t select_nameservers(bdns_resolver *resolver,
				      unsigned *count,
				      unsigned servers[]);

/* Destructor */
static void dns_resolver_on_destroy(void *member);

/* Close UDP socket */
static void close_sock(bdns_resolver *resv)
{
    /* Close existing socket */
    if (resv->udp_key != NULL) {
	bioqueue_unregister(resv->udp_key);
	resv->udp_key = NULL;
	resv->udp_sock = BASE_INVALID_SOCKET;
    } else if (resv->udp_sock != BASE_INVALID_SOCKET) {
	bsock_close(resv->udp_sock);
	resv->udp_sock = BASE_INVALID_SOCKET;
    }

#if BASE_HAS_IPV6
    if (resv->udp6_key != NULL) {
	bioqueue_unregister(resv->udp6_key);
	resv->udp6_key = NULL;
	resv->udp6_sock = BASE_INVALID_SOCKET;
    } else if (resv->udp6_sock != BASE_INVALID_SOCKET) {
	bsock_close(resv->udp6_sock);
	resv->udp6_sock = BASE_INVALID_SOCKET;
    }
#endif
}


/* Initialize UDP socket */
static bstatus_t init_sock(bdns_resolver *resv)
{
    bioqueue_callback socket_cb;
    bsockaddr bound_addr;
    bssize_t rx_pkt_size;
    bstatus_t status;

    /* Create the UDP socket */
    status = bsock_socket(bAF_INET(), bSOCK_DGRAM(), 0, &resv->udp_sock);
    if (status != BASE_SUCCESS)
	return status;

    /* Bind to any address/port */
    status = bsock_bind_in(resv->udp_sock, 0, 0);
    if (status != BASE_SUCCESS)
	return status;

    /* Register to ioqueue */
    bbzero(&socket_cb, sizeof(socket_cb));
    socket_cb.on_read_complete = &on_read_complete;
    status = bioqueue_register_sock2(resv->pool, resv->ioqueue,
				       resv->udp_sock, resv->grp_lock,
				       resv, &socket_cb, &resv->udp_key);
    if (status != BASE_SUCCESS)
	return status;

    bioqueue_op_key_init(&resv->udp_op_rx_key, sizeof(resv->udp_op_rx_key));
    bioqueue_op_key_init(&resv->udp_op_tx_key, sizeof(resv->udp_op_tx_key));

    /* Start asynchronous read to the UDP socket */
    rx_pkt_size = sizeof(resv->udp_rx_pkt);
    resv->udp_addr_len = sizeof(resv->udp_src_addr);
    status = bioqueue_recvfrom(resv->udp_key, &resv->udp_op_rx_key,
				 resv->udp_rx_pkt, &rx_pkt_size,
				 BASE_IOQUEUE_ALWAYS_ASYNC,
				 &resv->udp_src_addr, &resv->udp_addr_len);
    if (status != BASE_EPENDING)
	return status;


#if BASE_HAS_IPV6
    /* Also setup IPv6 socket */

    /* Create the UDP socket */
    status = bsock_socket(bAF_INET6(), bSOCK_DGRAM(), 0,
			    &resv->udp6_sock);
    if (status != BASE_SUCCESS) {
	/* Skip IPv6 socket on system without IPv6 (see ticket #1953) */
	if (status == BASE_STATUS_FROM_OS(OSERR_EAFNOSUPPORT)) {
	    BASE_STR_INFO(resv->name.ptr, "System does not support IPv6, resolver will "
		      "ignore any IPv6 nameservers");
	    return BASE_SUCCESS;
	}
	return status;
    }

    /* Bind to any address/port */
    bsockaddr_init(bAF_INET6(), &bound_addr, NULL, 0);
    status = bsock_bind(resv->udp6_sock, &bound_addr,
			  bsockaddr_get_len(&bound_addr));
    if (status != BASE_SUCCESS)
	return status;

    /* Register to ioqueue */
    bbzero(&socket_cb, sizeof(socket_cb));
    socket_cb.on_read_complete = &on_read_complete;
    status = bioqueue_register_sock2(resv->pool, resv->ioqueue,
				       resv->udp6_sock, resv->grp_lock,
				       resv, &socket_cb, &resv->udp6_key);
    if (status != BASE_SUCCESS)
	return status;

    bioqueue_op_key_init(&resv->udp6_op_rx_key,
			   sizeof(resv->udp6_op_rx_key));
    bioqueue_op_key_init(&resv->udp6_op_tx_key,
			   sizeof(resv->udp6_op_tx_key));

    /* Start asynchronous read to the UDP socket */
    rx_pkt_size = sizeof(resv->udp6_rx_pkt);
    resv->udp6_addr_len = sizeof(resv->udp6_src_addr);
    status = bioqueue_recvfrom(resv->udp6_key, &resv->udp6_op_rx_key,
				 resv->udp6_rx_pkt, &rx_pkt_size,
				 BASE_IOQUEUE_ALWAYS_ASYNC,
				 &resv->udp6_src_addr, &resv->udp6_addr_len);
    if (status != BASE_EPENDING)
	return status;
#else
    BASE_UNUSED_ARG(bound_addr);
#endif

    return BASE_SUCCESS;
}


/* Initialize DNS settings with default values */
void bdns_settings_default(bdns_settings *s)
{
    bbzero(s, sizeof(bdns_settings));
    s->qretr_delay = BASE_DNS_RESOLVER_QUERY_RETRANSMIT_DELAY;
    s->qretr_count = BASE_DNS_RESOLVER_QUERY_RETRANSMIT_COUNT;
    s->cache_max_ttl = BASE_DNS_RESOLVER_MAX_TTL;
    s->good_ns_ttl = BASE_DNS_RESOLVER_GOOD_NS_TTL;
    s->bad_ns_ttl = BASE_DNS_RESOLVER_BAD_NS_TTL;
}


/*
 * Create the resolver.
 */
bstatus_t bdns_resolver_create( bpool_factory *pf,
					    const char *name,
					    unsigned options,
					    btimer_heap_t *timer,
					    bioqueue_t *ioqueue,
					    bdns_resolver **p_resolver)
{
    bpool_t *pool;
    bdns_resolver *resv;
    bstatus_t status;

    /* Sanity check */
    BASE_ASSERT_RETURN(pf && p_resolver, BASE_EINVAL);

    if (name == NULL)
	name = THIS_FILE;

    /* Create and initialize resolver instance */
    pool = bpool_create(pf, name, 4000, 4000, NULL);
    if (!pool)
	return BASE_ENOMEM;

    /* Create pool and name */
    resv = BASE_POOL_ZALLOC_T(pool, struct bdns_resolver);
    resv->pool = pool;
    resv->udp_sock = BASE_INVALID_SOCKET;
    bstrdup2_with_null(pool, &resv->name, name);
    
    /* Create group lock */
    status = bgrp_lock_create_w_handler(pool, NULL, resv,
					  &dns_resolver_on_destroy,
					  &resv->grp_lock); 
    if (status != BASE_SUCCESS)
	goto on_error;

    bgrp_lock_add_ref(resv->grp_lock);

    /* Timer, ioqueue, and settings */
    resv->timer = timer;
    resv->ioqueue = ioqueue;
    resv->last_id = 1;

    bdns_settings_default(&resv->settings);
    resv->settings.options = options;

    /* Create the timer heap if one is not specified */
    if (resv->timer == NULL) {
	resv->own_timer = BASE_TRUE;
	status = btimer_heap_create(pool, TIMER_SIZE, &resv->timer);
	if (status != BASE_SUCCESS)
	    goto on_error;
    }

    /* Create the ioqueue if one is not specified */
    if (resv->ioqueue == NULL) {
	resv->own_ioqueue = BASE_TRUE;
	status = bioqueue_create(pool, MAX_FD, &resv->ioqueue);
	if (status != BASE_SUCCESS)
	    goto on_error;
    }

    /* Response cache hash table */
    resv->hrescache = bhash_create(pool, RES_HASH_TABLE_SIZE);

    /* Query hash table and free list. */
    resv->hquerybyid = bhash_create(pool, Q_HASH_TABLE_SIZE);
    resv->hquerybyres = bhash_create(pool, Q_HASH_TABLE_SIZE);
    blist_init(&resv->query_free_nodes);

    /* Initialize the UDP socket */
    status = init_sock(resv);
    if (status != BASE_SUCCESS)
	goto on_error;

    /* Looks like everything is okay */
    *p_resolver = resv;
    return BASE_SUCCESS;

on_error:
    bdns_resolver_destroy(resv, BASE_FALSE);
    return status;
}


void dns_resolver_on_destroy(void *member)
{
    bdns_resolver *resolver = (bdns_resolver*)member;
    bpool_safe_release(&resolver->pool);
}


/*
 * Destroy DNS resolver instance.
 */
bstatus_t bdns_resolver_destroy( bdns_resolver *resolver,
					     bbool_t notify)
{
    bhash_iterator_t it_buf, *it;
    BASE_ASSERT_RETURN(resolver, BASE_EINVAL);

    if (notify) {
	/*
	 * Notify pending queries if requested.
	 */
	it = bhash_first(resolver->hquerybyid, &it_buf);
	while (it) {
	    bdns_async_query *q = (bdns_async_query *)
	    			    bhash_this(resolver->hquerybyid, it);
	    bdns_async_query *cq;
	    if (q->cb)
		(*q->cb)(q->user_data, BASE_ECANCELLED, NULL);

	    cq = q->child_head.next;
	    while (cq != (bdns_async_query*)&q->child_head) {
		if (cq->cb)
		    (*cq->cb)(cq->user_data, BASE_ECANCELLED, NULL);
		cq = cq->next;
	    }
	    it = bhash_next(resolver->hquerybyid, it);
	}
    }

    /* Destroy cached entries */
    it = bhash_first(resolver->hrescache, &it_buf);
    while (it) {
	struct cached_res *cache;

	cache = (struct cached_res*) bhash_this(resolver->hrescache, it);
	bhash_set(NULL, resolver->hrescache, &cache->key, 
		    sizeof(cache->key), 0, NULL);
	bpool_release(cache->pool);

	it = bhash_first(resolver->hrescache, &it_buf);
    }

    if (resolver->own_timer && resolver->timer) {
	btimer_heap_destroy(resolver->timer);
	resolver->timer = NULL;
    }

    close_sock(resolver);

    if (resolver->own_ioqueue && resolver->ioqueue) {
	bioqueue_destroy(resolver->ioqueue);
	resolver->ioqueue = NULL;
    }

    bgrp_lock_dec_ref(resolver->grp_lock);

    return BASE_SUCCESS;
}



/*
 * Configure name servers for the DNS resolver. 
 */
bstatus_t bdns_resolver_set_ns( bdns_resolver *resolver,
					    unsigned count,
					    const bstr_t servers[],
					    const buint16_t ports[])
{
    unsigned i;
    btime_val now;
    bstatus_t status;

    BASE_ASSERT_RETURN(resolver && count && servers, BASE_EINVAL);
    BASE_ASSERT_RETURN(count < BASE_DNS_RESOLVER_MAX_NS, BASE_EINVAL);

    bgrp_lock_acquire(resolver->grp_lock);

    if (count > BASE_DNS_RESOLVER_MAX_NS)
	count = BASE_DNS_RESOLVER_MAX_NS;

    resolver->ns_count = 0;
    bbzero(resolver->ns, sizeof(resolver->ns));

    bgettimeofday(&now);

    for (i=0; i<count; ++i) {
	struct nameserver *ns = &resolver->ns[i];

	status = bsockaddr_init(bAF_INET(), &ns->addr, &servers[i], 
				  (buint16_t)(ports ? ports[i] : PORT));
	if (status != BASE_SUCCESS)
	    status = bsockaddr_init(bAF_INET6(), &ns->addr, &servers[i], 
				      (buint16_t)(ports ? ports[i] : PORT));
	if (status != BASE_SUCCESS) {
	    bgrp_lock_release(resolver->grp_lock);
	    return UTIL_EDNSINNSADDR;
	}

	ns->state = STATE_ACTIVE;
	ns->state_expiry = now;
	ns->rt_delay.sec = 10;
    }
    
    resolver->ns_count = count;

    bgrp_lock_release(resolver->grp_lock);
    return BASE_SUCCESS;
}



/*
 * Modify the resolver settings.
 */
bstatus_t bdns_resolver_set_settings(bdns_resolver *resolver,
						 const bdns_settings *st)
{
    BASE_ASSERT_RETURN(resolver && st, BASE_EINVAL);

    bgrp_lock_acquire(resolver->grp_lock);
    bmemcpy(&resolver->settings, st, sizeof(*st));
    bgrp_lock_release(resolver->grp_lock);
    return BASE_SUCCESS;
}


/*
 * Get the resolver current settings.
 */
bstatus_t bdns_resolver_get_settings( bdns_resolver *resolver,
						  bdns_settings *st)
{
    BASE_ASSERT_RETURN(resolver && st, BASE_EINVAL);

    bgrp_lock_acquire(resolver->grp_lock);
    bmemcpy(st, &resolver->settings, sizeof(*st));
    bgrp_lock_release(resolver->grp_lock);
    return BASE_SUCCESS;
}


/*
 * Poll for events from the resolver. 
 */
void bdns_resolver_handle_events(bdns_resolver *resolver,
					   const btime_val *timeout)
{
    BASE_ASSERT_ON_FAIL(resolver, return);

    bgrp_lock_acquire(resolver->grp_lock);
    btimer_heap_poll(resolver->timer, NULL);
    bgrp_lock_release(resolver->grp_lock);

    bioqueue_poll(resolver->ioqueue, timeout);
}


/* Get one query node from the free node, if any, or allocate 
 * a new one.
 */
static bdns_async_query *alloc_qnode(bdns_resolver *resolver,
				       unsigned options,
				       void *user_data,
				       bdns_callback *cb)
{
    bdns_async_query *q;

    /* Merge query options with resolver options */
    options |= resolver->settings.options;

    if (!blist_empty(&resolver->query_free_nodes)) {
	q = resolver->query_free_nodes.next;
	blist_erase(q);
	bbzero(q, sizeof(*q));
    } else {
	q = BASE_POOL_ZALLOC_T(resolver->pool, bdns_async_query);
    }

    /* Init query */
    q->resolver = resolver;
    q->options = options;
    q->user_data = user_data;
    q->cb = cb;
    blist_init(&q->child_head);

    return q;
}


/*
 * Transmit query.
 */
static bstatus_t transmit_query(bdns_resolver *resolver,
				  bdns_async_query *q)
{
    unsigned pkt_size;
    unsigned i, server_cnt, send_cnt;
    unsigned servers[BASE_DNS_RESOLVER_MAX_NS];
    btime_val now;
    bstr_t name;
    btime_val delay;
    bstatus_t status;

    /* Select which nameserver(s) to send requests to. */
    server_cnt = BASE_ARRAY_SIZE(servers);
    status = select_nameservers(resolver, &server_cnt, servers);
    if (status != BASE_SUCCESS) {
	return status;
    }

    if (server_cnt == 0) {
	return UTIL_EDNSNOWORKINGNS;
    }

    /* Start retransmit/timeout timer for the query */
    bassert(q->timer_entry.id == 0);
    q->timer_entry.id = 1;
    q->timer_entry.user_data = q;
#if 0
	/* Jack */
	q->timer_entry.cb = &on_timeout;
#else
	q->timer_entry.cb = on_timeout;
#endif

    delay.sec = 0;
    delay.msec = resolver->settings.qretr_delay;
    btime_val_normalize(&delay);
    status = btimer_heap_schedule_w_grp_lock(resolver->timer,
					       &q->timer_entry,
					       &delay, 1,
					       resolver->grp_lock);
    if (status != BASE_SUCCESS) {
	return status;
    }

    /* Check if the socket is available for sending */
    if (bioqueue_is_pending(resolver->udp_key, &resolver->udp_op_tx_key)
#if BASE_HAS_IPV6
	|| (resolver->udp6_key &&
	    bioqueue_is_pending(resolver->udp6_key,
				  &resolver->udp6_op_tx_key))
#endif
	)
    {
	++q->transmit_cnt;
	BASE_STR_INFO(resolver->name.ptr,
		  "Socket busy in transmitting DNS %s query for %s%s",
		  bdns_get_type_name(q->key.qtype),
		  q->key.name,
		  (q->transmit_cnt < resolver->settings.qretr_count?
		   ", will try again later":""));
	return BASE_SUCCESS;
    }

    /* Create DNS query packet */
    pkt_size = sizeof(resolver->udp_tx_pkt);
    name = bstr(q->key.name);
    status = bdns_make_query(resolver->udp_tx_pkt, &pkt_size,
			       q->id, q->key.qtype, &name);
    if (status != BASE_SUCCESS) {
	btimer_heap_cancel(resolver->timer, &q->timer_entry);
	return status;
    }

    /* Get current time. */
    bgettimeofday(&now);

    /* Send the packet to name servers */
    send_cnt = 0;
    for (i=0; i<server_cnt; ++i) {
        char addr[BASE_INET6_ADDRSTRLEN];
	bssize_t sent  = (bssize_t) pkt_size;
	struct nameserver *ns = &resolver->ns[servers[i]];

	if (ns->addr.addr.sa_family == bAF_INET()) {
	    status = bioqueue_sendto(resolver->udp_key,
				       &resolver->udp_op_tx_key,
				       resolver->udp_tx_pkt, &sent, 0,
				       &ns->addr,
				       bsockaddr_get_len(&ns->addr));
	    if (status == BASE_SUCCESS || status == BASE_EPENDING)
		send_cnt++;
	}
#if BASE_HAS_IPV6
	else if (resolver->udp6_key) {
	    status = bioqueue_sendto(resolver->udp6_key,
				       &resolver->udp6_op_tx_key,
				       resolver->udp_tx_pkt, &sent, 0,
				       &ns->addr,
				       bsockaddr_get_len(&ns->addr));
	    if (status == BASE_SUCCESS || status == BASE_EPENDING)
		send_cnt++;
	}
#endif
	else {
	    continue;
	}

	BASE_PERROR(4,(resolver->name.ptr, status,
		  "%s %d bytes to NS %d (%s:%d): DNS %s query for %s",
		  (q->transmit_cnt==0? "Transmitting":"Re-transmitting"),
		  (int)pkt_size, servers[i],
		  bsockaddr_print(&ns->addr, addr, sizeof(addr), 2),
		  bsockaddr_get_port(&ns->addr),
		  bdns_get_type_name(q->key.qtype), 
		  q->key.name));

	if (ns->q_id == 0) {
	    ns->q_id = q->id;
	    ns->sent_time = now;
	}
    }

    if (send_cnt == 0) {
        btimer_heap_cancel(resolver->timer, &q->timer_entry);
	return UTIL_EDNSNOWORKINGNS;
    }

    ++q->transmit_cnt;

    return BASE_SUCCESS;
}


/*
 * Initialize resource key for hash table lookup.
 */
static void init_res_key(struct res_key *key, int type, const bstr_t *name)
{
    unsigned i;
    bsize_t len;
    char *dst = key->name;
    const char *src = name->ptr;

    bbzero(key, sizeof(struct res_key));
    key->qtype = (buint16_t)type;

    len = name->slen;
    if (len > BASE_MAX_HOSTNAME) len = BASE_MAX_HOSTNAME;

    /* Copy key, in lowercase */
    for (i=0; i<len; ++i) {
	*dst++ = (char)btolower(*src++);
    }
}


/* Allocate new cache entry */
static struct cached_res *alloc_entry(bdns_resolver *resolver)
{
    bpool_t *pool;
    struct cached_res *cache;

    pool = bpool_create(resolver->pool->factory, "dnscache",
			  RES_BUF_SZ, 256, NULL);
    cache = BASE_POOL_ZALLOC_T(pool, struct cached_res);
    cache->pool = pool;
    cache->ref_cnt = 1;

    return cache;
}

/* Re-allocate cache entry, to free cached packet */
static void reset_entry(struct cached_res **p_cached)
{
    bpool_t *pool;
    struct cached_res *cache = *p_cached;
    unsigned ref_cnt;

    pool = cache->pool;
    ref_cnt = cache->ref_cnt;

    bpool_reset(pool);

    cache = BASE_POOL_ZALLOC_T(pool, struct cached_res);
    cache->pool = pool;
    cache->ref_cnt = ref_cnt;
    *p_cached = cache;
}

/* Put unused/expired cached entry to the free list */
static void free_entry(bdns_resolver *resolver, struct cached_res *cache)
{
    BASE_UNUSED_ARG(resolver);
    bpool_release(cache->pool);
}


/*
 * Create and start asynchronous DNS query for a single resource.
 */
bstatus_t bdns_resolver_start_query( bdns_resolver *resolver,
						 const bstr_t *name,
						 int type,
						 unsigned options,
						 bdns_callback *cb,
						 void *user_data,
						 bdns_async_query **p_query)
{
    btime_val now;
    struct res_key key;
    struct cached_res *cache;
    bdns_async_query *q, *p_q = NULL;
    buint32_t hval;
    bstatus_t status = BASE_SUCCESS;

    /* Validate arguments */
    BASE_ASSERT_RETURN(resolver && name && type, BASE_EINVAL);

    /* Check name is not too long. */
    BASE_ASSERT_RETURN(name->slen>0 && name->slen < BASE_MAX_HOSTNAME,
		     BASE_ENAMETOOLONG);

    /* Check type */
    BASE_ASSERT_RETURN(type > 0 && type < 0xFFFF, BASE_EINVAL);

    /* Build resource key for looking up hash tables */
    init_res_key(&key, type, name);

    /* Start working with the resolver */
    bgrp_lock_acquire(resolver->grp_lock);

    /* Get current time. */
    bgettimeofday(&now);

    /* First, check if we have cached response for the specified name/type,
     * and the cached entry has not expired.
     */
    hval = 0;
    cache = (struct cached_res *) bhash_get(resolver->hrescache, &key, 
    					      sizeof(key), &hval);
    if (cache) {
	/* We've found a cached entry. */

	/* Check for expiration */
	if (BASE_TIME_VAL_GT(cache->expiry_time, now)) {

	    /* Log */
	    BASE_STR_INFO(resolver->name.ptr, 
		      "Picked up DNS %s record for %.*s from cache, ttl=%d",
		      bdns_get_type_name(type),
		      (int)name->slen, name->ptr,
		      (int)(cache->expiry_time.sec - now.sec));

	    /* Map DNS Rcode in the response into  status name space */
	    status = BASE_DNS_GET_RCODE(cache->pkt->hdr.flags);
	    status = BASE_STATUS_FROM_DNS_RCODE(status);

	    /* Workaround for deadlock problem. Need to increment the cache's
	     * ref counter first before releasing mutex, so the cache won't be
	     * destroyed by other thread while in callback.
	     */
	    cache->ref_cnt++;
	    bgrp_lock_release(resolver->grp_lock);

	    /* This cached response is still valid. Just return this
	     * response to caller.
	     */
	    if (cb) {
		(*cb)(user_data, status, cache->pkt);
	    }

	    /* Done. No host resolution is necessary */
	    bgrp_lock_acquire(resolver->grp_lock);

	    /* Decrement the ref counter. Also check if it is time to free
	     * the cache (as it has been expired).
	     */
	    cache->ref_cnt--;
	    if (cache->ref_cnt <= 0)
		free_entry(resolver, cache);

	    /* Must return BASE_SUCCESS */
	    status = BASE_SUCCESS;

	    /*
	     * We cannot write to *p_query after calling cb because what
	     * p_query points to may have been freed by cb.
             * Refer to ticket #1974.
	     */
	    bgrp_lock_release(resolver->grp_lock);
	    return status;
	}

	/* At this point, we have a cached entry, but this entry has expired.
	 * Remove this entry from the cached list.
	 */
	bhash_set(NULL, resolver->hrescache, &key, sizeof(key), 0, NULL);

	/* Also free the cache, if it is not being used (by callback). */
	cache->ref_cnt--;
	if (cache->ref_cnt <= 0)
	    free_entry(resolver, cache);

	/* Must continue with creating a query now */
    }

    /* Next, check if we have pending query on the same resource */
    q = (bdns_async_query *) bhash_get(resolver->hquerybyres, &key, 
    					   sizeof(key), NULL);
    if (q) {
	/* Yes, there's another pending query to the same key.
	 * Just create a new child query and add this query to
	 * pending query's child queries.
	 */
	bdns_async_query *nq;

	nq = alloc_qnode(resolver, options, user_data, cb);
	blist_push_back(&q->child_head, nq);

	/* Done. This child query will be notified once the "parent"
	 * query completes.
	 */
	p_q = nq;
	status = BASE_SUCCESS;
	goto on_return;
    } 

    /* There's no pending query to the same key, initiate a new one. */
    q = alloc_qnode(resolver, options, user_data, cb);

    /* Save the ID and key */
    /* TODO: dnsext-forgery-resilient: randomize id for security */
    q->id = resolver->last_id++;
    if (resolver->last_id == 0)
	resolver->last_id = 1;
    bmemcpy(&q->key, &key, sizeof(struct res_key));

    /* Send the query */
    status = transmit_query(resolver, q);
    if (status != BASE_SUCCESS) {
	blist_push_back(&resolver->query_free_nodes, q);
	goto on_return;
    }

    /* Add query entry to the hash tables */
    bhash_set_np(resolver->hquerybyid, &q->id, sizeof(q->id), 
		   0, q->hbufid, q);
    bhash_set_np(resolver->hquerybyres, &q->key, sizeof(q->key),
		   0, q->hbufkey, q);

    p_q = q;

on_return:
    if (p_query)
	*p_query = p_q;

    bgrp_lock_release(resolver->grp_lock);
    return status;
}


/*
 * Cancel a pending query.
 */
bstatus_t bdns_resolver_cancel_query(bdns_async_query *query,
						 bbool_t notify)
{
    bdns_callback *cb;

    BASE_ASSERT_RETURN(query, BASE_EINVAL);

    bgrp_lock_acquire(query->resolver->grp_lock);

    if (query->timer_entry.id == 1) {
	btimer_heap_cancel_if_active(query->resolver->timer,
				       &query->timer_entry, 0);
    }

    cb = query->cb;
    query->cb = NULL;

    if (notify)
	(*cb)(query->user_data, BASE_ECANCELLED, NULL);

    bgrp_lock_release(query->resolver->grp_lock);
    return BASE_SUCCESS;
}


/* 
 * DNS response containing A packet. 
 */
bstatus_t bdns_parse_a_response(const bdns_parsed_packet *pkt,
					    bdns_a_record *rec)
{
    enum { MAX_SEARCH = 20 };
    bstr_t hostname, alias = {NULL, 0}, *resname;
    bsize_t bufstart = 0;
    bsize_t bufleft = sizeof(rec->buf_);
    unsigned i, ansidx, search_cnt=0;

    BASE_ASSERT_RETURN(pkt && rec, BASE_EINVAL);

    /* Init the record */
    bbzero(rec, sizeof(bdns_a_record));

    /* Return error if there's error in the packet. */
    if (BASE_DNS_GET_RCODE(pkt->hdr.flags))
	return BASE_STATUS_FROM_DNS_RCODE(BASE_DNS_GET_RCODE(pkt->hdr.flags));

    /* Return error if there's no query section */
    if (pkt->hdr.qdcount == 0)
	return UTIL_EDNSINANSWER;

    /* Return error if there's no answer */
    if (pkt->hdr.anscount == 0)
	return UTIL_EDNSNOANSWERREC;

    /* Get the hostname from the query. */
    hostname = pkt->q[0].name;

    /* Copy hostname to the record */
    if (hostname.slen > (int)bufleft) {
	return BASE_ENAMETOOLONG;
    }

    bmemcpy(&rec->buf_[bufstart], hostname.ptr, hostname.slen);
    rec->name.ptr = &rec->buf_[bufstart];
    rec->name.slen = hostname.slen;

    bufstart += hostname.slen;
    bufleft -= hostname.slen;

    /* Find the first RR which name matches the hostname */
    for (ansidx=0; ansidx < pkt->hdr.anscount; ++ansidx) {
	if (bstricmp(&pkt->ans[ansidx].name, &hostname)==0)
	    break;
    }

    if (ansidx == pkt->hdr.anscount)
	return UTIL_EDNSNOANSWERREC;

    resname = &hostname;

    /* Keep following CNAME records. */
    while (pkt->ans[ansidx].type == BASE_DNS_TYPE_CNAME &&
	   search_cnt++ < MAX_SEARCH)
    {
	resname = &pkt->ans[ansidx].rdata.cname.name;

	if (!alias.slen)
	    alias = *resname;

	for (i=0; i < pkt->hdr.anscount; ++i) {
	    if (bstricmp(resname, &pkt->ans[i].name)==0) {
		break;
	    }
	}

	if (i==pkt->hdr.anscount)
	    return UTIL_EDNSNOANSWERREC;

	ansidx = i;
    }

    if (search_cnt >= MAX_SEARCH)
	return UTIL_EDNSINANSWER;

    if (pkt->ans[ansidx].type != BASE_DNS_TYPE_A)
	return UTIL_EDNSINANSWER;

    /* Copy alias to the record, if present. */
    if (alias.slen) {
	if (alias.slen > (int)bufleft)
	    return BASE_ENAMETOOLONG;

	bmemcpy(&rec->buf_[bufstart], alias.ptr, alias.slen);
	rec->alias.ptr = &rec->buf_[bufstart];
	rec->alias.slen = alias.slen;

	bufstart += alias.slen;
	bufleft -= alias.slen;
    }

    /* Get the IP addresses. */
    for (i=0; i < pkt->hdr.anscount; ++i) {
	if (pkt->ans[i].type == BASE_DNS_TYPE_A &&
	    bstricmp(&pkt->ans[i].name, resname)==0 &&
	    rec->addr_count < BASE_DNS_MAX_IP_IN_A_REC)
	{
	    rec->addr[rec->addr_count++].s_addr =
		pkt->ans[i].rdata.a.ip_addr.s_addr;
	}
    }

    if (rec->addr_count == 0)
	return UTIL_EDNSNOANSWERREC;

    return BASE_SUCCESS;
}


/* 
 * DNS response containing A and/or AAAA packet. 
 */
bstatus_t bdns_parse_addr_response(
					    const bdns_parsed_packet *pkt,
					    bdns_addr_record *rec)
{
    enum { MAX_SEARCH = 20 };
    bstr_t hostname, alias = {NULL, 0}, *resname;
    bsize_t bufstart = 0;
    bsize_t bufleft;
    unsigned i, ansidx, cnt=0;

    BASE_ASSERT_RETURN(pkt && rec, BASE_EINVAL);

    /* Init the record */
    bbzero(rec, sizeof(*rec));

    bufleft = sizeof(rec->buf_);

    /* Return error if there's error in the packet. */
    if (BASE_DNS_GET_RCODE(pkt->hdr.flags))
	return BASE_STATUS_FROM_DNS_RCODE(BASE_DNS_GET_RCODE(pkt->hdr.flags));

    /* Return error if there's no query section */
    if (pkt->hdr.qdcount == 0)
	return UTIL_EDNSINANSWER;

    /* Return error if there's no answer */
    if (pkt->hdr.anscount == 0)
	return UTIL_EDNSNOANSWERREC;

    /* Get the hostname from the query. */
    hostname = pkt->q[0].name;

    /* Copy hostname to the record */
    if (hostname.slen > (int)bufleft) {
	return BASE_ENAMETOOLONG;
    }

    bmemcpy(&rec->buf_[bufstart], hostname.ptr, hostname.slen);
    rec->name.ptr = &rec->buf_[bufstart];
    rec->name.slen = hostname.slen;

    bufstart += hostname.slen;
    bufleft -= hostname.slen;

    /* Find the first RR which name matches the hostname. */
    for (ansidx=0; ansidx < pkt->hdr.anscount; ++ansidx) {
	if (bstricmp(&pkt->ans[ansidx].name, &hostname)==0)
	    break;
    }

    if (ansidx == pkt->hdr.anscount)
	return UTIL_EDNSNOANSWERREC;

    resname = &hostname;

    /* Keep following CNAME records. */
    while (pkt->ans[ansidx].type == BASE_DNS_TYPE_CNAME &&
	   cnt++ < MAX_SEARCH)
    {
	resname = &pkt->ans[ansidx].rdata.cname.name;

	if (!alias.slen)
	    alias = *resname;

	for (i=0; i < pkt->hdr.anscount; ++i) {
	    if (bstricmp(resname, &pkt->ans[i].name)==0)
		break;
	}

	if (i==pkt->hdr.anscount)
	    return UTIL_EDNSNOANSWERREC;

	ansidx = i;
    }

    if (cnt >= MAX_SEARCH)
	return UTIL_EDNSINANSWER;

    if (pkt->ans[ansidx].type != BASE_DNS_TYPE_A &&
	pkt->ans[ansidx].type != BASE_DNS_TYPE_AAAA)
    {
	return UTIL_EDNSINANSWER;
    }

    /* Copy alias to the record, if present. */
    if (alias.slen) {
	if (alias.slen > (int)bufleft)
	    return BASE_ENAMETOOLONG;

	bmemcpy(&rec->buf_[bufstart], alias.ptr, alias.slen);
	rec->alias.ptr = &rec->buf_[bufstart];
	rec->alias.slen = alias.slen;

	bufstart += alias.slen;
	bufleft -= alias.slen;
    }

    /* Get the IP addresses. */
    cnt = 0;
    for (i=0; i < pkt->hdr.anscount && cnt < BASE_DNS_MAX_IP_IN_A_REC ; ++i) {
	if ((pkt->ans[i].type == BASE_DNS_TYPE_A ||
	     pkt->ans[i].type == BASE_DNS_TYPE_AAAA) &&
	    bstricmp(&pkt->ans[i].name, resname)==0)
	{
	    if (pkt->ans[i].type == BASE_DNS_TYPE_A) {
		rec->addr[cnt].af = bAF_INET();
		rec->addr[cnt].ip.v4 = pkt->ans[i].rdata.a.ip_addr;
	    } else {
		rec->addr[cnt].af = bAF_INET6();
		rec->addr[cnt].ip.v6 = pkt->ans[i].rdata.aaaa.ip_addr;
	    }
	    ++cnt;
	}
    }
    rec->addr_count = cnt;

    if (cnt == 0)
	return UTIL_EDNSNOANSWERREC;

    return BASE_SUCCESS;
}


/* Set nameserver state */
static void set_nameserver_state(bdns_resolver *resolver,
				 unsigned index,
				 enum ns_state state,
				 const btime_val *now)
{
    struct nameserver *ns = &resolver->ns[index];
    enum ns_state old_state = ns->state;
    char addr[BASE_INET6_ADDRSTRLEN];

    ns->state = state;
    ns->state_expiry = *now;

    if (state == STATE_PROBING)
	ns->state_expiry.sec += ((resolver->settings.qretr_count + 2) *
				 resolver->settings.qretr_delay) / 1000;
    else if (state == STATE_ACTIVE)
	ns->state_expiry.sec += resolver->settings.good_ns_ttl;
    else
	ns->state_expiry.sec += resolver->settings.bad_ns_ttl;

    BASE_STR_INFO(resolver->name.ptr, "Nameserver %s:%d state changed %s --> %s",
	       bsockaddr_print(&ns->addr, addr, sizeof(addr), 2),
	       bsockaddr_get_port(&ns->addr),
	       state_names[old_state], state_names[state]);
}


/* Select which nameserver(s) to use. Note this may return multiple
 * name servers. The algorithm to select which nameservers to be
 * sent the request to is as follows:
 *  - select the first nameserver that is known to be good for the
 *    last BASE_DNS_RESOLVER_GOOD_NS_TTL interval.
 *  - for all NSes, if last_known_good >= BASE_DNS_RESOLVER_GOOD_NS_TTL, 
 *    include the NS to re-check again that the server is still good,
 *    unless the NS is known to be bad in the last BASE_DNS_RESOLVER_BAD_NS_TTL
 *    interval.
 *  - for all NSes, if last_known_bad >= BASE_DNS_RESOLVER_BAD_NS_TTL, 
 *    also include the NS to re-check again that the server is still bad.
 */
static bstatus_t select_nameservers(bdns_resolver *resolver,
				      unsigned *count,
				      unsigned servers[])
{
    unsigned i, max_count=*count;
    int min;
    btime_val now;

    bassert(max_count > 0);

    *count = 0;
    servers[0] = 0xFFFF;

    /* Check that nameservers are configured. */
    if (resolver->ns_count == 0)
	return UTIL_EDNSNONS;

    bgettimeofday(&now);

    /* Select one Active nameserver with best response time. */
    for (min=-1, i=0; i<resolver->ns_count; ++i) {
	struct nameserver *ns = &resolver->ns[i];

	if (ns->state != STATE_ACTIVE)
	    continue;

	if (min == -1)
	    min = i;
	else if (BASE_TIME_VAL_LT(ns->rt_delay, resolver->ns[min].rt_delay))
	    min = i;
    }
    if (min != -1) {
	servers[0] = min;
	++(*count);
    }

    /* Scan nameservers. */
    for (i=0; i<resolver->ns_count && *count < max_count; ++i) {
	struct nameserver *ns = &resolver->ns[i];

	if (BASE_TIME_VAL_LTE(ns->state_expiry, now)) {
	    if (ns->state == STATE_PROBING) {
		set_nameserver_state(resolver, i, STATE_BAD, &now);
	    } else {
		set_nameserver_state(resolver, i, STATE_PROBING, &now);
		if ((int)i != min) {
		    servers[*count] = i;
		    ++(*count);
		}
	    }
	} else if (ns->state == STATE_PROBING && (int)i != min) {
	    servers[*count] = i;
	    ++(*count);
	}
    }

    return BASE_SUCCESS;
}


/* Update name server status */
static void report_nameserver_status(bdns_resolver *resolver,
				     const bsockaddr *ns_addr,
				     const bdns_parsed_packet *pkt)
{
    unsigned i;
    int rcode;
    buint32_t q_id;
    btime_val now;
    bbool_t is_good;

    /* Only mark nameserver as "bad" if it returned non-parseable response or
     * it returned the following status codes
     */
    if (pkt) {
	rcode = BASE_DNS_GET_RCODE(pkt->hdr.flags);
	q_id = pkt->hdr.id;
    } else {
	rcode = 0;
	q_id = (buint32_t)-1;
    }

    /* Some nameserver is reported to respond with BASE_DNS_RCODE_SERVFAIL for
     * missing AAAA record, and the standard doesn't seem to specify that
     * SERVFAIL should prevent the server to be contacted again for other
     * queries. So let's not mark nameserver as bad for SERVFAIL response.
     */
    if (!pkt || /* rcode == BASE_DNS_RCODE_SERVFAIL || */
	        rcode == BASE_DNS_RCODE_REFUSED ||
	        rcode == BASE_DNS_RCODE_NOTAUTH) 
    {
	is_good = BASE_FALSE;
    } else {
	is_good = BASE_TRUE;
    }


    /* Mark time */
    bgettimeofday(&now);

    /* Recheck all nameservers. */
    for (i=0; i<resolver->ns_count; ++i) {
	struct nameserver *ns = &resolver->ns[i];

	if (bsockaddr_cmp(&ns->addr, ns_addr) == 0) {
	    if (q_id == ns->q_id) {
		/* Calculate response time */
		btime_val rt = now;
		BASE_TIME_VAL_SUB(rt, ns->sent_time);
		ns->rt_delay = rt;
		ns->q_id = 0;
	    }
	    set_nameserver_state(resolver, i, 
				 (is_good ? STATE_ACTIVE : STATE_BAD), &now);
	    break;
	}
    }
}


/* Update response cache */
static void update_res_cache(bdns_resolver *resolver,
			     const struct res_key *key,
			     bstatus_t status,
			     bbool_t set_expiry,
			     const bdns_parsed_packet *pkt)
{
    struct cached_res *cache;
    buint32_t hval=0, ttl;

    /* If status is unsuccessful, clear the same entry from the cache */
    if (status != BASE_SUCCESS) {
	cache = (struct cached_res *) bhash_get(resolver->hrescache, key, 
						  sizeof(*key), &hval);
	/* Remove the entry before releasing its pool (see ticket #1710) */
	bhash_set(NULL, resolver->hrescache, key, sizeof(*key), hval, NULL);
	
	/* Free the entry */
	if (cache && --cache->ref_cnt <= 0)
	    free_entry(resolver, cache);
    }


    /* Calculate expiration time. */
    if (set_expiry) {
	if (pkt->hdr.anscount == 0 || status != BASE_SUCCESS) {
	    /* If we don't have answers for the name, then give a different
	     * ttl value (note: BASE_DNS_RESOLVER_INVALID_TTL may be zero, 
	     * which means that invalid names won't be kept in the cache)
	     */
	    ttl = BASE_DNS_RESOLVER_INVALID_TTL;

	} else {
	    /* Otherwise get the minimum TTL from the answers */
	    unsigned i;
	    ttl = 0xFFFFFFFF;
	    for (i=0; i<pkt->hdr.anscount; ++i) {
		if (pkt->ans[i].ttl < ttl)
		    ttl = pkt->ans[i].ttl;
	    }
	}
    } else {
	ttl = 0xFFFFFFFF;
    }

    /* Apply maximum TTL */
    if (ttl > resolver->settings.cache_max_ttl)
	ttl = resolver->settings.cache_max_ttl;

    /* Get a cache response entry */
    cache = (struct cached_res *) bhash_get(resolver->hrescache, key,
    					      sizeof(*key), &hval);

    /* If TTL is zero, clear the same entry in the hash table */
    if (ttl == 0) {
	/* Remove the entry before releasing its pool (see ticket #1710) */
	bhash_set(NULL, resolver->hrescache, key, sizeof(*key), hval, NULL);

	/* Free the entry */
	if (cache && --cache->ref_cnt <= 0)
	    free_entry(resolver, cache);
	return;
    }

    if (cache == NULL) {
	cache = alloc_entry(resolver);
    } else {
	/* Remove the entry before resetting its pool (see ticket #1710) */
	bhash_set(NULL, resolver->hrescache, key, sizeof(*key), hval, NULL);

	if (cache->ref_cnt > 1) {
	    /* When cache entry is being used by callback (to app),
	     * just decrement ref_cnt so it will be freed after
	     * the callback returns and allocate new entry.
	     */
	    cache->ref_cnt--;
	    cache = alloc_entry(resolver);
	} else {
	    /* Reset cache to avoid bloated cache pool */
	    reset_entry(&cache);
	}
    }

    /* Duplicate the packet.
     * We don't need to keep the NS and AR sections from the packet,
     * so exclude from duplication. We do need to keep the Query
     * section since DNS A parser needs the query section to know
     * the name being requested.
     */
    bdns_packet_dup(cache->pool, pkt, 
		      BASE_DNS_NO_NS | BASE_DNS_NO_AR,
		      &cache->pkt);

    /* Calculate expiration time */
    if (set_expiry) {
	bgettimeofday(&cache->expiry_time);
	cache->expiry_time.sec += ttl;
    } else {
	cache->expiry_time.sec = 0x7FFFFFFFL;
	cache->expiry_time.msec = 0;
    }

    /* Copy key to the cached response */
    bmemcpy(&cache->key, key, sizeof(*key));

    /* Update the hash table */
    bhash_set_np(resolver->hrescache, &cache->key, sizeof(*key), hval,
		   cache->hbuf, cache);

}


/* Callback to be called when query has timed out */
static void on_timeout( btimer_heap_t *timer_heap, struct _btimer_entry *entry)
{
    bdns_resolver *resolver;
    bdns_async_query *q, *cq;
    bstatus_t status;

    BASE_UNUSED_ARG(timer_heap);

    q = (bdns_async_query *) entry->user_data;
    resolver = q->resolver;

    bgrp_lock_acquire(resolver->grp_lock);

    /* Recheck that this query is still pending, since there is a slight
     * possibility of race condition (timer elapsed while at the same time
     * response arrives)
     */
    if (bhash_get(resolver->hquerybyid, &q->id, sizeof(q->id), NULL)==NULL) {
	/* Yeah, this query is done. */
	bgrp_lock_release(resolver->grp_lock);
	return;
    }

    /* Invalidate id. */
    q->timer_entry.id = 0;

    /* Check to see if we should retransmit instead of time out */
    if (q->transmit_cnt < resolver->settings.qretr_count) {
	status = transmit_query(resolver, q);
	if (status == BASE_SUCCESS) {
	    bgrp_lock_release(resolver->grp_lock);
	    return;
	} else {
	    /* Error occurs */
	    BASE_PERROR(4,(resolver->name.ptr, status,
			 "Error transmitting request"));

	    /* Let it fallback to timeout section below */
	}
    }

    /* Clear hash table entries */
    bhash_set(NULL, resolver->hquerybyid, &q->id, sizeof(q->id), 0, NULL);
    bhash_set(NULL, resolver->hquerybyres, &q->key, sizeof(q->key), 0, NULL);

    /* Workaround for deadlock problem in #1565 (similar to #1108) */
    bgrp_lock_release(resolver->grp_lock);

    /* Call application callback, if any. */
    if (q->cb)
	(*q->cb)(q->user_data, BASE_ETIMEDOUT, NULL);

    /* Call application callback for child queries. */
    cq = q->child_head.next;
    while (cq != (void*)&q->child_head) {
	if (cq->cb)
	    (*cq->cb)(cq->user_data, BASE_ETIMEDOUT, NULL);
	cq = cq->next;
    }

    /* Workaround for deadlock problem in #1565 (similar to #1108) */
    bgrp_lock_acquire(resolver->grp_lock);

    /* Clear data */
    q->timer_entry.id = 0;
    q->user_data = NULL;

    /* Put child entries into recycle list */
    cq = q->child_head.next;
    while (cq != (void*)&q->child_head) {
	bdns_async_query *next = cq->next;
	blist_push_back(&resolver->query_free_nodes, cq);
	cq = next;
    }

    /* Put query entry into recycle list */
    blist_push_back(&resolver->query_free_nodes, q);

    bgrp_lock_release(resolver->grp_lock);
}


/* Callback from ioqueue when packet is received */
static void on_read_complete(bioqueue_key_t *key, 
                             bioqueue_op_key_t *op_key, 
                             bssize_t bytes_read)
{
    bdns_resolver *resolver;
    bpool_t *pool = NULL;
    bdns_parsed_packet *dns_pkt;
    bdns_async_query *q;
    char addr[BASE_INET6_ADDRSTRLEN];
    bsockaddr *src_addr;
    int *src_addr_len;
    unsigned char *rx_pkt;
    bssize_t rx_pkt_size;
    bstatus_t status;
    BASE_USE_EXCEPTION;


    resolver = (bdns_resolver *) bioqueue_get_user_data(key);
    bassert(resolver);

#if BASE_HAS_IPV6
    if (key == resolver->udp6_key) {
	src_addr = &resolver->udp6_src_addr;
	src_addr_len = &resolver->udp6_addr_len;
	rx_pkt = resolver->udp6_rx_pkt;
	rx_pkt_size = sizeof(resolver->udp6_rx_pkt);
    } else 
#endif
    {
	src_addr = &resolver->udp_src_addr;
	src_addr_len = &resolver->udp_addr_len;
	rx_pkt = resolver->udp_rx_pkt;
	rx_pkt_size = sizeof(resolver->udp_rx_pkt);
    }

    bgrp_lock_acquire(resolver->grp_lock);


    /* Check for errors */
    if (bytes_read < 0) {
	status = (bstatus_t)-bytes_read;
	BASE_PERROR(4,(resolver->name.ptr, status, "DNS resolver read error"));

	goto read_nbpacket;
    }

    BASE_STR_INFO(resolver->name.ptr, 
	      "Received %d bytes DNS response from %s:%d",
	      (int)bytes_read, 
	      bsockaddr_print(src_addr, addr, sizeof(addr), 2),
	      bsockaddr_get_port(src_addr));


    /* Check for zero packet */
    if (bytes_read == 0)
	goto read_nbpacket;

    /* Create temporary pool from a fixed buffer */
    pool = bpool_create_on_buf("restmp", resolver->tmp_pool, 
				 sizeof(resolver->tmp_pool));

    /* Parse DNS response */
    status = -1;
    dns_pkt = NULL;
    BASE_TRY {
	status = bdns_parse_packet(pool, rx_pkt, 
				     (unsigned)bytes_read, &dns_pkt);
    }
    BASE_CATCH_ANY {
	status = BASE_ENOMEM;
    }
    BASE_END;

    /* Update nameserver status */
    report_nameserver_status(resolver, src_addr, dns_pkt);

    /* Handle parse error */
    if (status != BASE_SUCCESS) {
	BASE_PERROR(3,(resolver->name.ptr, status,
		     "Error parsing DNS response from %s:%d", 
		     bsockaddr_print(src_addr, addr, sizeof(addr), 2),
		     bsockaddr_get_port(src_addr)));
	goto read_nbpacket;
    }

    /* Find the query based on the transaction ID */
    q = (bdns_async_query*) 
        bhash_get(resolver->hquerybyid, &dns_pkt->hdr.id,
		    sizeof(dns_pkt->hdr.id), NULL);
    if (!q) {
	BASE_STR_INFO(resolver->name.ptr,  "DNS response from %s:%d id=%d discarded",
		  bsockaddr_print(src_addr, addr, sizeof(addr), 2),
		  bsockaddr_get_port(src_addr),
		  (unsigned)dns_pkt->hdr.id);
	goto read_nbpacket;
    }

    /* Map DNS Rcode in the response into  status name space */
    status = BASE_STATUS_FROM_DNS_RCODE(BASE_DNS_GET_RCODE(dns_pkt->hdr.flags));

    /* Cancel query timeout timer. */
    bassert(q->timer_entry.id != 0);
    btimer_heap_cancel(resolver->timer, &q->timer_entry);
    q->timer_entry.id = 0;

    /* Clear hash table entries */
    bhash_set(NULL, resolver->hquerybyid, &q->id, sizeof(q->id), 0, NULL);
    bhash_set(NULL, resolver->hquerybyres, &q->key, sizeof(q->key), 0, NULL);

    /* Workaround for deadlock problem in #1108 */
    bgrp_lock_release(resolver->grp_lock);

    /* Notify applications first, to allow application to modify the 
     * record before it is saved to the hash table.
     */
    if (q->cb)
	(*q->cb)(q->user_data, status, dns_pkt);

    /* If query has subqueries, notify subqueries's application callback */
    if (!blist_empty(&q->child_head)) {
	bdns_async_query *child_q;

	child_q = q->child_head.next;
	while (child_q != (bdns_async_query*)&q->child_head) {
	    if (child_q->cb)
		(*child_q->cb)(child_q->user_data, status, dns_pkt);
	    child_q = child_q->next;
	}
    }

    /* Workaround for deadlock problem in #1108 */
    bgrp_lock_acquire(resolver->grp_lock);

    /* Truncated responses MUST NOT be saved (cached). */
    if (BASE_DNS_GET_TC(dns_pkt->hdr.flags) == 0) {
	/* Save/update response cache. */
	update_res_cache(resolver, &q->key, status, BASE_TRUE, dns_pkt);
    }

    /* Recycle query objects, starting with the child queries */
    if (!blist_empty(&q->child_head)) {
	bdns_async_query *child_q;

	child_q = q->child_head.next;
	while (child_q != (bdns_async_query*)&q->child_head) {
	    bdns_async_query *next = child_q->next;
	    blist_erase(child_q);
	    blist_push_back(&resolver->query_free_nodes, child_q);
	    child_q = next;
	}
    }
    blist_push_back(&resolver->query_free_nodes, q);

read_nbpacket:
    if (pool) {
	/* needed just in case BASE_HAS_POOL_ALT_API is set */
	bpool_release(pool);
    }

    status = bioqueue_recvfrom(key, op_key, rx_pkt, &rx_pkt_size,
				 BASE_IOQUEUE_ALWAYS_ASYNC,
				 src_addr, src_addr_len);

    if (status != BASE_EPENDING && status != BASE_ECANCELLED) {
	BASE_PERROR(4,(resolver->name.ptr, status,
		     "DNS resolver ioqueue read error"));

	bassert(!"Unhandled error");
    }

    bgrp_lock_release(resolver->grp_lock);
}


/*
 * Put the specified DNS packet into DNS cache. This function is mainly used
 * for testing the resolver, however it can also be used to inject entries
 * into the resolver.
 */
bstatus_t bdns_resolver_add_entry( bdns_resolver *resolver,
					       const bdns_parsed_packet *pkt,
					       bbool_t set_ttl)
{
    struct res_key key;

    /* Sanity check */
    BASE_ASSERT_RETURN(resolver && pkt, BASE_EINVAL);

    /* Packet must be a DNS response */
    BASE_ASSERT_RETURN(BASE_DNS_GET_QR(pkt->hdr.flags) & 1, BASE_EINVAL);

    /* Make sure there are answers in the packet */
    BASE_ASSERT_RETURN((pkt->hdr.anscount && pkt->ans) ||
		      (pkt->hdr.qdcount && pkt->q),
		     UTIL_EDNSNOANSWERREC);

    bgrp_lock_acquire(resolver->grp_lock);

    /* Build resource key for looking up hash tables */
    bbzero(&key, sizeof(struct res_key));
    if (pkt->hdr.anscount) {
	/* Make sure name is not too long. */
	BASE_ASSERT_RETURN(pkt->ans[0].name.slen < BASE_MAX_HOSTNAME, 
			 BASE_ENAMETOOLONG);

	init_res_key(&key, pkt->ans[0].type, &pkt->ans[0].name);

    } else {
	/* Make sure name is not too long. */
	BASE_ASSERT_RETURN(pkt->q[0].name.slen < BASE_MAX_HOSTNAME, 
			 BASE_ENAMETOOLONG);

	init_res_key(&key, pkt->q[0].type, &pkt->q[0].name);
    }

    /* Insert entry. */
    update_res_cache(resolver, &key, BASE_SUCCESS, set_ttl, pkt);

    bgrp_lock_release(resolver->grp_lock);

    return BASE_SUCCESS;
}


/*
 * Get the total number of response in the response cache.
 */
unsigned bdns_resolver_get_cached_count(bdns_resolver *resolver)
{
    unsigned count;

    BASE_ASSERT_RETURN(resolver, 0);

    bgrp_lock_acquire(resolver->grp_lock);
    count = bhash_count(resolver->hrescache);
    bgrp_lock_release(resolver->grp_lock);

    return count;
}


/*
 * Dump resolver state to the log.
 */
void bdns_resolver_dump(bdns_resolver *resolver,
				  bbool_t detail)
{
#if BASE_LOG_MAX_LEVEL >= 3
    unsigned i;
    btime_val now;

    bgrp_lock_acquire(resolver->grp_lock);

    bgettimeofday(&now);

    BASE_STR_INFO(resolver->name.ptr, " Dumping resolver state:");

    BASE_STR_INFO(resolver->name.ptr, "  Name servers:");
    for (i=0; i<resolver->ns_count; ++i) {
	char addr[BASE_INET6_ADDRSTRLEN];
	struct nameserver *ns = &resolver->ns[i];

	BASE_STR_INFO(resolver->name.ptr,
		  "   NS %d: %s:%d (state=%s until %ds, rtt=%d ms)",
		  i,
		  bsockaddr_print(&ns->addr, addr, sizeof(addr), 2),
		  bsockaddr_get_port(&ns->addr),
		  state_names[ns->state],
		  ns->state_expiry.sec - now.sec,
		  BASE_TIME_VAL_MSEC(ns->rt_delay));
    }

    BASE_STR_INFO(resolver->name.ptr, "  Nb. of cached responses: %u", bhash_count(resolver->hrescache));
    if (detail) {
	bhash_iterator_t itbuf, *it;
	it = bhash_first(resolver->hrescache, &itbuf);
	while (it) {
	    struct cached_res *cache;
	    cache = (struct cached_res*)bhash_this(resolver->hrescache, it);
	    BASE_STR_INFO(resolver->name.ptr, 
		      "   Type %s: %s",
		      bdns_get_type_name(cache->key.qtype), 
		      cache->key.name);
	    it = bhash_next(resolver->hrescache, it);
	}
    }
    BASE_STR_INFO(resolver->name.ptr, "  Nb. of pending queries: %u (%u)",
	      bhash_count(resolver->hquerybyid),
	      bhash_count(resolver->hquerybyres));
    if (detail) {
	bhash_iterator_t itbuf, *it;
	it = bhash_first(resolver->hquerybyid, &itbuf);
	while (it) {
	    struct bdns_async_query *q;
	    q = (bdns_async_query*) bhash_this(resolver->hquerybyid, it);
	    BASE_STR_INFO(resolver->name.ptr, 
		      "   Type %s: %s",
		      bdns_get_type_name(q->key.qtype), 
		      q->key.name);
	    it = bhash_next(resolver->hquerybyid, it);
	}
    }
    BASE_STR_INFO(resolver->name.ptr, "  Nb. of pending query free nodes: %u",
	      blist_size(&resolver->query_free_nodes));
    BASE_STR_INFO(resolver->name.ptr, "  Nb. of timer entries: %u",
	      btimer_heap_count(resolver->timer));
    BASE_STR_INFO(resolver->name.ptr, "  Pool capacity: %d, used size: %d",
	      bpool_get_capacity(resolver->pool),
	      bpool_get_used_size(resolver->pool));

    bgrp_lock_release(resolver->grp_lock);
#endif
}

