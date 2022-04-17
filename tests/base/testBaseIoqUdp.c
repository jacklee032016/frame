/* 
 *
 */
#include "testBaseTest.h"


/**
 * \page page_baselib_ioqueue_udp_test Test: I/O Queue (UDP)
 *
 * This file provides implementation to test the
 * functionality of the I/O queue when UDP socket is used.
 */


#if INCLUDE_UDP_IOQUEUE_TEST

#include <libBase.h>

#include <compat/socket.h>

#define PORT		    51233
#define LOOP		    2
///#define LOOP		    2
#define BUF_MIN_SIZE	    32
#define BUF_MAX_SIZE	    2048
#define SOCK_INACTIVE_MIN   (1)
#define SOCK_INACTIVE_MAX   (BASE_IOQUEUE_MAX_HANDLES - 2)
#define POOL_SIZE	    (2*BUF_MAX_SIZE + SOCK_INACTIVE_MAX*128 + 2048)



static bssize_t            callback_read_size,
                             callback_write_size,
                             callback_accept_status,
                             callback_connect_status;
static bioqueue_key_t     *callback_read_key,
                            *callback_write_key,
                            *callback_accept_key,
                            *callback_connect_key;
static bioqueue_op_key_t  *callback_read_op,
                            *callback_write_op,
                            *callback_accept_op;

static void on_ioqueue_read(bioqueue_key_t *key, bioqueue_op_key_t *op_key, bssize_t bytes_read)
{
	callback_read_key = key;
	callback_read_op = op_key;
	callback_read_size = bytes_read;
	MTRACE( "     callback_read_key = %p, bytes=%d", key, bytes_read);
}

static void on_ioqueue_write(bioqueue_key_t *key, 
                             bioqueue_op_key_t *op_key,
                             bssize_t bytes_written)
{
    callback_write_key = key;
    callback_write_op = op_key;
    callback_write_size = bytes_written;
}

static void on_ioqueue_accept(bioqueue_key_t *key, 
                              bioqueue_op_key_t *op_key,
                              bsock_t sock, int status)
{
    BASE_UNUSED_ARG(sock);
    callback_accept_key = key;
    callback_accept_op = op_key;
    callback_accept_status = status;
}

static void on_ioqueue_connect(bioqueue_key_t *key, int status)
{
    callback_connect_key = key;
    callback_connect_status = status;
}

static bioqueue_callback test_cb = 
{
    &on_ioqueue_read,
    &on_ioqueue_write,
    &on_ioqueue_accept,
    &on_ioqueue_connect,
};

#if defined(BASE_WIN32) || defined(BASE_WIN64)
#  define S_ADDR S_un.S_addr
#else
#  define S_ADDR s_addr
#endif

/*
 * compliance_test()
 * To test that the basic IOQueue functionality works. It will just exchange data between two sockets.
 */ 
static int compliance_test(bbool_t allow_concur)
{
    bsock_t ssock=-1, csock=-1;
    bsockaddr_in addr, dst_addr;
    int addrlen;
    bpool_t *pool = NULL;
    char *send_buf, *recv_buf;
    bioqueue_t *ioque = NULL;
    bioqueue_key_t *skey = NULL, *ckey = NULL;
    bioqueue_op_key_t read_op, write_op;
    int bufsize = BUF_MIN_SIZE;
    bssize_t bytes;
    int status = -1;
    bstr_t temp;
    bbool_t send_pending, recv_pending;
    bstatus_t rc;

    bset_os_error(BASE_SUCCESS);

    // Create pool.
    pool = bpool_create(mem, NULL, POOL_SIZE, 4000, NULL);

    // Allocate buffers for send and receive.
    send_buf = (char*)bpool_alloc(pool, bufsize);
    recv_buf = (char*)bpool_alloc(pool, bufsize);

    // Allocate sockets for sending and receiving.
    MTRACE( "creating sockets..." );
    rc = bsock_socket(bAF_INET(), bSOCK_DGRAM(), 0, &ssock);
    if (rc==BASE_SUCCESS)
        rc = bsock_socket(bAF_INET(), bSOCK_DGRAM(), 0, &csock);
    else
        csock = BASE_INVALID_SOCKET;
    if (rc != BASE_SUCCESS) {
        app_perror("...ERROR in bsock_socket()", rc);
	status=-1; goto on_error;
    }

    // Bind server socket.
    MTRACE("bind socket...");
    bbzero(&addr, sizeof(addr));
    addr.sin_family = bAF_INET();
    addr.sin_port = bhtons(PORT);
    if (bsock_bind(ssock, &addr, sizeof(addr))) {
	status=-10; goto on_error;
    }

    // Create I/O Queue.
    MTRACE("create ioqueue...");
    rc = bioqueue_create(pool, BASE_IOQUEUE_MAX_HANDLES, &ioque);
    if (rc != BASE_SUCCESS) {
	status=-20; goto on_error;
    }

    // Set concurrency
    MTRACE("set concurrency...");
    rc = bioqueue_set_default_concurrency(ioque, allow_concur);
    if (rc != BASE_SUCCESS) {
	status=-21; goto on_error;
    }

    // Register server and client socket.
    // We put this after inactivity socket, hopefully this can represent the
    // worst waiting time.
    MTRACE("registering first sockets...");
    rc = bioqueue_register_sock(pool, ioque, ssock, NULL, 
			          &test_cb, &skey);
    if (rc != BASE_SUCCESS) {
	app_perror("...error(10): ioqueue_register error", rc);
	status=-25; goto on_error;
    }
    MTRACE("registering second sockets...");
    rc = bioqueue_register_sock( pool, ioque, csock, NULL, 
			           &test_cb, &ckey);
    if (rc != BASE_SUCCESS) {
	app_perror("...error(11): ioqueue_register error", rc);
	status=-26; goto on_error;
    }

    // Randomize send_buf.
    bcreate_random_string(send_buf, bufsize);

    // Init operation keys.
    bioqueue_op_key_init(&read_op, sizeof(read_op));
    bioqueue_op_key_init(&write_op, sizeof(write_op));

    // Register reading from ioqueue.
    MTRACE("start recvfrom...");
    bbzero(&addr, sizeof(addr));
    addrlen = sizeof(addr);
    bytes = bufsize;
    rc = bioqueue_recvfrom(skey, &read_op, recv_buf, &bytes, 0,
			     &addr, &addrlen);
    if (rc != BASE_SUCCESS && rc != BASE_EPENDING) {
        app_perror("...error: bioqueue_recvfrom", rc);
	status=-28; goto on_error;
    } else if (rc == BASE_EPENDING) {
	recv_pending = 1;
	BASE_INFO("......ok: recvfrom returned pending");
    } else {
	BASE_ERROR("......error: recvfrom returned immediate ok!");
	status=-29; goto on_error;
    }

    // Set destination address to send the packet.
    MTRACE("set destination address...");
    temp = bstr("127.0.0.1");
    if ((rc=bsockaddr_in_init(&dst_addr, &temp, PORT)) != 0) {
	app_perror("...error: unable to resolve 127.0.0.1", rc);
	status=-290; goto on_error;
    }

    // Write must return the number of bytes.
    MTRACE("start sendto...");
    bytes = bufsize;
    rc = bioqueue_sendto(ckey, &write_op, send_buf, &bytes, 0, &dst_addr, 
			   sizeof(dst_addr));
    if (rc != BASE_SUCCESS && rc != BASE_EPENDING) {
        app_perror("...error: bioqueue_sendto", rc);
	status=-30; goto on_error;
    } else if (rc == BASE_EPENDING) {
	send_pending = 1;
	BASE_INFO("......ok: sendto returned pending");
    } else {
	send_pending = 0;
	BASE_INFO("......ok: sendto returned immediate success");
    }

    // reset callback variables.
    callback_read_size = callback_write_size = 0;
    callback_accept_status = callback_connect_status = -2;
    callback_read_key = callback_write_key = 
        callback_accept_key = callback_connect_key = NULL;
    callback_read_op = callback_write_op = NULL;

    // Poll if pending.
    while (send_pending || recv_pending) {
	int ret;
	btime_val timeout = { 5, 0 };

	MTRACE("poll...");
#ifdef BASE_SYMBIAN
	ret = bsymbianos_poll(-1, BASE_TIME_VAL_MSEC(timeout));
#else
	ret = bioqueue_poll(ioque, &timeout);
#endif

	if (ret == 0) {
	    BASE_CRIT("...ERROR: timed out...");
	    status=-45;
		goto on_error;
        } else if (ret < 0) {
            app_perror("...ERROR in ioqueue_poll()", -ret);
	    status=-50;
		goto on_error;
	}

	if (callback_read_key != NULL) {
            if (callback_read_size != bufsize) {
                status=-61; goto on_error;
            }
            if (callback_read_key != skey) {
                status=-65; goto on_error;
            }
            if (callback_read_op != &read_op) {
                status=-66; goto on_error;
            }

	    if (bmemcmp(send_buf, recv_buf, bufsize) != 0) {
		status=-67; goto on_error;
	    }
	    if (addrlen != sizeof(bsockaddr_in)) {
		status=-68; goto on_error;
	    }
	    if (addr.sin_family != bAF_INET()) {
		status=-69; goto on_error;
	    }


	    recv_pending = 0;
	} 

        if (callback_write_key != NULL) {
            if (callback_write_size != bufsize) {
                status=-73; goto on_error;
            }
            if (callback_write_key != ckey) {
                status=-75; goto on_error;
            }
            if (callback_write_op != &write_op) {
                status=-76; goto on_error;
            }

            send_pending = 0;
	}
    } 
    
    // Success
    status = 0;

on_error:
    if (skey)
    	bioqueue_unregister(skey);
    else if (ssock != -1)
	bsock_close(ssock);
    
    if (ckey)
    	bioqueue_unregister(ckey);
    else if (csock != -1)
	bsock_close(csock);
    
    if (ioque != NULL)
	bioqueue_destroy(ioque);
    bpool_release(pool);
    return status;

}


static void on_read_complete(bioqueue_key_t *key, 
                             bioqueue_op_key_t *op_key, 
                             bssize_t bytes_read)
{
    unsigned *p_packet_cnt = (unsigned*) bioqueue_get_user_data(key);

    BASE_UNUSED_ARG(op_key);
    BASE_UNUSED_ARG(bytes_read);

    (*p_packet_cnt)++;
}

/*
 * unregister_test()
 * Check if callback is still called after socket has been unregistered or 
 * closed.
 */ 
static int unregister_test(bbool_t allow_concur)
{
    enum { RPORT = 50000, SPORT = 50001 };
    bpool_t *pool;
    bioqueue_t *ioqueue;
    bsock_t ssock;
    bsock_t rsock;
    int addrlen;
    bsockaddr_in addr;
    bioqueue_key_t *key;
    bioqueue_op_key_t opkey;
    bioqueue_callback cb;
    unsigned packet_cnt;
    char sendbuf[10], recvbuf[10];
    bssize_t bytes;
    btime_val timeout;
    bstatus_t status;

    pool = bpool_create(mem, "test", 4000, 4000, NULL);
    if (!pool) {
	app_perror("Unable to create pool", BASE_ENOMEM);
	return -100;
    }

    status = bioqueue_create(pool, 16, &ioqueue);
    if (status != BASE_SUCCESS) {
	app_perror("Error creating ioqueue", status);
	return -110;
    }

    // Set concurrency
    MTRACE("set concurrency...");
    status = bioqueue_set_default_concurrency(ioqueue, allow_concur);
    if (status != BASE_SUCCESS) {
	return -112;
    }

    /* Create sender socket */
    status = app_socket(bAF_INET(), bSOCK_DGRAM(), 0, SPORT, &ssock);
    if (status != BASE_SUCCESS) {
	app_perror("Error initializing socket", status);
	return -120;
    }

    /* Create receiver socket. */
    status = app_socket(bAF_INET(), bSOCK_DGRAM(), 0, RPORT, &rsock);
    if (status != BASE_SUCCESS) {
	app_perror("Error initializing socket", status);
	return -130;
    }

    /* Register rsock to ioqueue. */
    bbzero(&cb, sizeof(cb));
    cb.on_read_complete = &on_read_complete;
    packet_cnt = 0;
    status = bioqueue_register_sock(pool, ioqueue, rsock, &packet_cnt,
				      &cb, &key);
    if (status != BASE_SUCCESS) {
	app_perror("Error registering to ioqueue", status);
	return -140;
    }

    /* Init operation key. */
    bioqueue_op_key_init(&opkey, sizeof(opkey));

    /* Start reading. */
    bytes = sizeof(recvbuf);
    status = bioqueue_recv( key, &opkey, recvbuf, &bytes, 0);
    if (status != BASE_EPENDING) {
	app_perror("Expecting BASE_EPENDING, but got this", status);
	return -150;
    }

    /* Init destination address. */
    addrlen = sizeof(addr);
    status = bsock_getsockname(rsock, &addr, &addrlen);
    if (status != BASE_SUCCESS) {
	app_perror("getsockname error", status);
	return -160;
    }

    /* Override address with 127.0.0.1, since getsockname will return
     * zero in the address field.
     */
    addr.sin_addr = binet_addr2("127.0.0.1");

    /* Init buffer to send */
    bansi_strcpy(sendbuf, "Hello0123");

    /* Send one packet. */
    bytes = sizeof(sendbuf);
    status = bsock_sendto(ssock, sendbuf, &bytes, 0,
			    &addr, sizeof(addr));

    if (status != BASE_SUCCESS) {
	app_perror("sendto error", status);
	return -170;
    }

    /* Check if packet is received. */
    timeout.sec = 1; timeout.msec = 0;
#ifdef BASE_SYMBIAN
    bsymbianos_poll(-1, 1000);
#else
    bioqueue_poll(ioqueue, &timeout);
#endif

    if (packet_cnt != 1) {
	return -180;
    }

    /* Just to make sure things are settled.. */
    bthreadSleepMs(100);

    /* Start reading again. */
    bytes = sizeof(recvbuf);
    status = bioqueue_recv( key, &opkey, recvbuf, &bytes, 0);
    if (status != BASE_EPENDING) {
	app_perror("Expecting BASE_EPENDING, but got this", status);
	return -190;
    }

    /* Reset packet counter */
    packet_cnt = 0;

    /* Send one packet. */
    bytes = sizeof(sendbuf);
    status = bsock_sendto(ssock, sendbuf, &bytes, 0,
			    &addr, sizeof(addr));

    if (status != BASE_SUCCESS) {
	app_perror("sendto error", status);
	return -200;
    }

    /* Now unregister and close socket. */
    bioqueue_unregister(key);

    /* Poll ioqueue. */
#ifdef BASE_SYMBIAN
    bsymbianos_poll(-1, 1000);
#else
    timeout.sec = 1; timeout.msec = 0;
    bioqueue_poll(ioqueue, &timeout);
#endif

    /* Must NOT receive any packets after socket is closed! */
    if (packet_cnt > 0) {
	BASE_ERROR("....errror: not expecting to receive packet after socket has been closed");
	return -210;
    }

    /* Success */
    bsock_close(ssock);
    bioqueue_destroy(ioqueue);

    bpool_release(pool);

    return 0;
}


/*
 * Testing with many handles.
 * This will just test registering BASE_IOQUEUE_MAX_HANDLES count
 * of sockets to the ioqueue.
 */
static int many_handles_test(bbool_t allow_concur)
{
    enum { MAX = BASE_IOQUEUE_MAX_HANDLES };
    bpool_t *pool;
    bioqueue_t *ioqueue;
    bsock_t *sock;
    bioqueue_key_t **key;
    bstatus_t rc;
    int count, i; /* must be signed */

    BASE_INFO("...testing with so many handles");

    pool = bpool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool)
	return BASE_ENOMEM;

    key = (bioqueue_key_t**) 
    	  bpool_alloc(pool, MAX*sizeof(bioqueue_key_t*));
    sock = (bsock_t*) bpool_alloc(pool, MAX*sizeof(bsock_t));
    
    /* Create IOQueue */
    rc = bioqueue_create(pool, MAX, &ioqueue);
    if (rc != BASE_SUCCESS || ioqueue == NULL) {
	app_perror("...error in bioqueue_create", rc);
	return -10;
    }

    // Set concurrency
    rc = bioqueue_set_default_concurrency(ioqueue, allow_concur);
    if (rc != BASE_SUCCESS) {
	return -11;
    }

    /* Register as many sockets. */
    for (count=0; count<MAX; ++count) {
	sock[count] = BASE_INVALID_SOCKET;
	rc = bsock_socket(bAF_INET(), bSOCK_DGRAM(), 0, &sock[count]);
	if (rc != BASE_SUCCESS || sock[count] == BASE_INVALID_SOCKET) {
	    BASE_ERROR("....unable to create %d-th socket, rc=%d", count, rc);
	    break;
	}
	key[count] = NULL;
	rc = bioqueue_register_sock(pool, ioqueue, sock[count],
				      NULL, &test_cb, &key[count]);
	if (rc != BASE_SUCCESS || key[count] == NULL) {
	    BASE_ERROR("....unable to register %d-th socket, rc=%d", count, rc);
	    return -30;
	}
    }

    /* Test complete. */

    /* Now deregister and close all handles. */ 

    /* NOTE for RTEMS:
     *  It seems that the order of close(sock) is pretty important here.
     *  If we close the sockets with the same order as when they were created,
     *  RTEMS doesn't seem to reuse the sockets, thus next socket created
     *  will have descriptor higher than the last socket created.
     *  If we close the sockets in the reverse order, then the descriptor will
     *  get reused.
     *  This used to cause problem with select ioqueue, since the ioqueue
     *  always gives FD_SETSIZE for the first select() argument. This ioqueue
     *  behavior can be changed with setting BASE_SELECT_NEEDS_NFDS macro.
     */
    for (i=count-1; i>=0; --i) {
    ///for (i=0; i<count; ++i) {
	rc = bioqueue_unregister(key[i]);
	if (rc != BASE_SUCCESS) {
	    app_perror("...error in bioqueue_unregister", rc);
	}
    }

    rc = bioqueue_destroy(ioqueue);
    if (rc != BASE_SUCCESS) {
	app_perror("...error in bioqueue_destroy", rc);
    }
    
    bpool_release(pool);

    BASE_INFO("....many_handles_test() ok");

    return 0;
}

/*
 * Multi-operation test.
 */

/*
 * Benchmarking IOQueue
 */
static int bench_test(bbool_t allow_concur, int bufsize, int inactive_sock_count)
{
    bsock_t ssock=-1, csock=-1;
    bsockaddr_in addr;
    bpool_t *pool = NULL;
    bsock_t *inactive_sock=NULL;
    bioqueue_op_key_t *inactive_read_op;
    char *send_buf, *recv_buf;
    bioqueue_t *ioque = NULL;
    bioqueue_key_t *skey, *ckey, *keys[SOCK_INACTIVE_MAX+2];
    btimestamp t1, t2, t_elapsed;
    int rc=0, i;    /* i must be signed */
    bstr_t temp;
    char errbuf[BASE_ERR_MSG_SIZE];

    MTRACE("   bench test %d", inactive_sock_count);

    // Create pool.
    pool = bpool_create(mem, NULL, POOL_SIZE, 4000, NULL);

    // Allocate buffers for send and receive.
    send_buf = (char*)bpool_alloc(pool, bufsize);
    recv_buf = (char*)bpool_alloc(pool, bufsize);

    // Allocate sockets for sending and receiving.
    rc = bsock_socket(bAF_INET(), bSOCK_DGRAM(), 0, &ssock);
    if (rc == BASE_SUCCESS) {
        rc = bsock_socket(bAF_INET(), bSOCK_DGRAM(), 0, &csock);
    } else
        csock = BASE_INVALID_SOCKET;
    if (rc != BASE_SUCCESS) {
	app_perror("...error: bsock_socket()", rc);
	goto on_error;
    }

    // Bind server socket.
    bbzero(&addr, sizeof(addr));
    addr.sin_family = bAF_INET();
    addr.sin_port = bhtons(PORT);
    if (bsock_bind(ssock, &addr, sizeof(addr)))
	goto on_error;

    bassert(inactive_sock_count+2 <= BASE_IOQUEUE_MAX_HANDLES);

    // Create I/O Queue.
    rc = bioqueue_create(pool, BASE_IOQUEUE_MAX_HANDLES, &ioque);
    if (rc != BASE_SUCCESS) {
	app_perror("...error: bioqueue_create()", rc);
	goto on_error;
    }

    // Set concurrency
    rc = bioqueue_set_default_concurrency(ioque, allow_concur);
    if (rc != BASE_SUCCESS) {
	app_perror("...error: bioqueue_set_default_concurrency()", rc);
	goto on_error;
    }

    // Allocate inactive sockets, and bind them to some arbitrary address.
    // Then register them to the I/O queue, and start a read operation.
    inactive_sock = (bsock_t*)bpool_alloc(pool, 
				    inactive_sock_count*sizeof(bsock_t));
    inactive_read_op = (bioqueue_op_key_t*)bpool_alloc(pool,
                              inactive_sock_count*sizeof(bioqueue_op_key_t));
    bbzero(&addr, sizeof(addr));
    addr.sin_family = bAF_INET();
    for (i=0; i<inactive_sock_count; ++i) {
        bssize_t bytes;

	rc = bsock_socket(bAF_INET(), bSOCK_DGRAM(), 0, &inactive_sock[i]);
	if (rc != BASE_SUCCESS || inactive_sock[i] < 0) {
	    app_perror("...error: bsock_socket()", rc);
	    goto on_error;
	}
	if ((rc=bsock_bind(inactive_sock[i], &addr, sizeof(addr))) != 0) {
	    bsock_close(inactive_sock[i]);
	    inactive_sock[i] = BASE_INVALID_SOCKET;
	    app_perror("...error: bsock_bind()", rc);
	    goto on_error;
	}
	rc = bioqueue_register_sock(pool, ioque, inactive_sock[i], 
			              NULL, &test_cb, &keys[i]);
	if (rc != BASE_SUCCESS) {
	    bsock_close(inactive_sock[i]);
	    inactive_sock[i] = BASE_INVALID_SOCKET;
	    app_perror("...error(1): bioqueue_register_sock()", rc);
	    BASE_ERROR("....i=%d", i);
	    goto on_error;
	}
        bytes = bufsize;
        bioqueue_op_key_init(&inactive_read_op[i],
        		       sizeof(inactive_read_op[i]));
	rc = bioqueue_recv(keys[i], &inactive_read_op[i], recv_buf, &bytes, 0);
	if (rc != BASE_EPENDING) {
	    bsock_close(inactive_sock[i]);
	    inactive_sock[i] = BASE_INVALID_SOCKET;
	    app_perror("...error: bioqueue_read()", rc);
	    goto on_error;
	}
    }

    // Register server and client socket.
    // We put this after inactivity socket, hopefully this can represent the
    // worst waiting time.
    rc = bioqueue_register_sock(pool, ioque, ssock, NULL, 
			          &test_cb, &skey);
    if (rc != BASE_SUCCESS) {
	app_perror("...error(2): bioqueue_register_sock()", rc);
	goto on_error;
    }

    rc = bioqueue_register_sock(pool, ioque, csock, NULL, 
			          &test_cb, &ckey);
    if (rc != BASE_SUCCESS) {
	app_perror("...error(3): bioqueue_register_sock()", rc);
	goto on_error;
    }

    // Set destination address to send the packet.
    bsockaddr_in_init(&addr, bcstr(&temp, "127.0.0.1"), PORT);

    // Test loop.
    t_elapsed.u64 = 0;
    for (i=0; i<LOOP; ++i) {
	bssize_t bytes;
        bioqueue_op_key_t read_op, write_op;

	// Randomize send buffer.
	bcreate_random_string(send_buf, bufsize);

        // Init operation keys.
        bioqueue_op_key_init(&read_op, sizeof(read_op));
        bioqueue_op_key_init(&write_op, sizeof(write_op));

	// Start reading on the server side.
        bytes = bufsize;
	rc = bioqueue_recv(skey, &read_op, recv_buf, &bytes, 0);
	if (rc != BASE_EPENDING) {
	    app_perror("...error: bioqueue_read()", rc);
	    break;
	}

	// Starts send on the client side.
        bytes = bufsize;
	rc = bioqueue_sendto(ckey, &write_op, send_buf, &bytes, 0,
			       &addr, sizeof(addr));
	if (rc != BASE_SUCCESS && rc != BASE_EPENDING) {
	    app_perror("...error: bioqueue_write()", rc);
	    break;
	}
	if (rc == BASE_SUCCESS) {
	    if (bytes < 0) {
		app_perror("...error: bioqueue_sendto()",(bstatus_t)-bytes);
		break;
	    }
	}

	// Begin time.
	bTimeStampGet(&t1);

	// Poll the queue until we've got completion event in the server side.
        callback_read_key = NULL;
        callback_read_size = 0;
	MTRACE("     waiting for key = %p", skey);
	do {
	    btime_val timeout = { 1, 0 };
#ifdef BASE_SYMBIAN
	    rc = bsymbianos_poll(-1, BASE_TIME_VAL_MSEC(timeout));
#else
	    rc = bioqueue_poll(ioque, &timeout);
#endif
	    MTRACE( "     poll rc=%d", rc);
	} while (rc >= 0 && callback_read_key != skey);

	// End time.
	bTimeStampGet(&t2);
	t_elapsed.u64 += (t2.u64 - t1.u64);

	if (rc < 0) {
	    app_perror("   error: bioqueue_poll", -rc);
	    break;
	}

	// Compare recv buffer with send buffer.
	if (callback_read_size != bufsize || 
	    bmemcmp(send_buf, recv_buf, bufsize)) 
	{
	    rc = -10;
	    BASE_ERROR("   error: size/buffer mismatch");
	    break;
	}

	// Poll until all events are exhausted, before we start the next loop.
	do {
	    btime_val timeout = { 0, 10 };
#ifdef BASE_SYMBIAN
	    BASE_UNUSED_ARG(timeout);
	    rc = bsymbianos_poll(-1, 100);
#else	    
	    rc = bioqueue_poll(ioque, &timeout);
#endif
	} while (rc>0);

	rc = 0;
    }

    // Print results
    if (rc == 0)
	{
		btimestamp tzero;
		buint32_t usec_delay;

		tzero.u32.hi = tzero.u32.lo = 0;
		usec_delay = belapsed_usec( &tzero, &t_elapsed);

		BASE_INFO("...%10d %15d  % 9d", bufsize, inactive_sock_count, usec_delay);
    }
	else
	{
		BASE_CRIT("...ERROR rc=%d (buf:%d, fds:%d)", rc, bufsize, inactive_sock_count+2);
    }

    // Cleaning up.
    for (i=inactive_sock_count-1; i>=0; --i)
	{
		bioqueue_unregister(keys[i]);
    }

    bioqueue_unregister(skey);
    bioqueue_unregister(ckey);


    bioqueue_destroy(ioque);
    bpool_release( pool);
    return rc;

on_error:
    BASE_CRIT("...ERROR: %s", extStrError(bget_netos_error(), errbuf, sizeof(errbuf)));
    if (ssock)
		bsock_close(ssock);

	if (csock)
		bsock_close(csock);
	
    for (i=0; i<inactive_sock_count && inactive_sock && inactive_sock[i]!=BASE_INVALID_SOCKET; ++i) 
    {
		bsock_close(inactive_sock[i]);
    }
    if (ioque != NULL)
		bioqueue_destroy(ioque);
    bpool_release( pool);
    return -1;
}

static int udp_ioqueue_test_imp(bbool_t allow_concur)
{
	int status;
	int bufsize, sock_count;

	BASE_INFO("..testing with concurency=%d", allow_concur);

	//goto pass1;

	BASE_INFO("...compliance test (%s)", bioqueue_name());
	if ((status=compliance_test(allow_concur)) != 0)
	{
		return status;
	}
	BASE_INFO("....compliance test ok");


	BASE_INFO("...unregister test (%s)", bioqueue_name());
	if ((status=unregister_test(allow_concur)) != 0)
	{
		return status;
	}
	BASE_INFO("....unregister test ok");

	if ((status=many_handles_test(allow_concur)) != 0)
	{
		return status;
	}

	//return 0;

	BASE_INFO("...benchmarking different buffer size:");
	BASE_INFO("... note: buf=bytes sent, fds=# of fds, elapsed=in timer ticks");

	//pass1:
	BASE_INFO("...Benchmarking poll times for %s:", bioqueue_name());
	BASE_INFO("...=====================================");
	BASE_INFO("...Buf.size   #inactive-socks  Time/poll");
	BASE_INFO("... (bytes)                    (nanosec)");
	BASE_INFO("...=====================================");

	//goto pass2;

	for (bufsize=BUF_MIN_SIZE; bufsize <= BUF_MAX_SIZE; bufsize *= 2)
	{
		if ((status=bench_test(allow_concur, bufsize, SOCK_INACTIVE_MIN)) != 0)
			return status;
	}
	
	//pass2:
	bufsize = 512;
	for (sock_count=SOCK_INACTIVE_MIN+2; sock_count<=SOCK_INACTIVE_MAX+2; sock_count *= 2) 
	{
		//BASE_INFO( "...testing with %d fds", sock_count));
		if ((status=bench_test(allow_concur, bufsize, sock_count-2)) != 0)
			return status;
	}
	
	return 0;
}

int testBaseUdpIoqueue(void)
{
	int rc;

	rc = udp_ioqueue_test_imp(BASE_TRUE);
	if (rc != 0)
		return rc;

	rc = udp_ioqueue_test_imp(BASE_FALSE);
	if (rc != 0)
		return rc;

	return 0;
}

#else
int dummy_uiq_udp;
#endif


