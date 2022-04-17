/* $Id: ssl_sock_imp_common.c 6004 2019-05-24 03:32:17Z riza $ */
/* 
 * Copyright (C) 2019-2019 Teluu Inc. (http://www.teluu.com)
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
#include <baseSslSock.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseLog.h>
#include <math.h>
#include <basePool.h>
#include <baseString.h>

#include "ssl_sock_imp_common.h"

/* Workaround for ticket #985 and #1930 */
#ifndef BASE_SSL_SOCK_DELAYED_CLOSE_TIMEOUT
#   define BASE_SSL_SOCK_DELAYED_CLOSE_TIMEOUT	500
#endif

enum { MAX_BIND_RETRY = 100 };

#ifdef SSL_SOCK_IMP_USE_CIRC_BUF
/*
 *******************************************************************
 * Circular buffer functions.
 *******************************************************************
 */

static bstatus_t circ_init(bpool_factory *factory,
                             circ_buf_t *cb, bsize_t cap)
{
    cb->cap    = cap;
    cb->readp  = 0;
    cb->writep = 0;
    cb->size   = 0;

    /* Initial pool holding the buffer elements */
    cb->pool = bpool_create(factory, "tls-circ%p", cap, cap, NULL);
    if (!cb->pool)
        return BASE_ENOMEM;

    /* Allocate circular buffer */
    cb->buf = bpool_alloc(cb->pool, cap);
    if (!cb->buf) {
        bpool_release(cb->pool);
        return BASE_ENOMEM;
    }

    return BASE_SUCCESS;
}

static void circ_deinit(circ_buf_t *cb)
{
    if (cb->pool) {
        bpool_release(cb->pool);
        cb->pool = NULL;
    }
}

static bbool_t circ_empty(const circ_buf_t *cb)
{
    return cb->size == 0;
}

static bsize_t circ_size(const circ_buf_t *cb)
{
    return cb->size;
}

static bsize_t circ_avail(const circ_buf_t *cb)
{
    return cb->cap - cb->size;
}

static void circ_read(circ_buf_t *cb, buint8_t *dst, bsize_t len)
{
    bsize_t size_after = cb->cap - cb->readp;
    bsize_t tbc = BASE_MIN(size_after, len);
    bsize_t rem = len - tbc;

    bmemcpy(dst, cb->buf + cb->readp, tbc);
    bmemcpy(dst + tbc, cb->buf, rem);

    cb->readp += len;
    cb->readp &= (cb->cap - 1);

    cb->size -= len;
}

static bstatus_t circ_write(circ_buf_t *cb,
                              const buint8_t *src, bsize_t len)
{
    /* Overflow condition: resize */
    if (len > circ_avail(cb)) {
        /* Minimum required capacity */
        bsize_t min_cap = len + cb->size;

        /* Next 32-bit power of two */
        min_cap--;
        min_cap |= min_cap >> 1;
        min_cap |= min_cap >> 2;
        min_cap |= min_cap >> 4;
        min_cap |= min_cap >> 8;
        min_cap |= min_cap >> 16;
        min_cap++;

        /* Create a new pool to hold a bigger buffer, using the same factory */
        bpool_t *pool = bpool_create(cb->pool->factory, "tls-circ%p",
                                         min_cap, min_cap, NULL);
        if (!pool)
            return BASE_ENOMEM;

        /* Allocate our new buffer */
        buint8_t *buf = bpool_alloc(pool, min_cap);
        if (!buf) {
            bpool_release(pool);
            return BASE_ENOMEM;
        }

        /* Save old size, which we shall restore after the next read */
        bsize_t old_size = cb->size;

        /* Copy old data into beginning of new buffer */
        circ_read(cb, buf, cb->size);

        /* Restore old size now */
        cb->size = old_size;

        /* Release the previous pool */
        bpool_release(cb->pool);

        /* Update circular buffer members */
        cb->pool = pool;
        cb->buf = buf;
        cb->readp = 0;
        cb->writep = cb->size;
        cb->cap = min_cap;
    }

    bsize_t size_after = cb->cap - cb->writep;
    bsize_t tbc = BASE_MIN(size_after, len);
    bsize_t rem = len - tbc;

    bmemcpy(cb->buf + cb->writep, src, tbc);
    bmemcpy(cb->buf, src + tbc, rem);

    cb->writep += len;
    cb->writep &= (cb->cap - 1);

    cb->size += len;

    return BASE_SUCCESS;
}
#endif

/*
 *******************************************************************
 * Helper functions.
 *******************************************************************
 */

/* Close sockets */
static void ssl_close_sockets(bssl_sock_t *ssock)
{
    bactivesock_t *asock;
    bsock_t sock;

    /* This can happen when bssl_sock_create() fails. */
    if (!ssock->write_mutex)
    	return;

    block_acquire(ssock->write_mutex);
    asock = ssock->asock;
    if (asock) {
        // Don't set ssock->asock to NULL, as it may trigger assertion in
        // send operation. This should be safe as active socket will simply
        // return BASE_EINVALIDOP on any operation if it is already closed.
        //ssock->asock = NULL;
        ssock->sock = BASE_INVALID_SOCKET;
    }
    sock = ssock->sock;
    if (sock != BASE_INVALID_SOCKET)
        ssock->sock = BASE_INVALID_SOCKET;
    block_release(ssock->write_mutex);

    if (asock)
        bactivesock_close(asock);

    if (sock != BASE_INVALID_SOCKET)
        bsock_close(sock);
}

/* When handshake completed:
 * - notify application
 * - if handshake failed, reset SSL state
 * - return BASE_FALSE when SSL socket instance is destroyed by application.
 */
static bbool_t on_handshake_complete(bssl_sock_t *ssock, 
				       bstatus_t status)
{
    /* Cancel handshake timer */
    if (ssock->timer.id == TIMER_HANDSHAKE_TIMEOUT) {
	btimer_heap_cancel(ssock->param.timer_heap, &ssock->timer);
	ssock->timer.id = TIMER_NONE;
    }

    /* Update certificates info on successful handshake */
    if (status == BASE_SUCCESS)
	ssl_update_certs_info(ssock);

    /* Accepting */
    if (ssock->is_server) {
	if (status != BASE_SUCCESS) {
	    /* Handshake failed in accepting, destroy our self silently. */

	    char buf[BASE_INET6_ADDRSTRLEN+10];

	    BASE_PERROR(3,(ssock->pool->objName, status,
			 "Handshake failed in accepting %s",
			 bsockaddr_print(&ssock->rem_addr, buf,
					   sizeof(buf), 3)));

	    if (ssock->param.cb.on_accept_complete2) {
		(*ssock->param.cb.on_accept_complete2) 
		      (ssock->parent, ssock, (bsockaddr_t*)&ssock->rem_addr, 
		      bsockaddr_get_len((bsockaddr_t*)&ssock->rem_addr), 
		      status);
	    }

	    /* Originally, this is a workaround for ticket #985. However,
	     * a race condition may occur in multiple worker threads
	     * environment when we are destroying SSL objects while other
	     * threads are still accessing them.
	     * Please see ticket #1930 for more info.
	     */
#if 1 //(defined(BASE_WIN32) && BASE_WIN32!=0)||(defined(BASE_WIN64) && BASE_WIN64!=0)
	    if (ssock->param.timer_heap) {
		btime_val interval = {0, BASE_SSL_SOCK_DELAYED_CLOSE_TIMEOUT};
		bstatus_t status1;

		ssock->ssl_state = SSL_STATE_NULL;
		ssl_close_sockets(ssock);

		if (ssock->timer.id != TIMER_NONE) {
		    btimer_heap_cancel(ssock->param.timer_heap,
					 &ssock->timer);
		}
		ssock->timer.id = TIMER_CLOSE;
		btime_val_normalize(&interval);
		status1 = btimer_heap_schedule(ssock->param.timer_heap, 
						 &ssock->timer, &interval);
		if (status1 != BASE_SUCCESS) {
	    	    BASE_PERROR(3,(ssock->pool->objName, status,
				 "Failed to schedule a delayed close. "
				 "Race condition may occur."));
		    ssock->timer.id = TIMER_NONE;
		    bssl_sock_close(ssock);
		}
	    } else {
		bssl_sock_close(ssock);
	    }
#else
	    {
		bssl_sock_close(ssock);
	    }
#endif

	    return BASE_FALSE;
	}
	/* Notify application the newly accepted SSL socket */
	if (ssock->param.cb.on_accept_complete2) {
	    bbool_t ret;
	    ret = (*ssock->param.cb.on_accept_complete2) 
		    (ssock->parent, ssock, (bsockaddr_t*)&ssock->rem_addr, 
		    bsockaddr_get_len((bsockaddr_t*)&ssock->rem_addr), 
		    status);
	    if (ret == BASE_FALSE)
		return BASE_FALSE;	
	} else if (ssock->param.cb.on_accept_complete) {
	    bbool_t ret;
	    ret = (*ssock->param.cb.on_accept_complete)
		      (ssock->parent, ssock, (bsockaddr_t*)&ssock->rem_addr,
		       bsockaddr_get_len((bsockaddr_t*)&ssock->rem_addr));
	    if (ret == BASE_FALSE)
		return BASE_FALSE;
	}
    }

    /* Connecting */
    else {
	/* On failure, reset SSL socket state first, as app may try to 
	 * reconnect in the callback.
	 */
	if (status != BASE_SUCCESS) {
	    /* Server disconnected us, possibly due to SSL nego failure */
	    ssl_reset_sock_state(ssock);
	}
	if (ssock->param.cb.on_connect_complete) {
	    bbool_t ret;
	    ret = (*ssock->param.cb.on_connect_complete)(ssock, status);
	    if (ret == BASE_FALSE)
		return BASE_FALSE;
	}
    }

    return BASE_TRUE;
}

static write_data_t* alloc_send_data(bssl_sock_t *ssock, bsize_t len)
{
    send_buf_t *send_buf = &ssock->send_buf;
    bsize_t avail_len, skipped_len = 0;
    char *reg1, *reg2;
    bsize_t reg1_len, reg2_len;
    write_data_t *p;

    /* Check buffer availability */
    avail_len = send_buf->max_len - send_buf->len;
    if (avail_len < len)
	return NULL;

    /* If buffer empty, reset start pointer and return it */
    if (send_buf->len == 0) {
	send_buf->start = send_buf->buf;
	send_buf->len   = len;
	p = (write_data_t*)send_buf->start;
	goto init_send_data;
    }

    /* Free space may be wrapped/splitted into two regions, so let's
     * analyze them if any region can hold the write data.
     */
    reg1 = send_buf->start + send_buf->len;
    if (reg1 >= send_buf->buf + send_buf->max_len)
	reg1 -= send_buf->max_len;
    reg1_len = send_buf->max_len - send_buf->len;
    if (reg1 + reg1_len > send_buf->buf + send_buf->max_len) {
	reg1_len = send_buf->buf + send_buf->max_len - reg1;
	reg2 = send_buf->buf;
	reg2_len = send_buf->start - send_buf->buf;
    } else {
	reg2 = NULL;
	reg2_len = 0;
    }

    /* More buffer availability check, note that the write data must be in
     * a contigue buffer.
     */
    avail_len = BASE_MAX(reg1_len, reg2_len);
    if (avail_len < len)
	return NULL;

    /* Get the data slot */
    if (reg1_len >= len) {
	p = (write_data_t*)reg1;
    } else {
	p = (write_data_t*)reg2;
	skipped_len = reg1_len;
    }

    /* Update buffer length */
    send_buf->len += len + skipped_len;

init_send_data:
    /* Init the new send data */
    bbzero(p, sizeof(*p));
    blist_init(p);
    blist_push_back(&ssock->send_pending, p);

    return p;
}

static void free_send_data(bssl_sock_t *ssock, write_data_t *wdata)
{
    send_buf_t *buf = &ssock->send_buf;
    write_data_t *spl = &ssock->send_pending;

    bassert(!blist_empty(&ssock->send_pending));
    
    /* Free slot from the buffer */
    if (spl->next == wdata && spl->prev == wdata) {
	/* This is the only data, reset the buffer */
	buf->start = buf->buf;
	buf->len = 0;
    } else if (spl->next == wdata) {
	/* This is the first data, shift start pointer of the buffer and
	 * adjust the buffer length.
	 */
	buf->start = (char*)wdata->next;
	if (wdata->next > wdata) {
	    buf->len -= ((char*)wdata->next - buf->start);
	} else {
	    /* Overlapped */
	    bsize_t right_len, left_len;
	    right_len = buf->buf + buf->max_len - (char*)wdata;
	    left_len  = (char*)wdata->next - buf->buf;
	    buf->len -= (right_len + left_len);
	}
    } else if (spl->prev == wdata) {
	/* This is the last data, just adjust the buffer length */
	if (wdata->prev < wdata) {
	    bsize_t jump_len;
	    jump_len = (char*)wdata -
		       ((char*)wdata->prev + wdata->prev->record_len);
	    buf->len -= (wdata->record_len + jump_len);
	} else {
	    /* Overlapped */
	    bsize_t right_len, left_len;
	    right_len = buf->buf + buf->max_len -
			((char*)wdata->prev + wdata->prev->record_len);
	    left_len  = (char*)wdata + wdata->record_len - buf->buf;
	    buf->len -= (right_len + left_len);
	}
    }
    /* For data in the middle buffer, just do nothing on the buffer. The slot
     * will be freed later when freeing the first/last data.
     */
    
    /* Remove the data from send pending list */
    blist_erase(wdata);
}

/* Flush write circular buffer to network socket. */
static bstatus_t flush_circ_buf_output(bssl_sock_t *ssock,
                                         bioqueue_op_key_t *send_key,
                                         bsize_t orig_len, unsigned flags)
{
    bssize_t len;
    write_data_t *wdata;
    bsize_t needed_len;
    bstatus_t status;

    block_acquire(ssock->write_mutex);

    /* Check if there is data in the circular buffer, flush it if any */
    if (io_empty(ssock, &ssock->circ_buf_output)) {
	block_release(ssock->write_mutex);
	return BASE_SUCCESS;
    }

    /* Get data and its length */
    len = io_size(ssock, &ssock->circ_buf_output);
    if (len == 0) {
	block_release(ssock->write_mutex);
	return BASE_SUCCESS;
    }

    /* Calculate buffer size needed, and align it to 8 */
    needed_len = len + sizeof(write_data_t);
    needed_len = ((needed_len + 7) >> 3) << 3;

    /* Allocate buffer for send data */
    wdata = alloc_send_data(ssock, needed_len);
    if (wdata == NULL) {
	/* Oops, the send buffer is full, let's just
	 * queue it for sending and return BASE_EPENDING.
	 */
	ssock->send_buf_pending.data_len = needed_len;
	ssock->send_buf_pending.app_key = send_key;
	ssock->send_buf_pending.flags = flags;
	ssock->send_buf_pending.plain_data_len = orig_len;
	block_release(ssock->write_mutex);
	return BASE_EPENDING;
    }

    /* Copy the data and set its properties into the send data */
    bioqueue_op_key_init(&wdata->key, sizeof(bioqueue_op_key_t));
    wdata->key.user_data = wdata;
    wdata->app_key = send_key;
    wdata->record_len = needed_len;
    wdata->data_len = len;
    wdata->plain_data_len = orig_len;
    wdata->flags = flags;
    io_read(ssock, &ssock->circ_buf_output, (buint8_t *)&wdata->data, len);

    /* Ticket #1573: Don't hold mutex while calling  socket send(). */
    block_release(ssock->write_mutex);

    /* Send it */
    if (ssock->param.sock_type == bSOCK_STREAM()) {
	status = bactivesock_send(ssock->asock, &wdata->key, 
				    wdata->data.content, &len,
				    flags);
    } else {
	status = bactivesock_sendto(ssock->asock, &wdata->key, 
				      wdata->data.content, &len,
				      flags,
				      (bsockaddr_t*)&ssock->rem_addr,
				      ssock->addr_len);
    }

    if (status != BASE_EPENDING) {
	/* When the sending is not pending, remove the wdata from send
	 * pending list.
	 */
	block_acquire(ssock->write_mutex);
	free_send_data(ssock, wdata);
	block_release(ssock->write_mutex);
    }

    return status;
}

#if 0
/* Just for testing send buffer alloc/free */
#include <baseRand.h>
bstatus_t bssl_sock_ossl_test_send_buf(bpool_t *pool)
{
    enum { MAX_CHUNK_NUM = 20 };
    unsigned chunk_size, chunk_cnt, i;
    write_data_t *wdata[MAX_CHUNK_NUM] = {0};
    btime_val now;
    bssl_sock_t *ssock = NULL;
    bssl_sock_param param;
    bstatus_t status;

    bgettimeofday(&now);
    bsrand((unsigned)now.sec);

    bssl_sock_param_default(&param);
    status = bssl_sock_create(pool, &param, &ssock);
    if (status != BASE_SUCCESS) {
	return status;
    }

    if (ssock->send_buf.max_len == 0) {
	ssock->send_buf.buf = (char*)
			      bpool_alloc(ssock->pool, 
					    ssock->param.send_buffer_size);
	ssock->send_buf.max_len = ssock->param.send_buffer_size;
	ssock->send_buf.start = ssock->send_buf.buf;
	ssock->send_buf.len = 0;
    }

    chunk_size = ssock->param.send_buffer_size / MAX_CHUNK_NUM / 2;
    chunk_cnt = 0;
    for (i = 0; i < MAX_CHUNK_NUM; i++) {
	wdata[i] = alloc_send_data(ssock, brand() % chunk_size + 321);
	if (wdata[i])
	    chunk_cnt++;
	else
	    break;
    }

    while (chunk_cnt) {
	i = brand() % MAX_CHUNK_NUM;
	if (wdata[i]) {
	    free_send_data(ssock, wdata[i]);
	    wdata[i] = NULL;
	    chunk_cnt--;
	}
    }

    if (ssock->send_buf.len != 0)
	status = BASE_EBUG;

    bssl_sock_close(ssock);
    return status;
}
#endif

static void on_timer(btimer_heap_t *th, struct btimer_entry *te)
{
    bssl_sock_t *ssock = (bssl_sock_t*)te->user_data;
    int timer_id = te->id;

    te->id = TIMER_NONE;

    BASE_UNUSED_ARG(th);

    switch (timer_id) {
    case TIMER_HANDSHAKE_TIMEOUT:
	BASE_STR_INFO(ssock->pool->objName, "SSL timeout after %d.%ds", ssock->param.timeout.sec, ssock->param.timeout.msec);

	on_handshake_complete(ssock, BASE_ETIMEDOUT);
	break;
    case TIMER_CLOSE:
	bssl_sock_close(ssock);
	break;
    default:
	bassert(!"Unknown timer");
	break;
    }
}

static void ssl_on_destroy(void *arg)
{
    bssl_sock_t *ssock = (bssl_sock_t*)arg;

    ssl_destroy(ssock);

    if (ssock->circ_buf_input_mutex) {
        block_destroy(ssock->circ_buf_input_mutex);
	ssock->circ_buf_input_mutex = NULL;
    }

    if (ssock->circ_buf_output_mutex) {
        block_destroy(ssock->circ_buf_output_mutex);
	ssock->circ_buf_output_mutex = NULL;
	ssock->write_mutex = NULL;
    }

    /* Secure release pool, i.e: all memory blocks will be zeroed first */
    bpool_secure_release(&ssock->pool);
}


/*
 *******************************************************************
 * Active socket callbacks.
 *******************************************************************
 */

/*
 * Get the offset of pointer to read-buffer of SSL socket from read-buffer
 * of active socket. Note that both SSL socket and active socket employ 
 * different but correlated read-buffers (as much as async_cnt for each),
 * and to make it easier/faster to find corresponding SSL socket's read-buffer
 * from known active socket's read-buffer, the pointer of corresponding 
 * SSL socket's read-buffer is stored right after the end of active socket's
 * read-buffer.
 */
#define OFFSET_OF_READ_DATA_PTR(ssock, asock_rbuf) \
					(read_data_t**) \
					((bint8_t*)(asock_rbuf) + \
					ssock->param.read_buffer_size)

static bbool_t asock_on_data_read (bactivesock_t *asock,
				     void *data,
				     bsize_t size,
				     bstatus_t status,
				     bsize_t *remainder)
{
    bssl_sock_t *ssock = (bssl_sock_t*)
			   bactivesock_get_user_data(asock);

    /* Socket error or closed */
    if (data && size > 0) {
    	bstatus_t status_;

	/* Consume the whole data */
        if (ssock->circ_buf_input_mutex)
	    block_acquire(ssock->circ_buf_input_mutex);
        status_ = io_write(ssock,&ssock->circ_buf_input, data, size);
        if (ssock->circ_buf_input_mutex)
            block_release(ssock->circ_buf_input_mutex);
        if (status_ != BASE_SUCCESS) {
            status = status_;
	    goto on_error;
	}
    }

    /* Check if SSL handshake hasn't finished yet */
    if (ssock->ssl_state == SSL_STATE_HANDSHAKING) {
	bbool_t ret = BASE_TRUE;

	if (status == BASE_SUCCESS)
	    status = ssl_do_handshake(ssock);

	/* Not pending is either success or failed */
	if (status != BASE_EPENDING)
	    ret = on_handshake_complete(ssock, status);

	return ret;
    }

    /* See if there is any decrypted data for the application */
    if (ssock->read_started) {
	do {
	    read_data_t *buf = *(OFFSET_OF_READ_DATA_PTR(ssock, data));
	    void *data_ = (bint8_t*)buf->data + buf->len;
	    int size_ = (int)(ssock->read_size - buf->len);
	    bstatus_t status_;

	    status_ = ssl_read(ssock, data_, &size_);

	    if (size_ > 0 || status != BASE_SUCCESS) {
		if (ssock->param.cb.on_data_read) {
		    bbool_t ret;
		    bsize_t remainder_ = 0;

		    if (size_ > 0)
			buf->len += size_;
    		
                    if (status != BASE_SUCCESS) {
                        ssock->ssl_state = SSL_STATE_ERROR;
                    }

		    ret = (*ssock->param.cb.on_data_read)(ssock, buf->data,
							  buf->len, status,
							  &remainder_);
		    if (!ret) {
			/* We've been destroyed */
			return BASE_FALSE;
		    }

		    /* Application may have left some data to be consumed 
		     * later.
		     */
		    buf->len = remainder_;
		}

		/* Active socket signalled connection closed/error, this has
		 * been signalled to the application along with any remaining
		 * buffer. So, let's just reset SSL socket now.
		 */
		if (status != BASE_SUCCESS) {
		    ssl_reset_sock_state(ssock);
		    return BASE_FALSE;
		}

	    } else if (status_ == BASE_SUCCESS) {
	    	break;
	    } else if (status_ == BASE_EEOF) {
		status = ssl_do_handshake(ssock);
		if (status == BASE_SUCCESS) {
		    /* Renegotiation completed */

		    /* Update certificates */
		    ssl_update_certs_info(ssock);

		    // Ticket #1573: Don't hold mutex while calling
		    //                socket send(). 
		    //block_acquire(ssock->write_mutex);
		    status = flush_delayed_send(ssock);
		    //block_release(ssock->write_mutex);

		    /* If flushing is ongoing, treat it as success */
		    if (status == BASE_EBUSY)
			status = BASE_SUCCESS;

		    if (status != BASE_SUCCESS && status != BASE_EPENDING) {
			BASE_PERROR(1,(ssock->pool->objName, status, 
				     "Failed to flush delayed send"));
			goto on_error;
		    }
		} else if (status != BASE_EPENDING) {
		    BASE_PERROR(1,(ssock->pool->objName, status, 
			         "Renegotiation failed"));
		    goto on_error;
		}

		break;
	    } else {
	    	/* Error */
	    	status = status_;
	    	goto on_error;
	    }

	} while (1);
    }

    return BASE_TRUE;

on_error:
    if (ssock->ssl_state == SSL_STATE_HANDSHAKING)
	return on_handshake_complete(ssock, status);

    if (ssock->read_started && ssock->param.cb.on_data_read) {
	bbool_t ret;
	ret = (*ssock->param.cb.on_data_read)(ssock, NULL, 0, status,
					      remainder);
	if (!ret) {
	    /* We've been destroyed */
	    return BASE_FALSE;
	}
    }

    ssl_reset_sock_state(ssock);
    return BASE_FALSE;
}

static bbool_t asock_on_data_sent (bactivesock_t *asock,
				     bioqueue_op_key_t *send_key,
				     bssize_t sent)
{
    bssl_sock_t *ssock = (bssl_sock_t*)
			   bactivesock_get_user_data(asock);
    write_data_t *wdata = (write_data_t*)send_key->user_data;
    bioqueue_op_key_t *app_key = wdata->app_key;
    bssize_t sent_len;

    sent_len = (sent > 0)? wdata->plain_data_len : sent;
    
    /* Update write buffer state */
    block_acquire(ssock->write_mutex);
    free_send_data(ssock, wdata);
    block_release(ssock->write_mutex);
    wdata = NULL;

    if (ssock->ssl_state == SSL_STATE_HANDSHAKING) {
	/* Initial handshaking */
	bstatus_t status;
	
	status = ssl_do_handshake(ssock);
	/* Not pending is either success or failed */
	if (status != BASE_EPENDING)
	    return on_handshake_complete(ssock, status);

    } else if (send_key != &ssock->handshake_op_key) {
	/* Some data has been sent, notify application */
	if (ssock->param.cb.on_data_sent) {
	    bbool_t ret;
	    ret = (*ssock->param.cb.on_data_sent)(ssock, app_key, 
						  sent_len);
	    if (!ret) {
		/* We've been destroyed */
		return BASE_FALSE;
	    }
	}
    } else {
	/* SSL re-negotiation is on-progress, just do nothing */
    }

    /* Send buffer has been updated, let's try to send any pending data */
    if (ssock->send_buf_pending.data_len) {
	bstatus_t status;
	status = flush_circ_buf_output(ssock, ssock->send_buf_pending.app_key,
				 ssock->send_buf_pending.plain_data_len,
				 ssock->send_buf_pending.flags);
	if (status == BASE_SUCCESS || status == BASE_EPENDING) {
	    ssock->send_buf_pending.data_len = 0;
	}
    }

    return BASE_TRUE;
}

static bbool_t asock_on_accept_complete (bactivesock_t *asock,
					   bsock_t newsock,
					   const bsockaddr_t *src_addr,
					   int src_addr_len)
{
    bssl_sock_t *ssock_parent = (bssl_sock_t*)
				  bactivesock_get_user_data(asock);
    bssl_sock_t *ssock;
    bactivesock_cb asock_cb;
    bactivesock_cfg asock_cfg;
    unsigned i;
    bstatus_t status;

    /* Create new SSL socket instance */
    status = bssl_sock_create(ssock_parent->pool,
				&ssock_parent->newsock_param, &ssock);
    if (status != BASE_SUCCESS)
	goto on_return;

    /* Update new SSL socket attributes */
    ssock->sock = newsock;
    ssock->parent = ssock_parent;
    ssock->is_server = BASE_TRUE;
    if (ssock_parent->cert) {
	status = bssl_sock_set_certificate(ssock, ssock->pool, 
					     ssock_parent->cert);
	if (status != BASE_SUCCESS)
	    goto on_return;
    }

    /* Apply QoS, if specified */
    status = bsock_apply_qos2(ssock->sock, ssock->param.qos_type,
				&ssock->param.qos_params, 1, 
				ssock->pool->objName, NULL);
    if (status != BASE_SUCCESS && !ssock->param.qos_ignore_error)
	goto on_return;

    /* Apply socket options, if specified */
    if (ssock->param.sockopt_params.cnt) {
	status = bsock_setsockopt_params(ssock->sock, 
					   &ssock->param.sockopt_params);
	if (status != BASE_SUCCESS && !ssock->param.sockopt_ignore_error)
	    goto on_return;
    }

    /* Update local address */
    ssock->addr_len = src_addr_len;
    status = bsock_getsockname(ssock->sock, &ssock->local_addr, 
				 &ssock->addr_len);
    if (status != BASE_SUCCESS) {
	/* This fails on few envs, e.g: win IOCP, just tolerate this and
	 * use parent local address instead.
	 */
	bsockaddr_cp(&ssock->local_addr, &ssock_parent->local_addr);
    }

    /* Set remote address */
    bsockaddr_cp(&ssock->rem_addr, src_addr);

    /* Create SSL context */
    status = ssl_create(ssock);
    if (status != BASE_SUCCESS)
	goto on_return;

    /* Prepare read buffer */
    ssock->asock_rbuf = (void**)bpool_calloc(ssock->pool, 
					       ssock->param.async_cnt,
					       sizeof(void*));
    if (!ssock->asock_rbuf)
        return BASE_ENOMEM;

    for (i = 0; i<ssock->param.async_cnt; ++i) {
	ssock->asock_rbuf[i] = (void*) bpool_alloc(
					    ssock->pool, 
					    ssock->param.read_buffer_size + 
					    sizeof(read_data_t*));
        if (!ssock->asock_rbuf[i])
            return BASE_ENOMEM;
    }

    /* Create active socket */
    bactivesock_cfg_default(&asock_cfg);
    asock_cfg.async_cnt = ssock->param.async_cnt;
    asock_cfg.concurrency = ssock->param.concurrency;
    asock_cfg.whole_data = BASE_TRUE;
    
    /* If listener socket has group lock, automatically create group lock
     * for the new socket.
     */
    if (ssock_parent->param.grp_lock) {
	bgrp_lock_t *glock;

	status = bgrp_lock_create(ssock->pool, NULL, &glock);
	if (status != BASE_SUCCESS)
	    goto on_return;

	bgrp_lock_add_ref(glock);
	asock_cfg.grp_lock = ssock->param.grp_lock = glock;
	bgrp_lock_add_handler(ssock->param.grp_lock, ssock->pool, ssock,
				ssl_on_destroy);
    }

    bbzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_data_read = asock_on_data_read;
    asock_cb.on_data_sent = asock_on_data_sent;

    status = bactivesock_create(ssock->pool,
				  ssock->sock, 
				  ssock->param.sock_type,
				  &asock_cfg,
				  ssock->param.ioqueue, 
				  &asock_cb,
				  ssock,
				  &ssock->asock);

    if (status != BASE_SUCCESS)
	goto on_return;

    /* Start read */
    status = bactivesock_start_read2(ssock->asock, ssock->pool, 
				       (unsigned)ssock->param.read_buffer_size,
				       ssock->asock_rbuf,
				       BASE_IOQUEUE_ALWAYS_ASYNC);
    if (status != BASE_SUCCESS)
	goto on_return;

    /* Prepare write/send state */
    bassert(ssock->send_buf.max_len == 0);
    ssock->send_buf.buf = (char*)
			  bpool_alloc(ssock->pool, 
					ssock->param.send_buffer_size);
    if (!ssock->send_buf.buf)
        return BASE_ENOMEM;

    ssock->send_buf.max_len = ssock->param.send_buffer_size;
    ssock->send_buf.start = ssock->send_buf.buf;
    ssock->send_buf.len = 0;

    /* Start handshake timer */
    if (ssock->param.timer_heap && (ssock->param.timeout.sec != 0 ||
	ssock->param.timeout.msec != 0))
    {
	bassert(ssock->timer.id == TIMER_NONE);
	ssock->timer.id = TIMER_HANDSHAKE_TIMEOUT;
	status = btimer_heap_schedule(ssock->param.timer_heap, 
				        &ssock->timer,
					&ssock->param.timeout);
	if (status != BASE_SUCCESS) {
	    ssock->timer.id = TIMER_NONE;
	    status = BASE_SUCCESS;
	}
    }

    /* Start SSL handshake */
    ssock->ssl_state = SSL_STATE_HANDSHAKING;
    ssl_set_state(ssock, BASE_TRUE);
    status = ssl_do_handshake(ssock);

on_return:
    if (ssock && status != BASE_EPENDING) {
	on_handshake_complete(ssock, status);
    }

    /* Must return BASE_TRUE whatever happened, as active socket must 
     * continue listening.
     */
    return BASE_TRUE;
}

static bbool_t asock_on_accept_complete2(bactivesock_t *asock,
					   bsock_t newsock,
					   const bsockaddr_t *src_addr,
					   int src_addr_len,
					   bstatus_t status)
{
    bbool_t ret = BASE_TRUE;
    if (status != BASE_SUCCESS) {
	bssl_sock_t *ssock = (bssl_sock_t*)
			bactivesock_get_user_data(asock);

	if (ssock->param.cb.on_accept_complete2) {
	    (*ssock->param.cb.on_accept_complete2) (ssock, NULL, 
						    src_addr, src_addr_len, 
						    status);
	}
    } else {
	ret = asock_on_accept_complete(asock, newsock, src_addr, src_addr_len);
    }
    return ret;
}

static bbool_t asock_on_connect_complete (bactivesock_t *asock,
					    bstatus_t status)
{
    bssl_sock_t *ssock = (bssl_sock_t*)
			   bactivesock_get_user_data(asock);
    unsigned i;

    if (status != BASE_SUCCESS)
	goto on_return;

    /* Update local address */
    ssock->addr_len = sizeof(bsockaddr);
    status = bsock_getsockname(ssock->sock, &ssock->local_addr, 
				 &ssock->addr_len);
    if (status != BASE_SUCCESS)
	goto on_return;

    /* Create SSL context */
    status = ssl_create(ssock);
    if (status != BASE_SUCCESS)
	goto on_return;

    /* Prepare read buffer */
    ssock->asock_rbuf = (void**)bpool_calloc(ssock->pool, 
					       ssock->param.async_cnt,
					       sizeof(void*));
    if (!ssock->asock_rbuf)
        return BASE_ENOMEM;

    for (i = 0; i<ssock->param.async_cnt; ++i) {
	ssock->asock_rbuf[i] = (void*) bpool_alloc(
					    ssock->pool, 
					    ssock->param.read_buffer_size + 
					    sizeof(read_data_t*));
        if (!ssock->asock_rbuf[i])
            return BASE_ENOMEM;
    }

    /* Start read */
    status = bactivesock_start_read2(ssock->asock, ssock->pool, 
				       (unsigned)ssock->param.read_buffer_size,
				       ssock->asock_rbuf,
				       BASE_IOQUEUE_ALWAYS_ASYNC);
    if (status != BASE_SUCCESS)
	goto on_return;

    /* Prepare write/send state */
    bassert(ssock->send_buf.max_len == 0);
    ssock->send_buf.buf = (char*)
			     bpool_alloc(ssock->pool, 
					   ssock->param.send_buffer_size);
    if (!ssock->send_buf.buf)
        return BASE_ENOMEM;

    ssock->send_buf.max_len = ssock->param.send_buffer_size;
    ssock->send_buf.start = ssock->send_buf.buf;
    ssock->send_buf.len = 0;

    /* Set peer name */
    ssl_set_peer_name(ssock);

    /* Start SSL handshake */
    ssock->ssl_state = SSL_STATE_HANDSHAKING;
    ssl_set_state(ssock, BASE_FALSE);

    status = ssl_do_handshake(ssock);
    if (status != BASE_EPENDING)
	goto on_return;

    return BASE_TRUE;

on_return:
    return on_handshake_complete(ssock, status);
}


/*
 *******************************************************************
 * API
 *******************************************************************
 */

/* Get available ciphers. */
bstatus_t bssl_cipher_get_availables(bssl_cipher ciphers[],
					         unsigned *cipher_num)
{
    unsigned i;

    BASE_ASSERT_RETURN(ciphers && cipher_num, BASE_EINVAL);

    ssl_ciphers_populate();

    if (ssl_cipher_num == 0) {
	*cipher_num = 0;
	return BASE_ENOTFOUND;
    }

    *cipher_num = BASE_MIN(*cipher_num, ssl_cipher_num);

    for (i = 0; i < *cipher_num; ++i)
	ciphers[i] = ssl_ciphers[i].id;

    return BASE_SUCCESS;
}

/* Get cipher name string */
const char* bssl_cipher_name(bssl_cipher cipher)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_cipher_num; ++i) {
	if (cipher == ssl_ciphers[i].id)
	    return ssl_ciphers[i].name;
    }

    return NULL;
}

/* Get cipher identifier */
bssl_cipher bssl_cipher_id(const char *cipher_name)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_cipher_num; ++i) {
        if (!bansi_stricmp(ssl_ciphers[i].name, cipher_name))
            return ssl_ciphers[i].id;
    }

    return BASE_TLS_UNKNOWN_CIPHER;
}

/* Check if the specified cipher is supported by SSL/TLS backend. */
bbool_t bssl_cipher_is_supported(bssl_cipher cipher)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_cipher_num; ++i) {
	if (cipher == ssl_ciphers[i].id)
	    return BASE_TRUE;
    }

    return BASE_FALSE;
}

/* Get available curves. */
bstatus_t bssl_curve_get_availables(bssl_curve curves[],
						unsigned *curve_num)
{
    unsigned i;

    BASE_ASSERT_RETURN(curves && curve_num, BASE_EINVAL);

    ssl_ciphers_populate();

    if (ssl_curves_num == 0) {
	*curve_num = 0;
	return BASE_ENOTFOUND;
    }

    *curve_num = BASE_MIN(*curve_num, ssl_curves_num);

    for (i = 0; i < *curve_num; ++i)
	curves[i] = ssl_curves[i].id;

    return BASE_SUCCESS;
}

/* Get curve name string. */
const char* bssl_curve_name(bssl_curve curve)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_curves_num; ++i) {
	if (curve == ssl_curves[i].id)
	    return ssl_curves[i].name;
    }

    return NULL;
}

/* Get curve ID from curve name string. */
bssl_curve bssl_curve_id(const char *curve_name)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_curves_num; ++i) {
        if (ssl_curves[i].name &&
        	!bansi_stricmp(ssl_curves[i].name, curve_name))
        {
            return ssl_curves[i].id;
        }
    }

    return BASE_TLS_UNKNOWN_CURVE;
}

/* Check if the specified curve is supported by SSL/TLS backend. */
bbool_t bssl_curve_is_supported(bssl_curve curve)
{
    unsigned i;

    ssl_ciphers_populate();

    for (i = 0; i < ssl_curves_num; ++i) {
	if (curve == ssl_curves[i].id)
	    return BASE_TRUE;
    }

    return BASE_FALSE;
}

/*
 * Create SSL socket instance. 
 */
bstatus_t bssl_sock_create (bpool_t *pool,
					const bssl_sock_param *param,
					bssl_sock_t **p_ssock)
{
    bssl_sock_t *ssock;
    bstatus_t status;

    BASE_ASSERT_RETURN(pool && param && p_ssock, BASE_EINVAL);
    BASE_ASSERT_RETURN(param->sock_type == bSOCK_STREAM(), BASE_ENOTSUP);

    pool = bpool_create(pool->factory, "ssl%p", 512, 512, NULL);

    /* Create secure socket */
    ssock = ssl_alloc(pool);
    ssock->pool = pool;
    ssock->sock = BASE_INVALID_SOCKET;
    ssock->ssl_state = SSL_STATE_NULL;
    ssock->circ_buf_input.owner = ssock;
    ssock->circ_buf_output.owner = ssock;
    blist_init(&ssock->write_pending);
    blist_init(&ssock->write_pending_empty);
    blist_init(&ssock->send_pending);
    btimer_entry_init(&ssock->timer, 0, ssock, &on_timer);
    bioqueue_op_key_init(&ssock->handshake_op_key,
			   sizeof(bioqueue_op_key_t));

    /* Create secure socket mutex */
    status = block_create_recursive_mutex(pool, pool->objName,
                                            &ssock->circ_buf_output_mutex);
    ssock->write_mutex = ssock->circ_buf_output_mutex;
    if (status != BASE_SUCCESS)
        return status;

    /* Create input circular buffer mutex */
    status = block_create_simple_mutex(pool, pool->objName,
                                         &ssock->circ_buf_input_mutex);
    if (status != BASE_SUCCESS)
        return status;

    /* Init secure socket param */
    bssl_sock_param_copy(pool, &ssock->param, param);

    if (ssock->param.grp_lock) {
	bgrp_lock_add_ref(ssock->param.grp_lock);
	bgrp_lock_add_handler(ssock->param.grp_lock, pool, ssock,
				ssl_on_destroy);
    }

    ssock->param.read_buffer_size = ((ssock->param.read_buffer_size+7)>>3)<<3;
    if (!ssock->param.timer_heap) {
	BASE_STR_INFO(ssock->pool->objName, "Warning: timer heap is not "
		  "available. It is recommended to supply one to avoid "
	          "a race condition if more than one worker threads are used.");
    }

    /* Finally */
    *p_ssock = ssock;

    return BASE_SUCCESS;
}


/*
 * Close the secure socket. This will unregister the socket from the
 * ioqueue and ultimately close the socket.
 */
bstatus_t bssl_sock_close(bssl_sock_t *ssock)
{
    BASE_ASSERT_RETURN(ssock, BASE_EINVAL);

    if (!ssock->pool)
	return BASE_SUCCESS;

    if (ssock->timer.id != TIMER_NONE) {
	btimer_heap_cancel(ssock->param.timer_heap, &ssock->timer);
	ssock->timer.id = TIMER_NONE;
    }

    ssl_reset_sock_state(ssock);

    /* Wipe out cert & key buffer. */
    if (ssock->cert) {
	bssl_cert_wipe_keys(ssock->cert);
	ssock->cert = NULL;
    }

    if (ssock->param.grp_lock) {
	bgrp_lock_dec_ref(ssock->param.grp_lock);
    } else {
	ssl_on_destroy(ssock);
    }

    return BASE_SUCCESS;
}


/*
 * Associate arbitrary data with the secure socket.
 */
bstatus_t bssl_sock_set_user_data(bssl_sock_t *ssock,
					      void *user_data)
{
    BASE_ASSERT_RETURN(ssock, BASE_EINVAL);

    ssock->param.user_data = user_data;
    return BASE_SUCCESS;
}


/*
 * Retrieve the user data previously associated with this secure
 * socket.
 */
void* bssl_sock_get_user_data(bssl_sock_t *ssock)
{
    BASE_ASSERT_RETURN(ssock, NULL);

    return ssock->param.user_data;
}

/*
 * Retrieve the local address and port used by specified SSL socket.
 */
bstatus_t bssl_sock_get_info (bssl_sock_t *ssock,
					  bssl_sock_info *info)
{
    bbzero(info, sizeof(*info));

    /* Established flag */
    info->established = (ssock->ssl_state == SSL_STATE_ESTABLISHED);

    /* Protocol */
    info->proto = ssock->param.proto;

    /* Local address */
    bsockaddr_cp(&info->local_addr, &ssock->local_addr);
    
    if (info->established) {
	info->cipher = ssl_get_cipher(ssock);

	/* Remote address */
	bsockaddr_cp(&info->remote_addr, &ssock->rem_addr);

	/* Certificates info */
	info->local_cert_info = &ssock->local_cert_info;
	info->remote_cert_info = &ssock->remote_cert_info;

	/* Verification status */
	info->verify_status = ssock->verify_status;
    }

    /* Last known SSL error code */
    info->last_native_err = ssock->last_err;

    /* Group lock */
    info->grp_lock = ssock->param.grp_lock;

    return BASE_SUCCESS;
}


/*
 * Starts read operation on this secure socket.
 */
bstatus_t bssl_sock_start_read (bssl_sock_t *ssock,
					    bpool_t *pool,
					    unsigned buff_size,
					    buint32_t flags)
{
    void **readbuf;
    unsigned i;

    BASE_ASSERT_RETURN(ssock && pool && buff_size, BASE_EINVAL);

    if (ssock->ssl_state != SSL_STATE_ESTABLISHED) 
	return BASE_EINVALIDOP;

    readbuf = (void**) bpool_calloc(pool, ssock->param.async_cnt, 
				      sizeof(void*));
    if (!readbuf)
        return BASE_ENOMEM;

    for (i=0; i<ssock->param.async_cnt; ++i) {
	readbuf[i] = bpool_alloc(pool, buff_size);
        if (!readbuf[i])
            return BASE_ENOMEM;
    }

    return bssl_sock_start_read2(ssock, pool, buff_size, 
				   readbuf, flags);
}


/*
 * Same as #bssl_sock_start_read(), except that the application
 * supplies the buffers for the read operation so that the acive socket
 * does not have to allocate the buffers.
 */
bstatus_t bssl_sock_start_read2 (bssl_sock_t *ssock,
					     bpool_t *pool,
					     unsigned buff_size,
					     void *readbuf[],
					     buint32_t flags)
{
    unsigned i;

    BASE_ASSERT_RETURN(ssock && pool && buff_size && readbuf, BASE_EINVAL);

    if (ssock->ssl_state != SSL_STATE_ESTABLISHED) 
	return BASE_EINVALIDOP;

    /* Create SSL socket read buffer */
    ssock->ssock_rbuf = (read_data_t*)bpool_calloc(pool, 
					       ssock->param.async_cnt,
					       sizeof(read_data_t));
    if (!ssock->ssock_rbuf)
        return BASE_ENOMEM;

    /* Store SSL socket read buffer pointer in the activesock read buffer */
    for (i=0; i<ssock->param.async_cnt; ++i) {
	read_data_t **p_ssock_rbuf = 
			OFFSET_OF_READ_DATA_PTR(ssock, ssock->asock_rbuf[i]);

	ssock->ssock_rbuf[i].data = readbuf[i];
	ssock->ssock_rbuf[i].len = 0;

	*p_ssock_rbuf = &ssock->ssock_rbuf[i];
    }

    ssock->read_size = buff_size;
    ssock->read_started = BASE_TRUE;
    ssock->read_flags = flags;

    for (i=0; i<ssock->param.async_cnt; ++i) {
	if (ssock->asock_rbuf[i]) {
	    bsize_t remainder = 0;
	    asock_on_data_read(ssock->asock, ssock->asock_rbuf[i], 0,
			       BASE_SUCCESS, &remainder);
	}
    }

    return BASE_SUCCESS;
}


/*
 * Same as bssl_sock_start_read(), except that this function is used
 * only for datagram sockets, and it will trigger \a on_data_recvfrom()
 * callback instead.
 */
bstatus_t bssl_sock_start_recvfrom (bssl_sock_t *ssock,
						bpool_t *pool,
						unsigned buff_size,
						buint32_t flags)
{
    BASE_UNUSED_ARG(ssock);
    BASE_UNUSED_ARG(pool);
    BASE_UNUSED_ARG(buff_size);
    BASE_UNUSED_ARG(flags);

    return BASE_ENOTSUP;
}


/*
 * Same as #bssl_sock_start_recvfrom() except that the recvfrom() 
 * operation takes the buffer from the argument rather than creating
 * new ones.
 */
bstatus_t bssl_sock_start_recvfrom2 (bssl_sock_t *ssock,
						 bpool_t *pool,
						 unsigned buff_size,
						 void *readbuf[],
						 buint32_t flags)
{
    BASE_UNUSED_ARG(ssock);
    BASE_UNUSED_ARG(pool);
    BASE_UNUSED_ARG(buff_size);
    BASE_UNUSED_ARG(readbuf);
    BASE_UNUSED_ARG(flags);

    return BASE_ENOTSUP;
}


/* Write plain data to SSL and flush the buffer. */
static bstatus_t ssl_send (bssl_sock_t *ssock, 
			     bioqueue_op_key_t *send_key,
			     const void *data,
			     bssize_t size,
			     unsigned flags)
{
    bstatus_t status;
    int nwritten;

    /* Write the plain data to SSL, after SSL encrypts it, the buffer will
     * contain the secured data to be sent via socket. Note that re-
     * negotitation may be on progress, so sending data should be delayed
     * until re-negotiation is completed.
     */
    block_acquire(ssock->write_mutex);
    /* Don't write to SSL if send buffer is full and some data is in
     * write buffer already, just return BASE_ENOMEM.
     */
    if (ssock->send_buf_pending.data_len) {
	block_release(ssock->write_mutex);
	return BASE_ENOMEM;
    }
    status = ssl_write(ssock, data, size, &nwritten);
    block_release(ssock->write_mutex);
    
    if (status == BASE_SUCCESS && nwritten == size) {
	/* All data written, flush write buffer to network socket */
	status = flush_circ_buf_output(ssock, send_key, size, flags);
    } else if (status == BASE_EEOF) {
        /* Re-negotiation is on progress, flush re-negotiation data */
	status = flush_circ_buf_output(ssock, &ssock->handshake_op_key, 0, 0);
	if (status == BASE_SUCCESS || status == BASE_EPENDING) {
	    /* Just return BASE_EBUSY when re-negotiation is on progress */
	    status = BASE_EBUSY;
	}
    }

    return status;
}

/* Flush delayed data sending in the write pending list. */
static bstatus_t flush_delayed_send(bssl_sock_t *ssock)
{
    /* Check for another ongoing flush */
    if (ssock->flushing_write_pend)
	return BASE_EBUSY;

    block_acquire(ssock->write_mutex);

    /* Again, check for another ongoing flush */
    if (ssock->flushing_write_pend) {
	block_release(ssock->write_mutex);
	return BASE_EBUSY;
    }

    /* Set ongoing flush flag */
    ssock->flushing_write_pend = BASE_TRUE;

    while (!blist_empty(&ssock->write_pending)) {
        write_data_t *wp;
	bstatus_t status;

	wp = ssock->write_pending.next;

	/* Ticket #1573: Don't hold mutex while calling socket send. */
	block_release(ssock->write_mutex);

	status = ssl_send (ssock, &wp->key, wp->data.ptr, 
			   wp->plain_data_len, wp->flags);
	if (status != BASE_SUCCESS) {
	    /* Reset ongoing flush flag first. */
	    ssock->flushing_write_pend = BASE_FALSE;
	    return status;
	}

	block_acquire(ssock->write_mutex);
	blist_erase(wp);
	blist_push_back(&ssock->write_pending_empty, wp);
    }

    /* Reset ongoing flush flag */
    ssock->flushing_write_pend = BASE_FALSE;

    block_release(ssock->write_mutex);

    return BASE_SUCCESS;
}

/* Sending is delayed, push back the sending data into pending list. */
static bstatus_t delay_send (bssl_sock_t *ssock,
			       bioqueue_op_key_t *send_key,
			       const void *data,
			       bssize_t size,
			       unsigned flags)
{
    write_data_t *wp;

    block_acquire(ssock->write_mutex);

    /* Init write pending instance */
    if (!blist_empty(&ssock->write_pending_empty)) {
	wp = ssock->write_pending_empty.next;
	blist_erase(wp);
    } else {
	wp = BASE_POOL_ZALLOC_T(ssock->pool, write_data_t);
    }

    wp->app_key = send_key;
    wp->plain_data_len = size;
    wp->data.ptr = data;
    wp->flags = flags;

    blist_push_back(&ssock->write_pending, wp);
    
    block_release(ssock->write_mutex);

    /* Must return BASE_EPENDING */
    return BASE_EPENDING;
}


/**
 * Send data using the socket.
 */
bstatus_t bssl_sock_send (bssl_sock_t *ssock,
				      bioqueue_op_key_t *send_key,
				      const void *data,
				      bssize_t *size,
				      unsigned flags)
{
    bstatus_t status;

    BASE_ASSERT_RETURN(ssock && data && size && (*size>0), BASE_EINVAL);

    if (ssock->ssl_state != SSL_STATE_ESTABLISHED) 
	return BASE_EINVALIDOP;

    // Ticket #1573: Don't hold mutex while calling  socket send().
    //block_acquire(ssock->write_mutex);

    /* Flush delayed send first. Sending data might be delayed when 
     * re-negotiation is on-progress.
     */
    status = flush_delayed_send(ssock);
    if (status == BASE_EBUSY) {
	/* Re-negotiation or flushing is on progress, delay sending */
	status = delay_send(ssock, send_key, data, *size, flags);
	goto on_return;
    } else if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Write data to SSL */
    status = ssl_send(ssock, send_key, data, *size, flags);
    if (status == BASE_EBUSY) {
	/* Re-negotiation is on progress, delay sending */
	status = delay_send(ssock, send_key, data, *size, flags);
    }

on_return:
    //block_release(ssock->write_mutex);
    return status;
}


/**
 * Send datagram using the socket.
 */
bstatus_t bssl_sock_sendto (bssl_sock_t *ssock,
					bioqueue_op_key_t *send_key,
					const void *data,
					bssize_t *size,
					unsigned flags,
					const bsockaddr_t *addr,
					int addr_len)
{
    BASE_UNUSED_ARG(ssock);
    BASE_UNUSED_ARG(send_key);
    BASE_UNUSED_ARG(data);
    BASE_UNUSED_ARG(size);
    BASE_UNUSED_ARG(flags);
    BASE_UNUSED_ARG(addr);
    BASE_UNUSED_ARG(addr_len);

    return BASE_ENOTSUP;
}


/**
 * Starts asynchronous socket accept() operations on this secure socket. 
 */
bstatus_t bssl_sock_start_accept (bssl_sock_t *ssock,
					      bpool_t *pool,
					      const bsockaddr_t *localaddr,
					      int addr_len)
{
    return bssl_sock_start_accept2(ssock, pool, localaddr, addr_len,
    				     &ssock->param);
}


/**
 * Same as #bssl_sock_start_accept(), but application provides parameter
 * for new accepted secure sockets.
 */
bstatus_t
bssl_sock_start_accept2(bssl_sock_t *ssock,
			  bpool_t *pool,
			  const bsockaddr_t *localaddr,
			  int addr_len,
			  const bssl_sock_param *newsock_param)
{
    bactivesock_cb asock_cb;
    bactivesock_cfg asock_cfg;
    bstatus_t status;

    BASE_ASSERT_RETURN(ssock && pool && localaddr && addr_len, BASE_EINVAL);

    /* Verify new socket parameters */
    if (newsock_param->grp_lock != ssock->param.grp_lock ||
        newsock_param->sock_af != ssock->param.sock_af ||
        newsock_param->sock_type != ssock->param.sock_type)
    {
        return BASE_EINVAL;
    }

    /* Create socket */
    status = bsock_socket(ssock->param.sock_af, ssock->param.sock_type, 0, 
			    &ssock->sock);
    if (status != BASE_SUCCESS)
	goto on_error;

    /* Apply SO_REUSEADDR */
    if (ssock->param.reuse_addr) {
	int enabled = 1;
	status = bsock_setsockopt(ssock->sock, bSOL_SOCKET(),
				    bSO_REUSEADDR(),
				    &enabled, sizeof(enabled));
	if (status != BASE_SUCCESS) {
	    BASE_PERROR(4,(ssock->pool->objName, status,
		         "Warning: error applying SO_REUSEADDR"));
	}
    }

    /* Apply QoS, if specified */
    status = bsock_apply_qos2(ssock->sock, ssock->param.qos_type,
				&ssock->param.qos_params, 2, 
				ssock->pool->objName, NULL);
    if (status != BASE_SUCCESS && !ssock->param.qos_ignore_error)
	goto on_error;

    /* Apply socket options, if specified */
    if (ssock->param.sockopt_params.cnt) {
	status = bsock_setsockopt_params(ssock->sock, 
					   &ssock->param.sockopt_params);

	if (status != BASE_SUCCESS && !ssock->param.sockopt_ignore_error)
	    goto on_error;
    }

    /* Bind socket */
    status = bsock_bind(ssock->sock, localaddr, addr_len);
    if (status != BASE_SUCCESS)
	goto on_error;

    /* Start listening to the address */
    status = bsock_listen(ssock->sock, BASE_SOMAXCONN);
    if (status != BASE_SUCCESS)
	goto on_error;

    /* Create active socket */
    bactivesock_cfg_default(&asock_cfg);
    asock_cfg.async_cnt = ssock->param.async_cnt;
    asock_cfg.concurrency = ssock->param.concurrency;
    asock_cfg.whole_data = BASE_TRUE;
    asock_cfg.grp_lock = ssock->param.grp_lock;

    bbzero(&asock_cb, sizeof(asock_cb));
    //asock_cb.on_accept_complete = asock_on_accept_complete;
    asock_cb.on_accept_complete2 = asock_on_accept_complete2;

    status = bactivesock_create(pool,
				  ssock->sock, 
				  ssock->param.sock_type,
				  &asock_cfg,
				  ssock->param.ioqueue, 
				  &asock_cb,
				  ssock,
				  &ssock->asock);

    if (status != BASE_SUCCESS)
	goto on_error;

    /* Start accepting */
    bssl_sock_param_copy(pool, &ssock->newsock_param, newsock_param);
    ssock->newsock_param.grp_lock = NULL;
    status = bactivesock_start_accept(ssock->asock, pool);
    if (status != BASE_SUCCESS)
	goto on_error;

    /* Update local address */
    ssock->addr_len = addr_len;
    status = bsock_getsockname(ssock->sock, &ssock->local_addr, 
				 &ssock->addr_len);
    if (status != BASE_SUCCESS)
	bsockaddr_cp(&ssock->local_addr, localaddr);

    ssock->is_server = BASE_TRUE;

    return BASE_SUCCESS;

on_error:
    ssl_reset_sock_state(ssock);
    return status;
}


/**
 * Starts asynchronous socket connect() operation.
 */
bstatus_t bssl_sock_start_connect(bssl_sock_t *ssock,
					      bpool_t *pool,
					      const bsockaddr_t *localaddr,
					      const bsockaddr_t *remaddr,
					      int addr_len)
{
    bssl_start_connect_param param;    
    param.pool = pool;
    param.localaddr = localaddr;
    param.local_port_range = 0;
    param.remaddr = remaddr;
    param.addr_len = addr_len;

    return bssl_sock_start_connect2(ssock, &param);
}

bstatus_t bssl_sock_start_connect2(
			       bssl_sock_t *ssock,
			       bssl_start_connect_param *connect_param)
{
    bactivesock_cb asock_cb;
    bactivesock_cfg asock_cfg;
    bstatus_t status;
    
    bpool_t *pool = connect_param->pool;
    const bsockaddr_t *localaddr = connect_param->localaddr;
    buint16_t port_range = connect_param->local_port_range;
    const bsockaddr_t *remaddr = connect_param->remaddr;
    int addr_len = connect_param->addr_len;

    BASE_ASSERT_RETURN(ssock && pool && localaddr && remaddr && addr_len,
		     BASE_EINVAL);

    /* Create socket */
    status = bsock_socket(ssock->param.sock_af, ssock->param.sock_type, 0, 
			    &ssock->sock);
    if (status != BASE_SUCCESS)
	goto on_error;

    /* Apply QoS, if specified */
    status = bsock_apply_qos2(ssock->sock, ssock->param.qos_type,
				&ssock->param.qos_params, 2, 
				ssock->pool->objName, NULL);
    if (status != BASE_SUCCESS && !ssock->param.qos_ignore_error)
	goto on_error;

    /* Apply socket options, if specified */
    if (ssock->param.sockopt_params.cnt) {
	status = bsock_setsockopt_params(ssock->sock, 
					   &ssock->param.sockopt_params);

	if (status != BASE_SUCCESS && !ssock->param.sockopt_ignore_error)
	    goto on_error;
    }

    /* Bind socket */
    if (port_range) {
	buint16_t max_bind_retry = MAX_BIND_RETRY;
	if (port_range && port_range < max_bind_retry)
	{
	    max_bind_retry = port_range;
	}
	status = bsock_bind_random(ssock->sock, localaddr, port_range,
				     max_bind_retry);
    } else {
	status = bsock_bind(ssock->sock, localaddr, addr_len);
    }

    if (status != BASE_SUCCESS)
	goto on_error;

    /* Create active socket */
    bactivesock_cfg_default(&asock_cfg);
    asock_cfg.async_cnt = ssock->param.async_cnt;
    asock_cfg.concurrency = ssock->param.concurrency;
    asock_cfg.whole_data = BASE_TRUE;
    asock_cfg.grp_lock = ssock->param.grp_lock;

    bbzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_connect_complete = asock_on_connect_complete;
    asock_cb.on_data_read = asock_on_data_read;
    asock_cb.on_data_sent = asock_on_data_sent;

    status = bactivesock_create(pool,
				  ssock->sock, 
				  ssock->param.sock_type,
				  &asock_cfg,
				  ssock->param.ioqueue, 
				  &asock_cb,
				  ssock,
				  &ssock->asock);

    if (status != BASE_SUCCESS)
	goto on_error;

    /* Save remote address */
    bsockaddr_cp(&ssock->rem_addr, remaddr);

    /* Start timer */
    if (ssock->param.timer_heap &&
        (ssock->param.timeout.sec != 0 || ssock->param.timeout.msec != 0))
    {
	bassert(ssock->timer.id == TIMER_NONE);
	ssock->timer.id = TIMER_HANDSHAKE_TIMEOUT;
	status = btimer_heap_schedule(ssock->param.timer_heap,
					&ssock->timer,
				        &ssock->param.timeout);
	if (status != BASE_SUCCESS) {
	    ssock->timer.id = TIMER_NONE;
	    status = BASE_SUCCESS;
	}
    }

    status = bactivesock_start_connect(ssock->asock, pool, remaddr,
					 addr_len);

    if (status == BASE_SUCCESS)
	asock_on_connect_complete(ssock->asock, BASE_SUCCESS);
    else if (status != BASE_EPENDING)
	goto on_error;

    /* Update local address */
    ssock->addr_len = addr_len;
    status = bsock_getsockname(ssock->sock, &ssock->local_addr,
				 &ssock->addr_len);
    /* Note that we may not get an IP address here. This can
     * happen for example on Windows, where getsockname()
     * would return 0.0.0.0 if socket has just started the
     * async connect. In this case, just leave the local
     * address with 0.0.0.0 for now; it will be updated
     * once the socket is established.
     */

    /* Update SSL state */
    ssock->is_server = BASE_FALSE;

    return BASE_EPENDING;

on_error:
    ssl_reset_sock_state(ssock);
    return status;
}


bstatus_t bssl_sock_renegotiate(bssl_sock_t *ssock)
{
    bstatus_t status;

    BASE_ASSERT_RETURN(ssock, BASE_EINVAL);

    if (ssock->ssl_state != SSL_STATE_ESTABLISHED) 
	return BASE_EINVALIDOP;

    status = ssl_renegotiate(ssock);
    if (status == BASE_SUCCESS) {
	status = ssl_do_handshake(ssock);
    }

    return status;
}

static void wipe_buf(bstr_t *buf)
{
    volatile char *p = buf->ptr;
    bssize_t len = buf->slen;
    while (len--) *p++ = 0;
    buf->slen = 0;
}

void bssl_cert_wipe_keys(bssl_cert_t *cert)
{    
    if (cert) {
	wipe_buf(&cert->CA_file);
	wipe_buf(&cert->CA_path);
	wipe_buf(&cert->cert_file);
	wipe_buf(&cert->privkey_file);
	wipe_buf(&cert->privkey_pass);
	wipe_buf(&cert->CA_buf);
	wipe_buf(&cert->cert_buf);
	wipe_buf(&cert->privkey_buf);
    }
}

/* Load credentials from files. */
bstatus_t bssl_cert_load_from_files (bpool_t *pool,
						 const bstr_t *CA_file,
						 const bstr_t *cert_file,
						 const bstr_t *privkey_file,
						 const bstr_t *privkey_pass,
						 bssl_cert_t **p_cert)
{
    return bssl_cert_load_from_files2(pool, CA_file, NULL, cert_file,
					privkey_file, privkey_pass, p_cert);
}

bstatus_t bssl_cert_load_from_files2(bpool_t *pool,
						 const bstr_t *CA_file,
						 const bstr_t *CA_path,
						 const bstr_t *cert_file,
						 const bstr_t *privkey_file,
						 const bstr_t *privkey_pass,
						 bssl_cert_t **p_cert)
{
    bssl_cert_t *cert;

    BASE_ASSERT_RETURN(pool && (CA_file || CA_path) && cert_file &&
		     privkey_file,
		     BASE_EINVAL);

    cert = BASE_POOL_ZALLOC_T(pool, bssl_cert_t);
    if (CA_file) {
    	bstrdup_with_null(pool, &cert->CA_file, CA_file);
    }
    if (CA_path) {
    	bstrdup_with_null(pool, &cert->CA_path, CA_path);
    }
    bstrdup_with_null(pool, &cert->cert_file, cert_file);
    bstrdup_with_null(pool, &cert->privkey_file, privkey_file);
    bstrdup_with_null(pool, &cert->privkey_pass, privkey_pass);

    *p_cert = cert;

    return BASE_SUCCESS;
}

bstatus_t bssl_cert_load_from_buffer(bpool_t *pool,
					const bssl_cert_buffer *CA_buf,
					const bssl_cert_buffer *cert_buf,
					const bssl_cert_buffer *privkey_buf,
					const bstr_t *privkey_pass,
					bssl_cert_t **p_cert)
{
    bssl_cert_t *cert;

    BASE_ASSERT_RETURN(pool && CA_buf && cert_buf && privkey_buf, BASE_EINVAL);

    cert = BASE_POOL_ZALLOC_T(pool, bssl_cert_t);
    bstrdup(pool, &cert->CA_buf, CA_buf);
    bstrdup(pool, &cert->cert_buf, cert_buf);
    bstrdup(pool, &cert->privkey_buf, privkey_buf);
    bstrdup_with_null(pool, &cert->privkey_pass, privkey_pass);

    *p_cert = cert;

    return BASE_SUCCESS;
}

/* Set SSL socket credentials. */
bstatus_t bssl_sock_set_certificate(
					    bssl_sock_t *ssock,
					    bpool_t *pool,
					    const bssl_cert_t *cert)
{
    bssl_cert_t *cert_;

    BASE_ASSERT_RETURN(ssock && pool && cert, BASE_EINVAL);

    cert_ = BASE_POOL_ZALLOC_T(pool, bssl_cert_t);
    bmemcpy(cert_, cert, sizeof(bssl_cert_t));
    bstrdup_with_null(pool, &cert_->CA_file, &cert->CA_file);
    bstrdup_with_null(pool, &cert_->CA_path, &cert->CA_path);
    bstrdup_with_null(pool, &cert_->cert_file, &cert->cert_file);
    bstrdup_with_null(pool, &cert_->privkey_file, &cert->privkey_file);
    bstrdup_with_null(pool, &cert_->privkey_pass, &cert->privkey_pass);

    bstrdup(pool, &cert_->CA_buf, &cert->CA_buf);
    bstrdup(pool, &cert_->cert_buf, &cert->cert_buf);
    bstrdup(pool, &cert_->privkey_buf, &cert->privkey_buf);

    ssock->cert = cert_;

    return BASE_SUCCESS;
}

