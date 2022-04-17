/* 
 *
 */
#include "testBaseTest.h"
#include <libBase.h>
#include <compat/baseHighPrecision.h>


/**
 * \page page_baselib_sock_perf_test Test: Socket Performance
 *
 * Test the performance of the socket communication. This will perform
 * simple producer-consumer type of test, where we calculate how long
 * does it take to send certain number of packets from producer to
 * consumer.
 *
 */

#if INCLUDE_SOCK_PERF_TEST

/*
 * sock_producer_consumer()
 *
 * Simple producer-consumer benchmarking. Send loop number of
 * buf_size size packets as fast as possible.
 */
static int sock_producer_consumer(int sock_type,
                                  bsize_t buf_size,
                                  unsigned loop, 
                                  unsigned *p_bandwidth)
{
    bsock_t consumer, producer;
    bpool_t *pool;
    char *outgoing_buffer, *incoming_buffer;
    btimestamp start, stop;
    unsigned i;
    bhighprec_t elapsed, bandwidth;
    bhighprec_t total_received;
    bstatus_t rc;

    /* Create pool. */
    pool = bpool_create(mem, NULL, 4096, 4096, NULL);
    if (!pool)
        return -10;

    /* Create producer-consumer pair. */
    rc = app_socketpair(bAF_INET(), sock_type, 0, &consumer, &producer);
    if (rc != BASE_SUCCESS) {
        app_perror("...error: create socket pair", rc);
        return -20;
    }

    /* Create buffers. */
    outgoing_buffer = (char*) bpool_alloc(pool, buf_size);
    incoming_buffer = (char*) bpool_alloc(pool, buf_size);

    /* Start loop. */
    bTimeStampGet(&start);
    total_received = 0;
    for (i=0; i<loop; ++i) {
        bssize_t sent, part_received, received;
	btime_val delay;

        sent = buf_size;
        rc = bsock_send(producer, outgoing_buffer, &sent, 0);
        if (rc != BASE_SUCCESS || sent != (bssize_t)buf_size) {
            app_perror("...error: send()", rc);
            return -61;
        }

        /* Repeat recv() until all data is part_received.
         * This applies only for non-UDP of course, since for UDP
         * we would expect all data to be part_received in one packet.
         */
        received = 0;
        do {
            part_received = buf_size-received;
	    rc = bsock_recv(consumer, incoming_buffer+received, 
			      &part_received, 0);
	    if (rc != BASE_SUCCESS) {
	        app_perror("...recv error", rc);
	        return -70;
	    }
            if (part_received <= 0) {
                BASE_ERROR("...error: socket has closed (part_received=%d)!", part_received);
                return -73;
            }
	    if ((bsize_t)part_received != buf_size-received) {
                if (sock_type != bSOCK_STREAM()) {
	            BASE_ERROR("...error: expecting %u bytes, got %u bytes", buf_size-received, part_received);
	            return -76;
                }
	    }
            received += part_received;
        } while ((bsize_t)received < buf_size);

	total_received += received;

	/* Stop test if it's been runnign for more than 10 secs. */
	bTimeStampGet(&stop);
	delay = belapsed_time(&start, &stop);
	if (delay.sec > 10)
	    break;
    }

    /* Stop timer. */
    bTimeStampGet(&stop);

    elapsed = belapsed_usec(&start, &stop);

    /* bandwidth = total_received * 1000 / elapsed */
    bandwidth = total_received;
    bhighprec_mul(bandwidth, 1000);
    bhighprec_div(bandwidth, elapsed);
    
    *p_bandwidth = (buint32_t)bandwidth;

    /* Close sockets. */
    bsock_close(consumer);
    bsock_close(producer);

    /* Done */
    bpool_release(pool);

    return 0;
}

int sock_perf_test(void)
{
    enum { LOOP = 64 * 1024 };
    int rc;
    unsigned bandwidth;

    BASE_INFO("...benchmarking socket (2 sockets, packet=512, single threaded):");

    /* Disable this test on Symbian since UDP connect()/send() failed
     * with S60 3rd edition (including MR2).
     * See http://www.extsip.org/trac/ticket/264
     */    
#if !defined(BASE_SYMBIAN) || BASE_SYMBIAN==0
    /* Benchmarking UDP */
    rc = sock_producer_consumer(bSOCK_DGRAM(), 512, LOOP, &bandwidth);
    if (rc != 0)
		return rc;
    BASE_INFO("....bandwidth UDP = %d KB/s", bandwidth);
#endif

    /* Benchmarking TCP */
    rc = sock_producer_consumer(bSOCK_STREAM(), 512, LOOP, &bandwidth);
    if (rc != 0)
		return rc;
    BASE_INFO("....bandwidth TCP = %d KB/s", bandwidth);

    return rc;
}


#else
int dummy_sock_perf_test;
#endif

