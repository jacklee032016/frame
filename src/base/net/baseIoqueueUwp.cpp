/* 
 *
 */
#include <baseIoQueue.h>
#include <baseErrno.h>
#include <baseLock.h>
#include <baseLog.h>
#include <baseOs.h>
#include <basePool.h>

#include <ppltasks.h>
#include <string>


#include "sock_uwp.h"
#include "_baseIoqueueCommonAbs.h"

 /*
 * This describes each key.
 */
struct bioqueue_key_t
{
    DECLARE_COMMON_KEY
};

/*
* This describes the I/O queue itself.
*/
struct bioqueue_t
{
    DECLARE_COMMON_IOQUEUE
    bthread_desc   thread_desc[16];
    unsigned	     thread_cnt;
};


#include "_baseIoqueueCommonAbs.c"

static void ioqueue_remove_from_set( bioqueue_t *ioqueue,
				     bioqueue_key_t *key, 
				     enum ioqueue_event_type event_type)
{
    BASE_UNUSED_ARG(ioqueue);
    BASE_UNUSED_ARG(key);
    BASE_UNUSED_ARG(event_type);
}


static void start_nbread(bioqueue_key_t *key)
{
    if (key_has_pending_read(key)) {
	PjUwpSocket *s = (PjUwpSocket*)key->fd;
	struct read_operation *op;
	op = (struct read_operation*)key->read_list.next;

	if (op->op == BASE_IOQUEUE_OP_RECV)
	    s->Recv(NULL, (bssize_t*)&op->size);
	else
	    s->RecvFrom(NULL, (bssize_t*)&op->size, NULL);
    }
}


static void start_nbwrite(bioqueue_key_t *key)
{
    if (key_has_pending_write(key)) {
	PjUwpSocket *s = (PjUwpSocket*)key->fd;
	struct write_operation *op;
	op = (struct write_operation*)key->write_list.next;

	if (op->op == BASE_IOQUEUE_OP_SEND)
	    s->Send(op->buf, (bssize_t*)&op->size);
	else
	    s->SendTo(op->buf, (bssize_t*)&op->size, &op->rmt_addr);
    }
}


static void ioqueue_add_to_set( bioqueue_t *ioqueue,
				bioqueue_key_t *key,
				enum ioqueue_event_type event_type )
{
    BASE_UNUSED_ARG(ioqueue);

    if (event_type == READABLE_EVENT) {
	/* This is either recv, recvfrom, or accept, do nothing on accept */
	start_nbread(key);
    } else if (event_type == WRITEABLE_EVENT) {
	/* This is either send, sendto, or connect, do nothing on connect */
	//start_nbwrite(key);
    }
}


static void check_thread(bioqueue_t *ioq) {
    if (bthreadIsRegistered())
	return;

    bthread_t *t;
    char tmp[16];
    bansi_snprintf(tmp, sizeof(tmp), "UwpThread%02d", ioq->thread_cnt);
    bthreadRegister(tmp, ioq->thread_desc[ioq->thread_cnt++], &t);
    bassert(ioq->thread_cnt < BASE_ARRAY_SIZE(ioq->thread_desc));
    ioq->thread_cnt %= BASE_ARRAY_SIZE(ioq->thread_desc);
}

static void on_read(PjUwpSocket *s, int bytes_read)
{
    bioqueue_key_t *key = (bioqueue_key_t*)s->GetUserData();
    bioqueue_t *ioq = key->ioqueue;
    check_thread(ioq);

    ioqueue_dispatch_read_event(key->ioqueue, key);
    
    if (bytes_read > 0)
	start_nbread(key);
}

static void on_write(PjUwpSocket *s, int bytes_sent)
{
    BASE_UNUSED_ARG(bytes_sent);
    bioqueue_key_t *key = (bioqueue_key_t*)s->GetUserData();
    bioqueue_t *ioq = key->ioqueue;
    check_thread(ioq);

    ioqueue_dispatch_write_event(key->ioqueue, key);

    //start_nbwrite(key);
}

static void on_accept(PjUwpSocket *s)
{
    bioqueue_key_t *key = (bioqueue_key_t*)s->GetUserData();
    bioqueue_t *ioq = key->ioqueue;
    check_thread(ioq);

    ioqueue_dispatch_read_event(key->ioqueue, key);
}

static void on_connect(PjUwpSocket *s, bstatus_t status)
{
    BASE_UNUSED_ARG(status);
    bioqueue_key_t *key = (bioqueue_key_t*)s->GetUserData();
    bioqueue_t *ioq = key->ioqueue;
    check_thread(ioq);

    ioqueue_dispatch_write_event(key->ioqueue, key);
}


/*
 * Return the name of the ioqueue implementation.
 */
const char* bioqueue_name(void)
{
    return "ioqueue-uwp";
}


/*
 * Create a new I/O Queue framework.
 */
bstatus_t bioqueue_create(	bpool_t *pool, 
					bsize_t max_fd,
					bioqueue_t **p_ioqueue)
{
    bioqueue_t *ioq;
    block_t *lock;
    bstatus_t rc;

    BASE_UNUSED_ARG(max_fd);

    ioq = BASE_POOL_ZALLOC_T(pool, bioqueue_t);

    /* Create and init ioqueue mutex */
    rc = block_create_null_mutex(pool, "ioq%p", &lock);
    if (rc != BASE_SUCCESS)
	return rc;

    rc = bioqueue_set_lock(ioq, lock, BASE_TRUE);
    if (rc != BASE_SUCCESS)
	return rc;

    BASE_INFO("select() I/O Queue created (%p)", ioq);

    *p_ioqueue = ioq;
    return BASE_SUCCESS;
}


/*
 * Destroy the I/O queue.
 */
bstatus_t bioqueue_destroy( bioqueue_t *ioq )
{
    return ioqueue_destroy(ioq);
}


/*
 * Register a socket to the I/O queue framework. 
 */
bstatus_t bioqueue_register_sock( bpool_t *pool,
					      bioqueue_t *ioqueue,
					      bsock_t sock,
					      void *user_data,
					      const bioqueue_callback *cb,
                                              bioqueue_key_t **p_key )
{
    return bioqueue_register_sock2(pool, ioqueue, sock, NULL, user_data,
				     cb, p_key);
}

bstatus_t bioqueue_register_sock2(bpool_t *pool,
					      bioqueue_t *ioqueue,
					      bsock_t sock,
					      bgrp_lock_t *grp_lock,
					      void *user_data,
					      const bioqueue_callback *cb,
                                              bioqueue_key_t **p_key)
{
    PjUwpSocketCallback uwp_cb =
			    { &on_read, &on_write, &on_accept, &on_connect };
    bioqueue_key_t *key;
    bstatus_t rc;

    block_acquire(ioqueue->lock);

    key = BASE_POOL_ZALLOC_T(pool, bioqueue_key_t);
    rc = ioqueue_init_key(pool, ioqueue, key, sock, grp_lock, user_data, cb);
    if (rc != BASE_SUCCESS) {
	key = NULL;
	goto on_return;
    }

    /* Create ioqueue key lock, if not yet */
    if (!key->lock) {
	rc = block_create_simple_mutex(pool, NULL, &key->lock);
	if (rc != BASE_SUCCESS) {
	    key = NULL;
	    goto on_return;
	}
    }

    PjUwpSocket *s = (PjUwpSocket*)sock;
    s->SetNonBlocking(&uwp_cb, key);

on_return:
    if (rc != BASE_SUCCESS) {
	if (key && key->grp_lock)
	    bgrp_lock_dec_ref_dbg(key->grp_lock, "ioqueue", 0);
    }
    *p_key = key;
    block_release(ioqueue->lock);

    return rc;

}

/*
 * Unregister from the I/O Queue framework. 
 */
bstatus_t bioqueue_unregister( bioqueue_key_t *key )
{
    bioqueue_t *ioqueue;

    BASE_ASSERT_RETURN(key, BASE_EINVAL);

    ioqueue = key->ioqueue;

    /* Lock the key to make sure no callback is simultaneously modifying
     * the key. We need to lock the key before ioqueue here to prevent
     * deadlock.
     */
    bioqueue_lock_key(key);

    /* Also lock ioqueue */
    block_acquire(ioqueue->lock);

    /* Close socket. */
    bsock_close(key->fd);

    /* Clear callback */
    key->cb.on_accept_complete = NULL;
    key->cb.on_connect_complete = NULL;
    key->cb.on_read_complete = NULL;
    key->cb.on_write_complete = NULL;

    block_release(ioqueue->lock);

    if (key->grp_lock) {
	bgrp_lock_t *grp_lock = key->grp_lock;
	bgrp_lock_dec_ref_dbg(grp_lock, "ioqueue", 0);
	bgrp_lock_release(grp_lock);
    } else {
	bioqueue_unlock_key(key);
    }

    block_destroy(key->lock);

    return BASE_SUCCESS;
}


/*
 * Poll the I/O Queue for completed events.
 */
int bioqueue_poll( bioqueue_t *ioq, const btime_val *timeout)
{
	/* Polling is not necessary on UWP, since each socket handles
	* its events already.
	*/
	BASE_UNUSED_ARG(ioq);

	bthreadSleepMs(BASE_TIME_VAL_MSEC(*timeout));

	return 0;
}

