/* 
 *
 */
 
#include "testUtilTest.h"
#include <libBase.h>
#include <libUtil.h>


/*
 * TODO: create various invalid DNS packets.
 */


#define ACTION_REPLY	0
#define ACTION_IGNORE	-1
#define ACTION_CB	-2

static struct server_t
{
    bsock_t	     sock;
    buint16_t	     port;
    bthread_t	    *thread;

    /* Action:
     *	0:    reply with the response in resp.
     * -1:    ignore query (to simulate timeout).
     * other: reply with that error
     */
    int		    action;

    bdns_parsed_packet    resp;
    void		  (*action_cb)(const bdns_parsed_packet *pkt,
				       bdns_parsed_packet **p_res);

    unsigned	    pkt_count;

} g_server[2];

static bpool_t *pool;
static bdns_resolver *resolver;
static bbool_t thread_quit;
static btimer_heap_t *timer_heap;
static bioqueue_t *ioqueue;
static bthread_t *poll_thread;
static bsem_t *sem;
static bdns_settings set;

#define MAX_LABEL   32

struct label_tab
{
    unsigned count;

    struct {
	unsigned pos;
	bstr_t label;
    } a[MAX_LABEL];
};

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

#if 0
    benter_critical_section();
    BASE_INFO("Sending response:"));
    bdns_dump_packet(rec);
    bleave_critical_section();
#endif

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


static int server_thread(void *p)
{
    struct server_t *srv = (struct server_t*)p;

    while (!thread_quit) {
	bfd_set_t rset;
	btime_val timeout = {0, 500};
	bsockaddr src_addr;
	bdns_parsed_packet *req;
	char pkt[1024];
	bssize_t pkt_len;
	int rc, src_len;

	BASE_FD_ZERO(&rset);
	BASE_FD_SET(srv->sock, &rset);

	rc = bsock_select((int)(srv->sock+1), &rset, NULL, NULL, &timeout);
	if (rc != 1)
	    continue;

	src_len = sizeof(src_addr);
	pkt_len = sizeof(pkt);
	rc = bsock_recvfrom(srv->sock, pkt, &pkt_len, 0, 
			      &src_addr, &src_len);
	if (rc != 0) {
	    app_perror("Server error receiving packet", rc);
	    continue;
	}

	BASE_INFO( "Server %d processing packet", srv - &g_server[0]);
	srv->pkt_count++;

	rc = bdns_parse_packet(pool, pkt, (unsigned)pkt_len, &req);
	if (rc != BASE_SUCCESS) {
	    app_perror("server error parsing packet", rc);
	    continue;
	}

	/* Verify packet */
	bassert(req->hdr.qdcount == 1);
	bassert(req->q[0].dnsclass == 1);

	/* Simulate network RTT */
	bthreadSleepMs(50);

	if (srv->action == ACTION_IGNORE) {
	    continue;
	} else if (srv->action == ACTION_REPLY) {
	    srv->resp.hdr.id = req->hdr.id;
	    pkt_len = print_packet(&srv->resp, (buint8_t*)pkt, sizeof(pkt));
	    bsock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
	} else if (srv->action == ACTION_CB) {
	    bdns_parsed_packet *resp;
	    (*srv->action_cb)(req, &resp);
	    resp->hdr.id = req->hdr.id;
	    pkt_len = print_packet(resp, (buint8_t*)pkt, sizeof(pkt));
	    bsock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
	} else if (srv->action > 0) {
	    req->hdr.flags |= BASE_DNS_SET_RCODE(srv->action);
	    pkt_len = print_packet(req, (buint8_t*)pkt, sizeof(pkt));
	    bsock_sendto(srv->sock, pkt, &pkt_len, 0, &src_addr, src_len);
	}
    }

    return 0;
}

static int poll_worker_thread(void *p)
{
    BASE_UNUSED_ARG(p);

    while (!thread_quit) {
	btime_val delay = {0, 100};
	btimer_heap_poll(timer_heap, NULL);
	bioqueue_poll(ioqueue, &delay);
    }

    return 0;
}

static void destroy(void);

static int init(bbool_t use_ipv6)
{
    bstatus_t status;
    bstr_t nameservers[2];
    buint16_t ports[2];
    int i;

    if (use_ipv6) {
	nameservers[0] = bstr("::1");
	nameservers[1] = bstr("::1");
    } else {
	nameservers[0] = bstr("127.0.0.1");
	nameservers[1] = bstr("127.0.0.1");
    }
    ports[0] = 5553;
    ports[1] = 5554;

    g_server[0].port = ports[0];
    g_server[1].port = ports[1];

    pool = bpool_create(mem, NULL, 2000, 2000, NULL);

    status = bsem_create(pool, NULL, 0, 2, &sem);
    bassert(status == BASE_SUCCESS);

    thread_quit = BASE_FALSE;

    for (i=0; i<2; ++i) {
	bsockaddr addr;

	status = bsock_socket((use_ipv6? bAF_INET6() : bAF_INET()),
				bSOCK_DGRAM(), 0, &g_server[i].sock);
	if (status != BASE_SUCCESS)
	    return -10;

	bsockaddr_init((use_ipv6? bAF_INET6() : bAF_INET()),
			 &addr, NULL, (buint16_t)g_server[i].port);

	status = bsock_bind(g_server[i].sock, &addr, bsockaddr_get_len(&addr));
	if (status != BASE_SUCCESS)
	    return -20;

	status = bthreadCreate(pool, NULL, &server_thread, &g_server[i],
				  0, 0, &g_server[i].thread);
	if (status != BASE_SUCCESS)
	    return -30;
    }

    status = btimer_heap_create(pool, 16, &timer_heap);
    bassert(status == BASE_SUCCESS);

    status = bioqueue_create(pool, 16, &ioqueue);
    bassert(status == BASE_SUCCESS);

    status = bdns_resolver_create(mem, NULL, 0, timer_heap, ioqueue, &resolver);
    if (status != BASE_SUCCESS)
	return -40;

    bdns_resolver_get_settings(resolver, &set);
    set.good_ns_ttl = 20;
    set.bad_ns_ttl = 20;
    bdns_resolver_set_settings(resolver, &set);

    status = bdns_resolver_set_ns(resolver, 2, nameservers, ports);
    bassert(status == BASE_SUCCESS);

    status = bthreadCreate(pool, NULL, &poll_worker_thread, NULL, 0, 0, &poll_thread);
    bassert(status == BASE_SUCCESS);

    return 0;
}


static void destroy(void)
{
    int i;

    thread_quit = BASE_TRUE;

    for (i=0; i<2; ++i) {
	bthreadJoin(g_server[i].thread);
	bsock_close(g_server[i].sock);
    }

    bthreadJoin(poll_thread);

    bdns_resolver_destroy(resolver, BASE_FALSE);
    bioqueue_destroy(ioqueue);
    btimer_heap_destroy(timer_heap);

    bsem_destroy(sem);
    bpool_release(pool);
}


////////////////////////////////////////////////////////////////////////////
/* DNS A parser tests */
static int a_parser_test(void)
{
    bdns_parsed_packet pkt;
    bdns_a_record rec;
    bstatus_t rc;

   BASE_INFO("  DNS A record parser tests");

    pkt.q = BASE_POOL_ZALLOC_T(pool, bdns_parsed_query);
    pkt.ans = (bdns_parsed_rr*)
	      bpool_calloc(pool, 32, sizeof(bdns_parsed_rr));

    /* Simple answer with direct A record, but with addition of
     * a CNAME and another A to confuse the parser.
     */
    BASE_INFO("    A RR with duplicate CNAME/A");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 3;

    /* This is the RR corresponding to the query */
    pkt.ans[0].name = bstr("ahost");
    pkt.ans[0].type = BASE_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x01020304;

    /* CNAME to confuse the parser */
    pkt.ans[1].name = bstr("ahost");
    pkt.ans[1].type = BASE_DNS_TYPE_CNAME;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.cname.name = bstr("bhost");

    /* DNS A RR to confuse the parser */
    pkt.ans[2].name = bstr("bhost");
    pkt.ans[2].type = BASE_DNS_TYPE_A;
    pkt.ans[2].dnsclass = 1;
    pkt.ans[2].ttl = 1;
    pkt.ans[2].rdata.a.ip_addr.s_addr = 0x0203;


    rc = bdns_parse_a_response(&pkt, &rec);
    bassert(rc == BASE_SUCCESS);
    bassert(bstrcmp2(&rec.name, "ahost")==0);
    bassert(rec.alias.slen == 0);
    bassert(rec.addr_count == 1);
    bassert(rec.addr[0].s_addr == 0x01020304);

    /* Answer with the target corresponds to a CNAME entry, but not
     * as the first record, and with additions of some CNAME and A
     * entries to confuse the parser.
     */
    BASE_INFO("    CNAME RR with duplicate CNAME/A");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 4;

    /* This is the DNS A record for the alias */
    pkt.ans[0].name = bstr("ahostalias");
    pkt.ans[0].type = BASE_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x02020202;

    /* CNAME entry corresponding to the query */
    pkt.ans[1].name = bstr("ahost");
    pkt.ans[1].type = BASE_DNS_TYPE_CNAME;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.cname.name = bstr("ahostalias");

    /* Another CNAME to confuse the parser */
    pkt.ans[2].name = bstr("ahost");
    pkt.ans[2].type = BASE_DNS_TYPE_CNAME;
    pkt.ans[2].dnsclass = 1;
    pkt.ans[2].ttl = 1;
    pkt.ans[2].rdata.cname.name = bstr("ahostalias2");

    /* Another DNS A to confuse the parser */
    pkt.ans[3].name = bstr("ahostalias2");
    pkt.ans[3].type = BASE_DNS_TYPE_A;
    pkt.ans[3].dnsclass = 1;
    pkt.ans[3].ttl = 1;
    pkt.ans[3].rdata.a.ip_addr.s_addr = 0x03030303;

    rc = bdns_parse_a_response(&pkt, &rec);
    bassert(rc == BASE_SUCCESS);
    bassert(bstrcmp2(&rec.name, "ahost")==0);
    bassert(bstrcmp2(&rec.alias, "ahostalias")==0);
    bassert(rec.addr_count == 1);
    bassert(rec.addr[0].s_addr == 0x02020202);

    /*
     * No query section.
     */
    BASE_INFO("    No query section");
    pkt.hdr.qdcount = 0;
    pkt.hdr.anscount = 0;

    rc = bdns_parse_a_response(&pkt, &rec);
    bassert(rc == UTIL_EDNSINANSWER);

    /*
     * No answer section.
     */
    BASE_INFO("    No answer section");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 0;

    rc = bdns_parse_a_response(&pkt, &rec);
    bassert(rc == UTIL_EDNSNOANSWERREC);

    /*
     * Answer doesn't match query.
     */
    BASE_INFO("    Answer doesn't match query");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 1;

    /* An answer that doesn't match the query */
    pkt.ans[0].name = bstr("ahostalias");
    pkt.ans[0].type = BASE_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x02020202;

    rc = bdns_parse_a_response(&pkt, &rec);
    bassert(rc == UTIL_EDNSNOANSWERREC);


    /*
     * DNS CNAME that doesn't have corresponding DNS A.
     */
    BASE_INFO("    CNAME with no matching DNS A RR (1)");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 1;

    /* The CNAME */
    pkt.ans[0].name = bstr("ahost");
    pkt.ans[0].type = BASE_DNS_TYPE_CNAME;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.cname.name = bstr("ahostalias");

    rc = bdns_parse_a_response(&pkt, &rec);
    bassert(rc == UTIL_EDNSNOANSWERREC);


    /*
     * DNS CNAME that doesn't have corresponding DNS A.
     */
    BASE_INFO("    CNAME with no matching DNS A RR (2)");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 2;

    /* The CNAME */
    pkt.ans[0].name = bstr("ahost");
    pkt.ans[0].type = BASE_DNS_TYPE_CNAME;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.cname.name = bstr("ahostalias");

    /* DNS A record, but the name doesn't match */
    pkt.ans[1].name = bstr("ahost");
    pkt.ans[1].type = BASE_DNS_TYPE_A;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.a.ip_addr.s_addr = 0x01020304;

    rc = bdns_parse_a_response(&pkt, &rec);
    bassert(rc == UTIL_EDNSNOANSWERREC);
    BASE_UNUSED_ARG(rc);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* DNS A/AAAA parser tests */
static int addr_parser_test(void)
{
    bdns_parsed_packet pkt;
    bdns_addr_record rec;
    bstatus_t rc;

    BASE_INFO("  DNS A/AAAA record parser tests");

    pkt.q = BASE_POOL_ZALLOC_T(pool, bdns_parsed_query);
    pkt.ans = (bdns_parsed_rr*)
	      bpool_calloc(pool, 32, sizeof(bdns_parsed_rr));

    /* Simple answer with direct A record, but with addition of
     * a CNAME and another A to confuse the parser.
     */
    BASE_INFO("    A RR with duplicate CNAME/A");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 4;

    /* This is the RR corresponding to the query */
    pkt.ans[0].name = bstr("ahost");
    pkt.ans[0].type = BASE_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x01020304;

    /* CNAME to confuse the parser */
    pkt.ans[1].name = bstr("ahost");
    pkt.ans[1].type = BASE_DNS_TYPE_CNAME;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.cname.name = bstr("bhost");

    /* DNS A RR to confuse the parser */
    pkt.ans[2].name = bstr("bhost");
    pkt.ans[2].type = BASE_DNS_TYPE_A;
    pkt.ans[2].dnsclass = 1;
    pkt.ans[2].ttl = 1;
    pkt.ans[2].rdata.a.ip_addr.s_addr = 0x0203;

    /* Additional RR corresponding to the query, DNS AAAA RR */
    pkt.ans[3].name = bstr("ahost");
    pkt.ans[3].type = BASE_DNS_TYPE_AAAA;
    pkt.ans[3].dnsclass = 1;
    pkt.ans[3].ttl = 1;
    pkt.ans[3].rdata.aaaa.ip_addr.u6_addr32[0] = 0x01020304;


    rc = bdns_parse_addr_response(&pkt, &rec);
    bassert(rc == BASE_SUCCESS);
    bassert(bstrcmp2(&rec.name, "ahost")==0);
    bassert(rec.alias.slen == 0);
    bassert(rec.addr_count == 2);
    bassert(rec.addr[0].af==bAF_INET() && rec.addr[0].ip.v4.s_addr == 0x01020304);
    bassert(rec.addr[1].af==bAF_INET6() && rec.addr[1].ip.v6.u6_addr32[0] == 0x01020304);

    /* Answer with the target corresponds to a CNAME entry, but not
     * as the first record, and with additions of some CNAME and A
     * entries to confuse the parser.
     */
    BASE_INFO("    CNAME RR with duplicate CNAME/A");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 4;

    /* This is the DNS A record for the alias */
    pkt.ans[0].name = bstr("ahostalias");
    pkt.ans[0].type = BASE_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x02020202;

    /* CNAME entry corresponding to the query */
    pkt.ans[1].name = bstr("ahost");
    pkt.ans[1].type = BASE_DNS_TYPE_CNAME;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.cname.name = bstr("ahostalias");

    /* Another CNAME to confuse the parser */
    pkt.ans[2].name = bstr("ahost");
    pkt.ans[2].type = BASE_DNS_TYPE_CNAME;
    pkt.ans[2].dnsclass = 1;
    pkt.ans[2].ttl = 1;
    pkt.ans[2].rdata.cname.name = bstr("ahostalias2");

    /* Another DNS A to confuse the parser */
    pkt.ans[3].name = bstr("ahostalias2");
    pkt.ans[3].type = BASE_DNS_TYPE_A;
    pkt.ans[3].dnsclass = 1;
    pkt.ans[3].ttl = 1;
    pkt.ans[3].rdata.a.ip_addr.s_addr = 0x03030303;

    rc = bdns_parse_addr_response(&pkt, &rec);
    bassert(rc == BASE_SUCCESS);
    bassert(bstrcmp2(&rec.name, "ahost")==0);
    bassert(bstrcmp2(&rec.alias, "ahostalias")==0);
    bassert(rec.addr_count == 1);
    bassert(rec.addr[0].ip.v4.s_addr == 0x02020202);

    /*
     * No query section.
     */
    BASE_INFO("    No query section");
    pkt.hdr.qdcount = 0;
    pkt.hdr.anscount = 0;

    rc = bdns_parse_addr_response(&pkt, &rec);
    bassert(rc == UTIL_EDNSINANSWER);

    /*
     * No answer section.
     */
    BASE_INFO("    No answer section");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 0;

    rc = bdns_parse_addr_response(&pkt, &rec);
    bassert(rc == UTIL_EDNSNOANSWERREC);

    /*
     * Answer doesn't match query.
     */
    BASE_INFO("    Answer doesn't match query");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 1;

    /* An answer that doesn't match the query */
    pkt.ans[0].name = bstr("ahostalias");
    pkt.ans[0].type = BASE_DNS_TYPE_A;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.a.ip_addr.s_addr = 0x02020202;

    rc = bdns_parse_addr_response(&pkt, &rec);
    bassert(rc == UTIL_EDNSNOANSWERREC);


    /*
     * DNS CNAME that doesn't have corresponding DNS A.
     */
    BASE_INFO("    CNAME with no matching DNS A RR (1)");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 1;

    /* The CNAME */
    pkt.ans[0].name = bstr("ahost");
    pkt.ans[0].type = BASE_DNS_TYPE_CNAME;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.cname.name = bstr("ahostalias");

    rc = bdns_parse_addr_response(&pkt, &rec);
    bassert(rc == UTIL_EDNSNOANSWERREC);


    /*
     * DNS CNAME that doesn't have corresponding DNS A.
     */
    BASE_INFO("    CNAME with no matching DNS A RR (2)");
    pkt.hdr.flags = 0;
    pkt.hdr.qdcount = 1;
    pkt.q[0].type = BASE_DNS_TYPE_A;
    pkt.q[0].dnsclass = 1;
    pkt.q[0].name = bstr("ahost");
    pkt.hdr.anscount = 2;

    /* The CNAME */
    pkt.ans[0].name = bstr("ahost");
    pkt.ans[0].type = BASE_DNS_TYPE_CNAME;
    pkt.ans[0].dnsclass = 1;
    pkt.ans[0].ttl = 1;
    pkt.ans[0].rdata.cname.name = bstr("ahostalias");

    /* DNS A record, but the name doesn't match */
    pkt.ans[1].name = bstr("ahost");
    pkt.ans[1].type = BASE_DNS_TYPE_A;
    pkt.ans[1].dnsclass = 1;
    pkt.ans[1].ttl = 1;
    pkt.ans[1].rdata.a.ip_addr.s_addr = 0x01020304;

    rc = bdns_parse_addr_response(&pkt, &rec);
    bassert(rc == UTIL_EDNSNOANSWERREC);
    BASE_UNUSED_ARG(rc);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Simple DNS test */
#define IP_ADDR0    0x00010203

static void dns_callback(void *user_data,
			 bstatus_t status,
			 bdns_parsed_packet *resp)
{
    BASE_UNUSED_ARG(user_data);

    bsem_post(sem);

    BASE_ASSERT_ON_FAIL(status == BASE_SUCCESS, return);
    BASE_ASSERT_ON_FAIL(resp, return);
    BASE_ASSERT_ON_FAIL(resp->hdr.anscount == 1, return);
    BASE_ASSERT_ON_FAIL(resp->ans[0].type == BASE_DNS_TYPE_A, return);
    BASE_ASSERT_ON_FAIL(resp->ans[0].rdata.a.ip_addr.s_addr == IP_ADDR0, return);

}


static int simple_test(void)
{
    bstr_t name = bstr("helloworld");
    bdns_parsed_packet *r;
    bstatus_t status;

    BASE_INFO("  simple successful test");

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    g_server[0].action = ACTION_REPLY;
    r = &g_server[0].resp;
    r->hdr.qdcount = 1;
    r->hdr.anscount = 1;
    r->q = BASE_POOL_ZALLOC_T(pool, bdns_parsed_query);
    r->q[0].type = BASE_DNS_TYPE_A;
    r->q[0].dnsclass = 1;
    r->q[0].name = name;
    r->ans = BASE_POOL_ZALLOC_T(pool, bdns_parsed_rr);
    r->ans[0].type = BASE_DNS_TYPE_A;
    r->ans[0].dnsclass = 1;
    r->ans[0].name = name;
    r->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR0;

    g_server[1].action = ACTION_REPLY;
    r = &g_server[1].resp;
    r->hdr.qdcount = 1;
    r->hdr.anscount = 1;
    r->q = BASE_POOL_ZALLOC_T(pool, bdns_parsed_query);
    r->q[0].type = BASE_DNS_TYPE_A;
    r->q[0].dnsclass = 1;
    r->q[0].name = name;
    r->ans = BASE_POOL_ZALLOC_T(pool, bdns_parsed_rr);
    r->ans[0].type = BASE_DNS_TYPE_A;
    r->ans[0].dnsclass = 1;
    r->ans[0].name = name;
    r->ans[0].rdata.a.ip_addr.s_addr = IP_ADDR0;

    status = bdns_resolver_start_query(resolver, &name, BASE_DNS_TYPE_A, 0,
					 &dns_callback, NULL, NULL);
    if (status != BASE_SUCCESS)
	return -1000;

    bsem_wait(sem);
    bthreadSleepMs(1000);


    /* Both servers must get packet */
    bassert(g_server[0].pkt_count == 1);
    bassert(g_server[1].pkt_count == 1);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* DNS nameserver fail-over test */

static void dns_callback_1b(void *user_data,
			    bstatus_t status,
			    bdns_parsed_packet *resp)
{
    BASE_UNUSED_ARG(user_data);
    BASE_UNUSED_ARG(resp);

    bsem_post(sem);

    BASE_ASSERT_ON_FAIL(status==BASE_STATUS_FROM_DNS_RCODE(BASE_DNS_RCODE_NXDOMAIN),
		      return);
}




/* DNS test */
static int dns_test(void)
{
    bstr_t name = bstr("name00");
    bstatus_t status;

    BASE_INFO("  simple error response test");

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    g_server[0].action = BASE_DNS_RCODE_NXDOMAIN;
    g_server[1].action = BASE_DNS_RCODE_NXDOMAIN;

    status = bdns_resolver_start_query(resolver, &name, BASE_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != BASE_SUCCESS)
	return -1000;

    bsem_wait(sem);
    bthreadSleepMs(1000);

    /* Now only server 0 should get packet, since both servers are
     * in STATE_ACTIVE state
     */
    bassert((g_server[0].pkt_count == 1 && g_server[1].pkt_count == 0) ||
	      (g_server[1].pkt_count == 1 && g_server[0].pkt_count == 0));

    /* Wait to allow probing period to complete */
    BASE_INFO( "  waiting for active NS to expire (%d sec)", set.good_ns_ttl);
    bthreadSleepMs(set.good_ns_ttl * 1000);

    /* 
     * Fail-over test 
     */
    BASE_INFO("  failing server0");
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = BASE_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = bstr("name01");
    status = bdns_resolver_start_query(resolver, &name, BASE_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != BASE_SUCCESS)
	return -1000;

    bsem_wait(sem);

    /*
     * Check that both servers still receive requests, since they are
     * in probing state.
     */
    BASE_INFO("  checking both NS during probing period");
    g_server[0].action = ACTION_IGNORE;
    g_server[1].action = BASE_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = bstr("name02");
    status = bdns_resolver_start_query(resolver, &name, BASE_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != BASE_SUCCESS)
	return -1000;

    bsem_wait(sem);
    bthreadSleepMs(set.qretr_delay *  set.qretr_count);

    /* Both servers must get requests */
    bassert(g_server[0].pkt_count >= 1);
    bassert(g_server[1].pkt_count == 1);

    /* Wait to allow probing period to complete */
    BASE_INFO("  waiting for probing state to end (%d sec)", set.qretr_delay*(set.qretr_count+2) / 1000);
    bthreadSleepMs(set.qretr_delay * (set.qretr_count + 2));


    /*
     * Now only server 1 should get requests.
     */
    BASE_INFO("  verifying only good NS is used");
    g_server[0].action = BASE_DNS_RCODE_NXDOMAIN;
    g_server[1].action = BASE_DNS_RCODE_NXDOMAIN;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = bstr("name03");
    status = bdns_resolver_start_query(resolver, &name, BASE_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != BASE_SUCCESS)
	return -1000;

    bsem_wait(sem);
    bthreadSleepMs(1000);

    /* Both servers must get requests */
    bassert(g_server[0].pkt_count == 0);
    bassert(g_server[1].pkt_count == 1);

    /* Wait to allow probing period to complete */
    BASE_INFO("  waiting for active NS to expire (%d sec)", set.good_ns_ttl);
    bthreadSleepMs(set.good_ns_ttl * 1000);

    /*
     * Now fail server 1 to switch to server 0
     */
    g_server[0].action = BASE_DNS_RCODE_NXDOMAIN;
    g_server[1].action = ACTION_IGNORE;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = bstr("name04");
    status = bdns_resolver_start_query(resolver, &name, BASE_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != BASE_SUCCESS)
	return -1000;

    bsem_wait(sem);

    /* Wait to allow probing period to complete */
    BASE_INFO("  waiting for probing state (%d sec)", set.qretr_delay * (set.qretr_count+2) / 1000);
    bthreadSleepMs(set.qretr_delay * (set.qretr_count + 2));

    /*
     * Now only server 0 should get requests.
     */
    BASE_INFO("  verifying good NS");
    g_server[0].action = BASE_DNS_RCODE_NXDOMAIN;
    g_server[1].action = ACTION_IGNORE;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    name = bstr("name05");
    status = bdns_resolver_start_query(resolver, &name, BASE_DNS_TYPE_A, 0,
					 &dns_callback_1b, NULL, NULL);
    if (status != BASE_SUCCESS)
	return -1000;

    bsem_wait(sem);
    bthreadSleepMs(1000);

    /* Only good NS should get request */
    bassert(g_server[0].pkt_count == 1);
    bassert(g_server[1].pkt_count == 0);


    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Resolver test, normal, with CNAME */
#define IP_ADDR1    0x02030405
#define PORT1	    50061

static void action1_1(const bdns_parsed_packet *pkt,
		      bdns_parsed_packet **p_res)
{
    bdns_parsed_packet *res;
    char *target = "sip.somedomain.com";

    res = BASE_POOL_ZALLOC_T(pool, bdns_parsed_packet);

    if (res->q == NULL) {
	res->q = BASE_POOL_ZALLOC_T(pool, bdns_parsed_query);
    }
    if (res->ans == NULL) {
	res->ans = (bdns_parsed_rr*) 
		  bpool_calloc(pool, 4, sizeof(bdns_parsed_rr));
    }

    res->hdr.qdcount = 1;
    res->q[0].type = pkt->q[0].type;
    res->q[0].dnsclass = pkt->q[0].dnsclass;
    res->q[0].name = pkt->q[0].name;

    if (pkt->q[0].type == BASE_DNS_TYPE_SRV) {

	bassert(bstrcmp2(&pkt->q[0].name, "_sip._udp.somedomain.com")==0);

	res->hdr.anscount = 1;
	res->ans[0].type = BASE_DNS_TYPE_SRV;
	res->ans[0].dnsclass = 1;
	res->ans[0].name = res->q[0].name;
	res->ans[0].ttl = 1;
	res->ans[0].rdata.srv.prio = 1;
	res->ans[0].rdata.srv.weight = 2;
	res->ans[0].rdata.srv.port = PORT1;
	res->ans[0].rdata.srv.target = bstr(target);

    } else if (pkt->q[0].type == BASE_DNS_TYPE_A) {
	char *alias = "sipalias.somedomain.com";

	bassert(bstrcmp2(&res->q[0].name, target)==0);

	res->hdr.anscount = 2;
	res->ans[0].type = BASE_DNS_TYPE_CNAME;
	res->ans[0].dnsclass = 1;
	res->ans[0].ttl = 1000;	/* resolver should select minimum TTL */
	res->ans[0].name = res->q[0].name;
	res->ans[0].rdata.cname.name = bstr(alias);

	res->ans[1].type = BASE_DNS_TYPE_A;
	res->ans[1].dnsclass = 1;
	res->ans[1].ttl = 1;
	res->ans[1].name = bstr(alias);
	res->ans[1].rdata.a.ip_addr.s_addr = IP_ADDR1;

    } else if (pkt->q[0].type == BASE_DNS_TYPE_AAAA) {
	char *alias = "sipalias.somedomain.com";

	bassert(bstrcmp2(&res->q[0].name, target)==0);

	res->hdr.anscount = 2;
	res->ans[0].type = BASE_DNS_TYPE_CNAME;
	res->ans[0].dnsclass = 1;
	res->ans[0].ttl = 1000;	/* resolver should select minimum TTL */
	res->ans[0].name = res->q[0].name;
	res->ans[0].rdata.cname.name = bstr(alias);

	res->ans[1].type = BASE_DNS_TYPE_AAAA;
	res->ans[1].dnsclass = 1;
	res->ans[1].ttl = 1;
	res->ans[1].name = bstr(alias);
	res->ans[1].rdata.aaaa.ip_addr.u6_addr32[0] = IP_ADDR1;
	res->ans[1].rdata.aaaa.ip_addr.u6_addr32[1] = IP_ADDR1;
	res->ans[1].rdata.aaaa.ip_addr.u6_addr32[2] = IP_ADDR1;
	res->ans[1].rdata.aaaa.ip_addr.u6_addr32[3] = IP_ADDR1;
    }

    *p_res = res;
}

static void srv_cb_1(void *user_data,
		     bstatus_t status,
		     const bdns_srv_record *rec)
{
    BASE_UNUSED_ARG(user_data);

    bsem_post(sem);

    BASE_ASSERT_ON_FAIL(status == BASE_SUCCESS, return);
    BASE_ASSERT_ON_FAIL(rec->count == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].priority == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].weight == 2, return);
    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.name, "sip.somedomain.com")==0,
		      return);
    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.alias, "sipalias.somedomain.com")==0,
		      return);

    /* IPv4 only */
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr_count == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr[0].ip.v4.s_addr == IP_ADDR1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].port == PORT1, return);

    
}


static void srv_cb_1b(void *user_data,
		      bstatus_t status,
		      const bdns_srv_record *rec)
{
    BASE_UNUSED_ARG(user_data);

    bsem_post(sem);

    BASE_ASSERT_ON_FAIL(status==BASE_STATUS_FROM_DNS_RCODE(BASE_DNS_RCODE_NXDOMAIN),
		      return);
    BASE_ASSERT_ON_FAIL(rec->count == 0, return);
}


static void srv_cb_1c(void *user_data,
		      bstatus_t status,
		      const bdns_srv_record *rec)
{
    BASE_UNUSED_ARG(user_data);

    bsem_post(sem);

    BASE_ASSERT_ON_FAIL(status == BASE_SUCCESS, return);
    BASE_ASSERT_ON_FAIL(rec->count == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].priority == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].weight == 2, return);

    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.name, "sip.somedomain.com")==0,
		      return);
    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.alias, "sipalias.somedomain.com")==0,
		      return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].port == PORT1, return);

    /* IPv4 and IPv6 */
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr_count == 2, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr[0].af == bAF_INET() &&
		      rec->entry[0].server.addr[0].ip.v4.s_addr == IP_ADDR1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr[1].af == bAF_INET6() &&
		      rec->entry[0].server.addr[1].ip.v6.u6_addr32[0] == IP_ADDR1, return);
}


static void srv_cb_1d(void *user_data,
		      bstatus_t status,
		      const bdns_srv_record *rec)
{
    BASE_UNUSED_ARG(user_data);

    bsem_post(sem);

    BASE_ASSERT_ON_FAIL(status == BASE_SUCCESS, return);
    BASE_ASSERT_ON_FAIL(rec->count == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].priority == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].weight == 2, return);

    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.name, "sip.somedomain.com")==0,
		      return);
    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.alias, "sipalias.somedomain.com")==0,
		      return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].port == PORT1, return);

    /* IPv6 only */
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr_count == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr[0].af == bAF_INET6() &&
		      rec->entry[0].server.addr[0].ip.v6.u6_addr32[0] == IP_ADDR1, return);
}


static int srv_resolver_test(void)
{
    bstatus_t status;
    bstr_t domain = bstr("somedomain.com");
    bstr_t res_name = bstr("_sip._udp.");

    /* Successful scenario */
    BASE_INFO( "  srv_resolve(): success scenario");

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action1_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action1_1;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = bdns_srv_resolve(&domain, &res_name, 5061, pool, resolver, BASE_TRUE,
				NULL, &srv_cb_1, NULL);
    bassert(status == BASE_SUCCESS);

    bsem_wait(sem);

    /* Because of previous tests, only NS 1 should get the request */
    bassert(g_server[0].pkt_count == 2);  /* 2 because of SRV and A resolution */
    bassert(g_server[1].pkt_count == 0);


    /* Wait until cache expires and nameserver state moves out from STATE_PROBING */
    BASE_INFO( "  waiting for cache to expire (~15 secs)..");
    bthreadSleepMs(1000 + 
		    ((set.qretr_count + 2) * set.qretr_delay));


    /* DNS SRV option BASE_DNS_SRV_RESOLVE_AAAA */
    BASE_INFO("  srv_resolve(): option BASE_DNS_SRV_RESOLVE_AAAA");

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action1_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action1_1;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = bdns_srv_resolve(&domain, &res_name, 5061, pool, resolver,
				BASE_DNS_SRV_RESOLVE_AAAA,
				NULL, &srv_cb_1c, NULL);
    bassert(status == BASE_SUCCESS);

    bsem_wait(sem);

    bthreadSleepMs(1000);

    /* DNS SRV option BASE_DNS_SRV_RESOLVE_AAAA_ONLY */
    BASE_INFO("  srv_resolve(): option BASE_DNS_SRV_RESOLVE_AAAA_ONLY");

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action1_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action1_1;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = bdns_srv_resolve(&domain, &res_name, 5061, pool, resolver,
				BASE_DNS_SRV_RESOLVE_AAAA_ONLY,
				NULL, &srv_cb_1d, NULL);
    bassert(status == BASE_SUCCESS);

    bsem_wait(sem);

    bthreadSleepMs(1000);


    /* Successful scenario */
    BASE_INFO("  srv_resolve(): parallel queries");
    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = bdns_srv_resolve(&domain, &res_name, 5061, pool, resolver, BASE_TRUE,
				NULL, &srv_cb_1, NULL);
    bassert(status == BASE_SUCCESS);


    status = bdns_srv_resolve(&domain, &res_name, 5061, pool, resolver, BASE_TRUE,
				NULL, &srv_cb_1, NULL);
    bassert(status == BASE_SUCCESS);

    bsem_wait(sem);
    bsem_wait(sem);

    /* Only server one should get a query */
    bassert(g_server[0].pkt_count == 2);  /* 2 because of SRV and A resolution */
    bassert(g_server[1].pkt_count == 0);

    /* Since TTL is one, subsequent queries should fail */
    BASE_INFO("  srv_resolve(): cache expires scenario");

    bthreadSleepMs(1000);

    g_server[0].action = BASE_DNS_RCODE_NXDOMAIN;
    g_server[1].action = BASE_DNS_RCODE_NXDOMAIN;

    status = bdns_srv_resolve(&domain, &res_name, 5061, pool, resolver, BASE_TRUE,
				NULL, &srv_cb_1b, NULL);
    bassert(status == BASE_SUCCESS);

    bsem_wait(sem);


    return status;
}


////////////////////////////////////////////////////////////////////////////
/* Fallback because there's no SRV in answer */
#define TARGET	    "domain2.com"
#define IP_ADDR2    0x02030405
#define PORT2	    50062

static void action2_1(const bdns_parsed_packet *pkt,
		      bdns_parsed_packet **p_res)
{
    bdns_parsed_packet *res;

    res = BASE_POOL_ZALLOC_T(pool, bdns_parsed_packet);

    res->q = BASE_POOL_ZALLOC_T(pool, bdns_parsed_query);
    res->ans = (bdns_parsed_rr*) 
	       bpool_calloc(pool, 4, sizeof(bdns_parsed_rr));

    res->hdr.qdcount = 1;
    res->q[0].type = pkt->q[0].type;
    res->q[0].dnsclass = pkt->q[0].dnsclass;
    res->q[0].name = pkt->q[0].name;

    if (pkt->q[0].type == BASE_DNS_TYPE_SRV) {

	bassert(bstrcmp2(&pkt->q[0].name, "_sip._udp." TARGET)==0);

	res->hdr.anscount = 1;
	res->ans[0].type = BASE_DNS_TYPE_A;    // <-- this will cause the fallback
	res->ans[0].dnsclass = 1;
	res->ans[0].name = res->q[0].name;
	res->ans[0].ttl = 1;
	res->ans[0].rdata.srv.prio = 1;
	res->ans[0].rdata.srv.weight = 2;
	res->ans[0].rdata.srv.port = PORT2;
	res->ans[0].rdata.srv.target = bstr("sip01." TARGET);

    } else if (pkt->q[0].type == BASE_DNS_TYPE_A) {
	char *alias = "sipalias01." TARGET;

	bassert(bstrcmp2(&res->q[0].name, TARGET)==0);

	res->hdr.anscount = 2;
	res->ans[0].type = BASE_DNS_TYPE_CNAME;
	res->ans[0].dnsclass = 1;
	res->ans[0].name = res->q[0].name;
	res->ans[0].ttl = 1;
	res->ans[0].rdata.cname.name = bstr(alias);

	res->ans[1].type = BASE_DNS_TYPE_A;
	res->ans[1].dnsclass = 1;
	res->ans[1].name = bstr(alias);
	res->ans[1].ttl = 1;
	res->ans[1].rdata.a.ip_addr.s_addr = IP_ADDR2;

    } else if (pkt->q[0].type == BASE_DNS_TYPE_AAAA) {
	char *alias = "sipalias01." TARGET;

	bassert(bstrcmp2(&res->q[0].name, TARGET)==0);

	res->hdr.anscount = 2;
	res->ans[0].type = BASE_DNS_TYPE_CNAME;
	res->ans[0].dnsclass = 1;
	res->ans[0].name = res->q[0].name;
	res->ans[0].ttl = 1;
	res->ans[0].rdata.cname.name = bstr(alias);

	res->ans[1].type = BASE_DNS_TYPE_AAAA;
	res->ans[1].dnsclass = 1;
	res->ans[1].ttl = 1;
	res->ans[1].name = bstr(alias);
	res->ans[1].rdata.aaaa.ip_addr.u6_addr32[0] = IP_ADDR2;
	res->ans[1].rdata.aaaa.ip_addr.u6_addr32[1] = IP_ADDR2;
	res->ans[1].rdata.aaaa.ip_addr.u6_addr32[2] = IP_ADDR2;
	res->ans[1].rdata.aaaa.ip_addr.u6_addr32[3] = IP_ADDR2;
    }

    *p_res = res;
}

static void srv_cb_2(void *user_data,
		     bstatus_t status,
		     const bdns_srv_record *rec)
{
    BASE_UNUSED_ARG(user_data);

    bsem_post(sem);

    BASE_ASSERT_ON_FAIL(status == BASE_SUCCESS, return);
    BASE_ASSERT_ON_FAIL(rec->count == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].priority == 0, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].weight == 0, return);
    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.name, TARGET)==0,
		      return);
    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.alias, "sipalias01." TARGET)==0,
		      return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].port == PORT2, return);

    /* IPv4 only */
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr_count == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr[0].af == bAF_INET() &&
		      rec->entry[0].server.addr[0].ip.v4.s_addr == IP_ADDR2, return);
}

static void srv_cb_2a(void *user_data,
		      bstatus_t status,
		      const bdns_srv_record *rec)
{
    BASE_UNUSED_ARG(user_data);

    bsem_post(sem);

    BASE_ASSERT_ON_FAIL(status == BASE_SUCCESS, return);
    BASE_ASSERT_ON_FAIL(rec->count == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].priority == 0, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].weight == 0, return);
    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.name, TARGET)==0,
		      return);
    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.alias, "sipalias01." TARGET)==0,
		      return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].port == PORT2, return);

    /* IPv4 and IPv6 */
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr_count == 2, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr[0].af == bAF_INET() &&
		      rec->entry[0].server.addr[0].ip.v4.s_addr == IP_ADDR2, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr[1].af == bAF_INET6() &&
		      rec->entry[0].server.addr[1].ip.v6.u6_addr32[0] == IP_ADDR2, return);
}

static void srv_cb_2b(void *user_data,
		      bstatus_t status,
		      const bdns_srv_record *rec)
{
    BASE_UNUSED_ARG(user_data);

    bsem_post(sem);

    BASE_ASSERT_ON_FAIL(status == BASE_SUCCESS, return);
    BASE_ASSERT_ON_FAIL(rec->count == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].priority == 0, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].weight == 0, return);
    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.name, TARGET)==0,
		      return);
    BASE_ASSERT_ON_FAIL(bstrcmp2(&rec->entry[0].server.alias, "sipalias01." TARGET)==0,
		      return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].port == PORT2, return);

    /* IPv6 only */
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr_count == 1, return);
    BASE_ASSERT_ON_FAIL(rec->entry[0].server.addr[0].af == bAF_INET6() &&
		      rec->entry[0].server.addr[0].ip.v6.u6_addr32[0] == IP_ADDR2, return);
}

static int srv_resolver_fallback_test(void)
{
    bstatus_t status;
    bstr_t domain = bstr(TARGET);
    bstr_t res_name = bstr("_sip._udp.");

    /* Fallback test */
    BASE_INFO("  srv_resolve(): fallback test");

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action2_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action2_1;

    status = bdns_srv_resolve(&domain, &res_name, PORT2, pool, resolver, BASE_TRUE,
				NULL, &srv_cb_2, NULL);
    if (status != BASE_SUCCESS) {
	app_perror("   srv_resolve error", status);
	bassert(status == BASE_SUCCESS);
    }

    bsem_wait(sem);

    /* Subsequent query should just get the response from the cache */
    BASE_INFO("  srv_resolve(): cache test");
    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = bdns_srv_resolve(&domain, &res_name, PORT2, pool, resolver, BASE_TRUE,
				NULL, &srv_cb_2, NULL);
    if (status != BASE_SUCCESS) {
	app_perror("   srv_resolve error", status);
	bassert(status == BASE_SUCCESS);
    }

    bsem_wait(sem);

    bassert(g_server[0].pkt_count == 0);
    bassert(g_server[1].pkt_count == 0);

    /* Clear cache */
    bthreadSleepMs(1000);

    /* Fallback with BASE_DNS_SRV_FALLBACK_A and BASE_DNS_SRV_FALLBACK_AAAA */
    BASE_INFO("  srv_resolve(): fallback to DNS A and AAAA");

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action2_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action2_1;

    status = bdns_srv_resolve(&domain, &res_name, PORT2, pool, resolver,
				BASE_DNS_SRV_FALLBACK_A | BASE_DNS_SRV_FALLBACK_AAAA,
				NULL, &srv_cb_2a, NULL);
    if (status != BASE_SUCCESS) {
	app_perror("   srv_resolve error", status);
	bassert(status == BASE_SUCCESS);
    }

    bsem_wait(sem);

    /* Clear cache */
    bthreadSleepMs(1000);

    /* Fallback with BASE_DNS_SRV_FALLBACK_AAAA only */
    BASE_INFO("  srv_resolve(): fallback to DNS AAAA only");

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action2_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action2_1;

    status = bdns_srv_resolve(&domain, &res_name, PORT2, pool, resolver,
				BASE_DNS_SRV_FALLBACK_AAAA,
				NULL, &srv_cb_2b, NULL);
    if (status != BASE_SUCCESS) {
	app_perror("   srv_resolve error", status);
	bassert(status == BASE_SUCCESS);
    }

    bsem_wait(sem);

    /* Clear cache */
    bthreadSleepMs(1000);

    return 0;
}


////////////////////////////////////////////////////////////////////////////
/* Too many SRV or A entries */
#define DOMAIN3	    "d3"
#define SRV_COUNT3  (BASE_DNS_SRV_MAX_ADDR+1)
#define A_COUNT3    (BASE_DNS_MAX_IP_IN_A_REC+1)
#define PORT3	    50063
#define IP_ADDR3    0x03030303

static void action3_1(const bdns_parsed_packet *pkt,
		      bdns_parsed_packet **p_res)
{
    bdns_parsed_packet *res;
    unsigned i;

    res = BASE_POOL_ZALLOC_T(pool, bdns_parsed_packet);

    if (res->q == NULL) {
	res->q = BASE_POOL_ZALLOC_T(pool, bdns_parsed_query);
    }

    res->hdr.qdcount = 1;
    res->q[0].type = pkt->q[0].type;
    res->q[0].dnsclass = pkt->q[0].dnsclass;
    res->q[0].name = pkt->q[0].name;

    if (pkt->q[0].type == BASE_DNS_TYPE_SRV) {

	bassert(bstrcmp2(&pkt->q[0].name, "_sip._udp." DOMAIN3)==0);

	res->hdr.anscount = SRV_COUNT3;
	res->ans = (bdns_parsed_rr*) 
		   bpool_calloc(pool, SRV_COUNT3, sizeof(bdns_parsed_rr));

	for (i=0; i<SRV_COUNT3; ++i) {
	    char *target;

	    res->ans[i].type = BASE_DNS_TYPE_SRV;
	    res->ans[i].dnsclass = 1;
	    res->ans[i].name = res->q[0].name;
	    res->ans[i].ttl = 1;
	    res->ans[i].rdata.srv.prio = (buint16_t)i;
	    res->ans[i].rdata.srv.weight = 2;
	    res->ans[i].rdata.srv.port = (buint16_t)(PORT3+i);

	    target = (char*)bpool_alloc(pool, 16);
	    sprintf(target, "sip%02d." DOMAIN3, i);
	    res->ans[i].rdata.srv.target = bstr(target);
	}

    } else if (pkt->q[0].type == BASE_DNS_TYPE_A) {

	//bassert(bstrcmp2(&res->q[0].name, "sip." DOMAIN3)==0);

	res->hdr.anscount = A_COUNT3;
	res->ans = (bdns_parsed_rr*) 
		   bpool_calloc(pool, A_COUNT3, sizeof(bdns_parsed_rr));

	for (i=0; i<A_COUNT3; ++i) {
	    res->ans[i].type = BASE_DNS_TYPE_A;
	    res->ans[i].dnsclass = 1;
	    res->ans[i].ttl = 1;
	    res->ans[i].name = res->q[0].name;
	    res->ans[i].rdata.a.ip_addr.s_addr = IP_ADDR3+i;
	}
    }

    *p_res = res;
}

static void srv_cb_3(void *user_data,
		     bstatus_t status,
		     const bdns_srv_record *rec)
{
    unsigned i;

    BASE_UNUSED_ARG(user_data);
    BASE_UNUSED_ARG(status);
    BASE_UNUSED_ARG(rec);

    bassert(status == BASE_SUCCESS);
    bassert(rec->count == BASE_DNS_SRV_MAX_ADDR);

    for (i=0; i<BASE_DNS_SRV_MAX_ADDR; ++i) {
	unsigned j;

	bassert(rec->entry[i].priority == i);
	bassert(rec->entry[i].weight == 2);
	//bassert(bstrcmp2(&rec->entry[i].server.name, "sip." DOMAIN3)==0);
	bassert(rec->entry[i].server.alias.slen == 0);
	bassert(rec->entry[i].port == PORT3+i);

	bassert(rec->entry[i].server.addr_count == BASE_DNS_MAX_IP_IN_A_REC);

	for (j=0; j<BASE_DNS_MAX_IP_IN_A_REC; ++j) {
	    bassert(rec->entry[i].server.addr[j].ip.v4.s_addr == IP_ADDR3+j);
	}
    }

    bsem_post(sem);
}

static int srv_resolver_many_test(void)
{
    bstatus_t status;
    bstr_t domain = bstr(DOMAIN3);
    bstr_t res_name = bstr("_sip._udp.");

    /* Successful scenario */
    BASE_INFO("  srv_resolve(): too many entries test");

    g_server[0].action = ACTION_CB;
    g_server[0].action_cb = &action3_1;
    g_server[1].action = ACTION_CB;
    g_server[1].action_cb = &action3_1;

    g_server[0].pkt_count = 0;
    g_server[1].pkt_count = 0;

    status = bdns_srv_resolve(&domain, &res_name, 1, pool, resolver, BASE_TRUE,
				NULL, &srv_cb_3, NULL);
    bassert(status == BASE_SUCCESS);

    bsem_wait(sem);

    return status;
}


////////////////////////////////////////////////////////////////////////////


int resolver_test(void)
{
    int rc;
    
    rc = init(BASE_FALSE);
    if (rc != 0)
	goto on_error;

    rc = a_parser_test();
    if (rc != 0)
	goto on_error;

    rc = addr_parser_test();
    if (rc != 0)
	goto on_error;

    rc = simple_test();
    if (rc != 0)
	goto on_error;

    rc = dns_test();
    if (rc != 0)
	goto on_error;

    srv_resolver_test();
    srv_resolver_fallback_test();
    srv_resolver_many_test();

    destroy();


#if BASE_HAS_IPV6
    /* Similar tests using IPv6 socket and without parser tests */
    BASE_INFO("Re-run DNS resolution tests using IPv6 socket");

    rc = init(BASE_TRUE);
    if (rc != 0)
	goto on_error;

    rc = simple_test();
    if (rc != 0)
	goto on_error;

    rc = dns_test();
    if (rc != 0)
	goto on_error;

    srv_resolver_test();
    srv_resolver_fallback_test();
    srv_resolver_many_test();

    destroy();
#endif

    return 0;

on_error:
    destroy();
    return rc;
}

