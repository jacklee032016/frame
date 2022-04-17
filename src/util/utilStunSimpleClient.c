/*
 *
 */
#include <utilStunSimple.h>
#include <utilErrno.h>
#include <compat/socket.h> 
#include <baseLog.h>
#include <baseOs.h>
#include <basePool.h>
#include <baseRand.h>
#include <baseSockSelect.h>
#include <baseString.h>


enum { MAX_REQUEST = 4 };
static int stun_timer[] = {500, 500, 500, 500 };
#define STUN_MAGIC 0x2112A442

bstatus_t utilstun_get_mapped_addr( bpool_factory *pf,
					    int sock_cnt, bsock_t sock[],
					    const bstr_t *srv1, int port1,
					    const bstr_t *srv2, int port2,
					    bsockaddr_in mapped_addr[])
{
    utilstun_setting opt;

    bbzero(&opt, sizeof(opt));
    opt.use_stun2 = BASE_FALSE;
    opt.srv1 = *srv1;
    opt.port1 = port1;
    opt.srv2 = *srv2;
    opt.port2 = port2;

    return utilstun_get_mapped_addr2(pf, &opt, sock_cnt, sock, mapped_addr);
}

bstatus_t utilstun_get_mapped_addr2(bpool_factory *pf,
					    const utilstun_setting *opt,
					    int sock_cnt,
					    bsock_t sock[],
					    bsockaddr_in mapped_addr[])
{
    unsigned srv_cnt;
    const bstr_t *srv1, *srv2;
    int port1, port2;
    bsockaddr srv_addr[2];
    int i, send_cnt = 0, nfds;
    bpool_t *pool;
    struct query_rec {
	struct {
	    buint32_t	mapped_addr;
	    buint32_t	mapped_port;
	} srv[2];
    } *rec;
    void       *out_msg;
    bsize_t	out_msg_len;
    int wait_resp = 0;
    bstatus_t status;

    BASE_CHECK_STACK();

    srv1 = &opt->srv1;
    port1 = opt->port1;
    srv2 = &opt->srv1;
    port2 = opt->port2;

    MTRACE( "Entering utilstun_get_mapped_addr()");

    /* Create pool. */
    pool = bpool_create(pf, "stun%p", 400, 400, NULL);
    if (!pool)
	return BASE_ENOMEM;


    /* Allocate client records */
    rec = (struct query_rec*) bpool_calloc(pool, sock_cnt, sizeof(*rec));
    if (!rec) {
	status = BASE_ENOMEM;
	goto on_error;
    }

    MTRACE("  Memory allocated.");

    /* Create the outgoing BIND REQUEST message template */
    status = utilstun_create_bind_req( pool, &out_msg, &out_msg_len, 
				      brand(), brand());
    if (status != BASE_SUCCESS)
	goto on_error;

    /* Insert magic cookie (specified in RFC 5389) when requested to. */
    if (opt->use_stun2) {
	utilstun_msg_hdr *hdr = (utilstun_msg_hdr*)out_msg;
	hdr->tsx[0] = bhtonl(STUN_MAGIC);
    }

    MTRACE("  Binding request created.");

    /* Resolve servers. */
    status = bsockaddr_init(opt->af, &srv_addr[0], srv1, (buint16_t)port1);
    if (status != BASE_SUCCESS)
	goto on_error;

    srv_cnt = 1;

    if (srv2 && port2) {
	status = bsockaddr_init(opt->af, &srv_addr[1], srv2,
				  (buint16_t)port2);
	if (status != BASE_SUCCESS)
	    goto on_error;

	if (bsockaddr_cmp(&srv_addr[1], &srv_addr[0]) != 0) {
	    srv_cnt++;
	}
    }

    MTRACE("  Server initialized, using %d server(s)", srv_cnt);

    /* Init mapped addresses to zero */
    bmemset(mapped_addr, 0, sock_cnt * sizeof(bsockaddr_in));

    /* We need these many responses */
    wait_resp = sock_cnt * srv_cnt;

    MTRACE("  Done initialization.");

#if defined(BASE_SELECT_NEEDS_NFDS) && BASE_SELECT_NEEDS_NFDS!=0
    nfds = -1;
    for (i=0; i<sock_cnt; ++i) {
	if (sock[i] > nfds) {
	    nfds = sock[i];
	}
    }
#else
    nfds = FD_SETSIZE-1;
#endif

    /* Main retransmission loop. */
    for (send_cnt=0; send_cnt<MAX_REQUEST; ++send_cnt) {
	btime_val nbtx, now;
	bfd_set_t r;
	int select_rc;

	BASE_FD_ZERO(&r);

	/* Send messages to servers that has not given us response. */
	for (i=0; i<sock_cnt && status==BASE_SUCCESS; ++i) {
	    unsigned j;
	    for (j=0; j<srv_cnt && status==BASE_SUCCESS; ++j) {
		utilstun_msg_hdr *msg_hdr = (utilstun_msg_hdr*) out_msg;
                bssize_t sent_len;

		if (rec[i].srv[j].mapped_port != 0)
		    continue;

		/* Modify message so that we can distinguish response. */
		msg_hdr->tsx[2] = bhtonl(i);
		msg_hdr->tsx[3] = bhtonl(j);

		/* Send! */
                sent_len = out_msg_len;
		status = bsock_sendto(sock[i], out_msg, &sent_len, 0,
					(bsockaddr_t*)&srv_addr[j],
					bsockaddr_get_len(&srv_addr[j]));
	    }
	}

	/* All requests sent.
	 * The loop below will wait for responses until all responses have
	 * been received (i.e. wait_resp==0) or timeout occurs, which then
	 * we'll go to the next retransmission iteration.
	 */
	MTRACE("  Request(s) sent, counter=%d", send_cnt);

	/* Calculate time of next retransmission. */
	bgettickcount(&nbtx);
	nbtx.sec += (stun_timer[send_cnt]/1000);
	nbtx.msec += (stun_timer[send_cnt]%1000);
	btime_val_normalize(&nbtx);

	for (bgettickcount(&now), select_rc=1;
	     status==BASE_SUCCESS && select_rc>=1 && wait_resp>0 
	       && BASE_TIME_VAL_LT(now, nbtx); 
	     bgettickcount(&now))
	{
	    btime_val timeout;

	    timeout = nbtx;
	    BASE_TIME_VAL_SUB(timeout, now);

	    for (i=0; i<sock_cnt; ++i) {
		BASE_FD_SET(sock[i], &r);
	    }

	    select_rc = bsock_select(nfds+1, &r, NULL, NULL, &timeout);
	    MTRACE( "  select() rc=%d", select_rc);
	    if (select_rc < 1)
		continue;

	    for (i=0; i<sock_cnt; ++i) {
		int sock_idx, srv_idx;
                bssize_t len;
		utilstun_msg msg;
		bsockaddr addr;
		int addrlen = sizeof(addr);
		utilstun_mapped_addr_attr *attr;
		char recv_buf[128];

		if (!BASE_FD_ISSET(sock[i], &r))
		    continue;

                len = sizeof(recv_buf);
		status = bsock_recvfrom( sock[i], recv_buf, 
				           &len, 0,
				           (bsockaddr_t*)&addr,
					   &addrlen);

		if (status != BASE_SUCCESS) {
		    BASE_PERROR(4,(THIS_FILE, status,
				 "recvfrom() error ignored"));

		    /* Ignore non-BASE_SUCCESS status.
		     * It possible that other SIP entity is currently 
		     * sending SIP request to us, and because SIP message
		     * is larger than STUN, we could get EMSGSIZE when
		     * we call recvfrom().
		     */
		    status = BASE_SUCCESS;
		    continue;
		}

		status = utilstun_parse_msg(recv_buf, len, &msg);
		if (status != BASE_SUCCESS) {
		    BASE_PERROR(4,(THIS_FILE, status,
				 "STUN parsing error ignored"));

		    /* Also ignore non-successful parsing. This may not
		     * be STUN response at all. See the comment above.
		     */
		    status = BASE_SUCCESS;
		    continue;
		}

		sock_idx = bntohl(msg.hdr->tsx[2]);
		srv_idx = bntohl(msg.hdr->tsx[3]);

		if (sock_idx<0 || sock_idx>=sock_cnt || sock_idx!=i ||
			srv_idx<0 || srv_idx>=2)
		{
		    status = UTIL_ESTUNININDEX;
		    continue;
		}

		if (bntohs(msg.hdr->type) != USTUN_BINDING_RESPONSE) {
		    status = UTIL_ESTUNNOBINDRES;
		    continue;
		}

		if (rec[sock_idx].srv[srv_idx].mapped_port != 0) {
		    /* Already got response */
		    continue;
		}

		/* From this part, we consider the packet as a valid STUN
		 * response for our request.
		 */
		--wait_resp;

		if (utilstun_msg_find_attr(&msg, USTUN_ATTR_ERROR_CODE) != NULL) {
		    status = UTIL_ESTUNRECVERRATTR;
		    continue;
		}

		attr = (utilstun_mapped_addr_attr*) 
		       utilstun_msg_find_attr(&msg, USTUN_ATTR_MAPPED_ADDR);
		if (!attr) {
		    attr = (utilstun_mapped_addr_attr*) 
			   utilstun_msg_find_attr(&msg, USTUN_ATTR_XOR_MAPPED_ADDR);
		    if (!attr || attr->family != 1) {
			status = UTIL_ESTUNNOMAP;
			continue;
		    }
		}

		rec[sock_idx].srv[srv_idx].mapped_addr = attr->addr;
		rec[sock_idx].srv[srv_idx].mapped_port = attr->port;
		if (bntohs(attr->hdr.type) == USTUN_ATTR_XOR_MAPPED_ADDR) {
		    rec[sock_idx].srv[srv_idx].mapped_addr ^= bhtonl(STUN_MAGIC);
		    rec[sock_idx].srv[srv_idx].mapped_port ^= bhtons(STUN_MAGIC >> 16);
		}
	    }
	}

	/* The best scenario is if all requests have been replied.
	 * Then we don't need to go to the next retransmission iteration.
	 */
	if (wait_resp <= 0)
	    break;
    }

    MTRACE("  All responses received, calculating result..");

    for (i=0; i<sock_cnt && status==BASE_SUCCESS; ++i) {
	if (srv_cnt == 1) {
	    mapped_addr[i].sin_family = bAF_INET();
	    mapped_addr[i].sin_addr.s_addr = rec[i].srv[0].mapped_addr;
	    mapped_addr[i].sin_port = (buint16_t)rec[i].srv[0].mapped_port;

	    if (rec[i].srv[0].mapped_addr == 0 || rec[i].srv[0].mapped_port == 0) {
		status = UTIL_ESTUNNOTRESPOND;
		break;
	    }
	} else if (rec[i].srv[0].mapped_addr == rec[i].srv[1].mapped_addr &&
	           rec[i].srv[0].mapped_port == rec[i].srv[1].mapped_port)
	{
	    mapped_addr[i].sin_family = bAF_INET();
	    mapped_addr[i].sin_addr.s_addr = rec[i].srv[0].mapped_addr;
	    mapped_addr[i].sin_port = (buint16_t)rec[i].srv[0].mapped_port;

	    if (rec[i].srv[0].mapped_addr == 0 || rec[i].srv[0].mapped_port == 0) {
		status = UTIL_ESTUNNOTRESPOND;
		break;
	    }
	} else {
	    status = UTIL_ESTUNSYMMETRIC;
	    break;
	}
    }

    MTRACE("  Pool usage=%d of %d", bpool_get_used_size(pool),  bpool_get_capacity(pool));

    bpool_release(pool);

    MTRACE( "  Done.");
    return status;

on_error:
    if (pool) bpool_release(pool);
    return status;
}

