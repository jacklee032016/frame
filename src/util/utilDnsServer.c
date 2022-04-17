/*
 * 
 */
#include <utilDnsServer.h>
#include <utilErrno.h>
#include <baseActiveSock.h>
#include <baseAssert.h>
#include <baseList.h>
#include <baseLog.h>
#include <basePool.h>
#include <baseString.h>

#define MAX_ANS	    16
#define MAX_PKT	    1500
#define MAX_LABEL   32

struct label_tab
{
    unsigned count;

    struct {
	unsigned pos;
	bstr_t label;
    } a[MAX_LABEL];
};

struct rr
{
    BASE_DECL_LIST_MEMBER(struct rr);
    bdns_parsed_rr	rec;
};


struct bdns_server
{
    bpool_t		*pool;
    bpool_factory	*pf;
    bactivesock_t	*asock;
    bioqueue_op_key_t	 send_key;
    struct rr		 rr_list;
};


static bbool_t on_data_recvfrom(bactivesock_t *asock,
				  void *data,
				  bsize_t size,
				  const bsockaddr_t *src_addr,
				  int addr_len,
				  bstatus_t status);


bstatus_t bdns_server_create( bpool_factory *pf,
				          bioqueue_t *ioqueue,
					  int af,
					  unsigned port,
					  unsigned flags,
				          bdns_server **p_srv)
{
    bpool_t *pool;
    bdns_server *srv;
    bsockaddr sock_addr;
    bactivesock_cb sock_cb;
    bstatus_t status;

    BASE_ASSERT_RETURN(pf && ioqueue && p_srv && flags==0, BASE_EINVAL);
    BASE_ASSERT_RETURN(af==bAF_INET() || af==bAF_INET6(), BASE_EINVAL);
    
    pool = bpool_create(pf, "dnsserver", 256, 256, NULL);
    srv = (bdns_server*) BASE_POOL_ZALLOC_T(pool, bdns_server);
    srv->pool = pool;
    srv->pf = pf;
    blist_init(&srv->rr_list);

    bbzero(&sock_addr, sizeof(sock_addr));
    sock_addr.addr.sa_family = (buint16_t)af;
    bsockaddr_set_port(&sock_addr, (buint16_t)port);
    
    bbzero(&sock_cb, sizeof(sock_cb));
    sock_cb.on_data_recvfrom = &on_data_recvfrom;

    status = bactivesock_create_udp(pool, &sock_addr, NULL, ioqueue,
				      &sock_cb, srv, &srv->asock, NULL);
    if (status != BASE_SUCCESS)
	goto on_error;

    bioqueue_op_key_init(&srv->send_key, sizeof(srv->send_key));

    status = bactivesock_start_recvfrom(srv->asock, pool, MAX_PKT, 0);
    if (status != BASE_SUCCESS)
	goto on_error;

    *p_srv = srv;
    return BASE_SUCCESS;

on_error:
    bdns_server_destroy(srv);
    return status;
}


bstatus_t bdns_server_destroy(bdns_server *srv)
{
    BASE_ASSERT_RETURN(srv, BASE_EINVAL);

    if (srv->asock) {
	bactivesock_close(srv->asock);
	srv->asock = NULL;
    }

    bpool_safe_release(&srv->pool);

    return BASE_SUCCESS;
}


static struct rr* find_rr( bdns_server *srv,
			   unsigned dns_class,
			   unsigned type	/* bdns_type */,
			   const bstr_t *name)
{
    struct rr *r;

    r = srv->rr_list.next;
    while (r != &srv->rr_list) {
	if (r->rec.dnsclass == dns_class && r->rec.type == type && 
	    bstricmp(&r->rec.name, name)==0)
	{
	    return r;
	}
	r = r->next;
    }

    return NULL;
}


bstatus_t bdns_server_add_rec( bdns_server *srv,
					   unsigned count,
					   const bdns_parsed_rr rr_param[])
{
    unsigned i;

    BASE_ASSERT_RETURN(srv && count && rr_param, BASE_EINVAL);

    for (i=0; i<count; ++i) {
	struct rr *rr;

	BASE_ASSERT_RETURN(find_rr(srv, rr_param[i].dnsclass, rr_param[i].type,
				 &rr_param[i].name) == NULL,
			 BASE_EEXISTS);

	rr = (struct rr*) BASE_POOL_ZALLOC_T(srv->pool, struct rr);
	bmemcpy(&rr->rec, &rr_param[i], sizeof(bdns_parsed_rr));

	blist_push_back(&srv->rr_list, rr);
    }

    return BASE_SUCCESS;
}


bstatus_t bdns_server_del_rec( bdns_server *srv,
					   int dns_class,
					   bdns_type type,
					   const bstr_t *name)
{
    struct rr *rr;

    BASE_ASSERT_RETURN(srv && type && name, BASE_EINVAL);

    rr = find_rr(srv, dns_class, type, name);
    if (!rr)
	return BASE_ENOTFOUND;

    blist_erase(rr);

    return BASE_SUCCESS;
}


static void write16(buint8_t *p, buint16_t val)
{
    p[0] = (buint8_t)(val >> 8);
    p[1] = (buint8_t)(val & 0xFF);
}

static void write32(buint8_t *p, buint32_t val)
{
    val = bhtonl(val);
    bmemcpy(p, &val, 4);
}

static int print_name(buint8_t *pkt, int size,
		      buint8_t *pos, const bstr_t *name,
		      struct label_tab *tab)
{
    buint8_t *p = pos;
    const char *endlabel, *endname;
    unsigned i;
    bstr_t label;

    /* Check if name is in the table */
    for (i=0; i<tab->count; ++i) {
	if (bstrcmp(&tab->a[i].label, name)==0)
	    break;
    }

    if (i != tab->count) {
	write16(p, (buint16_t)(tab->a[i].pos | (0xc0 << 8)));
	return 2;
    } else {
	if (tab->count < MAX_LABEL) {
	    tab->a[tab->count].pos = (unsigned)(p-pkt);
	    tab->a[tab->count].label.ptr = (char*)(p+1);
	    tab->a[tab->count].label.slen = name->slen;
	    ++tab->count;
	}
    }

    endlabel = name->ptr;
    endname = name->ptr + name->slen;

    label.ptr = (char*)name->ptr;

    while (endlabel != endname) {

	while (endlabel != endname && *endlabel != '.')
	    ++endlabel;

	label.slen = (endlabel - label.ptr);

	if (size < label.slen+1)
	    return -1;

	*p = (buint8_t)label.slen;
	bmemcpy(p+1, label.ptr, label.slen);

	size -= (int)(label.slen+1);
	p += (label.slen+1);

	if (endlabel != endname && *endlabel == '.')
	    ++endlabel;
	label.ptr = (char*)endlabel;
    }

    if (size == 0)
	return -1;

    *p++ = '\0';

    return (int)(p-pos);
}

static int print_rr(buint8_t *pkt, int size, buint8_t *pos,
		    const bdns_parsed_rr *rr, struct label_tab *tab)
{
    buint8_t *p = pos;
    int len;

    len = print_name(pkt, size, pos, &rr->name, tab);
    if (len < 0)
	return -1;

    p += len;
    size -= len;

    if (size < 8)
	return -1;

    bassert(rr->dnsclass == 1);

    write16(p+0, (buint16_t)rr->type);	/* type	    */
    write16(p+2, (buint16_t)rr->dnsclass);	/* class    */
    write32(p+4, rr->ttl);			/* TTL	    */

    p += 8;
    size -= 8;

    if (rr->type == BASE_DNS_TYPE_A) {

	if (size < 6)
	    return -1;

	/* RDLEN is 4 */
	write16(p, 4);

	/* Address */
	bmemcpy(p+2, &rr->rdata.a.ip_addr, 4);

	p += 6;
	size -= 6;

    } else if (rr->type == BASE_DNS_TYPE_AAAA) {

	if (size < 18)
	    return -1;

	/* RDLEN is 16 */
	write16(p, 16);

	/* Address */
	bmemcpy(p+2, &rr->rdata.aaaa.ip_addr, 16);

	p += 18;
	size -= 18;
    
    } else if (rr->type == BASE_DNS_TYPE_CNAME ||
	       rr->type == BASE_DNS_TYPE_NS ||
	       rr->type == BASE_DNS_TYPE_PTR) {

	if (size < 4)
	    return -1;

	len = print_name(pkt, size-2, p+2, &rr->rdata.cname.name, tab);
	if (len < 0)
	    return -1;

	write16(p, (buint16_t)len);

	p += (len + 2);
	size -= (len + 2);

    } else if (rr->type == BASE_DNS_TYPE_SRV) {

	if (size < 10)
	    return -1;

	write16(p+2, rr->rdata.srv.prio);   /* Priority */
	write16(p+4, rr->rdata.srv.weight); /* Weight */
	write16(p+6, rr->rdata.srv.port);   /* Port */

	/* Target */
	len = print_name(pkt, size-8, p+8, &rr->rdata.srv.target, tab);
	if (len < 0)
	    return -1;

	/* RDLEN */
	write16(p, (buint16_t)(len + 6));

	p += (len + 8);
	size -= (len + 8);

    } else {
	bassert(!"Not supported");
	return -1;
    }

    return (int)(p-pos);
}

static int print_packet(const bdns_parsed_packet *rec, buint8_t *pkt,
			int size)
{
    buint8_t *p = pkt;
    struct label_tab tab;
    int i, len;

    tab.count = 0;

    bassert(sizeof(bdns_hdr)==12);
    if (size < (int)sizeof(bdns_hdr))
	return -1;

    /* Initialize header */
    write16(p+0,  rec->hdr.id);
    write16(p+2,  rec->hdr.flags);
    write16(p+4,  rec->hdr.qdcount);
    write16(p+6,  rec->hdr.anscount);
    write16(p+8,  rec->hdr.nscount);
    write16(p+10, rec->hdr.arcount);

    p = pkt + sizeof(bdns_hdr);
    size -= sizeof(bdns_hdr);

    /* Print queries */
    for (i=0; i<rec->hdr.qdcount; ++i) {

	len = print_name(pkt, size, p, &rec->q[i].name, &tab);
	if (len < 0)
	    return -1;

	p += len;
	size -= len;

	if (size < 4)
	    return -1;

	/* Set type */
	write16(p+0, (buint16_t)rec->q[i].type);

	/* Set class (IN=1) */
	bassert(rec->q[i].dnsclass == 1);
	write16(p+2, rec->q[i].dnsclass);

	p += 4;
    }

    /* Print answers */
    for (i=0; i<rec->hdr.anscount; ++i) {
	len = print_rr(pkt, size, p, &rec->ans[i], &tab);
	if (len < 0)
	    return -1;

	p += len;
	size -= len;
    }

    /* Print NS records */
    for (i=0; i<rec->hdr.nscount; ++i) {
	len = print_rr(pkt, size, p, &rec->ns[i], &tab);
	if (len < 0)
	    return -1;

	p += len;
	size -= len;
    }

    /* Print additional records */
    for (i=0; i<rec->hdr.arcount; ++i) {
	len = print_rr(pkt, size, p, &rec->arr[i], &tab);
	if (len < 0)
	    return -1;

	p += len;
	size -= len;
    }

    return (int)(p - pkt);
}


static bbool_t on_data_recvfrom(bactivesock_t *asock,
				  void *data,
				  bsize_t size,
				  const bsockaddr_t *src_addr,
				  int addr_len,
				  bstatus_t status)
{
    bdns_server *srv;
    bpool_t *pool;
    bdns_parsed_packet *req;
    bdns_parsed_packet ans;
    struct rr *rr;
    bssize_t pkt_len;
    unsigned i;

    if (status != BASE_SUCCESS)
	return BASE_TRUE;

    srv = (bdns_server*) bactivesock_get_user_data(asock);
    pool = bpool_create(srv->pf, "dnssrvrx", 512, 256, NULL);

    status = bdns_parse_packet(pool, data, (unsigned)size, &req);
    if (status != BASE_SUCCESS) {
	char addrinfo[BASE_INET6_ADDRSTRLEN+10];
	bsockaddr_print(src_addr, addrinfo, sizeof(addrinfo), 3);
	BASE_PERROR(4,(THIS_FILE, status, "Error parsing query from %s",
		     addrinfo));
	goto on_return;
    }

    /* Init answer */
    bbzero(&ans, sizeof(ans));
    ans.hdr.id = req->hdr.id;
    ans.hdr.qdcount = 1;
    ans.q = (bdns_parsed_query*) BASE_POOL_ALLOC_T(pool, bdns_parsed_query);
    bmemcpy(ans.q, req->q, sizeof(bdns_parsed_query));

    if (req->hdr.qdcount != 1) {
	ans.hdr.flags = BASE_DNS_SET_RCODE(BASE_DNS_RCODE_FORMERR);
	goto send_pkt;
    }

    if (req->q[0].dnsclass != BASE_DNS_CLASS_IN) {
	ans.hdr.flags = BASE_DNS_SET_RCODE(BASE_DNS_RCODE_NOTIMPL);
	goto send_pkt;
    }

    /* Find the record */
    rr = find_rr(srv, req->q->dnsclass, req->q->type, &req->q->name);
    if (rr == NULL) {
	ans.hdr.flags = BASE_DNS_SET_RCODE(BASE_DNS_RCODE_NXDOMAIN);
	goto send_pkt;
    }

    /* Init answer record */
    ans.hdr.anscount = 0;
    ans.ans = (bdns_parsed_rr*)
	      bpool_calloc(pool, MAX_ANS, sizeof(bdns_parsed_rr));

    /* DNS SRV query needs special treatment since it returns multiple
     * records
     */
    if (req->q->type == BASE_DNS_TYPE_SRV) {
	struct rr *r;

	r = srv->rr_list.next;
	while (r != &srv->rr_list) {
	    if (r->rec.dnsclass == req->q->dnsclass && 
		r->rec.type == BASE_DNS_TYPE_SRV && 
		bstricmp(&r->rec.name, &req->q->name)==0 &&
		ans.hdr.anscount < MAX_ANS)
	    {
		bmemcpy(&ans.ans[ans.hdr.anscount], &r->rec,
			  sizeof(bdns_parsed_rr));
		++ans.hdr.anscount;
	    }
	    r = r->next;
	}
    } else {
	/* Otherwise just copy directly from the server record */
	bmemcpy(&ans.ans[ans.hdr.anscount], &rr->rec,
			  sizeof(bdns_parsed_rr));
	++ans.hdr.anscount;
    }

    /* For each CNAME entry, add A entry */
    for (i=0; i<ans.hdr.anscount && ans.hdr.anscount < MAX_ANS; ++i) {
	if (ans.ans[i].type == BASE_DNS_TYPE_CNAME) {
	    struct rr *r;

	    r = find_rr(srv, ans.ans[i].dnsclass, BASE_DNS_TYPE_A,
		        &ans.ans[i].name);
	    bmemcpy(&ans.ans[ans.hdr.anscount], &r->rec,
			      sizeof(bdns_parsed_rr));
	    ++ans.hdr.anscount;
	}
    }

send_pkt:
    pkt_len = print_packet(&ans, (buint8_t*)data, MAX_PKT);
    if (pkt_len < 1) {
	BASE_ERROR( "Error: answer too large");
	goto on_return;
    }

    status = bactivesock_sendto(srv->asock, &srv->send_key, data, &pkt_len,
				  0, src_addr, addr_len);
    if (status != BASE_SUCCESS && status != BASE_EPENDING) {
	BASE_PERROR(4,(THIS_FILE, status, "Error sending answer"));
	goto on_return;
    }

on_return:
    bpool_release(pool);
    return BASE_TRUE;
}

