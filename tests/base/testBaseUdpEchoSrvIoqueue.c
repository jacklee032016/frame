/* $Id$ */
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
#include <libBase.h>
#include "testBaseTest.h"

static bioqueue_key_t *key;
static batomic_t *total_bytes;
static bbool_t thread_quit_flag;

struct op_key
{
    bioqueue_op_key_t  op_key_;
    struct op_key       *peer;
    char                *buffer;
    bsize_t            size;
    int                  is_pending;
    bstatus_t          last_err;
    bsockaddr_in       addr;
    int                  addrlen;
};

static void on_read_complete(bioqueue_key_t *ioq_key, 
                             bioqueue_op_key_t *op_key, 
                             bssize_t bytes_received)
{
    bstatus_t rc;
    struct op_key *recv_rec = (struct op_key *)op_key;

    for (;;) {
        struct op_key *send_rec = recv_rec->peer;
        recv_rec->is_pending = 0;

        if (bytes_received < 0) {
            if (-bytes_received != recv_rec->last_err) {
                recv_rec->last_err = (bstatus_t)-bytes_received;
                app_perror("...error receiving data", recv_rec->last_err);
            }
        } else if (bytes_received == 0) {
            /* note: previous error, or write callback */
        } else {
            batomic_add(total_bytes, (batomic_value_t)bytes_received);

            if (!send_rec->is_pending) {
                bssize_t sent = bytes_received;
                bmemcpy(send_rec->buffer, recv_rec->buffer, bytes_received);
                bmemcpy(&send_rec->addr, &recv_rec->addr, recv_rec->addrlen);
                send_rec->addrlen = recv_rec->addrlen;
                rc = bioqueue_sendto(ioq_key, &send_rec->op_key_,
                                       send_rec->buffer, &sent, 0,
                                       &send_rec->addr, send_rec->addrlen);
                send_rec->is_pending = (rc==BASE_EPENDING);

                if (rc!=BASE_SUCCESS && rc!=BASE_EPENDING) {
                    app_perror("...send error(1)", rc);
                }
            }
        }

        if (!send_rec->is_pending) {
            bytes_received = recv_rec->size;
            rc = bioqueue_recvfrom(ioq_key, &recv_rec->op_key_,
                                     recv_rec->buffer, &bytes_received, 0,
                                     &recv_rec->addr, &recv_rec->addrlen);
            recv_rec->is_pending = (rc==BASE_EPENDING);
            if (rc == BASE_SUCCESS) {
                /* fall through next loop. */
            } else if (rc == BASE_EPENDING) {
                /* quit callback. */
                break;
            } else {
                /* error */
                app_perror("...recv error", rc);
                recv_rec->last_err = rc;

                bytes_received = 0;
                /* fall through next loop. */
            }
        } else {
            /* recv will be done when write completion callback is called. */
            break;
        }
    }
}

static void on_write_complete(bioqueue_key_t *ioq_key, 
                              bioqueue_op_key_t *op_key, 
                              bssize_t bytes_sent)
{
    struct op_key *send_rec = (struct op_key*)op_key;

    if (bytes_sent <= 0) {
        bstatus_t rc = (bstatus_t)-bytes_sent;
        if (rc != send_rec->last_err) {
            send_rec->last_err = rc;
            app_perror("...send error(2)", rc);
        }
    }

    send_rec->is_pending = 0;
    on_read_complete(ioq_key, &send_rec->peer->op_key_, 0);
}

static int worker_thread(void *arg)
{
    bioqueue_t *ioqueue = (bioqueue_t*) arg;
    struct op_key read_op, write_op;
    char recv_buf[512], send_buf[512];
    bssize_t length;
    bstatus_t rc;

    read_op.peer = &write_op;
    read_op.is_pending = 0;
    read_op.last_err = 0;
    read_op.buffer = recv_buf;
    read_op.size = sizeof(recv_buf);
    read_op.addrlen = sizeof(read_op.addr);

    write_op.peer = &read_op;
    write_op.is_pending = 0;
    write_op.last_err = 0;
    write_op.buffer = send_buf;
    write_op.size = sizeof(send_buf);

    length = sizeof(recv_buf);
    rc = bioqueue_recvfrom(key, &read_op.op_key_, recv_buf, &length, 0,
                             &read_op.addr, &read_op.addrlen);
    if (rc == BASE_SUCCESS) {
        read_op.is_pending = 1;
        on_read_complete(key, &read_op.op_key_, length);
    }
    
    while (!thread_quit_flag) {
        btime_val timeout;
        timeout.sec = 0; timeout.msec = 10;
        rc = bioqueue_poll(ioqueue, &timeout);
    }
    return 0;
}

int udp_echo_srv_ioqueue(void)
{
    bpool_t *pool;
    bsock_t sock;
    bioqueue_t *ioqueue;
    bioqueue_callback callback;
    int i;
    bthread_t *thread[ECHO_SERVER_MAX_THREADS];
    bstatus_t rc;

    bbzero(&callback, sizeof(callback));
    callback.on_read_complete = &on_read_complete;
    callback.on_write_complete = &on_write_complete;

    pool = bpool_create(mem, NULL, 4000, 4000, NULL);
    if (!pool)
        return -10;

    rc = bioqueue_create(pool, 2, &ioqueue);
    if (rc != BASE_SUCCESS) {
        app_perror("...bioqueue_create error", rc);
        return -20;
    }

    rc = app_socket(bAF_INET(), bSOCK_DGRAM(), 0, 
                    ECHO_SERVER_START_PORT, &sock);
    if (rc != BASE_SUCCESS) {
        app_perror("...app_socket error", rc);
        return -30;
    }

    rc = bioqueue_register_sock(pool, ioqueue, sock, NULL,
                                  &callback, &key);
    if (rc != BASE_SUCCESS) {
        app_perror("...error registering socket", rc);
        return -40;
    }

    rc = batomic_create(pool, 0, &total_bytes);
    if (rc != BASE_SUCCESS) {
        app_perror("...error creating atomic variable", rc);
        return -45;
    }

    for (i=0; i<ECHO_SERVER_MAX_THREADS; ++i) {
        rc = bthreadCreate(pool, NULL, &worker_thread, ioqueue,
                              BASE_THREAD_DEFAULT_STACK_SIZE, 0,
                              &thread[i]);
        if (rc != BASE_SUCCESS) {
            app_perror("...create thread error", rc);
            return -50;
        }
    }

    echo_srv_common_loop(total_bytes);

    return 0;
}
