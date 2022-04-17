/* 
 *
 */
#include "testBaseTest.h"
#include <libBase.h>

#if INCLUDE_ECHO_CLIENT

enum { BUF_SIZE = 512 };

struct client
{
    int sock_type;
    const char *server;
    int port;
};

static batomic_t *totalBytes;
static batomic_t *timeout_counter;
static batomic_t *invalid_counter;

#define MSEC_PRINT_DURATION 1000

static int wait_socket(bsock_t sock, unsigned msec_timeout)
{
    bfd_set_t fdset;
    btime_val timeout;

    timeout.sec = 0;
    timeout.msec = msec_timeout;
    btime_val_normalize(&timeout);

    BASE_FD_ZERO(&fdset);
    BASE_FD_SET(sock, &fdset);
    
    return bsock_select(FD_SETSIZE, &fdset, NULL, NULL, &timeout);
}

static int echo_client_thread(void *arg)
{
    bsock_t sock;
    char send_buf[BUF_SIZE];
    char recv_buf[BUF_SIZE];
    char addr[BASE_INET_ADDRSTRLEN];
    bsockaddr_in addr;
    bstr_t s;
    bstatus_t rc;
    buint32_t buffer_id;
    buint32_t buffer_counter;
    struct client *client = arg;
    bstatus_t last_recv_err = BASE_SUCCESS, last_send_err = BASE_SUCCESS;
    unsigned counter = 0;

    rc = app_socket(bAF_INET(), client->sock_type, 0, -1, &sock);
    if (rc != BASE_SUCCESS) {
        app_perror("...unable to create socket", rc);
        return -10;
    }

    rc = bsockaddr_in_init( &addr, bcstr(&s, client->server), 
                              (buint16_t)client->port);
    if (rc != BASE_SUCCESS) {
        app_perror("...unable to resolve server", rc);
        return -15;
    }

    rc = bsock_connect(sock, &addr, sizeof(addr));
    if (rc != BASE_SUCCESS) {
        app_perror("...connect() error", rc);
        bsock_close(sock);
        return -20;
    }

    BASE_INFO("...socket connected to %s:%d", binet_ntop2(bAF_INET(), &addr.sin_addr, addr, sizeof(addr)), bntohs(addr.sin_port));

    bmemset(send_buf, 'A', BUF_SIZE);
    send_buf[BUF_SIZE-1]='\0';

    /* Give other thread chance to initialize themselves! */
    bthreadSleepMs(200);

    //BASE_INFO( "...thread %p running", bthreadThis()));

    buffer_id = (buint32_t) bthreadThis();
    buffer_counter = 0;

    *(buint32_t*)send_buf = buffer_id;

    for (;;) {
        int rc;
        bssize_t bytes;

	++counter;

	//while (wait_socket(sock,0) > 0)
	//    ;

        /* Send a packet. */
        bytes = BUF_SIZE;
	*(buint32_t*)(send_buf+4) = ++buffer_counter;
        rc = bsock_send(sock, send_buf, &bytes, 0);
        if (rc != BASE_SUCCESS || bytes != BUF_SIZE) {
            if (rc != last_send_err) {
                app_perror("...send() error", rc);
                BASE_ERROR( "...ignoring subsequent error..");
                last_send_err = rc;
                bthreadSleepMs(100);
            }
            continue;
        }

        rc = wait_socket(sock, 500);
        if (rc == 0) {
            BASE_ERROR("...timeout");
	    bytes = 0;
	    batomic_inc(timeout_counter);
	} else if (rc < 0) {
	    rc = bget_netos_error();
	    app_perror("...select() error", rc);
	    break;
        } else {
            /* Receive back the original packet. */
            bytes = 0;
            do {
                bssize_t received = BUF_SIZE - bytes;
                rc = bsock_recv(sock, recv_buf+bytes, &received, 0);
                if (rc != BASE_SUCCESS || received == 0) {
                    if (rc != last_recv_err) {
                        app_perror("...recv() error", rc);
                        BASE_WARN("...ignoring subsequent error..");
                        last_recv_err = rc;
                        bthreadSleepMs(100);
                    }
                    bytes = 0;
		    received = 0;
                    break;
                }
                bytes += received;
            } while (bytes != BUF_SIZE && bytes != 0);
        }

        if (bytes == 0)
            continue;

        if (bmemcmp(send_buf, recv_buf, BUF_SIZE) != 0) {
	    recv_buf[BUF_SIZE-1] = '\0';
            BASE_ERROR("...error: buffer %u has changed!\n"
			  "send_buf=%s\n"
			  "recv_buf=%s\n", 
			  counter, send_buf, recv_buf);
	    batomic_inc(invalid_counter);
        }

        /* Accumulate total received. */
	batomic_add(totalBytes, bytes);
    }

    bsock_close(sock);
    return 0;
}

int echo_client(int sock_type, const char *server, int port)
{
    bpool_t *pool;
    bthread_t *thread[ECHO_CLIENT_MAX_THREADS];
    bstatus_t rc;
    struct client client;
    int i;
    batomic_value_t last_received;
    btimestamp last_report;

    client.sock_type = sock_type;
    client.server = server;
    client.port = port;

    pool = bpool_create( mem, NULL, 4000, 4000, NULL );

    rc = batomic_create(pool, 0, &totalBytes);
    if (rc != BASE_SUCCESS) {
        BASE_ERROR("...error: unable to create atomic variable", rc);
        return -30;
    }
    rc = batomic_create(pool, 0, &invalid_counter);
    rc = batomic_create(pool, 0, &timeout_counter);

    BASE_INFO( "Echo client started");
    BASE_INFO("  Destination: %s:%d", ECHO_SERVER_ADDRESS, ECHO_SERVER_START_PORT));
    BASE_INFO("  Press Ctrl-C to exit");

    for (i=0; i<ECHO_CLIENT_MAX_THREADS; ++i) {
        rc = bthreadCreate( pool, NULL, &echo_client_thread, &client, 
                               BASE_THREAD_DEFAULT_STACK_SIZE, 0,
                               &thread[i]);
        if (rc != BASE_SUCCESS) {
            app_perror("...error: unable to create thread", rc);
            return -10;
        }
    }

    last_received = 0;
    bTimeStampGet(&last_report);

    for (;;) {
	btimestamp now;
	unsigned long received, cur_received;
	unsigned msec;
	bhighprec_t bw;
	btime_val elapsed;
	unsigned bw32;
	buint32_t timeout, invalid;

	bthreadSleepMs(1000);

	bTimeStampGet(&now);
	elapsed = belapsed_time(&last_report, &now);
	msec = BASE_TIME_VAL_MSEC(elapsed);

	received = batomic_get(totalBytes);
	cur_received = received - last_received;
	
	bw = cur_received;
	bhighprec_mul(bw, 1000);
	bhighprec_div(bw, msec);

	bw32 = (unsigned)bw;
	
	last_report = now;
	last_received = received;

	timeout = batomic_get(timeout_counter);
	invalid = batomic_get(invalid_counter);

        BASE_INFO("...%d threads, total bandwidth: %d KB/s, "
		  "timeout=%d, invalid=%d", 
                  ECHO_CLIENT_MAX_THREADS, bw32/1000,
		  timeout, invalid));
    }

    for (i=0; i<ECHO_CLIENT_MAX_THREADS; ++i) {
        bthreadJoin( thread[i] );
    }

    bpool_release(pool);
    return 0;
}


#else
int dummy_echo_client;
#endif

