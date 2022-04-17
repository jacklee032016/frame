/* $Id: udp_echo_srv_sync.c 4537 2013-06-19 06:47:43Z riza $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "testBaseTest.h"
#include <libBase.h>

static batomic_t *total_bytes;
static bbool_t thread_quit_flag = 0;

static int worker_thread(void *arg)
{
    bsock_t    sock = (bsock_t)arg;
    char         buf[512];
    bstatus_t  last_recv_err = BASE_SUCCESS, last_write_err = BASE_SUCCESS;

    while (!thread_quit_flag) {
        bssize_t len;
        bstatus_t rc;
        bsockaddr_in addr;
        int addrlen;

        len = sizeof(buf);
        addrlen = sizeof(addr);
        rc = bsock_recvfrom(sock, buf, &len, 0, &addr, &addrlen);
        if (rc != 0) {
            if (rc != last_recv_err) {
                app_perror("...recv error", rc);
                last_recv_err = rc;
            }
            continue;
        }

	batomic_add(total_bytes, (batomic_value_t)len);

        rc = bsock_sendto(sock, buf, &len, 0, &addr, addrlen);
        if (rc != BASE_SUCCESS) {
            if (rc != last_write_err) {
                app_perror("...send error", rc);
                last_write_err = rc;
            }
            continue;
        }
    }
    return 0;
}


int echo_srv_sync(void)
{
    bpool_t *pool;
    bsock_t sock;
    bthread_t *thread[ECHO_SERVER_MAX_THREADS];
    bstatus_t rc;
    int i;

    pool = bpool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool)
        return -5;

    rc = batomic_create(pool, 0, &total_bytes);
    if (rc != BASE_SUCCESS) {
        app_perror("...unable to create atomic_var", rc);
        return -6;
    }

    rc = app_socket(bAF_INET(), bSOCK_DGRAM(),0, ECHO_SERVER_START_PORT, &sock);
    if (rc != BASE_SUCCESS) {
        app_perror("...socket error", rc);
        return -10;
    }

    for (i=0; i<ECHO_SERVER_MAX_THREADS; ++i) {
        rc = bthreadCreate(pool, NULL, &worker_thread, (void*)sock,
                              BASE_THREAD_DEFAULT_STACK_SIZE, 0,
                              &thread[i]);
        if (rc != BASE_SUCCESS) {
            app_perror("...unable to create thread", rc);
            return -20;
        }
    }

    BASE_INFO("...UDP echo server running with %d threads at port %d",
                  ECHO_SERVER_MAX_THREADS, ECHO_SERVER_START_PORT);
    BASE_INFO("...Press Ctrl-C to abort");

    echo_srv_common_loop(total_bytes);
    return 0;
}


int echo_srv_common_loop(batomic_t *bytes_counter)
{
    bhighprec_t last_received, avg_bw, highest_bw;
    btime_val last_print;
    unsigned count;
    const char *ioqueue_name;

    last_received = 0;
    bgettimeofday(&last_print);
    avg_bw = highest_bw = 0;
    count = 0;

    ioqueue_name = bioqueue_name();

    for (;;) {
        bhighprec_t received, cur_received, bw;
        unsigned msec;
        btime_val now, duration;

        bthreadSleepMs(1000);

        received = cur_received = batomic_get(bytes_counter);
        cur_received = cur_received - last_received;

        bgettimeofday(&now);
        duration = now;
        BASE_TIME_VAL_SUB(duration, last_print);
        msec = BASE_TIME_VAL_MSEC(duration);
        
        bw = cur_received;
        bhighprec_mul(bw, 1000);
        bhighprec_div(bw, msec);

        last_print = now;
        last_received = received;

        avg_bw = avg_bw + bw;
        count++;

        BASE_INFO("%s UDP (%d threads): %u KB/s (avg=%u KB/s) %s", 
		  ioqueue_name,
                  ECHO_SERVER_MAX_THREADS, 
                  (unsigned)(bw / 1000),
                  (unsigned)(avg_bw / count / 1000),
                  (count==20 ? "<ses avg>" : ""));

        if (count==20) {
            if (avg_bw/count > highest_bw)
                highest_bw = avg_bw/count;

            count = 0;
            avg_bw = 0;

            BASE_INFO("Highest average bandwidth=%u KB/s",
                          (unsigned)(highest_bw/1000));
        }
    }
    BASE_UNREACHED(return 0;)
}


