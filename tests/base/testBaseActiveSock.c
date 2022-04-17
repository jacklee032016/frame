/* 
 *
 */
#include "testBaseTest.h"
#include <libBase.h>

/**
 * \page page_baselib_activesock_test Test: Active Socket
 */

#if INCLUDE_ACTIVESOCK_TEST

/*
 * Simple UDP echo server.
 */
struct udp_echo_srv
{
	bactivesock_t			*asock;
	bbool_t					echo_enabled;
	buint16_t				port;
	bioqueue_op_key_t		send_key;
	bstatus_t				status;
	unsigned				rx_cnt;
	unsigned				rx_err_cnt, tx_err_cnt;
};

static void udp_echo_err(const char *title, bstatus_t status)
{
	char errmsg[BASE_ERR_MSG_SIZE];

	extStrError(status, errmsg, sizeof(errmsg));
	BASE_ERROR("   error: %s: %s", title, errmsg);
}

static bbool_t udp_echo_srv_on_data_recvfrom(bactivesock_t *asock, void *data, bsize_t size,
	const bsockaddr_t *src_addr, int addr_len, bstatus_t status)
{
    struct udp_echo_srv *srv;
    bssize_t sent;


    srv = (struct udp_echo_srv*) bactivesock_get_user_data(asock);

    if (status != BASE_SUCCESS)
	{
		srv->status = status;
		srv->rx_err_cnt++;
		udp_echo_err("recvfrom() callback", status);
		return BASE_TRUE;
    }

    srv->rx_cnt++;

    /* Send back if echo is enabled */
    if (srv->echo_enabled)
	{
		sent = size;
		srv->status = bactivesock_sendto(asock, &srv->send_key, data, &sent, 0, src_addr, addr_len);
		if (srv->status != BASE_SUCCESS)
		{
	    	srv->tx_err_cnt++;
		    udp_echo_err("sendto()", status);
		}
    }

    return BASE_TRUE;
}


static bstatus_t udp_echo_srv_create(bpool_t *pool, bioqueue_t *ioqueue, bbool_t enable_echo, struct udp_echo_srv **p_srv)
{
	struct udp_echo_srv *srv;
	bsock_t sock_fd = BASE_INVALID_SOCKET;
	bsockaddr addr;
	bactivesock_cb activesock_cb;
	bstatus_t status;

	srv = BASE_POOL_ZALLOC_T(pool, struct udp_echo_srv);
	srv->echo_enabled = enable_echo;

	bsockaddr_in_init(&addr.ipv4, NULL, 0);

	bbzero(&activesock_cb, sizeof(activesock_cb));
	activesock_cb.on_data_recvfrom = &udp_echo_srv_on_data_recvfrom;

	status = bactivesock_create_udp(pool, &addr, NULL, ioqueue, &activesock_cb,  srv, &srv->asock, &addr);
	if (status != BASE_SUCCESS) {
		bsock_close(sock_fd);
		udp_echo_err("bactivesock_create()", status);
		return status;
	}

	srv->port = bntohs(addr.ipv4.sin_port);

	bioqueue_op_key_init(&srv->send_key, sizeof(srv->send_key));

	status = bactivesock_start_recvfrom(srv->asock, pool, 32, 0);
	if (status != BASE_SUCCESS) {
		bactivesock_close(srv->asock);
		udp_echo_err("bactivesock_start_recvfrom()", status);
		return status;
	}


	*p_srv = srv;
	return BASE_SUCCESS;
}

static void udp_echo_srv_destroy(struct udp_echo_srv *srv)
{
	bactivesock_close(srv->asock);
}

/*
 * UDP ping pong test (send packet back and forth between two UDP echo servers.
 */
static int udp_ping_pong_test(void)
{
    bioqueue_t *ioqueue = NULL;
    bpool_t *pool = NULL;
    struct udp_echo_srv *srv1=NULL, *srv2=NULL;
    bbool_t need_send = BASE_TRUE;
    unsigned data = 0;
    int count, ret;
    bstatus_t status;

    pool = bpool_create(mem, "pingpong", 512, 512, NULL);
    if (!pool)
		return -10;

    status = bioqueue_create(pool, 4, &ioqueue);
    if (status != BASE_SUCCESS)
	{
		ret = -20;
		udp_echo_err("bioqueue_create()", status);
		goto on_return;
    }

    status = udp_echo_srv_create(pool, ioqueue, BASE_TRUE, &srv1);
    if (status != BASE_SUCCESS)
	{
		ret = -30;
		goto on_return;
    }

    status = udp_echo_srv_create(pool, ioqueue, BASE_TRUE, &srv2);
    if (status != BASE_SUCCESS)
	{
		ret = -40;
		goto on_return;
    }

    /* initiate the first send */
    for (count=0; count<1000; ++count)
	{
		unsigned last_rx1, last_rx2;
		unsigned i;

		if (need_send)
		{
	    	bstr_t loopback;
		    bsockaddr_in addr;
		    bssize_t sent;

	    	++data;

		    sent = sizeof(data);
		    loopback = bstr("127.0.0.1");
	    	bsockaddr_in_init(&addr, &loopback, srv2->port);
		    status = bactivesock_sendto(srv1->asock, &srv1->send_key, &data, &sent, 0, &addr, sizeof(addr));
			if (status != BASE_SUCCESS && status != BASE_EPENDING)
			{
				ret = -50;
				udp_echo_err("sendto()", status);
				goto on_return;
			}

			need_send = BASE_FALSE;
		}

		last_rx1 = srv1->rx_cnt;
		last_rx2 = srv2->rx_cnt;

		for (i=0; i<10 && last_rx1 == srv1->rx_cnt && last_rx2 == srv2->rx_cnt; ++i)
		{
	    	btime_val delay = {0, 10};
#ifdef BASE_SYMBIAN
		    BASE_UNUSED_ARG(delay);
		    bsymbianos_poll(-1, 100);
#else
	    	bioqueue_poll(ioqueue, &delay);
#endif
		}

		if (srv1->rx_err_cnt+srv1->tx_err_cnt != 0 ||
	    	srv2->rx_err_cnt+srv2->tx_err_cnt != 0)
		{
		    /* Got error */
	    	ret = -60;
		    goto on_return;
		}

		if (last_rx1 == srv1->rx_cnt && last_rx2 == srv2->rx_cnt)
		{
		    /* Packet lost */
	    	ret = -70;
	    	udp_echo_err("packets have been lost", BASE_ETIMEDOUT);
		    goto on_return;
		}
    }

    ret = 0;

on_return:
	if (srv2)
		udp_echo_srv_destroy(srv2);
	if (srv1)
		udp_echo_srv_destroy(srv1);
	if (ioqueue)
		bioqueue_destroy(ioqueue);
	if (pool)
		bpool_release(pool);

	return ret;
}



#define SIGNATURE   0xdeadbeef
struct tcp_pkt
{
	buint32_t		signature;
	buint32_t		seq;
	char			fill[513];
};

struct tcp_state
{
	bbool_t			err;
	bbool_t			sent;
	buint32_t		nbrecv_seq;
	buint8_t		pkt[600];
};

struct send_key
{
    bioqueue_op_key_t	op_key;
};


static bbool_t __tcpOnDataRead(bactivesock_t *asock, void *data, bsize_t size, bstatus_t status, bsize_t *remainder)
{
	struct tcp_state *st = (struct tcp_state*) bactivesock_get_user_data(asock);
	char *next = (char*) data;

	BASE_INFO("TCP on data read");
	if (status != BASE_SUCCESS && status != BASE_EPENDING)
	{
		BASE_CRIT("err: status=%d", status);
		st->err = BASE_TRUE;
		return BASE_FALSE;
	}

	while (size >= sizeof(struct tcp_pkt))
	{
		struct tcp_pkt *tcp_pkt = (struct tcp_pkt*) next;

		if (tcp_pkt->signature != SIGNATURE)
		{
			BASE_CRIT("   err: invalid signature at seq=%d", st->nbrecv_seq);
			st->err = BASE_TRUE;
			return BASE_FALSE;
		}

		if (tcp_pkt->seq != st->nbrecv_seq)
		{
			BASE_CRIT("   err: wrong sequence");
			st->err = BASE_TRUE;
			return BASE_FALSE;
		}

		st->nbrecv_seq++;
		next += sizeof(struct tcp_pkt);
		size -= sizeof(struct tcp_pkt);
	}

	if (size)
	{
		bmemmove(data, next, size);
		*remainder = size;
	}

	return BASE_TRUE;
}

static bbool_t __tcpOnDataSent(bactivesock_t *asock, bioqueue_op_key_t *op_key, bssize_t sent)
{
    struct tcp_state *st=(struct tcp_state*)bactivesock_get_user_data(asock);

	BASE_INFO("TCP on data sent");
    BASE_UNUSED_ARG(op_key);

    st->sent = 1;
    if (sent < 1)
	{
		st->err = BASE_TRUE;
		return BASE_FALSE;
    }

    return BASE_TRUE;
}

static int _tcpPerfTest(void)
{
    enum { COUNT=10/*000*/ };
    bpool_t *pool = NULL;
    bioqueue_t *ioqueue = NULL;
    bsock_t sockSvr=BASE_INVALID_SOCKET, sockClient =BASE_INVALID_SOCKET;
    bactivesock_t *asockSvr = NULL, *asockClient = NULL;
    bactivesock_cb cb;
    struct tcp_state *stateSvr, *stateClient;
    unsigned i;
    bstatus_t status;

    pool = bpool_create(mem, "tcpperf", 256, 256, NULL);

    status = app_socketpair(bAF_INET(), bSOCK_STREAM(), 0, &sockSvr, &sockClient);
    if (status != BASE_SUCCESS)
	{
		status = -100;
		goto on_return;
    }

    status = bioqueue_create(pool, 4, &ioqueue);
    if (status != BASE_SUCCESS)
	{
		status = -110;
		goto on_return;
    }

    bbzero(&cb, sizeof(cb));
    cb.on_data_read = &__tcpOnDataRead;/* for service*/
    cb.on_data_sent = &__tcpOnDataSent; /* for client */

    stateSvr = BASE_POOL_ZALLOC_T(pool, struct tcp_state);
	/* create active socket for Svr socket on same ioqueue */
    status = bactivesock_create(pool, sockSvr, bSOCK_STREAM(), NULL, ioqueue, &cb, stateSvr, &asockSvr);
    if (status != BASE_SUCCESS)
	{
		status = -120;
		goto on_return;
    }

    stateClient = BASE_POOL_ZALLOC_T(pool, struct tcp_state);
    status = bactivesock_create(pool, sockClient, bSOCK_STREAM(), NULL, ioqueue, &cb, stateClient, &asockClient);
    if (status != BASE_SUCCESS)
	{
		status = -130;
		goto on_return;
    }

	/* start async op of service, wait to to called. No op_key handler is needed */
    status = bactivesock_start_read(asockSvr, pool, 1000, 0);
    if (status != BASE_SUCCESS)
	{
		status = -140;
		goto on_return;
    }

    /* Send packet as quickly as possible */
    for (i=0; i<COUNT && !stateSvr->err && !stateClient->err; ++i)
	{
		struct tcp_pkt *pkt;
		struct send_key send_key[2], *op_key;
		bssize_t len;

		pkt = (struct tcp_pkt*)stateClient->pkt;
		pkt->signature = SIGNATURE;
		pkt->seq = i;
		bmemset(pkt->fill, 'a', sizeof(pkt->fill));

		op_key = &send_key[i%2];
		bioqueue_op_key_init(&op_key->op_key, sizeof(*op_key));

		stateClient->sent = BASE_FALSE;
		len = sizeof(*pkt);
		
		BASE_INFO("TCP send#%d: %d bytes..", i, len);
		status = bactivesock_send(asockClient, &op_key->op_key, pkt, &len, 0);
		if (status == BASE_EPENDING)
		{
			BASE_INFO("   TCP send#%d Pending", i);
	    	do
			{
#if BASE_SYMBIAN
				bsymbianos_poll(-1, -1);
#else
				bioqueue_poll(ioqueue, NULL);
#endif
	    	} while (!stateClient->sent);
		}
		else
		{
#if BASE_SYMBIAN
			/* The Symbian socket always returns BASE_SUCCESS for TCP send,
			 * eventhough the remote end hasn't received the data yet.
			 * If we continue sending, eventually send() will block,
			 * possibly because the send buffer is full. So we need to
			 * poll the ioqueue periodically, to let receiver gets the 
			 * data.
			 */
			bsymbianos_poll(-1, 0);
#endif
			if (status != BASE_SUCCESS)
			{
				BASE_INFO("   TCP send#%d error", i);
		    	BASE_CRIT("   err: send status=%d", status);
			    status = -180;
			    break;
			}
			else if (status == BASE_SUCCESS)
			{
				BASE_INFO("   TCP send#%d OK", i);
		    	if (len != sizeof(*pkt))
				{
					BASE_CRIT("   err: shouldn't report partial sent");
					status = -190;
					break;
			    }
			}
		}

		for (;;)
		{
		    btime_val timeout = {0, 10};
	    	if (bioqueue_poll(ioqueue, &timeout) < 1)
	    	{
	    		BTRACE();
				break;
	    	}
		}
    }


    /* Wait until everything has been sent/received */
    if (stateSvr->nbrecv_seq < COUNT)
	{
#ifdef BASE_SYMBIAN
		while (bsymbianos_poll(-1, 1000) == BASE_TRUE)
	    ;
#else
		btime_val delay = {0, 100};
		while (bioqueue_poll(ioqueue, &delay) > 0)
		    ;
#endif
    }

    if (status == BASE_EPENDING)
		status = BASE_SUCCESS;

    if (status != 0)
		goto on_return;

    if (stateSvr->err)
	{
		status = -183;
		goto on_return;
    }
	
    if (stateClient->err)
	{
		status = -186;
		goto on_return;
    }
	
    if (stateSvr->nbrecv_seq != COUNT)
	{
		BASE_ERROR("   err: only %u packets received, expecting %u", stateSvr->nbrecv_seq, COUNT);
		status = -195;
		goto on_return;
    }

on_return:
    if (asockClient)
		bactivesock_close(asockClient);

	if (asockSvr)
		bactivesock_close(asockSvr);
	
    if (ioqueue)
		bioqueue_destroy(ioqueue);
	
    if (pool)
		bpool_release(pool);

    return status;
}

int activesock_test(void)
{
    int ret;

    BASE_INFO("..udp ping/pong test");
    ret = udp_ping_pong_test();
    if (ret != 0)
		return ret;

    BASE_INFO("..tcp perf test");
    ret = _tcpPerfTest();
    if (ret != 0)
    {
    	BASE_ERROR("TCP perf failed: %d", ret);
		return ret;
    }

    return 0;
}

#else
int dummy_active_sock_test;
#endif

