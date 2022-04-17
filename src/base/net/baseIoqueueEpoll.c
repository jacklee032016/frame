/* 
 *
 */
 
/*
 * This is the implementation of IOQueue framework using /dev/epoll
 * API in _both_ Linux user-mode and kernel-mode.
 */

#include <baseIoQueue.h>
#include <baseOs.h>
#include <baseLock.h>
#include <baseLog.h>
#include <baseList.h>
#include <basePool.h>
#include <baseString.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseSock.h>
#include <compat/socket.h>
#include <baseRand.h>

#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>

#define epoll_data		data.ptr
#define epoll_data_type		void*
#define ioctl_val_type		unsigned long
#define getsockopt_val_ptr	int*
#define os_getsockopt		getsockopt
#define os_ioctl		ioctl
#define os_read			read
#define os_close		close
#define os_epoll_create		epoll_create
#define os_epoll_ctl		epoll_ctl
#define os_epoll_wait		epoll_wait


/*
 * Include common ioqueue abstraction.
 */
#include "_baseIoqueueCommonAbs.h"

/*
 * This describes each key.
 */
struct bioqueue_key_t
{
    DECLARE_COMMON_KEY
};

struct queue
{
    bioqueue_key_t	    *key;
    enum ioqueue_event_type  event_type;
};

/*
 * This describes the I/O queue.
 */
struct bioqueue_t
{
    DECLARE_COMMON_IOQUEUE

    unsigned		max, count;
    //bioqueue_key_t	hlist;
    bioqueue_key_t	active_list;    
    int			epfd;
    //struct epoll_event *events;
    //struct queue       *queue;

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    bmutex_t	       *ref_cnt_mutex;
    bioqueue_key_t	closing_list;
    bioqueue_key_t	free_list;
#endif
};

/* Include implementation for common abstraction after we declare
 * bioqueue_key_t and bioqueue_t.
 */
#include "_baseIoqueueCommonAbs.c"

#if BASE_IOQUEUE_HAS_SAFE_UNREG
/* Scan closing keys to be put to free list again */
static void scan_closing_keys(bioqueue_t *ioqueue);
#endif

/*
 * bioqueue_name()
 */
const char* bioqueue_name(void)
{
    return "epoll";
}

/*
 * bioqueue_create()
 *
 * Create select ioqueue.
 */
bstatus_t bioqueue_create( bpool_t *pool, 
                                       bsize_t max_fd,
                                       bioqueue_t **p_ioqueue)
{
    bioqueue_t *ioqueue;
    bstatus_t rc;
    block_t *lock;
    int i;

    /* Check that arguments are valid. */
    BASE_ASSERT_RETURN(pool != NULL && p_ioqueue != NULL && 
                     max_fd > 0, BASE_EINVAL);

    /* Check that size of bioqueue_op_key_t is sufficient */
    BASE_ASSERT_RETURN(sizeof(bioqueue_op_key_t)-sizeof(void*) >=
                     sizeof(union operation_key), BASE_EBUG);

    ioqueue = bpool_alloc(pool, sizeof(bioqueue_t));

    ioqueue_init(ioqueue);

    ioqueue->max = max_fd;
    ioqueue->count = 0;
    blist_init(&ioqueue->active_list);

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* When safe unregistration is used (the default), we pre-create
     * all keys and put them in the free list.
     */

    /* Mutex to protect key's reference counter 
     * We don't want to use key's mutex or ioqueue's mutex because
     * that would create deadlock situation in some cases.
     */
    rc = bmutex_create_simple(pool, NULL, &ioqueue->ref_cnt_mutex);
    if (rc != BASE_SUCCESS)
	return rc;


    /* Init key list */
    blist_init(&ioqueue->free_list);
    blist_init(&ioqueue->closing_list);


    /* Pre-create all keys according to max_fd */
    for ( i=0; i<max_fd; ++i)
	{
		bioqueue_key_t *key;

		key = BASE_POOL_ALLOC_T(pool, bioqueue_key_t);
		key->ref_count = 0;
		rc = block_create_recursive_mutex(pool, NULL, &key->lock);
		if (rc != BASE_SUCCESS)
		{
			key = ioqueue->free_list.next;
			while (key != &ioqueue->free_list)
			{
				block_destroy(key->lock);
				key = key->next;
			}

			bmutex_destroy(ioqueue->ref_cnt_mutex);
			return rc;
		}

		blist_push_back(&ioqueue->free_list, key);
    }
#endif

    rc = block_create_simple_mutex(pool, "ioq%p", &lock);
    if (rc != BASE_SUCCESS)
		return rc;

    rc = bioqueue_set_lock(ioqueue, lock, BASE_TRUE);
    if (rc != BASE_SUCCESS)
        return rc;

    ioqueue->epfd = os_epoll_create(max_fd);
    if (ioqueue->epfd < 0)
	{
		ioqueue_destroy(ioqueue);
		return BASE_RETURN_OS_ERROR(bget_native_os_error());
    }
    
    /*ioqueue->events = bpool_calloc(pool, max_fd, sizeof(struct epoll_event));
    BASE_ASSERT_RETURN(ioqueue->events != NULL, BASE_ENOMEM);

    ioqueue->queue = bpool_calloc(pool, max_fd, sizeof(struct queue));
    BASE_ASSERT_RETURN(ioqueue->queue != NULL, BASE_ENOMEM);
   */
    BASE_INFO("epoll I/O Queue created (%p)", ioqueue);

    *p_ioqueue = ioqueue;
    return BASE_SUCCESS;
}

/*
 * bioqueue_destroy()
 *
 * Destroy ioqueue.
 */
bstatus_t bioqueue_destroy(bioqueue_t *ioqueue)
{
    bioqueue_key_t *key;

    BASE_ASSERT_RETURN(ioqueue, BASE_EINVAL);
    BASE_ASSERT_RETURN(ioqueue->epfd > 0, BASE_EINVALIDOP);

    block_acquire(ioqueue->lock);
    os_close(ioqueue->epfd);
    ioqueue->epfd = 0;

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Destroy reference counters */
    key = ioqueue->active_list.next;
    while (key != &ioqueue->active_list) {
	block_destroy(key->lock);
	key = key->next;
    }

    key = ioqueue->closing_list.next;
    while (key != &ioqueue->closing_list) {
	block_destroy(key->lock);
	key = key->next;
    }

    key = ioqueue->free_list.next;
    while (key != &ioqueue->free_list) {
	block_destroy(key->lock);
	key = key->next;
    }

    bmutex_destroy(ioqueue->ref_cnt_mutex);
#endif
    return ioqueue_destroy(ioqueue);
}

/*
 * bioqueue_register_sock()
 *
 * Register a socket to ioqueue.
 */
bstatus_t bioqueue_register_sock2(bpool_t *pool,
					      bioqueue_t *ioqueue,
					      bsock_t sock,
					      bgrp_lock_t *grp_lock,
					      void *user_data,
					      const bioqueue_callback *cb,
                                              bioqueue_key_t **p_key)
{
    bioqueue_key_t *key = NULL;
    buint32_t value;
    struct epoll_event ev;
    int status;
    bstatus_t rc = BASE_SUCCESS;
    
    BASE_ASSERT_RETURN(pool && ioqueue && sock != BASE_INVALID_SOCKET &&
                     cb && p_key, BASE_EINVAL);

    block_acquire(ioqueue->lock);

    if (ioqueue->count >= ioqueue->max) {
        rc = BASE_ETOOMANY;
	MTRACE( "bioqueue_register_sock error: too many files");
	goto on_return;
    }

    /* Set socket to nonblocking. */
    value = 1;
    if ((rc=os_ioctl(sock, FIONBIO, (ioctl_val_type)&value))) {
	MTRACE("bioqueue_register_sock error: ioctl rc=%d", rc);
        rc = bget_netos_error();
	goto on_return;
    }

    /* If safe unregistration (BASE_IOQUEUE_HAS_SAFE_UNREG) is used, get
     * the key from the free list. Otherwise allocate a new one. 
     */
#if BASE_IOQUEUE_HAS_SAFE_UNREG

    /* Scan closing_keys first to let them come back to free_list */
    scan_closing_keys(ioqueue);

    bassert(!blist_empty(&ioqueue->free_list));
    if (blist_empty(&ioqueue->free_list)) {
	rc = BASE_ETOOMANY;
	goto on_return;
    }

    key = ioqueue->free_list.next;
    blist_erase(key);
#else
    /* Create key. */
    key = (bioqueue_key_t*)bpool_zalloc(pool, sizeof(bioqueue_key_t));
#endif

    rc = ioqueue_init_key(pool, ioqueue, key, sock, grp_lock, user_data, cb);
    if (rc != BASE_SUCCESS) {
	key = NULL;
	goto on_return;
    }

    /* Create key's mutex */
 /*   rc = bmutex_create_recursive(pool, NULL, &key->mutex);
    if (rc != BASE_SUCCESS) {
	key = NULL;
	goto on_return;
    }
*/
    /* os_epoll_ctl. */
    ev.events = EPOLLIN | EPOLLERR;
    ev.epoll_data = (epoll_data_type)key;
    status = os_epoll_ctl(ioqueue->epfd, EPOLL_CTL_ADD, sock, &ev);
    if (status < 0) {
	rc = bget_os_error();
	block_destroy(key->lock);
	key = NULL;
	MTRACE("bioqueue_register_sock error: os_epoll_ctl rc=%d", 	status);
	goto on_return;
    }
    
    /* Register */
    blist_insert_before(&ioqueue->active_list, key);
    ++ioqueue->count;

    MTRACE( "socket registered, count=%d", ioqueue->count);

on_return:
    if (rc != BASE_SUCCESS) {
	if (key && key->grp_lock)
	    bgrp_lock_dec_ref_dbg(key->grp_lock, "ioqueue", 0);
    }
    *p_key = key;
    block_release(ioqueue->lock);
    
    return rc;
}

bstatus_t bioqueue_register_sock( bpool_t *pool,
					      bioqueue_t *ioqueue,
					      bsock_t sock,
					      void *user_data,
					      const bioqueue_callback *cb,
					      bioqueue_key_t **p_key)
{
    return bioqueue_register_sock2(pool, ioqueue, sock, NULL, user_data,
                                     cb, p_key);
}

#if BASE_IOQUEUE_HAS_SAFE_UNREG
/* Increment key's reference counter */
static void increment_counter(bioqueue_key_t *key)
{
    bmutex_lock(key->ioqueue->ref_cnt_mutex);
    ++key->ref_count;
    bmutex_unlock(key->ioqueue->ref_cnt_mutex);
}

/* Decrement the key's reference counter, and when the counter reach zero,
 * destroy the key.
 *
 * Note: MUST NOT CALL THIS FUNCTION WHILE HOLDING ioqueue's LOCK.
 */
static void decrement_counter(bioqueue_key_t *key)
{
    block_acquire(key->ioqueue->lock);
    bmutex_lock(key->ioqueue->ref_cnt_mutex);
    --key->ref_count;
    if (key->ref_count == 0) {

	bassert(key->closing == 1);
	bgettickcount(&key->free_time);
	key->free_time.msec += BASE_IOQUEUE_KEY_FREE_DELAY;
	btime_val_normalize(&key->free_time);

	blist_erase(key);
	blist_push_back(&key->ioqueue->closing_list, key);

    }
    bmutex_unlock(key->ioqueue->ref_cnt_mutex);
    block_release(key->ioqueue->lock);
}
#endif

/*
 * bioqueue_unregister()
 *
 * Unregister handle from ioqueue.
 */
bstatus_t bioqueue_unregister( bioqueue_key_t *key)
{
    bioqueue_t *ioqueue;
    struct epoll_event ev;
    int status;
    
    BASE_ASSERT_RETURN(key != NULL, BASE_EINVAL);

    ioqueue = key->ioqueue;

    /* Lock the key to make sure no callback is simultaneously modifying
     * the key. We need to lock the key before ioqueue here to prevent
     * deadlock.
     */
    bioqueue_lock_key(key);

    /* Best effort to avoid double key-unregistration */
    if (IS_CLOSING(key)) {
	bioqueue_unlock_key(key);
	return BASE_SUCCESS;
    }

    /* Also lock ioqueue */
    block_acquire(ioqueue->lock);

    /* Avoid "negative" ioqueue count */
    if (ioqueue->count > 0) {
	--ioqueue->count;
    } else {
	/* If this happens, very likely there is double unregistration
	 * of a key.
	 */
	bassert(!"Bad ioqueue count in key unregistration!");
	BASE_CRIT("Bad ioqueue count in key unregistration!");
    }

#if !BASE_IOQUEUE_HAS_SAFE_UNREG
    blist_erase(key);
#endif

    ev.events = 0;
    ev.epoll_data = (epoll_data_type)key;
    status = os_epoll_ctl( ioqueue->epfd, EPOLL_CTL_DEL, key->fd, &ev);
    if (status != 0) {
	bstatus_t rc = bget_os_error();
	block_release(ioqueue->lock);
	bioqueue_unlock_key(key);
	return rc;
    }

    /* Destroy the key. */
    bsock_close(key->fd);

    block_release(ioqueue->lock);


#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Mark key is closing. */
    key->closing = 1;

    /* Decrement counter. */
    decrement_counter(key);

    /* Done. */
    if (key->grp_lock) {
	/* just dec_ref and unlock. we will set grp_lock to NULL
	 * elsewhere */
	bgrp_lock_t *grp_lock = key->grp_lock;
	// Don't set grp_lock to NULL otherwise the other thread
	// will crash. Just leave it as dangling pointer, but this
	// should be safe
	//key->grp_lock = NULL;
	bgrp_lock_dec_ref_dbg(grp_lock, "ioqueue", 0);
	bgrp_lock_release(grp_lock);
    } else {
	bioqueue_unlock_key(key);
    }
#else
    if (key->grp_lock) {
	/* set grp_lock to NULL and unlock */
	bgrp_lock_t *grp_lock = key->grp_lock;
	// Don't set grp_lock to NULL otherwise the other thread
	// will crash. Just leave it as dangling pointer, but this
	// should be safe
	//key->grp_lock = NULL;
	bgrp_lock_dec_ref_dbg(grp_lock, "ioqueue", 0);
	bgrp_lock_release(grp_lock);
    } else {
	bioqueue_unlock_key(key);
    }

    block_destroy(key->lock);
#endif

    return BASE_SUCCESS;
}

/* ioqueue_remove_from_set()
 * This function is called from ioqueue_dispatch_event() to instruct
 * the ioqueue to remove the specified descriptor from ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_remove_from_set( bioqueue_t *ioqueue,
                                     bioqueue_key_t *key, 
                                     enum ioqueue_event_type event_type)
{
    if (event_type == WRITEABLE_EVENT) {
	struct epoll_event ev;

	ev.events = EPOLLIN | EPOLLERR;
	ev.epoll_data = (epoll_data_type)key;
	os_epoll_ctl( ioqueue->epfd, EPOLL_CTL_MOD, key->fd, &ev);
    }	
}

/*
 * ioqueue_add_to_set()
 * This function is called from bioqueue_recv(), bioqueue_send() etc
 * to instruct the ioqueue to add the specified handle to ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_add_to_set( bioqueue_t *ioqueue,
                                bioqueue_key_t *key,
                                enum ioqueue_event_type event_type )
{
    if (event_type == WRITEABLE_EVENT) {
	struct epoll_event ev;

	ev.events = EPOLLIN | EPOLLOUT | EPOLLERR;
	ev.epoll_data = (epoll_data_type)key;
	os_epoll_ctl( ioqueue->epfd, EPOLL_CTL_MOD, key->fd, &ev);
    }	
}

#if BASE_IOQUEUE_HAS_SAFE_UNREG
/* Scan closing keys to be put to free list again */
static void scan_closing_keys(bioqueue_t *ioqueue)
{
    btime_val now;
    bioqueue_key_t *h;

    bgettickcount(&now);
    h = ioqueue->closing_list.next;
    while (h != &ioqueue->closing_list) {
	bioqueue_key_t *next = h->next;

	bassert(h->closing != 0);

	if (BASE_TIME_VAL_GTE(now, h->free_time)) {
	    blist_erase(h);
	    // Don't set grp_lock to NULL otherwise the other thread
	    // will crash. Just leave it as dangling pointer, but this
	    // should be safe
	    //h->grp_lock = NULL;
	    blist_push_back(&ioqueue->free_list, h);
	}
	h = next;
    }
}
#endif

/*
 * bioqueue_poll()
 *
 */
int bioqueue_poll( bioqueue_t *ioqueue, const btime_val *timeout)
{
    int i, count, event_cnt, processed_cnt;
    int msec;
    //struct epoll_event *events = ioqueue->events;
    //struct queue *queue = ioqueue->queue;
    enum { MAX_EVENTS = BASE_IOQUEUE_MAX_CAND_EVENTS };
    struct epoll_event events[MAX_EVENTS];
    struct queue queue[MAX_EVENTS];
    btimestamp t1, t2;
    
    BASE_CHECK_STACK();

    msec = timeout ? BASE_TIME_VAL_MSEC(*timeout) : 9000;

    MTRACE( "start os_epoll_wait, msec=%d", msec);
    bTimeStampGet(&t1);
 
    //count = os_epoll_wait( ioqueue->epfd, events, ioqueue->max, msec);
    count = os_epoll_wait( ioqueue->epfd, events, MAX_EVENTS, msec);
    if (count == 0)
	{
#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Check the closing keys only when there's no activity and when there are
     * pending closing keys.
     */
		if (count == 0 && !blist_empty(&ioqueue->closing_list))
		{
			block_acquire(ioqueue->lock);
			scan_closing_keys(ioqueue);
			block_release(ioqueue->lock);
		}
#endif
		MTRACE("os_epoll_wait timed out");
		return count;
    }
    else if (count < 0)
	{
		MTRACE( "os_epoll_wait error");
		return -bget_netos_error();
    }

    bTimeStampGet(&t2);
    MTRACE("os_epoll_wait returns %d, time=%d usec", count, belapsed_usec(&t1, &t2));

    /* Lock ioqueue. */
    block_acquire(ioqueue->lock);

    for (event_cnt=0, i=0; i<count; ++i)
	{
		bioqueue_key_t *h = (bioqueue_key_t*)(epoll_data_type)events[i].epoll_data;

		MTRACE("event %d: events=%d", i, events[i].events);
		/*
		* Check readability.
		*/
		if ((events[i].events & EPOLLIN) && (key_has_pending_read(h) || key_has_pending_accept(h)) && !IS_CLOSING(h) )
		{

#if BASE_IOQUEUE_HAS_SAFE_UNREG
	    	increment_counter(h);
#endif
		    queue[event_cnt].key = h;
		    queue[event_cnt].event_type = READABLE_EVENT;
	    	++event_cnt;
		    continue;
		}

		/*
		* Check for writeability.
		*/

		if ((events[i].events & EPOLLOUT) && key_has_pending_write(h) && !IS_CLOSING(h))
		{
#if BASE_IOQUEUE_HAS_SAFE_UNREG
		    increment_counter(h);
#endif
		    queue[event_cnt].key = h;
	    	queue[event_cnt].event_type = WRITEABLE_EVENT;
		    ++event_cnt;
		    continue;
		}

#if BASE_HAS_TCP
		/*
		* Check for completion of connect() operation.
		*/
		if ((events[i].events & EPOLLOUT) && (h->connecting) && !IS_CLOSING(h))
		{
#if BASE_IOQUEUE_HAS_SAFE_UNREG
		    increment_counter(h);
#endif
		    queue[event_cnt].key = h;
	    	queue[event_cnt].event_type = WRITEABLE_EVENT;
		    ++event_cnt;
		    continue;
		}
#endif /* BASE_HAS_TCP */

		/*
		* Check for error condition.
		*/
		if ((events[i].events & EPOLLERR) && !IS_CLOSING(h))
		{
			/*
			* We need to handle this exception event.  If it's related to us
			* connecting, report it as such.  If not, just report it as a
			* read event and the higher layers will handle it.
			*/

			if (h->connecting)
			{
#if BASE_IOQUEUE_HAS_SAFE_UNREG
				increment_counter(h);
#endif
				queue[event_cnt].key = h;
				queue[event_cnt].event_type = EXCEPTION_EVENT;
				++event_cnt;
			}
			else if (key_has_pending_read(h) || key_has_pending_accept(h))
			{
#if BASE_IOQUEUE_HAS_SAFE_UNREG
				increment_counter(h);
#endif
				queue[event_cnt].key = h;
				queue[event_cnt].event_type = READABLE_EVENT;
				++event_cnt;
	    	}
			continue;
		}
	}
	
    for (i=0; i<event_cnt; ++i)
	{
		if (queue[i].key->grp_lock)
			bgrp_lock_add_ref_dbg(queue[i].key->grp_lock, "ioqueue", 0);
	}

    BASE_RACE_ME(5);

    block_release(ioqueue->lock);

    BASE_RACE_ME(5);

    processed_cnt = 0;

    /* Now process the events. */
    for (i=0; i<event_cnt; ++i)
	{
		/* Just do not exceed BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL */
		if (processed_cnt < BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL)
		{
			switch (queue[i].event_type)
			{
				case READABLE_EVENT:
					if (ioqueue_dispatch_read_event(ioqueue, queue[i].key))
						++processed_cnt;
					break;
					
				case WRITEABLE_EVENT:
					if (ioqueue_dispatch_write_event(ioqueue, queue[i].key))
						++processed_cnt;
					break;

				case EXCEPTION_EVENT:
					if (ioqueue_dispatch_exception_event(ioqueue, queue[i].key))
						++processed_cnt;
					break;

				case NO_EVENT:
					bassert(!"Invalid event!");
					break;
			}
		}

#if BASE_IOQUEUE_HAS_SAFE_UNREG
		decrement_counter(queue[i].key);
#endif

		if (queue[i].key->grp_lock)
			bgrp_lock_dec_ref_dbg(queue[i].key->grp_lock, "ioqueue", 0);
		
    }

    /* Special case:
     * When epoll returns > 0 but no descriptors are actually set!
     */
    if (count > 0 && !event_cnt && msec > 0)
	{
		bthreadSleepMs(msec);
	}

    MTRACE("     poll: count=%d events=%d processed=%d", count, event_cnt, processed_cnt);

    bTimeStampGet(&t1);
    MTRACE("ioqueue_poll() returns %d, time=%d usec",  processed, belapsed_usec(&t2, &t1));

    return processed_cnt;
}

