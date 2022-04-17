/* 
 *
 */
#include <utilSrvResolver.h>
#include <utilErrno.h>
#include <baseArray.h>
#include <baseAssert.h>
#include <baseLog.h>
#include <baseOs.h>
#include <basePool.h>
#include <baseRand.h>
#include <baseString.h>

#define ADDR_MAX_COUNT	    BASE_DNS_MAX_IP_IN_A_REC

struct common
{
    bdns_type		     type;	    /**< Type of this structure.*/
};

#pragma pack(1)
struct srv_target
{
    struct common	    common;
    struct common	    common_aaaa;
    bdns_srv_async_query *parent;
    bstr_t		    target_name;
    bdns_async_query	   *q_a;
    bdns_async_query	   *q_aaaa;
    char		    target_buf[BASE_MAX_HOSTNAME];
    bstr_t		    cname;
    char		    cname_buf[BASE_MAX_HOSTNAME];
    buint16_t		    port;
    unsigned		    priority;
    unsigned		    weight;
    unsigned		    sum;
    unsigned		    addr_cnt;
    bsockaddr		    addr[ADDR_MAX_COUNT];/**< Address family and IP.*/
};
#pragma pack()

struct bdns_srv_async_query
{
    struct common	     common;
    char		    *objname;

    bdns_type		     dns_state;	    /**< DNS type being resolved.   */
    bdns_resolver	    *resolver;	    /**< Resolver SIP instance.	    */
    void		    *token;
    bdns_async_query	    *q_srv;
    bdns_srv_resolver_cb  *cb;
    bstatus_t		     last_error;

    /* Original request: */
    unsigned		     option;
    bstr_t		     full_name;
    bstr_t		     domain_part;
    buint16_t		     def_port;

    /* SRV records and their resolved IP addresses: */
    unsigned		     srv_cnt;
    struct srv_target	     srv[BASE_DNS_SRV_MAX_ADDR];

    /* Number of hosts in SRV records that the IP address has been resolved */
    unsigned		     host_resolved;

};


/* Async resolver callback, forward decl. */
static void dns_callback(void *user_data,
			 bstatus_t status,
			 bdns_parsed_packet *pkt);



/*
 * The public API to invoke DNS SRV resolution.
 */
bstatus_t bdns_srv_resolve( const bstr_t *domain_name,
				        const bstr_t *res_name,
					unsigned def_port,
					bpool_t *pool,
					bdns_resolver *resolver,
					unsigned option,
					void *token,
					bdns_srv_resolver_cb *cb,
					bdns_srv_async_query **p_query)
{
    bsize_t len;
    bstr_t target_name;
    bdns_srv_async_query *query_job;
    bstatus_t status;

    BASE_ASSERT_RETURN(domain_name && domain_name->slen &&
		     res_name && res_name->slen &&
		     pool && resolver && cb, BASE_EINVAL);

    /* Build full name */
    len = domain_name->slen + res_name->slen + 2;
    target_name.ptr = (char*) bpool_alloc(pool, len);
    bstrcpy(&target_name, res_name);
    if (res_name->ptr[res_name->slen-1] != '.')
	bstrcat2(&target_name, ".");
    len = target_name.slen;
    bstrcat(&target_name, domain_name);
    target_name.ptr[target_name.slen] = '\0';


    /* Build the query_job state */
    query_job = BASE_POOL_ZALLOC_T(pool, bdns_srv_async_query);
    query_job->common.type = BASE_DNS_TYPE_SRV;
    query_job->objname = target_name.ptr;
    query_job->resolver = resolver;
    query_job->token = token;
    query_job->cb = cb;
    query_job->option = option;
    query_job->full_name = target_name;
    query_job->domain_part.ptr = target_name.ptr + len;
    query_job->domain_part.slen = target_name.slen - len;
    query_job->def_port = (buint16_t)def_port;

    /* Normalize query job option BASE_DNS_SRV_RESOLVE_AAAA_ONLY */
    if (query_job->option & BASE_DNS_SRV_RESOLVE_AAAA_ONLY)
	query_job->option |= BASE_DNS_SRV_RESOLVE_AAAA;

    /* Start the asynchronous query_job */

    query_job->dns_state = BASE_DNS_TYPE_SRV;

    BASE_STR_INFO(query_job->objname, 
	       "Starting async DNS %s query_job: target=%.*s:%d",
	       bdns_get_type_name(query_job->dns_state),
	       (int)target_name.slen, target_name.ptr,
	       def_port);

    status = bdns_resolver_start_query(resolver, &target_name, 
				         query_job->dns_state, 0, 
					 &dns_callback,
    					 query_job, &query_job->q_srv);
    if (status==BASE_SUCCESS && p_query)
	*p_query = query_job;

    return status;
}


/*
 * Cancel pending query.
 */
bstatus_t bdns_srv_cancel_query(bdns_srv_async_query *query,
					    bbool_t notify)
{
    bbool_t has_pending = BASE_FALSE;
    unsigned i;

    if (query->q_srv) {
	bdns_resolver_cancel_query(query->q_srv, BASE_FALSE);
	query->q_srv = NULL;
	has_pending = BASE_TRUE;
    }

    for (i=0; i<query->srv_cnt; ++i) {
	struct srv_target *srv = &query->srv[i];
	if (srv->q_a) {
	    bdns_resolver_cancel_query(srv->q_a, BASE_FALSE);
	    srv->q_a = NULL;
	    has_pending = BASE_TRUE;
	}
	if (srv->q_aaaa) {
	    /* Check if it is a dummy query. */
	    if (srv->q_aaaa != (bdns_async_query*)0x1) {
		bdns_resolver_cancel_query(srv->q_aaaa, BASE_FALSE);
		has_pending = BASE_TRUE;
	    }
	    srv->q_aaaa = NULL;
	}
    }

    if (has_pending && notify && query->cb) {
	(*query->cb)(query->token, BASE_ECANCELLED, NULL);
    }

    return has_pending? BASE_SUCCESS : BASE_EINVALIDOP;
}


#define SWAP(type,ptr1,ptr2) if (ptr1 != ptr2) { \
				type tmp; \
				bmemcpy(&tmp, ptr1, sizeof(type)); \
				bmemcpy(ptr1, ptr2, sizeof(type)); \
				(ptr1)->target_name.ptr = (ptr1)->target_buf;\
				bmemcpy(ptr2, &tmp, sizeof(type)); \
				(ptr2)->target_name.ptr = (ptr2)->target_buf;\
			     } else {}


/* Build server entries in the query_job based on received SRV response */
static void build_server_entries(bdns_srv_async_query *query_job, 
				 bdns_parsed_packet *response)
{
    unsigned i;

    /* Save the Resource Records in DNS answer into SRV targets. */
    query_job->srv_cnt = 0;
    for (i=0; i<response->hdr.anscount && 
	      query_job->srv_cnt < BASE_DNS_SRV_MAX_ADDR; ++i) 
    {
	bdns_parsed_rr *rr = &response->ans[i];
	struct srv_target *srv = &query_job->srv[query_job->srv_cnt];

	if (rr->type != BASE_DNS_TYPE_SRV) {
	    BASE_STR_INFO(query_job->objname, 
		      "Received non SRV answer for SRV query_job!");
	    continue;
	}

	if (rr->rdata.srv.target.slen > BASE_MAX_HOSTNAME) {
	    BASE_STR_INFO(query_job->objname, "Hostname is too long!");
	    continue;
	}

	if (rr->rdata.srv.target.slen == 0) {
	    BASE_STR_INFO(query_job->objname, "Hostname is empty!");
	    continue;
	}

	/* Build the SRV entry for RR */
	bbzero(srv, sizeof(*srv));
	srv->target_name.ptr = srv->target_buf;
	bstrncpy(&srv->target_name, &rr->rdata.srv.target,
		   sizeof(srv->target_buf));
	srv->port = rr->rdata.srv.port;
	srv->priority = rr->rdata.srv.prio;
	srv->weight = rr->rdata.srv.weight;
	
	++query_job->srv_cnt;
    }

    if (query_job->srv_cnt == 0) {
	BASE_STR_INFO(query_job->objname, "Could not find SRV record in DNS answer!");
	return;
    }

    /* First pass: 
     *	order the entries based on priority.
     */
    for (i=0; i<query_job->srv_cnt-1; ++i) {
	unsigned min = i, j;
	for (j=i+1; j<query_job->srv_cnt; ++j) {
	    if (query_job->srv[j].priority < query_job->srv[min].priority)
		min = j;
	}
	SWAP(struct srv_target, &query_job->srv[i], &query_job->srv[min]);
    }

    /* Second pass:
     *	Order the entry in a list.
     *
     *  The algorithm for selecting server among servers with the same
     *  priority is described in RFC 2782.
     */
    for (i=0; i<query_job->srv_cnt; ++i) {
	unsigned j, count=1, sum;

	/* Calculate running sum for servers with the same priority */
	sum = query_job->srv[i].sum = query_job->srv[i].weight;
	for (j=i+1; j<query_job->srv_cnt && 
		    query_job->srv[j].priority == query_job->srv[i].priority; ++j)
	{
	    sum += query_job->srv[j].weight;
	    query_job->srv[j].sum = sum;
	    ++count;
	}

	if (count > 1) {
	    unsigned r;

	    /* Elect one random number between zero and the total sum of
	     * weight (inclusive).
	     */
	    r = brand() % (sum + 1);

	    /* Select the first server which running sum is greater than or
	     * equal to the random number.
	     */
	    for (j=i; j<i+count; ++j) {
		if (query_job->srv[j].sum >= r)
		    break;
	    }

	    /* Must have selected one! */
	    bassert(j != i+count);

	    /* Put this entry in front (of entries with same priority) */
	    SWAP(struct srv_target, &query_job->srv[i], &query_job->srv[j]);

	    /* Remove all other entries (of the same priority) */
	    /* Don't need to do this.
	     * See https://trac.extsip.org/repos/ticket/1719
	    while (count > 1) {
		barray_erase(query_job->srv, sizeof(struct srv_target), 
			       query_job->srv_cnt, i+1);
		--count;
		--query_job->srv_cnt;
	    }
	    */
	}
    }

    /* Since we've been moving around SRV entries, update the pointers
     * in target_name.
     */
    for (i=0; i<query_job->srv_cnt; ++i) {
	query_job->srv[i].target_name.ptr = query_job->srv[i].target_buf;
    }

    /* Check for Additional Info section if A/AAAA records are available, and
     * fill in the IP address (so that we won't need to resolve the A/AAAA 
     * record with another DNS query_job). 
     */
    for (i=0; i<response->hdr.arcount; ++i) {
	bdns_parsed_rr *rr = &response->arr[i];
	unsigned j;

	/* Skip non-A/AAAA record */
	if (rr->type != BASE_DNS_TYPE_A && rr->type != BASE_DNS_TYPE_AAAA)
	    continue;

	/* Also skip if:
	 * - it is A record and app only want AAAA record, or
	 * - it is AAAA record and app does not want AAAA record
	 */
	if ((rr->type == BASE_DNS_TYPE_A &&
	    (query_job->option & BASE_DNS_SRV_RESOLVE_AAAA_ONLY)!=0) ||
	    (rr->type == BASE_DNS_TYPE_AAAA &&
	    (query_job->option & BASE_DNS_SRV_RESOLVE_AAAA)==0))
	{
	    continue;
	}	    

	/* Yippeaiyee!! There is an "A/AAAA" record! 
	 * Update the IP address of the corresponding SRV record.
	 */
	for (j=0; j<query_job->srv_cnt; ++j) {
	    if (bstricmp(&rr->name, &query_job->srv[j].target_name)==0
		&& query_job->srv[j].addr_cnt < ADDR_MAX_COUNT)
	    {
		unsigned cnt = query_job->srv[j].addr_cnt;
		if (rr->type == BASE_DNS_TYPE_A) {
		    bsockaddr_init(bAF_INET(),
					&query_job->srv[j].addr[cnt], NULL,
					query_job->srv[j].port);
		    query_job->srv[j].addr[cnt].ipv4.sin_addr =
						rr->rdata.a.ip_addr;
		} else {
		    bsockaddr_init(bAF_INET6(),
					&query_job->srv[j].addr[cnt], NULL,
					query_job->srv[j].port);
		    query_job->srv[j].addr[cnt].ipv6.sin6_addr =
						rr->rdata.aaaa.ip_addr;
		}

		/* Only increment host_resolved once per SRV record */
		if (query_job->srv[j].addr_cnt == 0)
		    ++query_job->host_resolved;

		++query_job->srv[j].addr_cnt;
		break;
	    }
	}

	/* Not valid message; SRV entry might have been deleted in
	 * server selection process.
	 */
	/*
	if (j == query_job->srv_cnt) {
	    BASE_STR_INFO(query_job->objname, 
		      "Received DNS SRV answer with A record, but "
		      "couldn't find matching name (name=%.*s)",
		      (int)rr->name.slen,
		      rr->name.ptr);
	}
	*/
	
    }

    /* Rescan again the name specified in the SRV record to see if IP
     * address is specified as the target name (unlikely, but well, who 
     * knows..).
     */
    for (i=0; i<query_job->srv_cnt; ++i) {
	bin_addr addr;
	bin6_addr addr6;
	unsigned cnt = query_job->srv[i].addr_cnt;

	if (cnt != 0) {
	    /* IP address already resolved */
	    continue;
	}

	if ((query_job->option & BASE_DNS_SRV_RESOLVE_AAAA_ONLY)==0 &&
	    binet_pton(bAF_INET(), &query_job->srv[i].target_name,
			 &addr) == BASE_SUCCESS)
	{
	    bsockaddr_init(bAF_INET(), &query_job->srv[i].addr[cnt],
			     NULL, query_job->srv[i].port);
	    query_job->srv[i].addr[cnt].ipv4.sin_addr = addr;
	    ++query_job->srv[i].addr_cnt;
	    ++query_job->host_resolved;
	} else if ((query_job->option & BASE_DNS_SRV_RESOLVE_AAAA)!=0 &&
		   binet_pton(bAF_INET6(), &query_job->srv[i].target_name,
				&addr6) == BASE_SUCCESS)
	{
	    bsockaddr_init(bAF_INET6(), &query_job->srv[i].addr[cnt],
			     NULL, query_job->srv[i].port);
	    query_job->srv[i].addr[cnt].ipv6.sin6_addr = addr6;
	    ++query_job->srv[i].addr_cnt;
	    ++query_job->host_resolved;
	}
    }

    /* Print resolved entries to the log */
    BASE_STR_INFO(query_job->objname, 
	      "SRV query_job for %.*s completed, "
	      "%d of %d total entries selected%c",
	      (int)query_job->full_name.slen,
	      query_job->full_name.ptr,
	      query_job->srv_cnt,
	      response->hdr.anscount,
	      (query_job->srv_cnt ? ':' : ' '));

    for (i=0; i<query_job->srv_cnt; ++i) {
	char addr[BASE_INET6_ADDRSTRLEN];

	if (query_job->srv[i].addr_cnt != 0) {
	    bsockaddr_print(&query_job->srv[i].addr[0],
			 addr, sizeof(addr), 2);
	} else
	    bansi_strcpy(addr, "-");

	BASE_STR_INFO(query_job->objname, 
		  " %d: SRV %d %d %d %.*s (%s)",
		  i, query_job->srv[i].priority, 
		  query_job->srv[i].weight, 
		  query_job->srv[i].port, 
		  (int)query_job->srv[i].target_name.slen, 
		  query_job->srv[i].target_name.ptr,
		  addr);
    }
}


/* Start DNS A and/or AAAA record queries for all SRV records in
 * the query_job structure.
 */
static bstatus_t resolve_hostnames(bdns_srv_async_query *query_job)
{
    unsigned i, err_cnt = 0;
    bstatus_t err=BASE_SUCCESS, status;

    query_job->dns_state = BASE_DNS_TYPE_A;

    for (i=0; i<query_job->srv_cnt; ++i) {
	struct srv_target *srv = &query_job->srv[i];

	if (srv->addr_cnt != 0) {
	    /*
	     * This query is already counted as resolved because of the
	     * additional records in the SRV response or the target name
	     * is an IP address exception in build_server_entries().
	     */
	    continue;
	}

	BASE_STR_INFO(query_job->objname, 
		   "Starting async DNS A query_job for %.*s",
		   (int)srv->target_name.slen, 
		   srv->target_name.ptr);

	srv->common.type = BASE_DNS_TYPE_A;
	srv->common_aaaa.type = BASE_DNS_TYPE_AAAA;
	srv->parent = query_job;
	srv->q_a = NULL;
	srv->q_aaaa = NULL;

	status = BASE_SUCCESS;

	/* Start DNS A record query */
	if ((query_job->option & BASE_DNS_SRV_RESOLVE_AAAA_ONLY) == 0)
	{
	    if ((query_job->option & BASE_DNS_SRV_RESOLVE_AAAA) != 0) {
		/* If there will be DNS AAAA query too, let's setup
		 * a dummy one here, otherwise app callback may be called
		 * immediately (before DNS AAAA query is sent) when
		 * DNS A record is available in the cache.
		 */
		srv->q_aaaa = (bdns_async_query*)0x1;
	    }
	    status = bdns_resolver_start_query(query_job->resolver,
						 &srv->target_name,
						 BASE_DNS_TYPE_A, 0,
						 &dns_callback,
						 &srv->common, &srv->q_a);
	}

	/* Start DNS AAAA record query */
	if (status == BASE_SUCCESS &&
	    (query_job->option & BASE_DNS_SRV_RESOLVE_AAAA) != 0)
	{
	    status = bdns_resolver_start_query(query_job->resolver,
						 &srv->target_name,
						 BASE_DNS_TYPE_AAAA, 0,
						 &dns_callback,
						 &srv->common_aaaa, &srv->q_aaaa);
	}

	/* See also #1809: dns_callback() will be invoked synchronously when response
	 * is available in the cache, and var 'query_job->host_resolved' will get
	 * incremented within the dns_callback(), which will cause this function
	 * returning false error, so don't use that variable for counting errors.
	 */
	if (status != BASE_SUCCESS) {
	    query_job->host_resolved++;
	    err_cnt++;
	    err = status;
	}
    }
    
    return (err_cnt == query_job->srv_cnt) ? err : BASE_SUCCESS;
}

/* 
 * This callback is called by -UTIL DNS resolver when asynchronous
 * query_job has completed (successfully or with error).
 */
static void dns_callback(void *user_data,
			 bstatus_t status,
			 bdns_parsed_packet *pkt)
{
    struct common *common = (struct common*) user_data;
    bdns_srv_async_query *query_job;
    struct srv_target *srv = NULL;
    unsigned i;

    if (common->type == BASE_DNS_TYPE_SRV) {
	query_job = (bdns_srv_async_query*) common;
	srv = NULL;
    } else if (common->type == BASE_DNS_TYPE_A) {
	srv = (struct srv_target*) common;
	query_job = srv->parent;
    } else if (common->type == BASE_DNS_TYPE_AAAA) {
	srv = (struct srv_target*)((bint8_t*)common-sizeof(struct common));
	query_job = srv->parent;
    } else {
	bassert(!"Unexpected user data!");
	return;
    }

    /* Proceed to next stage */
    if (query_job->dns_state == BASE_DNS_TYPE_SRV) {

	/* We are getting SRV response */

	/* Clear the outstanding job */
	query_job->q_srv = NULL;

	if (status == BASE_SUCCESS) {
	    if (BASE_DNS_GET_TC(pkt->hdr.flags)) {
		/* Got truncated answer, the standard recommends to follow up
		 * the query using TCP. Since we currently don't support it,
		 * just return error.
		 */
		BASE_STR_INFO(query_job->objname,
			  "Discard truncated DNS SRV response for %.*s",
			  (int)query_job->full_name.slen,
			  query_job->full_name.ptr);

		status = BASE_EIGNORED;
		query_job->last_error = status;
		goto on_error;
	    } else if (pkt->hdr.anscount != 0) {
		/* Got SRV response, build server entry. If A records are
		 * available in additional records section of the DNS response,
		 * save them too.
		 */
		build_server_entries(query_job, pkt);
	    }

	} else {
	    char errmsg[BASE_ERR_MSG_SIZE];

	    /* Update query_job last error */
	    query_job->last_error = status;

	    extStrError(status, errmsg, sizeof(errmsg));
	    BASE_STR_INFO(query_job->objname, "DNS SRV resolution failed for %.*s: %s", 
		      (int)query_job->full_name.slen, 
		      query_job->full_name.ptr,
		      errmsg);

	    /* Trigger error when fallback is disabled */
	    if ((query_job->option &
		 (BASE_DNS_SRV_FALLBACK_A | BASE_DNS_SRV_FALLBACK_AAAA)) == 0) 
	    {
		goto on_error;
	    }
	}

	/* If we can't build SRV record, assume the original target is
	 * an A record and resolve with DNS A resolution.
	 */
	if (query_job->srv_cnt == 0 && query_job->domain_part.slen > 0) {
	    unsigned new_option = 0;

	    /* Looks like we aren't getting any SRV responses.
	     * Resolve the original target as A record by creating a 
	     * single "dummy" srv record and start the hostname resolution.
	     */
	    BASE_STR_INFO(query_job->objname, 
		       "DNS SRV resolution failed for %.*s, trying "
		       "resolving A/AAAA record for %.*s",
		       (int)query_job->full_name.slen, 
		       query_job->full_name.ptr,
		       (int)query_job->domain_part.slen,
		       query_job->domain_part.ptr);

	    /* Create a "dummy" srv record using the original target */
	    i = query_job->srv_cnt++;
	    bbzero(&query_job->srv[i], sizeof(query_job->srv[i]));
	    query_job->srv[i].target_name = query_job->domain_part;
	    query_job->srv[i].priority = 0;
	    query_job->srv[i].weight = 0;
	    query_job->srv[i].port = query_job->def_port;

	    /* Update query_job resolution option based on fallback option */
	    if (query_job->option & BASE_DNS_SRV_FALLBACK_AAAA)
		new_option |= (BASE_DNS_SRV_RESOLVE_AAAA |
			       BASE_DNS_SRV_RESOLVE_AAAA_ONLY);
	    if (query_job->option & BASE_DNS_SRV_FALLBACK_A)
		new_option &= (~BASE_DNS_SRV_RESOLVE_AAAA_ONLY);
	    
	    query_job->option = new_option;
	}
	

	/* Resolve server hostnames (DNS A/AAAA record) for hosts which
	 * don't have A/AAAA record yet.
	 */
	if (query_job->host_resolved != query_job->srv_cnt) {
	    status = resolve_hostnames(query_job);
	    if (status != BASE_SUCCESS)
		goto on_error;

	    /* Must return now. Callback may have been called and query_job
	     * may have been destroyed.
	     */
	    return;
	}

    } else if (query_job->dns_state == BASE_DNS_TYPE_A) {
	bbool_t is_type_a, srv_completed;
        bdns_addr_record rec;

	/* Avoid warning: potentially uninitialized local variable 'rec' */
	rec.alias.slen = 0;
	rec.addr_count = 0;

	/* Clear outstanding job */
	if (common->type == BASE_DNS_TYPE_A) {
	    srv_completed = (srv->q_aaaa == NULL);
	    srv->q_a = NULL;
	} else if (common->type == BASE_DNS_TYPE_AAAA) {
	    srv_completed = (srv->q_a == NULL);
	    srv->q_aaaa = NULL;
	} else {
	    bassert(!"Unexpected job type");
	    query_job->last_error = status = BASE_EINVALIDOP;
	    goto on_error;
	}

	is_type_a = (common->type == BASE_DNS_TYPE_A);

        /* Parse response */
        if (status==BASE_SUCCESS && pkt->hdr.anscount != 0) {
            status = bdns_parse_addr_response(pkt, &rec);
            if (status!=BASE_SUCCESS) {
                BASE_PERROR(4,(query_job->objname, status,
			     "DNS %s record parse error for '%.*s'.",
			     (is_type_a ? "A" : "AAAA"),
			     (int)query_job->domain_part.slen,
			     query_job->domain_part.ptr));
	    }
	}

	/* Check that we really have answer */
	if (status==BASE_SUCCESS && pkt->hdr.anscount != 0) {
	    char addr[BASE_INET6_ADDRSTRLEN];

	    bassert(rec.addr_count != 0);

	    /* Update CNAME alias, if present. */
	    if (srv->cname.slen==0 && rec.alias.slen) {
		bassert(rec.alias.slen <= (int)sizeof(srv->cname_buf));
		srv->cname.ptr = srv->cname_buf;
		bstrcpy(&srv->cname, &rec.alias);
	    //} else {
		//srv->cname.slen = 0;
	    }

	    /* Update IP address of the corresponding hostname or CNAME */
	    for (i=0; i<rec.addr_count && srv->addr_cnt<ADDR_MAX_COUNT; ++i)
	    {
		bbool_t added = BASE_FALSE;

		if (is_type_a && rec.addr[i].af == bAF_INET()) {
		    bsockaddr_init(bAF_INET(), &srv->addr[srv->addr_cnt],
				     NULL, srv->port);
		    srv->addr[srv->addr_cnt].ipv4.sin_addr =
				     rec.addr[i].ip.v4;
		    added = BASE_TRUE;
		} else if (!is_type_a && rec.addr[i].af == bAF_INET6()) {
		    bsockaddr_init(bAF_INET6(), &srv->addr[srv->addr_cnt],
				     NULL, srv->port);
		    srv->addr[srv->addr_cnt].ipv6.sin6_addr =
				     rec.addr[i].ip.v6;
		    added = BASE_TRUE;
		} else {
		    /* Mismatched address family, e.g: getting IPv6 address in
		     * DNS A query resolution.
		     */
		    BASE_STR_INFO(query_job->objname, 
			      "Bad address family in DNS %s query for %.*s",
			      (is_type_a? "A" : "AAAA"),
			      (int)srv->target_name.slen, 
			      srv->target_name.ptr);
		}

		if (added) {
		    BASE_STR_INFO(query_job->objname, 
			      "DNS %s for %.*s: %s",
			      (is_type_a? "A" : "AAAA"),
			      (int)srv->target_name.slen, 
			      srv->target_name.ptr,
			      bsockaddr_print(&srv->addr[srv->addr_cnt],
						addr, sizeof(addr), 2));

		    ++srv->addr_cnt;
		}
	    }

	} else if (status != BASE_SUCCESS) {
	    /* Update last error */
	    query_job->last_error = status;

	    /* Log error */
	    BASE_PERROR(4,(query_job->objname, status,
			 "DNS %s record resolution failed",
			 (is_type_a? "A" : "AAAA")));
	}

	/* Increment host resolved count when both DNS A and AAAA record
	 * queries for this server are completed.
	 */
	if (srv_completed)
	    ++query_job->host_resolved;

    } else {
	bassert(!"Unexpected state!");
	query_job->last_error = status = BASE_EINVALIDOP;
	goto on_error;
    }

    /* Check if all hosts have been resolved */
    if (query_job->host_resolved == query_job->srv_cnt) {
	/* Got all answers, build server addresses */
	bdns_srv_record srv_rec;

	srv_rec.count = 0;
	for (i=0; i<query_job->srv_cnt; ++i) {
	    unsigned j;
	    struct srv_target *srv2 = &query_job->srv[i];
	    bdns_addr_record *s = &srv_rec.entry[srv_rec.count].server;

	    srv_rec.entry[srv_rec.count].priority = srv2->priority;
	    srv_rec.entry[srv_rec.count].weight = srv2->weight;
	    srv_rec.entry[srv_rec.count].port = (buint16_t)srv2->port ;

	    srv_rec.entry[srv_rec.count].server.name = srv2->target_name;
	    srv_rec.entry[srv_rec.count].server.alias = srv2->cname;
	    srv_rec.entry[srv_rec.count].server.addr_count = 0;

	    bassert(srv2->addr_cnt <= BASE_DNS_MAX_IP_IN_A_REC);

	    for (j=0; j<srv2->addr_cnt; ++j) {		
		s->addr[j].af = srv2->addr[j].addr.sa_family;
		if (s->addr[j].af == bAF_INET())
		    s->addr[j].ip.v4 = srv2->addr[j].ipv4.sin_addr;
		else
		    s->addr[j].ip.v6 = srv2->addr[j].ipv6.sin6_addr;
		++s->addr_count;
	    }

	    if (srv2->addr_cnt > 0) {
		++srv_rec.count;
		if (srv_rec.count == BASE_DNS_SRV_MAX_ADDR)
		    break;
	    }
	}

	BASE_STR_INFO(query_job->objname, "Server resolution complete, %d server entry(s) found", srv_rec.count);


	if (srv_rec.count > 0)
	    status = BASE_SUCCESS;
	else {
	    status = query_job->last_error;
	    if (status == BASE_SUCCESS)
		status = UTIL_EDNSNOANSWERREC;
	}

	/* Call the callback */
	(*query_job->cb)(query_job->token, status, &srv_rec);
    }


    return;

on_error:
    /* Check for failure */
    if (status != BASE_SUCCESS) {
	BASE_PERROR(4,(query_job->objname, status,
		     "DNS %s record resolution error for '%.*s'.",
		     bdns_get_type_name(query_job->dns_state),
		     (int)query_job->domain_part.slen,
		     query_job->domain_part.ptr));

	/* Cancel any pending query */
	bdns_srv_cancel_query(query_job, BASE_FALSE);

	(*query_job->cb)(query_job->token, status, NULL);
	return;
    }
}


