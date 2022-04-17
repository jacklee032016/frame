/* 
 *
 */
#include <utilDns.h>
#include <utilErrno.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <basePool.h>
#include <baseSock.h>
#include <baseString.h>


const char * bdns_get_type_name(int type)
{
    switch (type) {
    case BASE_DNS_TYPE_A:	    return "A";
    case BASE_DNS_TYPE_AAAA:  return "AAAA";
    case BASE_DNS_TYPE_SRV:   return "SRV";
    case BASE_DNS_TYPE_NS:    return "NS";
    case BASE_DNS_TYPE_CNAME: return "CNAME";
    case BASE_DNS_TYPE_PTR:   return "PTR";
    case BASE_DNS_TYPE_MX:    return "MX";
    case BASE_DNS_TYPE_TXT:   return "TXT";
    case BASE_DNS_TYPE_NAPTR: return "NAPTR";
    }
    return "(Unknown)";
}


static void write16(buint8_t *p, buint16_t val)
{
    p[0] = (buint8_t)(val >> 8);
    p[1] = (buint8_t)(val & 0xFF);
}


/**
 * Initialize a DNS query transaction.
 */
bstatus_t bdns_make_query( void *packet,
				       unsigned *size,
				       buint16_t id,
				       int qtype,
				       const bstr_t *name)
{
    buint8_t *p = (buint8_t*)packet;
    const char *startlabel, *endlabel, *endname;
    bsize_t d;

    /* Sanity check */
    BASE_ASSERT_RETURN(packet && size && qtype && name, BASE_EINVAL);

    /* Calculate total number of bytes required. */
    d = sizeof(bdns_hdr) + name->slen + 4;

    /* Check that size is sufficient. */
    BASE_ASSERT_RETURN(*size >= d, UTIL_EDNSQRYTOOSMALL);

    /* Initialize header */
    bassert(sizeof(bdns_hdr)==12);
    bbzero(p, sizeof(struct bdns_hdr));
    write16(p+0, id);
    write16(p+2, (buint16_t)BASE_DNS_SET_RD(1));
    write16(p+4, (buint16_t)1);

    /* Initialize query */
    p = ((buint8_t*)packet)+sizeof(bdns_hdr);

    /* Tokenize name */
    startlabel = endlabel = name->ptr;
    endname = name->ptr + name->slen;
    while (endlabel != endname) {
	while (endlabel != endname && *endlabel != '.')
	    ++endlabel;
	*p++ = (buint8_t)(endlabel - startlabel);
	bmemcpy(p, startlabel, endlabel-startlabel);
	p += (endlabel-startlabel);
	if (endlabel != endname && *endlabel == '.')
	    ++endlabel;
	startlabel = endlabel;
    }
    *p++ = '\0';

    /* Set type */
    write16(p, (buint16_t)qtype);
    p += 2;

    /* Set class (IN=1) */
    write16(p, 1);
    p += 2;

    /* Done, calculate length */
    *size = (unsigned)(p - (buint8_t*)packet);

    return 0;
}


/* Get a name length (note: name consists of multiple labels and
 * it may contain pointers when name compression is applied) 
 */
static bstatus_t get_name_len(int rec_counter, const buint8_t *pkt, 
				const buint8_t *start, const buint8_t *max, 
				int *parsed_len, int *name_len)
{
    const buint8_t *p;
    bstatus_t status;

    /* Limit the number of recursion */
    if (rec_counter > 10) {
	/* Too many name recursion */
	return UTIL_EDNSINNAMEPTR;
    }

    *name_len = *parsed_len = 0;
    p = start;
    while (*p) {
	if ((*p & 0xc0) == 0xc0) {
	    /* Compression is found! */
	    int ptr_len = 0;
	    int dummy;
	    buint16_t offset;

	    /* Get the 14bit offset */
	    bmemcpy(&offset, p, 2);
	    offset ^= bhtons((buint16_t)(0xc0 << 8));
	    offset = bntohs(offset);

	    /* Check that offset is valid */
	    if (offset >= max - pkt)
		return UTIL_EDNSINNAMEPTR;

	    /* Get the name length from that offset. */
	    status = get_name_len(rec_counter+1, pkt, pkt + offset, max, 
				  &dummy, &ptr_len);
	    if (status != BASE_SUCCESS)
		return status;

	    *parsed_len += 2;
	    *name_len += ptr_len;

	    return BASE_SUCCESS;
	} else {
	    unsigned label_len = *p;

	    /* Check that label length is valid */
	    if (pkt+label_len > max)
		return UTIL_EDNSINNAMEPTR;

	    p += (label_len + 1);
	    *parsed_len += (label_len + 1);

	    if (*p != 0)
		++label_len;
	    
	    *name_len += label_len;

	    if (p >= max)
		return UTIL_EDNSINSIZE;
	}
    }
    ++p;
    (*parsed_len)++;

    return BASE_SUCCESS;
}


/* Parse and copy name (note: name consists of multiple labels and
 * it may contain pointers when compression is applied).
 */
static bstatus_t get_name(int rec_counter, const buint8_t *pkt, 
			    const buint8_t *start, const buint8_t *max,
			    bstr_t *name)
{
    const buint8_t *p;
    bstatus_t status;

    /* Limit the number of recursion */
    if (rec_counter > 10) {
	/* Too many name recursion */
	return UTIL_EDNSINNAMEPTR;
    }

    p = start;
    while (*p) {
	if ((*p & 0xc0) == 0xc0) {
	    /* Compression is found! */
	    buint16_t offset;

	    /* Get the 14bit offset */
	    bmemcpy(&offset, p, 2);
	    offset ^= bhtons((buint16_t)(0xc0 << 8));
	    offset = bntohs(offset);

	    /* Check that offset is valid */
	    if (offset >= max - pkt)
		return UTIL_EDNSINNAMEPTR;

	    /* Retrieve the name from that offset. */
	    status = get_name(rec_counter+1, pkt, pkt + offset, max, name);
	    if (status != BASE_SUCCESS)
		return status;

	    return BASE_SUCCESS;
	} else {
	    unsigned label_len = *p;

	    /* Check that label length is valid */
	    if (pkt+label_len > max)
		return UTIL_EDNSINNAMEPTR;

	    bmemcpy(name->ptr + name->slen, p+1, label_len);
	    name->slen += label_len;

	    p += label_len + 1;
	    if (*p != 0) {
		*(name->ptr + name->slen) = '.';
		++name->slen;
	    }

	    if (p >= max)
		return UTIL_EDNSINSIZE;
	}
    }

    return BASE_SUCCESS;
}


/* Parse query records. */
static bstatus_t parse_query(bdns_parsed_query *q, bpool_t *pool,
			       const buint8_t *pkt, const buint8_t *start,
			       const buint8_t *max, int *parsed_len)
{
    const buint8_t *p = start;
    int name_len, name_part_len;
    bstatus_t status;

    /* Get the length of the name */
    status = get_name_len(0, pkt, start, max, &name_part_len, &name_len);
    if (status != BASE_SUCCESS)
	return status;

    /* Allocate memory for the name */
    q->name.ptr = (char*) bpool_alloc(pool, name_len+4);
    q->name.slen = 0;

    /* Get the name */
    status = get_name(0, pkt, start, max, &q->name);
    if (status != BASE_SUCCESS)
	return status;

    p = (start + name_part_len);

    /* Get the type */
    bmemcpy(&q->type, p, 2);
    q->type = bntohs(q->type);
    p += 2;

    /* Get the class */
    bmemcpy(&q->dnsclass, p, 2);
    q->dnsclass = bntohs(q->dnsclass);
    p += 2;

    *parsed_len = (int)(p - start);

    return BASE_SUCCESS;
}


/* Parse RR records */
static bstatus_t parse_rr(bdns_parsed_rr *rr, bpool_t *pool,
			    const buint8_t *pkt,
			    const buint8_t *start, const buint8_t *max,
			    int *parsed_len)
{
    const buint8_t *p = start;
    int name_len, name_part_len;
    bstatus_t status;

    /* Get the length of the name */
    status = get_name_len(0, pkt, start, max, &name_part_len, &name_len);
    if (status != BASE_SUCCESS)
	return status;

    /* Allocate memory for the name */
    rr->name.ptr = (char*) bpool_alloc(pool, name_len+4);
    rr->name.slen = 0;

    /* Get the name */
    status = get_name(0, pkt, start, max, &rr->name);
    if (status != BASE_SUCCESS)
	return status;

    p = (start + name_part_len);

    /* Check the size can accomodate next few fields. */
    if (p+10 > max)
	return UTIL_EDNSINSIZE;

    /* Get the type */
    bmemcpy(&rr->type, p, 2);
    rr->type = bntohs(rr->type);
    p += 2;
    
    /* Get the class */
    bmemcpy(&rr->dnsclass, p, 2);
    rr->dnsclass = bntohs(rr->dnsclass);
    p += 2;

    /* Class MUST be IN */
    if (rr->dnsclass != 1) {
	/* Class is not IN, return error only if type is known (see #1889) */
	if (rr->type == BASE_DNS_TYPE_A     || rr->type == BASE_DNS_TYPE_AAAA  ||
	    rr->type == BASE_DNS_TYPE_CNAME || rr->type == BASE_DNS_TYPE_NS    ||
	    rr->type == BASE_DNS_TYPE_PTR   || rr->type == BASE_DNS_TYPE_SRV)
	{
	    return UTIL_EDNSINCLASS;
	}
    }

    /* Get TTL */
    bmemcpy(&rr->ttl, p, 4);
    rr->ttl = bntohl(rr->ttl);
    p += 4;

    /* Get rdlength */
    bmemcpy(&rr->rdlength, p, 2);
    rr->rdlength = bntohs(rr->rdlength);
    p += 2;

    /* Check that length is valid */
    if (p + rr->rdlength > max)
	return UTIL_EDNSINSIZE;

    /* Parse some well known records */
    if (rr->type == BASE_DNS_TYPE_A) {
	bmemcpy(&rr->rdata.a.ip_addr, p, 4);
	p += 4;

    } else if (rr->type == BASE_DNS_TYPE_AAAA) {
	bmemcpy(&rr->rdata.aaaa.ip_addr, p, 16);
	p += 16;

    } else if (rr->type == BASE_DNS_TYPE_CNAME ||
	       rr->type == BASE_DNS_TYPE_NS ||
	       rr->type == BASE_DNS_TYPE_PTR) 
    {

	/* Get the length of the target name */
	status = get_name_len(0, pkt, p, max, &name_part_len, &name_len);
	if (status != BASE_SUCCESS)
	    return status;

	/* Allocate memory for the name */
	rr->rdata.cname.name.ptr = (char*) bpool_alloc(pool, name_len);
	rr->rdata.cname.name.slen = 0;

	/* Get the name */
	status = get_name(0, pkt, p, max, &rr->rdata.cname.name);
	if (status != BASE_SUCCESS)
	    return status;

	p += name_part_len;

    } else if (rr->type == BASE_DNS_TYPE_SRV) {

	/* Priority */
	bmemcpy(&rr->rdata.srv.prio, p, 2);
	rr->rdata.srv.prio = bntohs(rr->rdata.srv.prio);
	p += 2;

	/* Weight */
	bmemcpy(&rr->rdata.srv.weight, p, 2);
	rr->rdata.srv.weight = bntohs(rr->rdata.srv.weight);
	p += 2;

	/* Port */
	bmemcpy(&rr->rdata.srv.port, p, 2);
	rr->rdata.srv.port = bntohs(rr->rdata.srv.port);
	p += 2;
	
	/* Get the length of the target name */
	status = get_name_len(0, pkt, p, max, &name_part_len, &name_len);
	if (status != BASE_SUCCESS)
	    return status;

	/* Allocate memory for the name */
	rr->rdata.srv.target.ptr = (char*) bpool_alloc(pool, name_len);
	rr->rdata.srv.target.slen = 0;

	/* Get the name */
	status = get_name(0, pkt, p, max, &rr->rdata.srv.target);
	if (status != BASE_SUCCESS)
	    return status;
	p += name_part_len;

    } else {
	/* Copy the raw data */
	rr->data = bpool_alloc(pool, rr->rdlength);
	bmemcpy(rr->data, p, rr->rdlength);

	p += rr->rdlength;
    }

    *parsed_len = (int)(p - start);
    return BASE_SUCCESS;
}


/*
 * Parse raw DNS packet into DNS packet structure.
 */
bstatus_t bdns_parse_packet( bpool_t *pool,
				  	 const void *packet,
					 unsigned size,
					 bdns_parsed_packet **p_res)
{
    bdns_parsed_packet *res;
    const buint8_t *start, *end;
    bstatus_t status;
    unsigned i;

    /* Sanity checks */
    BASE_ASSERT_RETURN(pool && packet && size && p_res, BASE_EINVAL);

    /* Packet size must be at least as big as the header */
    if (size < sizeof(bdns_hdr))
	return UTIL_EDNSINSIZE;

    /* Create the structure */
    res = BASE_POOL_ZALLOC_T(pool, bdns_parsed_packet);

    /* Copy the DNS header, and convert endianness to host byte order */
    bmemcpy(&res->hdr, packet, sizeof(bdns_hdr));
    res->hdr.id	      = bntohs(res->hdr.id);
    res->hdr.flags    = bntohs(res->hdr.flags);
    res->hdr.qdcount  = bntohs(res->hdr.qdcount);
    res->hdr.anscount = bntohs(res->hdr.anscount);
    res->hdr.nscount  = bntohs(res->hdr.nscount);
    res->hdr.arcount  = bntohs(res->hdr.arcount);

    /* Mark start and end of payload */
    start = ((const buint8_t*)packet) + sizeof(bdns_hdr);
    end = ((const buint8_t*)packet) + size;

    /* Parse query records (if any).
     */
    if (res->hdr.qdcount) {
	res->q = (bdns_parsed_query*)
		 bpool_zalloc(pool, res->hdr.qdcount *
				      sizeof(bdns_parsed_query));
	for (i=0; i<res->hdr.qdcount; ++i) {
	    int parsed_len = 0;
	    
	    status = parse_query(&res->q[i], pool, (const buint8_t*)packet, 
	    			 start, end, &parsed_len);
	    if (status != BASE_SUCCESS)
		return status;

	    start += parsed_len;
	}
    }

    /* Parse answer, if any */
    if (res->hdr.anscount) {
	res->ans = (bdns_parsed_rr*)
		   bpool_zalloc(pool, res->hdr.anscount * 
					sizeof(bdns_parsed_rr));

	for (i=0; i<res->hdr.anscount; ++i) {
	    int parsed_len;

	    status = parse_rr(&res->ans[i], pool, (const buint8_t*)packet, 
	    		      start, end, &parsed_len);
	    if (status != BASE_SUCCESS)
		return status;

	    start += parsed_len;
	}
    }

    /* Parse authoritative NS records, if any */
    if (res->hdr.nscount) {
	res->ns = (bdns_parsed_rr*)
		  bpool_zalloc(pool, res->hdr.nscount *
				       sizeof(bdns_parsed_rr));

	for (i=0; i<res->hdr.nscount; ++i) {
	    int parsed_len;

	    status = parse_rr(&res->ns[i], pool, (const buint8_t*)packet, 
	    		      start, end, &parsed_len);
	    if (status != BASE_SUCCESS)
		return status;

	    start += parsed_len;
	}
    }

    /* Parse additional RR answer, if any */
    if (res->hdr.arcount) {
	res->arr = (bdns_parsed_rr*)
		   bpool_zalloc(pool, res->hdr.arcount *
					sizeof(bdns_parsed_rr));

	for (i=0; i<res->hdr.arcount; ++i) {
	    int parsed_len;

	    status = parse_rr(&res->arr[i], pool, (const buint8_t*)packet, 
	    		      start, end, &parsed_len);
	    if (status != BASE_SUCCESS)
		return status;

	    start += parsed_len;
	}
    }

    /* Looks like everything is okay */
    *p_res = res;

    return BASE_SUCCESS;
}


/* Perform name compression scheme.
 * If a name is already in the nametable, when no need to duplicate
 * the string with the pool, but rather just use the pointer there.
 */
static void apply_name_table( unsigned *count,
			      bstr_t nametable[],
		    	      const bstr_t *src,
			      bpool_t *pool,
			      bstr_t *dst)
{
    unsigned i;

    /* Scan strings in nametable */
    for (i=0; i<*count; ++i) {
	if (bstricmp(&nametable[i], src) == 0)
	    break;
    }

    /* If name is found in nametable, use the pointer in the nametable */
    if (i != *count) {
	dst->ptr = nametable[i].ptr;
	dst->slen = nametable[i].slen;
	return;
    }

    /* Otherwise duplicate the string, and insert new name in nametable */
    bstrdup(pool, dst, src);

    if (*count < BASE_DNS_MAX_NAMES_IN_NAMETABLE) {
	nametable[*count].ptr = dst->ptr;
	nametable[*count].slen = dst->slen;

	++(*count);
    }
}

static void copy_query(bpool_t *pool, bdns_parsed_query *dst,
		       const bdns_parsed_query *src,
		       unsigned *nametable_count,
		       bstr_t nametable[])
{
    bmemcpy(dst, src, sizeof(*src));
    apply_name_table(nametable_count, nametable, &src->name, pool, &dst->name);
}


static void copy_rr(bpool_t *pool, bdns_parsed_rr *dst,
		    const bdns_parsed_rr *src,
		    unsigned *nametable_count,
		    bstr_t nametable[])
{
    bmemcpy(dst, src, sizeof(*src));
    apply_name_table(nametable_count, nametable, &src->name, pool, &dst->name);

    if (src->data) {
	dst->data = bpool_alloc(pool, src->rdlength);
	bmemcpy(dst->data, src->data, src->rdlength);
    }

    if (src->type == BASE_DNS_TYPE_SRV) {
	apply_name_table(nametable_count, nametable, &src->rdata.srv.target, 
			 pool, &dst->rdata.srv.target);
    } else if (src->type == BASE_DNS_TYPE_A) {
	dst->rdata.a.ip_addr.s_addr =  src->rdata.a.ip_addr.s_addr;
    } else if (src->type == BASE_DNS_TYPE_AAAA) {
	bmemcpy(&dst->rdata.aaaa.ip_addr, &src->rdata.aaaa.ip_addr,
		  sizeof(bin6_addr));
    } else if (src->type == BASE_DNS_TYPE_CNAME) {
	bstrdup(pool, &dst->rdata.cname.name, &src->rdata.cname.name);
    } else if (src->type == BASE_DNS_TYPE_NS) {
	bstrdup(pool, &dst->rdata.ns.name, &src->rdata.ns.name);
    } else if (src->type == BASE_DNS_TYPE_PTR) {
	bstrdup(pool, &dst->rdata.ptr.name, &src->rdata.ptr.name);
    }
}

/*
 * Duplicate DNS packet.
 */
void bdns_packet_dup(bpool_t *pool,
			       const bdns_parsed_packet*p,
			       unsigned options,
			       bdns_parsed_packet **p_dst)
{
    bdns_parsed_packet *dst;
    unsigned nametable_count = 0;
#if BASE_DNS_MAX_NAMES_IN_NAMETABLE
    bstr_t nametable[BASE_DNS_MAX_NAMES_IN_NAMETABLE];
#else
    bstr_t *nametable = NULL;
#endif
    unsigned i;

    BASE_ASSERT_ON_FAIL(pool && p && p_dst, return);

    /* Create packet and copy header */
    *p_dst = dst = BASE_POOL_ZALLOC_T(pool, bdns_parsed_packet);
    bmemcpy(&dst->hdr, &p->hdr, sizeof(p->hdr));

    /* Initialize section counts in the target packet to zero.
     * If memory allocation fails during copying process, the target packet
     * should have a correct section counts.
     */
    dst->hdr.qdcount = 0;
    dst->hdr.anscount = 0;
    dst->hdr.nscount = 0;
    dst->hdr.arcount = 0;
	

    /* Copy query section */
    if (p->hdr.qdcount && (options & BASE_DNS_NO_QD)==0) {
	dst->q = (bdns_parsed_query*)
		 bpool_alloc(pool, p->hdr.qdcount * 
				     sizeof(bdns_parsed_query));
	for (i=0; i<p->hdr.qdcount; ++i) {
	    copy_query(pool, &dst->q[i], &p->q[i], 
		       &nametable_count, nametable);
	    ++dst->hdr.qdcount;
	}
    }

    /* Copy answer section */
    if (p->hdr.anscount && (options & BASE_DNS_NO_ANS)==0) {
	dst->ans = (bdns_parsed_rr*)
		   bpool_alloc(pool, p->hdr.anscount * 
				       sizeof(bdns_parsed_rr));
	for (i=0; i<p->hdr.anscount; ++i) {
	    copy_rr(pool, &dst->ans[i], &p->ans[i],
		    &nametable_count, nametable);
	    ++dst->hdr.anscount;
	}
    }

    /* Copy NS section */
    if (p->hdr.nscount && (options & BASE_DNS_NO_NS)==0) {
	dst->ns = (bdns_parsed_rr*)
		  bpool_alloc(pool, p->hdr.nscount * 
				      sizeof(bdns_parsed_rr));
	for (i=0; i<p->hdr.nscount; ++i) {
	    copy_rr(pool, &dst->ns[i], &p->ns[i],
		    &nametable_count, nametable);
	    ++dst->hdr.nscount;
	}
    }

    /* Copy additional info section */
    if (p->hdr.arcount && (options & BASE_DNS_NO_AR)==0) {
	dst->arr = (bdns_parsed_rr*)
		   bpool_alloc(pool, p->hdr.arcount * 
				       sizeof(bdns_parsed_rr));
	for (i=0; i<p->hdr.arcount; ++i) {
	    copy_rr(pool, &dst->arr[i], &p->arr[i],
		    &nametable_count, nametable);
	    ++dst->hdr.arcount;
	}
    }
}


void bdns_init_srv_rr( bdns_parsed_rr *rec,
				 const bstr_t *res_name,
				 unsigned dnsclass,
				 unsigned ttl,
				 unsigned prio,
				 unsigned weight,
				 unsigned port,
				 const bstr_t *target)
{
    bbzero(rec, sizeof(*rec));
    rec->name = *res_name;
    rec->type = BASE_DNS_TYPE_SRV;
    rec->dnsclass = (buint16_t) dnsclass;
    rec->ttl = ttl;
    rec->rdata.srv.prio = (buint16_t) prio;
    rec->rdata.srv.weight = (buint16_t) weight;
    rec->rdata.srv.port = (buint16_t) port;
    rec->rdata.srv.target = *target;
}


void bdns_init_cname_rr( bdns_parsed_rr *rec,
				   const bstr_t *res_name,
				   unsigned dnsclass,
				   unsigned ttl,
				   const bstr_t *name)
{
    bbzero(rec, sizeof(*rec));
    rec->name = *res_name;
    rec->type = BASE_DNS_TYPE_CNAME;
    rec->dnsclass = (buint16_t) dnsclass;
    rec->ttl = ttl;
    rec->rdata.cname.name = *name;
}


void bdns_init_a_rr( bdns_parsed_rr *rec,
			       const bstr_t *res_name,
			       unsigned dnsclass,
			       unsigned ttl,
			       const bin_addr *ip_addr)
{
    bbzero(rec, sizeof(*rec));
    rec->name = *res_name;
    rec->type = BASE_DNS_TYPE_A;
    rec->dnsclass = (buint16_t) dnsclass;
    rec->ttl = ttl;
    rec->rdata.a.ip_addr = *ip_addr;
}


void bdns_init_aaaa_rr(bdns_parsed_rr *rec,
				 const bstr_t *res_name,
				 unsigned dnsclass,
				 unsigned ttl,
				 const bin6_addr *ip_addr)
{
    bbzero(rec, sizeof(*rec));
    rec->name = *res_name;
    rec->type = BASE_DNS_TYPE_AAAA;
    rec->dnsclass = (buint16_t) dnsclass;
    rec->ttl = ttl;
    rec->rdata.aaaa.ip_addr = *ip_addr;
}

