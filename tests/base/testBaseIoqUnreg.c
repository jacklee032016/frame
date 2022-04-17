/* 
 *
 */
#include "testBaseTest.h"

#if INCLUDE_IOQUEUE_UNREG_TEST
/*
 * This tests the thread safety of ioqueue unregistration operation.
 */

#include <baseErrno.h>
#include <baseIoQueue.h>
#include <baseLog.h>
#include <baseOs.h>
#include <basePool.h>
#include <baseSock.h>
#include <compat/socket.h>
#include <baseString.h>


enum test_method
{
    UNREGISTER_IN_APP,
    UNREGISTER_IN_CALLBACK,
};

static int thread_quitting;
static enum test_method test_method;
static btime_val time_to_unregister;

struct sock_data
{
    bsock_t		 sock;
    bsock_t		 csock;
    bpool_t		*pool;
    bioqueue_key_t	*key;
    bmutex_t		*mutex;
    bioqueue_op_key_t	*op_key;
    char		*buffer;
    bsize_t		 bufsize;
    bbool_t		 unregistered;
    bssize_t		 received;
} sock_data;

static void on_read_complete(bioqueue_key_t *key, 
                             bioqueue_op_key_t *op_key, 
                             bssize_t bytes_read)
{
    bssize_t size;
    char *sendbuf = "Hello world";
    bstatus_t status;

    if (sock_data.unregistered)
	return;

    bmutex_lock(sock_data.mutex);

    if (sock_data.unregistered) {
	bmutex_unlock(sock_data.mutex);
	return;
    }

    if (bytes_read < 0) {
	if (-bytes_read != BASE_STATUS_FROM_OS(BASE_BLOCKING_ERROR_VAL))
	    app_perror("ioqueue reported recv error", (bstatus_t)-bytes_read);
    } else {
	sock_data.received += bytes_read;
    }

    if (test_method == UNREGISTER_IN_CALLBACK) {
	btime_val now;

	bgettimeofday(&now);
	if (BASE_TIME_VAL_GTE(now, time_to_unregister)) { 
	    sock_data.unregistered = 1;
	    bioqueue_unregister(key);
	    bmutex_unlock(sock_data.mutex);
	    return;
	}
    }
 
    do { 
	size = sock_data.bufsize;
	status = bioqueue_recv(key, op_key, sock_data.buffer, &size, 0);
	if (status != BASE_EPENDING && status != BASE_SUCCESS)
	    app_perror("recv() error", status);

    } while (status == BASE_SUCCESS);

    bmutex_unlock(sock_data.mutex);

    size = bansi_strlen(sendbuf);
    status = bsock_send(sock_data.csock, sendbuf, &size, 0);
    if (status != BASE_SUCCESS)
	app_perror("send() error", status);

    size = bansi_strlen(sendbuf);
    status = bsock_send(sock_data.csock, sendbuf, &size, 0);
    if (status != BASE_SUCCESS)
	app_perror("send() error", status);

} 

static int worker_thread(void *arg)
{
    bioqueue_t *ioqueue = (bioqueue_t*) arg;

    while (!thread_quitting) {
	btime_val timeout = { 0, 20 };
	bioqueue_poll(ioqueue, &timeout);
    }

    return 0;
}

/*
 * Perform unregistration test.
 *
 * This will create ioqueue and register a server socket. Depending
 * on the test method, either the callback or the main thread will
 * unregister and destroy the server socket after some period of time.
 */
static int perform_unreg_test(bioqueue_t *ioqueue,
			      bpool_t *test_pool,
			      const char *title, 
			      bbool_t other_socket)
{
    enum { WORKER_CNT = 1, MSEC = 500, QUIT_MSEC = 500 };
    int i;
    bthread_t *thread[WORKER_CNT];
    struct sock_data osd;
    bioqueue_callback callback;
    btime_val end_time;
    bstatus_t status;


    /* Sometimes its important to have other sockets registered to
     * the ioqueue, because when no sockets are registered, the ioqueue
     * will return from the poll early.
     */
    if (other_socket) {
	status = app_socket(bAF_INET(), bSOCK_DGRAM(), 0, 56127, &osd.sock);
	if (status != BASE_SUCCESS) {
	    app_perror("Error creating other socket", status);
	    return -12;
	}

	bbzero(&callback, sizeof(callback));
	status = bioqueue_register_sock(test_pool, ioqueue, osd.sock,
					  NULL, &callback, &osd.key);
	if (status != BASE_SUCCESS) {
	    app_perror("Error registering other socket", status);
	    return -13;
	}

    } else {
	osd.key = NULL;
	osd.sock = BASE_INVALID_SOCKET;
    }

    /* Init both time duration of testing */
    thread_quitting = 0;
    bgettimeofday(&time_to_unregister);
    time_to_unregister.msec += MSEC;
    btime_val_normalize(&time_to_unregister);

    end_time = time_to_unregister;
    end_time.msec += QUIT_MSEC;
    btime_val_normalize(&end_time);

    
    /* Create polling thread */
    for (i=0; i<WORKER_CNT; ++i) {
	status = bthreadCreate(test_pool, "unregtest", &worker_thread,
				   ioqueue, 0, 0, &thread[i]);
	if (status != BASE_SUCCESS) {
	    app_perror("Error creating thread", status);
	    return -20;
	}
    }

    /* Create pair of client/server sockets */
    status = app_socketpair(bAF_INET(), bSOCK_DGRAM(), 0, 
			    &sock_data.sock, &sock_data.csock);
    if (status != BASE_SUCCESS) {
	app_perror("app_socketpair error", status);
	return -30;
    }


    /* Initialize test data */
    sock_data.pool = bpool_create(mem, "sd", 1000, 1000, NULL);
    sock_data.buffer = (char*) bpool_alloc(sock_data.pool, 128);
    sock_data.bufsize = 128;
    sock_data.op_key = (bioqueue_op_key_t*) 
    		       bpool_alloc(sock_data.pool, 
				     sizeof(*sock_data.op_key));
    sock_data.received = 0;
    sock_data.unregistered = 0;

    bioqueue_op_key_init(sock_data.op_key, sizeof(*sock_data.op_key));

    status = bmutex_create_simple(sock_data.pool, "sd", &sock_data.mutex);
    if (status != BASE_SUCCESS) {
	app_perror("create_mutex() error", status);
	return -35;
    }

    /* Register socket to ioqueue */
    bbzero(&callback, sizeof(callback));
    callback.on_read_complete = &on_read_complete;
    status = bioqueue_register_sock(sock_data.pool, ioqueue, sock_data.sock,
				      NULL, &callback, &sock_data.key);
    if (status != BASE_SUCCESS) {
	app_perror("bioqueue_register error", status);
	return -40;
    }

    /* Bootstrap the first send/receive */
    on_read_complete(sock_data.key, sock_data.op_key, 0);

    /* Loop until test time ends */
    for (;;) {
	btime_val now, timeout;
	int n;

	bgettimeofday(&now);

	if (test_method == UNREGISTER_IN_APP && 
	    BASE_TIME_VAL_GTE(now, time_to_unregister) &&
	    !sock_data.unregistered) 
	{
	    sock_data.unregistered = 1;
	    /* Wait (as much as possible) for callback to complete */
	    bmutex_lock(sock_data.mutex);
	    bmutex_unlock(sock_data.mutex);
	    bioqueue_unregister(sock_data.key);
	}

	if (BASE_TIME_VAL_GT(now, end_time) && sock_data.unregistered)
	    break;

	timeout.sec = 0; timeout.msec = 10;
	n = bioqueue_poll(ioqueue, &timeout);
	if (n < 0) {
	    app_perror("bioqueue_poll error", -n);
	    bthreadSleepMs(1);
	}
    }

    thread_quitting = 1;

    for (i=0; i<WORKER_CNT; ++i) {
	bthreadJoin(thread[i]);
	bthreadDestroy(thread[i]);
    }

    /* Destroy data */
    bmutex_destroy(sock_data.mutex);
    bpool_release(sock_data.pool);
    sock_data.pool = NULL;

    if (other_socket) {
	bioqueue_unregister(osd.key);
    }

    bsock_close(sock_data.csock);

    BASE_INFO("....%s: done (%d KB/s)",title, sock_data.received * 1000 / MSEC / 1000);
    return 0;
}

static int udp_ioqueue_unreg_test_imp(bbool_t allow_concur)
{
    enum { LOOP = 10 };
    int i, rc;
    char title[30];
    bioqueue_t *ioqueue;
    bpool_t *test_pool;
	
    BASE_INFO("..testing with concurency=%d", allow_concur);

    test_method = UNREGISTER_IN_APP;

    test_pool = bpool_create(mem, "unregtest", 4000, 4000, NULL);

    rc = bioqueue_create(test_pool, 16, &ioqueue);
    if (rc != BASE_SUCCESS) {
	app_perror("Error creating ioqueue", rc);
	return -10;
    }

    rc = bioqueue_set_default_concurrency(ioqueue, allow_concur);
    if (rc != BASE_SUCCESS) {
	app_perror("Error in bioqueue_set_default_concurrency()", rc);
	return -12;
    }

    BASE_INFO("...ioqueue unregister stress test 0/3, unregister in app (%s)", bioqueue_name());
    for (i=0; i<LOOP; ++i) {
	bansi_sprintf(title, "repeat %d/%d", i, LOOP);
	rc = perform_unreg_test(ioqueue, test_pool, title, 0);
	if (rc != 0)
	    return rc;
    }


    BASE_INFO("...ioqueue unregister stress test 1/3, unregister in app (%s)", bioqueue_name());
    for (i=0; i<LOOP; ++i) {
	bansi_sprintf(title, "repeat %d/%d", i, LOOP);
	rc = perform_unreg_test(ioqueue, test_pool, title, 1);
	if (rc != 0)
	    return rc;
    }

    test_method = UNREGISTER_IN_CALLBACK;

    BASE_INFO("...ioqueue unregister stress test 2/3, unregister in cb (%s)", bioqueue_name());
    for (i=0; i<LOOP; ++i) {
	bansi_sprintf(title, "repeat %d/%d", i, LOOP);
	rc = perform_unreg_test(ioqueue, test_pool, title, 0);
	if (rc != 0)
	    return rc;
    }


    BASE_INFO("...ioqueue unregister stress test 3/3, unregister in cb (%s)", bioqueue_name());
    for (i=0; i<LOOP; ++i) {
	bansi_sprintf(title, "repeat %d/%d", i, LOOP);
	rc = perform_unreg_test(ioqueue, test_pool, title, 1);
	if (rc != 0)
	    return rc;
    }

    bioqueue_destroy(ioqueue);
    bpool_release(test_pool);

    return 0;
}

int udp_ioqueue_unreg_test(void)
{
    int rc;

    rc = udp_ioqueue_unreg_test_imp(BASE_TRUE);
    if (rc != 0)
    	return rc;

    rc = udp_ioqueue_unreg_test_imp(BASE_FALSE);
    if (rc != 0)
	return rc;

    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_uiq_unreg;
#endif	/* INCLUDE_IOQUEUE_UNREG_TEST */


