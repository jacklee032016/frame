/* 
 *
 */
#include <utilDns.h>
#include <baseAssert.h>
#include <baseLog.h>
#include <baseString.h>

#define LEVEL	    3

static const char *spell_ttl(char *buf, int size, unsigned ttl)
{
#define DAY	(3600*24)
#define HOUR	(3600)
#define MINUTE	(60)

    char *p = buf;
    int len;

    if (ttl > DAY) {
	len = bansi_snprintf(p, size, "%dd ", ttl/DAY);
	if (len < 1 || len >= size)
	    return "-err-";
	size -= len;
	p += len;
	ttl %= DAY;
    }

    if (ttl > HOUR) {
	len = bansi_snprintf(p, size, "%dh ", ttl/HOUR);
	if (len < 1 || len >= size)
	    return "-err-";
	size -= len;
	p += len;
	ttl %= HOUR;
    }

    if (ttl > MINUTE) {
	len = bansi_snprintf(p, size, "%dm ", ttl/MINUTE);
	if (len < 1 || len >= size)
	    return "-err-";
	size -= len;
	p += len;
	ttl %= MINUTE;
    }

    if (ttl > 0) {
	len = bansi_snprintf(p, size, "%ds ", ttl);
	if (len < 1 || len >= size)
	    return "-err-";
	size -= len;
	p += len;
	ttl = 0;
    }

    *p = '\0';
    return buf;
}


static void dump_query(unsigned index, const bdns_parsed_query *q)
{
	BASE_INFO(" %d. Name: %.*s", index, (int)q->name.slen, q->name.ptr);
	BASE_INFO("    Type: %s (%d)", bdns_get_type_name(q->type), q->type);
	BASE_INFO("    Class: %s (%d)", (q->dnsclass==1 ? "IN" : "<Unknown>"), q->dnsclass);
}

static void dump_answer(unsigned index, const bdns_parsed_rr *rr)
{
	const bstr_t root_name = { "<Root>", 6 };
	const bstr_t *name = &rr->name;
	char ttl_words[32];
	char addr[BASE_INET6_ADDRSTRLEN];

	if (name->slen == 0)
		name = &root_name;

	BASE_INFO(" %d. %s record (type=%d)", index, bdns_get_type_name(rr->type), rr->type);
	BASE_INFO("    Name: %.*s", (int)name->slen, name->ptr);
	BASE_INFO("    TTL: %u (%s)", rr->ttl, spell_ttl(ttl_words, sizeof(ttl_words), rr->ttl));
	BASE_INFO("    Data length: %u", rr->rdlength);

	if (rr->type == BASE_DNS_TYPE_SRV)
	{
		BASE_INFO("    SRV: prio=%d, weight=%d %.*s:%d", rr->rdata.srv.prio, rr->rdata.srv.weight, (int)rr->rdata.srv.target.slen, 
			rr->rdata.srv.target.ptr, rr->rdata.srv.port);
	}
	else if (rr->type == BASE_DNS_TYPE_CNAME ||rr->type == BASE_DNS_TYPE_NS ||rr->type == BASE_DNS_TYPE_PTR) 
	{
		BASE_INFO("    Name: %.*s", (int)rr->rdata.cname.name.slen, rr->rdata.cname.name.ptr);
	}
	else if (rr->type == BASE_DNS_TYPE_A)
	{
		BASE_INFO("    IP address: %s", binet_ntop2(bAF_INET(), &rr->rdata.a.ip_addr, addr, sizeof(addr)));
	}
	else if (rr->type == BASE_DNS_TYPE_AAAA)
	{
		BASE_INFO("    IPv6 address: %s", binet_ntop2(bAF_INET6(), &rr->rdata.aaaa.ip_addr, addr, sizeof(addr)));
	}
}


void bdns_dump_packet(const bdns_parsed_packet *res)
{
	unsigned i;

	BASE_ASSERT_ON_FAIL(res != NULL, return);

	/* Header part */
	BASE_INFO( "Domain Name System packet (%s):", (BASE_DNS_GET_QR(res->hdr.flags) ? "response" : "query"));
	BASE_INFO( " Transaction ID: %d", res->hdr.id);
	BASE_INFO(" Flags: opcode=%d, authoritative=%d, truncated=%d, rcode=%d", BASE_DNS_GET_OPCODE(res->hdr.flags),
		BASE_DNS_GET_AA(res->hdr.flags), BASE_DNS_GET_TC(res->hdr.flags), BASE_DNS_GET_RCODE(res->hdr.flags));
	BASE_INFO(" Nb of queries: %d", res->hdr.qdcount);
	BASE_INFO(" Nb of answer RR: %d", res->hdr.anscount);
	BASE_INFO(" Nb of authority RR: %d", res->hdr.nscount);
	BASE_INFO(" Nb of additional RR: %d", res->hdr.arcount);
	BASE_INFO("");

	/* Dump queries */
	if (res->hdr.qdcount)
	{
		BASE_INFO(" Queries:");
		for (i=0; i<res->hdr.qdcount; ++i)
		{
			dump_query(i, &res->q[i]);
		}
		BASE_INFO("");
	}

	/* Dump answers */
	if (res->hdr.anscount)
	{
		BASE_INFO(" Answers RR:");
		for (i=0; i<res->hdr.anscount; ++i)
		{
			dump_answer(i, &res->ans[i]);
		}
		BASE_INFO("");
	}

	/* Dump NS sections */
	if (res->hdr.nscount)
	{
		BASE_INFO(" NS Authority RR:");
		for (i=0; i<res->hdr.nscount; ++i)
		{
			dump_answer(i, &res->ns[i]);
		}
		BASE_INFO("");
	}

	/* Dump Additional info sections */
	if (res->hdr.arcount)
	{
		BASE_INFO(" Additional Info RR:");

		for (i=0; i<res->hdr.arcount; ++i)
		{
			dump_answer(i, &res->arr[i]);
		}

		BASE_INFO("");
	}

}

