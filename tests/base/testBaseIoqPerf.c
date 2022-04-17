/* 
 *
 */
#include "testBaseTest.h"
#include <libBase.h>
#include <compat/baseHighPrecision.h>

/**
 * \page page_baselib_ioqueue_perf_test Test: I/O Queue Performance
 *
 * Test the performance of the I/O queue, using typical producer
 * consumer test. The test should examine the effect of using multiple
 * threads on the performance.
 *
 */

#if INCLUDE_IOQUEUE_PERF_TEST

#ifdef _MSC_VER
#   pragma warning ( disable: 4204)     // non-constant aggregate initializer
#endif

static bbool_t thread_quit_flag;
static bstatus_t last_error;
static unsigned last_error_counter;

/* Descriptor for each producer/consumer pair. */
typedef struct test_item
{
    bsock_t            server_fd, 
                         client_fd;
    bioqueue_t        *ioqueue;
    bioqueue_key_t    *server_key,
                        *client_key;
    bioqueue_op_key_t  recv_op,
                         send_op;
    int                  has_pending_send;
    bsize_t            buffer_size;
    char                *outgoing_buffer;
    char                *incoming_buffer;
    bsize_t            bytes_sent, 
                         bytes_recv;
} test_item;

/* Callback when data has been read.
 * Increment item->bytes_recv and ready to read the next data.
 */
static void on_read_complete(bioqueue_key_t *key, 
                             bioqueue_op_key_t *op_key,
                             bssize_t bytes_read)
{
    test_item *item = (test_item*)bioqueue_get_user_data(key);
    bstatus_t rc;
    int data_is_available = 1;

    //MTRACE("     read complete, bytes_read=%d", bytes_read);

    do {
        if (thread_quit_flag)
            return;

        if (bytes_read < 0) {
            char errmsg[BASE_ERR_MSG_SIZE];

	    rc = (bstatus_t)-bytes_read;
	    if (rc != last_error) {
	        //last_error = rc;
	        extStrError(rc, errmsg, sizeof(errmsg));
	        BASE_ERROR("...error: read error, bytes_read=%d (%s)", bytes_read, errmsg);
	        BASE_ERROR(".....additional info: total read=%u, total sent=%u",item->bytes_recv, item->bytes_sent);
	    } else {
	        last_error_counter++;
	    }
            bytes_read = 0;

        } else if (bytes_read == 0) {
            BASE_INFO( "...socket has closed!");
        }

        item->bytes_recv += bytes_read;
    
        /* To assure that the test quits, even if main thread
         * doesn't have time to run.
         */
        if (item->bytes_recv > item->buffer_size * 10000) 
	    thread_quit_flag = 1;

        bytes_read = item->buffer_size;
        rc = bioqueue_recv( key, op_key,
                              item->incoming_buffer, &bytes_read, 0 );

        if (rc == BASE_SUCCESS) {
            data_is_available = 1;
        } else if (rc == BASE_EPENDING) {
            data_is_available = 0;
        } else {
            data_is_available = 0;
	    if (rc != last_error) {
	        last_error = rc;
	        app_perror("...error: read error(1)", rc);
	    } else {
	        last_error_counter++;
	    }
        }

        if (!item->has_pending_send) {
            bssize_t sent = item->buffer_size;
            rc = bioqueue_send(item->client_key, &item->send_op,
                                 item->outgoing_buffer, &sent, 0);
            if (rc != BASE_SUCCESS && rc != BASE_EPENDING) {
                app_perror("...error: write error", rc);
            }

            item->has_pending_send = (rc==BASE_EPENDING);
        }

    } while (data_is_available);
}

/* Callback when data has been written.
 * Increment item->bytes_sent and write the next data.
 */
static void on_write_complete(bioqueue_key_t *key, 
                              bioqueue_op_key_t *op_key,
                              bssize_t bytes_sent)
{
    test_item *item = (test_item*) bioqueue_get_user_data(key);
    
    //MTRACE( "     write complete: sent = %d", bytes_sent);

    if (thread_quit_flag)
        return;

    item->has_pending_send = 0;
    item->bytes_sent += bytes_sent;

    if (bytes_sent <= 0) {
        BASE_ERROR( "...error: sending stopped. bytes_sent=%d", bytes_sent);
    } 
    else {
        bstatus_t rc;

        bytes_sent = item->buffer_size;
        rc = bioqueue_send( item->client_key, op_key,
                              item->outgoing_buffer, &bytes_sent, 0);
        if (rc != BASE_SUCCESS && rc != BASE_EPENDING) {
            app_perror("...error: write error", rc);
        }

        item->has_pending_send = (rc==BASE_EPENDING);
    }
}

struct thread_arg
{
    int		  id;
    bioqueue_t *ioqueue;
    unsigned	  counter;
};

/* The worker thread. */
static int worker_thread(void *p)
{
    struct thread_arg *arg = (struct thread_arg*) p;
    const btime_val timeout = {0, 100};
    int rc;

    while (!thread_quit_flag) {

	++arg->counter;
        rc = bioqueue_poll(arg->ioqueue, &timeout);
	//MTRACE("     thread: poll returned rc=%d", rc);
        if (rc < 0) {
	    char errmsg[BASE_ERR_MSG_SIZE];
	    extStrError(-rc, errmsg, sizeof(errmsg));
            BASE_ERROR("...error in bioqueue_poll() in thread %d "
		       "after %d loop: %s [bstatus_t=%d]", 
		       arg->id, arg->counter, errmsg, -rc);
            //return -1;
        }
    }
    return 0;
}

/* Calculate the bandwidth for the specific test configuration.
 * The test is simple:
 *  - create sockpair_cnt number of producer-consumer socket pair.
 *  - create thread_cnt number of worker threads.
 *  - each producer will send buffer_size bytes data as fast and
 *    as soon as it can.
 *  - each consumer will read buffer_size bytes of data as fast 
 *    as it could.
 *  - measure the total bytes received by all consumers during a
 *    period of time.
 */
static int perform_test(bbool_t allow_concur,
			int sock_type, const char *type_name,
                        unsigned thread_cnt, unsigned sockpair_cnt,
                        bsize_t buffer_size, 
                        bsize_t *p_bandwidth)
{
    enum { MSEC_DURATION = 5000 };
    bpool_t *pool;
    test_item *items;
    bthread_t **thread;
    bioqueue_t *ioqueue;
    bstatus_t rc;
    bioqueue_callback ioqueue_callback;
    buint32_t total_elapsed_usec, total_received;
    bhighprec_t bandwidth;
    btimestamp start, stop;
    unsigned i;

    MTRACE("    starting test..");

    ioqueue_callback.on_read_complete = &on_read_complete;
    ioqueue_callback.on_write_complete = &on_write_complete;

    thread_quit_flag = 0;

    pool = bpool_create(mem, NULL, 4096, 4096, NULL);
    if (!pool)
        return -10;

    items = (test_item*) bpool_alloc(pool, sockpair_cnt*sizeof(test_item));
    thread = (bthread_t**)
    	     bpool_alloc(pool, thread_cnt*sizeof(bthread_t*));

    MTRACE( "     creating ioqueue..");
    rc = bioqueue_create(pool, sockpair_cnt*2, &ioqueue);
    if (rc != BASE_SUCCESS) {
        app_perror("...error: unable to create ioqueue", rc);
        return -15;
    }

    rc = bioqueue_set_default_concurrency(ioqueue, allow_concur);
    if (rc != BASE_SUCCESS) {
	app_perror("...error: bioqueue_set_default_concurrency()", rc);
        return -16;
    }

    /* Initialize each producer-consumer pair. */
    for (i=0; i<sockpair_cnt; ++i) {
        bssize_t bytes;

        items[i].ioqueue = ioqueue;
        items[i].buffer_size = buffer_size;
        items[i].outgoing_buffer = (char*) bpool_alloc(pool, buffer_size);
        items[i].incoming_buffer = (char*) bpool_alloc(pool, buffer_size);
        items[i].bytes_recv = items[i].bytes_sent = 0;

        /* randomize outgoing buffer. */
        bcreate_random_string(items[i].outgoing_buffer, buffer_size);

        /* Init operation keys. */
        bioqueue_op_key_init(&items[i].recv_op, sizeof(items[i].recv_op));
        bioqueue_op_key_init(&items[i].send_op, sizeof(items[i].send_op));

        /* Create socket pair. */
	MTRACE( "      calling socketpair..");
        rc = app_socketpair(bAF_INET(), sock_type, 0, 
                            &items[i].server_fd, &items[i].client_fd);
        if (rc != BASE_SUCCESS) {
            app_perror("...error: unable to create socket pair", rc);
            return -20;
        }

        /* Register server socket to ioqueue. */
	MTRACE("      register(1)..");
        rc = bioqueue_register_sock(pool, ioqueue, 
                                      items[i].server_fd,
                                      &items[i], &ioqueue_callback,
                                      &items[i].server_key);
        if (rc != BASE_SUCCESS) {
            app_perror("...error: registering server socket to ioqueue", rc);
            return -60;
        }

        /* Register client socket to ioqueue. */
	MTRACE( "      register(2)..");
        rc = bioqueue_register_sock(pool, ioqueue, 
                                      items[i].client_fd,
                                      &items[i],  &ioqueue_callback,
                                      &items[i].client_key);
        if (rc != BASE_SUCCESS) {
            app_perror("...error: registering server socket to ioqueue", rc);
            return -70;
        }

        /* Start reading. */
	MTRACE("      bioqueue_recv..");
        bytes = items[i].buffer_size;
        rc = bioqueue_recv(items[i].server_key, &items[i].recv_op,
                             items[i].incoming_buffer, &bytes,
			     0);
        if (rc != BASE_EPENDING) {
            app_perror("...error: bioqueue_recv", rc);
            return -73;
        }

        /* Start writing. */
	MTRACE("      bioqueue_write..");
        bytes = items[i].buffer_size;
        rc = bioqueue_send(items[i].client_key, &items[i].send_op,
                             items[i].outgoing_buffer, &bytes, 0);
        if (rc != BASE_SUCCESS && rc != BASE_EPENDING) {
            app_perror("...error: bioqueue_write", rc);
            return -76;
        }

        items[i].has_pending_send = (rc==BASE_EPENDING);
    }

    /* Create the threads. */
    for (i=0; i<thread_cnt; ++i) {
	struct thread_arg *arg;

	arg = (struct thread_arg*) bpool_zalloc(pool, sizeof(*arg));
	arg->id = i;
	arg->ioqueue = ioqueue;
	arg->counter = 0;

        rc = bthreadCreate( pool, NULL, 
                               &worker_thread, 
                               arg, 
                               BASE_THREAD_DEFAULT_STACK_SIZE, 
                               BASE_THREAD_SUSPENDED, &thread[i] );
        if (rc != BASE_SUCCESS) {
            app_perror("...error: unable to create thread", rc);
            return -80;
        }
    }

    /* Mark start time. */
    rc = bTimeStampGet(&start);
    if (rc != BASE_SUCCESS)
        return -90;

    /* Start the thread. */
    MTRACE( "     resuming all threads..");
    for (i=0; i<thread_cnt; ++i) {
        rc = bthreadResume(thread[i]);
        if (rc != 0)
            return -100;
    }

    /* Wait for MSEC_DURATION seconds. 
     * This should be as simple as bthreadSleepMs(MSEC_DURATION) actually,
     * but unfortunately it doesn't work when system doesn't employ
     * timeslicing for threads.
     */
    MTRACE("     wait for few seconds..");
    do {
	bthreadSleepMs(1);

	/* Mark end time. */
	rc = bTimeStampGet(&stop);

	if (thread_quit_flag) {
	    MTRACE("      transfer limit reached..");
	    break;
	}

	if (belapsed_usec(&start,&stop)<MSEC_DURATION * 1000) {
	    MTRACE( "      time limit reached..");
	    break;
	}

    } while (1);

    /* Terminate all threads. */
    MTRACE("     terminating all threads..");
    thread_quit_flag = 1;

    for (i=0; i<thread_cnt; ++i) {
	MTRACE( "      join thread %d..", i);
        bthreadJoin(thread[i]);
    }

    /* Close all sockets. */
    MTRACE("     closing all sockets..");
    for (i=0; i<sockpair_cnt; ++i) {
        bioqueue_unregister(items[i].server_key);
        bioqueue_unregister(items[i].client_key);
    }

    /* Destroy threads */
    for (i=0; i<thread_cnt; ++i) {
        bthreadDestroy(thread[i]);
    }

    /* Destroy ioqueue. */
    MTRACE("     destroying ioqueue..");
    bioqueue_destroy(ioqueue);

    /* Calculate actual time in usec. */
    total_elapsed_usec = belapsed_usec(&start, &stop);

    /* Calculate total bytes received. */
    total_received = 0;
    for (i=0; i<sockpair_cnt; ++i) {
        total_received = (buint32_t)items[i].bytes_recv;
    }

    /* bandwidth = total_received*1000/total_elapsed_usec */
    bandwidth = total_received;
    bhighprec_mul(bandwidth, 1000);
    bhighprec_div(bandwidth, total_elapsed_usec);
    
    *p_bandwidth = (buint32_t)bandwidth;

    BASE_INFO("   %.4s    %2d        %2d       %8d KB/s",
              type_name, thread_cnt, sockpair_cnt,
              *p_bandwidth);

    /* Done. */
    bpool_release(pool);

    MTRACE( "    done..");
    return 0;
}

static int ioqueue_perf_test_imp(bbool_t allow_concur)
{
    enum { BUF_SIZE = 512 };
    int i, rc;
    struct {
        int         type;
        const char *type_name;
        int         thread_cnt;
        int         sockpair_cnt;
    } test_param[] = 
    {
        { bSOCK_DGRAM(), "udp", 1, 1},
        { bSOCK_DGRAM(), "udp", 1, 2},
        { bSOCK_DGRAM(), "udp", 1, 4},
        { bSOCK_DGRAM(), "udp", 1, 8},
        { bSOCK_DGRAM(), "udp", 2, 1},
        { bSOCK_DGRAM(), "udp", 2, 2},
        { bSOCK_DGRAM(), "udp", 2, 4},
        { bSOCK_DGRAM(), "udp", 2, 8},
        { bSOCK_DGRAM(), "udp", 4, 1},
        { bSOCK_DGRAM(), "udp", 4, 2},
        { bSOCK_DGRAM(), "udp", 4, 4},
        { bSOCK_DGRAM(), "udp", 4, 8},
        { bSOCK_DGRAM(), "udp", 4, 16},
        { bSOCK_STREAM(), "tcp", 1, 1},
        { bSOCK_STREAM(), "tcp", 1, 2},
        { bSOCK_STREAM(), "tcp", 1, 4},
        { bSOCK_STREAM(), "tcp", 1, 8},
        { bSOCK_STREAM(), "tcp", 2, 1},
        { bSOCK_STREAM(), "tcp", 2, 2},
        { bSOCK_STREAM(), "tcp", 2, 4},
        { bSOCK_STREAM(), "tcp", 2, 8},
        { bSOCK_STREAM(), "tcp", 4, 1},
        { bSOCK_STREAM(), "tcp", 4, 2},
        { bSOCK_STREAM(), "tcp", 4, 4},
        { bSOCK_STREAM(), "tcp", 4, 8},
        { bSOCK_STREAM(), "tcp", 4, 16},
/*
	{ bSOCK_DGRAM(), "udp", 32, 1},
	{ bSOCK_DGRAM(), "udp", 32, 1},
	{ bSOCK_DGRAM(), "udp", 32, 1},
	{ bSOCK_DGRAM(), "udp", 32, 1},
	{ bSOCK_DGRAM(), "udp", 1, 32},
	{ bSOCK_DGRAM(), "udp", 1, 32},
	{ bSOCK_DGRAM(), "udp", 1, 32},
	{ bSOCK_DGRAM(), "udp", 1, 32},
	{ bSOCK_STREAM(), "tcp", 32, 1},
	{ bSOCK_STREAM(), "tcp", 32, 1},
	{ bSOCK_STREAM(), "tcp", 32, 1},
	{ bSOCK_STREAM(), "tcp", 32, 1},
	{ bSOCK_STREAM(), "tcp", 1, 32},
	{ bSOCK_STREAM(), "tcp", 1, 32},
	{ bSOCK_STREAM(), "tcp", 1, 32},
	{ bSOCK_STREAM(), "tcp", 1, 32},
*/
    };
    bsize_t best_bandwidth;
    int best_index = 0;

    BASE_INFO("   Benchmarking %s ioqueue:", bioqueue_name());
    BASE_INFO("   Testing with concurency=%d", allow_concur);
    BASE_INFO("   =======================================");
    BASE_INFO("   Type  Threads  Skt.Pairs      Bandwidth");
    BASE_INFO("   =======================================");

    best_bandwidth = 0;
    for (i=0; i<(int)(sizeof(test_param)/sizeof(test_param[0])); ++i) {
        bsize_t bandwidth;

        rc = perform_test(allow_concur,
			  test_param[i].type, 
                          test_param[i].type_name,
                          test_param[i].thread_cnt, 
                          test_param[i].sockpair_cnt, 
                          BUF_SIZE, 
                          &bandwidth);
        if (rc != 0)
            return rc;

        if (bandwidth > best_bandwidth)
            best_bandwidth = bandwidth, best_index = i;

        /* Give it a rest before next test, to allow system to close the
	 * sockets properly. 
	 */
        bthreadSleepMs(500);
    }

    BASE_INFO("   Best: Type=%s Threads=%d, Skt.Pairs=%d, Bandwidth=%u KB/s",
              test_param[best_index].type_name,
              test_param[best_index].thread_cnt,
              test_param[best_index].sockpair_cnt,
              best_bandwidth);
    BASE_INFO("   (Note: packet size=%d, total errors=%u)", BUF_SIZE, last_error_counter);
    return 0;
}

/*
 * main test entry.
 */
int ioqueue_perf_test(void)
{
    int rc;

    rc = ioqueue_perf_test_imp(BASE_TRUE);
    if (rc != 0)
	return rc;

    rc = ioqueue_perf_test_imp(BASE_FALSE);
    if (rc != 0)
	return rc;

    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_uiq_perf_test;
#endif  /* INCLUDE_IOQUEUE_PERF_TEST */


