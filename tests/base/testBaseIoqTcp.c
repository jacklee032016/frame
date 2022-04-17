/* 
 */
#include "testBaseTest.h"

/**
 * \page page_baselib_ioqueue_tcp_test Test: I/O Queue (TCP)
 *
 * This file provides implementation to test the
 * functionality of the I/O queue when TCP socket is used.
 */


#if INCLUDE_TCP_IOQUEUE_TEST

#include <libBase.h>

#if BASE_HAS_TCP

#define NON_EXISTANT_PORT   50123
#define LOOP		    100
#define BUF_MIN_SIZE	    32
#define BUF_MAX_SIZE	    2048
#define SOCK_INACTIVE_MIN   (4-2)
#define SOCK_INACTIVE_MAX   (BASE_IOQUEUE_MAX_HANDLES - 2)
#define POOL_SIZE	    (2*BUF_MAX_SIZE + SOCK_INACTIVE_MAX*128 + 2048)

static bssize_t	     callback_read_size,
                             callback_write_size,
                             callback_accept_status,
                             callback_connect_status;
static unsigned		     callback_call_count;
static bioqueue_key_t     *callback_read_key,
                            *callback_write_key,
                            *callback_accept_key,
                            *callback_connect_key;
static bioqueue_op_key_t  *callback_read_op,
                            *callback_write_op,
                            *callback_accept_op;

static void on_ioqueue_read(bioqueue_key_t *key, 
                            bioqueue_op_key_t *op_key,
                            bssize_t bytes_read)
{
    callback_read_key = key;
    callback_read_op = op_key;
    callback_read_size = bytes_read;
    callback_call_count++;
}

static void on_ioqueue_write(bioqueue_key_t *key, 
                             bioqueue_op_key_t *op_key,
                             bssize_t bytes_written)
{
    callback_write_key = key;
    callback_write_op = op_key;
    callback_write_size = bytes_written;
    callback_call_count++;
}

static void on_ioqueue_accept(bioqueue_key_t *key, 
                              bioqueue_op_key_t *op_key,
                              bsock_t sock, 
                              int status)
{
    if (sock == BASE_INVALID_SOCKET) {

	if (status != BASE_SUCCESS) {
	    /* Ignore. Could be blocking error */
	    app_perror(".....warning: received error in on_ioqueue_accept() callback",
		       status);
	} else {
	    callback_accept_status = -61;
	    BASE_ERROR("..... on_ioqueue_accept() callback was given invalid socket and status is %d", status);
	}
    } else {
        bsockaddr addr;
        int client_addr_len;

        client_addr_len = sizeof(addr);
        status = bsock_getsockname(sock, &addr, &client_addr_len);
        if (status != BASE_SUCCESS) {
            app_perror("...ERROR in bsock_getsockname()", status);
        }

	callback_accept_key = key;
	callback_accept_op = op_key;
	callback_accept_status = status;
	callback_call_count++;
    }
}

static void on_ioqueue_connect(bioqueue_key_t *key, int status)
{
    callback_connect_key = key;
    callback_connect_status = status;
    callback_call_count++;
}

static bioqueue_callback test_cb = 
{
    &on_ioqueue_read,
    &on_ioqueue_write,
    &on_ioqueue_accept,
    &on_ioqueue_connect,
};

static int send_recv_test(bioqueue_t *ioque,
			  bioqueue_key_t *skey,
			  bioqueue_key_t *ckey,
			  void *send_buf,
			  void *recv_buf,
			  bssize_t bufsize,
			  btimestamp *t_elapsed)
{
    bstatus_t status;
    bssize_t bytes;
    btime_val timeout;
    btimestamp t1, t2;
    int pending_op = 0;
    bioqueue_op_key_t read_op, write_op;

    // Init operation keys.
    bioqueue_op_key_init(&read_op, sizeof(read_op));
    bioqueue_op_key_init(&write_op, sizeof(write_op));

    // Start reading on the server side.
    bytes = bufsize;
    status = bioqueue_recv(skey, &read_op, recv_buf, &bytes, 0);
    if (status != BASE_SUCCESS && status != BASE_EPENDING) {
        app_perror("...bioqueue_recv error", status);
	return -100;
    }
    
    if (status == BASE_EPENDING)
        ++pending_op;
    else {
        /* Does not expect to return error or immediate data. */
        return -115;
    }

    // Randomize send buffer.
    bcreate_random_string((char*)send_buf, bufsize);

    // Starts send on the client side.
    bytes = bufsize;
    status = bioqueue_send(ckey, &write_op, send_buf, &bytes, 0);
    if (status != BASE_SUCCESS && bytes != BASE_EPENDING) {
	return -120;
    }
    if (status == BASE_EPENDING) {
	++pending_op;
    }

    // Begin time.
    bTimeStampGet(&t1);

    // Reset indicators
    callback_read_size = callback_write_size = 0;
    callback_read_key = callback_write_key = NULL;
    callback_read_op = callback_write_op = NULL;

    // Poll the queue until we've got completion event in the server side.
    status = 0;
    while (pending_op > 0) {
        timeout.sec = 1; timeout.msec = 0;
#ifdef BASE_SYMBIAN
	BASE_UNUSED_ARG(ioque);
	status = bsymbianos_poll(-1, 1000);
#else
	status = bioqueue_poll(ioque, &timeout);
#endif
	if (status > 0) {
            if (callback_read_size) {
                if (callback_read_size != bufsize)
                    return -160;
                if (callback_read_key != skey)
                    return -161;
                if (callback_read_op != &read_op)
                    return -162;
            }
            if (callback_write_size) {
                if (callback_write_key != ckey)
                    return -163;
                if (callback_write_op != &write_op)
                    return -164;
            }
	    pending_op -= status;
	}
        if (status == 0) {
            BASE_ERROR("...error: timed out");
        }
	if (status < 0) {
	    return -170;
	}
    }

    // Pending op is zero.
    // Subsequent poll should yield zero too.
    timeout.sec = timeout.msec = 0;
#ifdef BASE_SYMBIAN
    status = bsymbianos_poll(-1, 1);
#else
    status = bioqueue_poll(ioque, &timeout);
#endif
    if (status != 0)
        return -173;

    // End time.
    bTimeStampGet(&t2);
    t_elapsed->u32.lo += (t2.u32.lo - t1.u32.lo);

    // Compare recv buffer with send buffer.
    if (bmemcmp(send_buf, recv_buf, bufsize) != 0) {
	return -180;
    }

    // Success
    return 0;
}


/*
 * Compliance test for success scenario.
 */
static int compliance_test_0(bbool_t allow_concur)
{
    bsock_t ssock=-1, csock0=-1, csock1=-1;
    bsockaddr_in addr, client_addr, rmt_addr;
    int client_addr_len;
    bpool_t *pool = NULL;
    char *send_buf, *recv_buf;
    bioqueue_t *ioque = NULL;
    bioqueue_key_t *skey=NULL, *ckey0=NULL, *ckey1=NULL;
    bioqueue_op_key_t accept_op;
    int bufsize = BUF_MIN_SIZE;
    int status = -1;
    int pending_op = 0;
    btimestamp t_elapsed;
    bstr_t s;
    bstatus_t rc;

    // Create pool.
    pool = bpool_create(mem, NULL, POOL_SIZE, 4000, NULL);

    // Allocate buffers for send and receive.
    send_buf = (char*)bpool_alloc(pool, bufsize);
    recv_buf = (char*)bpool_alloc(pool, bufsize);

    // Create server socket and client socket for connecting
    rc = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, &ssock);
    if (rc != BASE_SUCCESS) {
        app_perror("...error creating socket", rc);
        status=-1; goto on_error;
    }

    rc = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, &csock1);
    if (rc != BASE_SUCCESS) {
        app_perror("...error creating socket", rc);
	status=-1; goto on_error;
    }

    // Bind server socket.
    bsockaddr_in_init(&addr, 0, 0);
    if ((rc=bsock_bind(ssock, &addr, sizeof(addr))) != 0 ) {
        app_perror("...bind error", rc);
	status=-10; goto on_error;
    }

    // Get server address.
    client_addr_len = sizeof(addr);
    rc = bsock_getsockname(ssock, &addr, &client_addr_len);
    if (rc != BASE_SUCCESS) {
        app_perror("...ERROR in bsock_getsockname()", rc);
	status=-15; goto on_error;
    }
    addr.sin_addr = binet_addr(bcstr(&s, "127.0.0.1"));

    // Create I/O Queue.
    rc = bioqueue_create(pool, BASE_IOQUEUE_MAX_HANDLES, &ioque);
    if (rc != BASE_SUCCESS) {
        app_perror("...ERROR in bioqueue_create()", rc);
	status=-20; goto on_error;
    }

    // Init operation key.
    bioqueue_op_key_init(&accept_op, sizeof(accept_op));

    // Concurrency
    rc = bioqueue_set_default_concurrency(ioque, allow_concur);
    if (rc != BASE_SUCCESS) {
        app_perror("...ERROR in bioqueue_set_default_concurrency()", rc);
	status=-21; goto on_error;
    }

    // Register server socket and client socket.
    rc = bioqueue_register_sock(pool, ioque, ssock, NULL, &test_cb, &skey);
    if (rc == BASE_SUCCESS)
        rc = bioqueue_register_sock(pool, ioque, csock1, NULL, &test_cb, 
                                      &ckey1);
    else
        ckey1 = NULL;
    if (rc != BASE_SUCCESS) {
        app_perror("...ERROR in bioqueue_register_sock()", rc);
	status=-23; goto on_error;
    }

    // Server socket listen().
    if (bsock_listen(ssock, 5)) {
        app_perror("...ERROR in bsock_listen()", rc);
	status=-25; goto on_error;
    }

    // Server socket accept()
    client_addr_len = sizeof(bsockaddr_in);
    status = bioqueue_accept(skey, &accept_op, &csock0, 
                               &client_addr, &rmt_addr, &client_addr_len);
    if (status != BASE_EPENDING) {
        app_perror("...ERROR in bioqueue_accept()", rc);
	status=-30; goto on_error;
    }
    if (status==BASE_EPENDING) {
	++pending_op;
    }

    // Client socket connect()
    status = bioqueue_connect(ckey1, &addr, sizeof(addr));
    if (status!=BASE_SUCCESS && status != BASE_EPENDING) {
        app_perror("...ERROR in bioqueue_connect()", rc);
	status=-40; goto on_error;
    }
    if (status==BASE_EPENDING) {
	++pending_op;
    }

    // Poll until connected
    callback_read_size = callback_write_size = 0;
    callback_accept_status = callback_connect_status = -2;
    callback_call_count = 0;

    callback_read_key = callback_write_key = 
        callback_accept_key = callback_connect_key = NULL;
    callback_accept_op = callback_read_op = callback_write_op = NULL;

    while (pending_op) {
	btime_val timeout = {1, 0};

#ifdef BASE_SYMBIAN
	callback_call_count = 0;
	bsymbianos_poll(-1, BASE_TIME_VAL_MSEC(timeout));
	status = callback_call_count;
#else
	status = bioqueue_poll(ioque, &timeout);
#endif
	if (status > 0) {
            if (callback_accept_status != -2) {
                if (callback_accept_status != 0) {
                    status=-41; goto on_error;
                }
                if (callback_accept_key != skey) {
                    status=-42; goto on_error;
                }
                if (callback_accept_op != &accept_op) {
                    status=-43; goto on_error;
                }
                callback_accept_status = -2;
            }

            if (callback_connect_status != -2) {
                if (callback_connect_status != 0) {
                    status=-50; goto on_error;
                }
                if (callback_connect_key != ckey1) {
                    status=-51; goto on_error;
                }
                callback_connect_status = -2;
            }

	    if (status > pending_op) {
		BASE_ERROR("...error: bioqueue_poll() returned %d (only expecting %d)", status, pending_op);
		return -52;
	    }
	    pending_op -= status;

	    if (pending_op == 0) {
		status = 0;
	    }
	}
    }

    // There's no pending operation.
    // When we poll the ioqueue, there must not be events.
    if (pending_op == 0) {
        btime_val timeout = {1, 0};
#ifdef BASE_SYMBIAN
	status = bsymbianos_poll(-1, BASE_TIME_VAL_MSEC(timeout));
#else
        status = bioqueue_poll(ioque, &timeout);
#endif
        if (status != 0) {
            status=-60; goto on_error;
        }
    }

    // Check accepted socket.
    if (csock0 == BASE_INVALID_SOCKET) {
	status = -69;
        app_perror("...accept() error", bget_os_error());
	goto on_error;
    }

    // Register newly accepted socket.
    rc = bioqueue_register_sock(pool, ioque, csock0, NULL, 
                                  &test_cb, &ckey0);
    if (rc != BASE_SUCCESS) {
        app_perror("...ERROR in bioqueue_register_sock", rc);
	status = -70;
	goto on_error;
    }

    // Test send and receive.
    t_elapsed.u32.lo = 0;
    status = send_recv_test(ioque, ckey0, ckey1, send_buf, 
                            recv_buf, bufsize, &t_elapsed);
    if (status != 0) {
	goto on_error;
    }

    // Success
    status = 0;

on_error:
    if (skey != NULL)
    	bioqueue_unregister(skey);
    else if (ssock != BASE_INVALID_SOCKET)
	bsock_close(ssock);
    
    if (ckey1 != NULL)
    	bioqueue_unregister(ckey1);
    else if (csock1 != BASE_INVALID_SOCKET)
	bsock_close(csock1);
    
    if (ckey0 != NULL)
    	bioqueue_unregister(ckey0);
    else if (csock0 != BASE_INVALID_SOCKET)
	bsock_close(csock0);
    
    if (ioque != NULL)
	bioqueue_destroy(ioque);
    bpool_release(pool);
    return status;

}

/*
 * Compliance test for failed scenario.
 * In this case, the client connects to a non-existant service.
 */
static int compliance_test_1(bbool_t allow_concur)
{
	bsock_t csock1=BASE_INVALID_SOCKET;
	bsockaddr_in addr;
	bpool_t *pool = NULL;
	bioqueue_t *ioque = NULL;
	bioqueue_key_t *ckey1 = NULL;
	int status = -1;
	int pending_op = 0;
	bstr_t s;
	bstatus_t rc;

	// Create pool.
	pool = bpool_create(mem, NULL, POOL_SIZE, 4000, NULL);

	// Create I/O Queue.
	rc = bioqueue_create(pool, BASE_IOQUEUE_MAX_HANDLES, &ioque);
	if (!ioque) {
		status=-20; goto on_error;
	}

	// Concurrency
	rc = bioqueue_set_default_concurrency(ioque, allow_concur);
	if (rc != BASE_SUCCESS)
	{
		status=-21; goto on_error;
	}

	// Create client socket
	rc = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, &csock1);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...ERROR in bsock_socket()", rc);
		status=-1; goto on_error;
	}

	// Register client socket.
	rc = bioqueue_register_sock(pool, ioque, csock1, NULL, &test_cb, &ckey1);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...ERROR in bioqueue_register_sock()", rc);
		status=-23; goto on_error;
	}

	// Initialize remote address.
	bsockaddr_in_init(&addr, bcstr(&s, "127.0.0.1"), NON_EXISTANT_PORT);

	// Client socket connect()
	status = bioqueue_connect(ckey1, &addr, sizeof(addr));
	if (status==BASE_SUCCESS)
	{
		// unexpectedly success!
		status = -30;
		goto on_error;
	}
	
	if (status != BASE_EPENDING)
	{
		// success
	}
	else
	{
		++pending_op;
	}

	callback_connect_status = -2;
	callback_connect_key = NULL;

	// Poll until we've got result
	while (pending_op)
	{
		btime_val timeout = {1, 0};

#ifdef BASE_SYMBIAN
		callback_call_count = 0;
		bsymbianos_poll(-1, BASE_TIME_VAL_MSEC(timeout));
		status = callback_call_count;
#else
		status = bioqueue_poll(ioque, &timeout);
#endif
		if (status > 0)
		{
			if (callback_connect_key==ckey1)
			{
				if (callback_connect_status == 0)
				{
					// unexpectedly connected!
					status = -50;
					goto on_error;
				}
			}

			if (status > pending_op)
			{
				BASE_ERROR("...error: bioqueue_poll() returned %d (only expecting %d)", status, pending_op);
				return -552;
			}

			pending_op -= status;
			if (pending_op == 0)
			{
				status = 0;
			}
		}
	}

	// There's no pending operation.
	// When we poll the ioqueue, there must not be events.
	if (pending_op == 0)
	{
		btime_val timeout = {1, 0};
#ifdef BASE_SYMBIAN
		status = bsymbianos_poll(-1, BASE_TIME_VAL_MSEC(timeout));
#else
		status = bioqueue_poll(ioque, &timeout);
#endif
		if (status != 0)
		{
			status=-60; goto on_error;
		}
	}

	// Success
	status = 0;

on_error:
	if (ckey1 != NULL)
		bioqueue_unregister(ckey1);
	else if (csock1 != BASE_INVALID_SOCKET)
		bsock_close(csock1);

	if (ioque != NULL)
		bioqueue_destroy(ioque);
	bpool_release(pool);
	return status;
}


/*
 * Repeated connect/accept on the same listener socket.
 */
static int compliance_test_2(bbool_t allow_concur)
{
#if defined(BASE_SYMBIAN) && BASE_SYMBIAN!=0
	enum { MAX_PAIR = 1, TEST_LOOP = 2 };
#else
	enum { MAX_PAIR = 4, TEST_LOOP = 2 };
#endif

	struct listener
	{
		bsock_t	     sock;
		bioqueue_key_t    *key;
		bsockaddr_in	     addr;
		int		     addr_len;
	} listener;

	struct server
	{
		bsock_t	     sock;
		bioqueue_key_t    *key;
		bsockaddr_in	     local_addr;
		bsockaddr_in	     rem_addr;
		int		     rem_addr_len;
		bioqueue_op_key_t  accept_op;
	} server[MAX_PAIR];

	struct client
	{
		bsock_t	     sock;
		bioqueue_key_t    *key;
	} client[MAX_PAIR];

	bpool_t *pool = NULL;
	char *send_buf, *recv_buf;
	bioqueue_t *ioque = NULL;
	int i, bufsize = BUF_MIN_SIZE;
	int status;
	int test_loop, pending_op = 0;
	btimestamp t_elapsed;
	bstr_t s;
	bstatus_t rc;

	listener.sock = BASE_INVALID_SOCKET;
	listener.key = NULL;

	for (i=0; i<MAX_PAIR; ++i)
	{
		server[i].sock = BASE_INVALID_SOCKET;
		server[i].key = NULL;
	}

	for (i=0; i<MAX_PAIR; ++i)
	{
		client[i].sock = BASE_INVALID_SOCKET;
		client[i].key = NULL;	
	}

	// Create pool.
	pool = bpool_create(mem, NULL, POOL_SIZE, 4000, NULL);


	// Create I/O Queue.
	rc = bioqueue_create(pool, BASE_IOQUEUE_MAX_HANDLES, &ioque);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...ERROR in bioqueue_create()", rc);
		return -10;
	}


	// Concurrency
	rc = bioqueue_set_default_concurrency(ioque, allow_concur);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...ERROR in bioqueue_set_default_concurrency()", rc);
		return -11;
	}

	// Allocate buffers for send and receive.
	send_buf = (char*)bpool_alloc(pool, bufsize);
	recv_buf = (char*)bpool_alloc(pool, bufsize);

	// Create listener socket
	rc = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, &listener.sock);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...error creating socket", rc);
		status=-20; goto on_error;
	}

	// Bind listener socket.
	bsockaddr_in_init(&listener.addr, 0, 0);
	if ((rc=bsock_bind(listener.sock, &listener.addr, sizeof(listener.addr))) != 0 )
	{
		app_perror("...bind error", rc);
		status=-30; goto on_error;
	}

	// Get listener address.
	listener.addr_len = sizeof(listener.addr);
	rc = bsock_getsockname(listener.sock, &listener.addr, &listener.addr_len);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...ERROR in bsock_getsockname()", rc);
		status=-40; goto on_error;
	}
	listener.addr.sin_addr = binet_addr(bcstr(&s, "127.0.0.1"));


	// Register listener socket.
	rc = bioqueue_register_sock(pool, ioque, listener.sock, NULL, &test_cb, &listener.key);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...ERROR", rc);
		status=-50; goto on_error;
	}


	// Listener socket listen().
	if (bsock_listen(listener.sock, 5))
	{
		app_perror("...ERROR in bsock_listen()", rc);
		status=-60; goto on_error;
	}


	for (test_loop=0; test_loop < TEST_LOOP; ++test_loop)
	{
		// Client connect and server accept.
		for (i=0; i<MAX_PAIR; ++i)
		{
			rc = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, &client[i].sock);
			if (rc != BASE_SUCCESS)
			{
				app_perror("...error creating socket", rc);
				status=-70; goto on_error;
			}

			rc = bioqueue_register_sock(pool, ioque, client[i].sock, NULL, &test_cb, &client[i].key);
			if (rc != BASE_SUCCESS)
			{
				app_perror("...error ", rc);
				status=-80; goto on_error;
			}

			// Server socket accept()
			bioqueue_op_key_init(&server[i].accept_op, sizeof(server[i].accept_op));
			server[i].rem_addr_len = sizeof(bsockaddr_in);
			status = bioqueue_accept(listener.key, &server[i].accept_op, 
			&server[i].sock, &server[i].local_addr, 
			&server[i].rem_addr, 
			&server[i].rem_addr_len);
			if (status!=BASE_SUCCESS && status != BASE_EPENDING)
			{
				app_perror("...ERROR in bioqueue_accept()", rc);
				status=-90; goto on_error;
			}
			
			if (status==BASE_EPENDING)
			{
				++pending_op;
			}


			// Client socket connect()
			status = bioqueue_connect(client[i].key, &listener.addr, sizeof(listener.addr));
			if (status!=BASE_SUCCESS && status != BASE_EPENDING)
			{
				app_perror("...ERROR in bioqueue_connect()", rc);
				status=-100; goto on_error;
			}
			
			if (status==BASE_EPENDING)
			{
				++pending_op;
			}

			// Poll until connection of this pair established
			while (pending_op)
			{
				btime_val timeout = {1, 0};

#ifdef BASE_SYMBIAN
				status = bsymbianos_poll(-1, BASE_TIME_VAL_MSEC(timeout));
#else
				status = bioqueue_poll(ioque, &timeout);
#endif
				if (status > 0)
				{
					if (status > pending_op)
					{
						BASE_ERROR("...error: bioqueue_poll() returned %d (only expecting %d)", status, pending_op);
						return -110;
					}

					pending_op -= status;

					if (pending_op == 0)
					{
						status = 0;
					}
				}
			}
		}

		// There's no pending operation.
		// When we poll the ioqueue, there must not be events.
		if (pending_op == 0)
		{
			btime_val timeout = {1, 0};
#ifdef BASE_SYMBIAN
			status = bsymbianos_poll(-1, BASE_TIME_VAL_MSEC(timeout));
#else
			status = bioqueue_poll(ioque, &timeout);
#endif
			if (status != 0)
			{
				status=-120; goto on_error;
			}
		}

		for (i=0; i<MAX_PAIR; ++i)
		{
			// Check server socket.
			if (server[i].sock == BASE_INVALID_SOCKET)
			{
				status = -130;
				app_perror("...accept() error", bget_os_error());
				goto on_error;
			}

			// Check addresses
			if (server[i].local_addr.sin_family != bAF_INET() || server[i].local_addr.sin_addr.s_addr == 0 ||
				server[i].local_addr.sin_port == 0)
			{
				app_perror("...ERROR address not set", rc);
				status = -140;
				goto on_error;
			}

			if (server[i].rem_addr.sin_family != bAF_INET() || server[i].rem_addr.sin_addr.s_addr == 0 ||
				server[i].rem_addr.sin_port == 0)
			{
				app_perror("...ERROR address not set", rc);
				status = -150;
				goto on_error;
			}


			// Register newly accepted socket.
			rc = bioqueue_register_sock(pool, ioque, server[i].sock, NULL, &test_cb, &server[i].key);
			if (rc != BASE_SUCCESS)
			{
				app_perror("...ERROR in bioqueue_register_sock", rc);
				status = -160;
				goto on_error;
			}

			// Test send and receive.
			t_elapsed.u32.lo = 0;
			status = send_recv_test(ioque, server[i].key, client[i].key, 
			send_buf, recv_buf, bufsize, &t_elapsed);
			if (status != 0)
			{
				goto on_error;
			}
		}

		// Success
		status = 0;

		for (i=0; i<MAX_PAIR; ++i)
		{
			if (server[i].key != NULL)
			{
				bioqueue_unregister(server[i].key);
				server[i].key = NULL;
				server[i].sock = BASE_INVALID_SOCKET;
			}
			else if (server[i].sock != BASE_INVALID_SOCKET)
			{
				bsock_close(server[i].sock);
				server[i].sock = BASE_INVALID_SOCKET;
			}

			if (client[i].key != NULL)
			{
				bioqueue_unregister(client[i].key);
				client[i].key = NULL;
				client[i].sock = BASE_INVALID_SOCKET;
			}
			else if (client[i].sock != BASE_INVALID_SOCKET)
			{
				bsock_close(client[i].sock);
				client[i].sock = BASE_INVALID_SOCKET;
			}
		}
	}

	status = 0;

on_error:
	for (i=0; i<MAX_PAIR; ++i)
	{
		if (server[i].key != NULL)
		{
			bioqueue_unregister(server[i].key);
			server[i].key = NULL;
			server[i].sock = BASE_INVALID_SOCKET;
		}
		else if (server[i].sock != BASE_INVALID_SOCKET)
		{
			bsock_close(server[i].sock);
			server[i].sock = BASE_INVALID_SOCKET;
		}

		if (client[i].key != NULL)
		{
			bioqueue_unregister(client[i].key);
			client[i].key = NULL;
			server[i].sock = BASE_INVALID_SOCKET;
		}
		else if (client[i].sock != BASE_INVALID_SOCKET)
		{
			bsock_close(client[i].sock);
			client[i].sock = BASE_INVALID_SOCKET;
		}
	}

	if (listener.key)
	{
		bioqueue_unregister(listener.key);
		listener.key = NULL;
	}
	else if (listener.sock != BASE_INVALID_SOCKET)
	{
		bsock_close(listener.sock);
		listener.sock = BASE_INVALID_SOCKET;
	}

	if (ioque != NULL)
		bioqueue_destroy(ioque);
	bpool_release(pool);
	return status;

}


static int tcp_ioqueue_test_impl(bbool_t allow_concur)
{
    int status;

    BASE_INFO("..testing with concurency=%d", allow_concur);

    BASE_INFO("..%s compliance test 0 (success scenario)", bioqueue_name());
    if ((status=compliance_test_0(allow_concur)) != 0) {
	BASE_CRIT("....FAILED (status=%d)\n", status);
	return status;
    }
    BASE_INFO("..%s compliance test 1 (failed scenario)", bioqueue_name());
    if ((status=compliance_test_1(allow_concur)) != 0) {
	BASE_CRIT("....FAILED (status=%d)\n", status);
	return status;
    }

    BASE_INFO("..%s compliance test 2 (repeated accept)", bioqueue_name());
    if ((status=compliance_test_2(allow_concur)) != 0) {
	BASE_CRIT("....FAILED (status=%d)\n", status);
	return status;
    }

    return 0;
}

int tcp_ioqueue_test()
{
    int rc;

    rc = tcp_ioqueue_test_impl(BASE_TRUE);
    if (rc != 0)
	return rc;

    rc = tcp_ioqueue_test_impl(BASE_FALSE);
    if (rc != 0)
	return rc;

    return 0;
}

#endif	/* BASE_HAS_TCP */


#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_uiq_tcp;
#endif	/* INCLUDE_TCP_IOQUEUE_TEST */


