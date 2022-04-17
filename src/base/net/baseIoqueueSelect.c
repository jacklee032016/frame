/* 
 *
 */

/*
 * sock_select.c
 * This is the implementation of IOQueue using bsock_select().
 * It runs anywhere where bsock_select() is available (currently
 * Win32, Linux, Linux kernel, etc.).
 */

#include <baseIoQueue.h>
#include <baseOs.h>
#include <baseLock.h>
#include <baseLog.h>
#include <baseList.h>
#include <basePool.h>
#include <baseString.h>
#include <baseAssert.h>
#include <baseSock.h>
#include <compat/socket.h>
#include <baseSockSelect.h>
#include <baseSockQos.h>
#include <baseErrno.h>
#include <baseRand.h>

/* Now that we have access to OS'es <sys/select>, lets check again that
 * BASE_IOQUEUE_MAX_HANDLES is not greater than FD_SETSIZE
 */
#if BASE_IOQUEUE_MAX_HANDLES > FD_SETSIZE
#   error "BASE_IOQUEUE_MAX_HANDLES cannot be greater than FD_SETSIZE"
#endif


/*
 * Include declaration from common abstraction.
 */
#include "_baseIoqueueCommonAbs.h"

/*
 * ISSUES with ioqueue_select()
 *
 * EAGAIN/EWOULDBLOCK error in recv():
 *  - when multiple threads are working with the ioqueue, application
 *    may receive EAGAIN or EWOULDBLOCK in the receive callback.
 *    This error happens because more than one thread is watching for
 *    the same descriptor set, so when all of them call recv() or recvfrom()
 *    simultaneously, only one will succeed and the rest will get the error.
 *
 */

/*
 * The select ioqueue relies on socket functions (bsock_xxx()) to return
 * the correct error code.
 */
#if BASE_RETURN_OS_ERROR(100) != BASE_STATUS_FROM_OS(100)
#   error "Error reporting must be enabled for this function to work!"
#endif

/*
 * During debugging build, VALIDATE_FD_SET is set.
 * This will check the validity of the fd_sets.
 */
/*
#if defined(BASE_LIB_DEBUG) && BASE_LIB_DEBUG != 0
#  define VALIDATE_FD_SET		1
#else
#  define VALIDATE_FD_SET		0
#endif
*/
#define VALIDATE_FD_SET     0


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

    unsigned		max, count;	/* Max and current key count	    */
    int			nfds;		/* The largest fd value (for select)*/
    bioqueue_key_t	active_list;	/* List of active keys.		    */
    bfd_set_t		rfdset;
    bfd_set_t		wfdset;
#if BASE_HAS_TCP
    bfd_set_t		xfdset;
#endif

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    bmutex_t	       *ref_cnt_mutex;
    bioqueue_key_t	closing_list;
    bioqueue_key_t	free_list;
#endif
};

/* Proto */
#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	    BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
static bstatus_t replace_udp_sock(bioqueue_key_t *h);
#endif

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
    return "select";
}

/* 
 * Scan the socket descriptor sets for the largest descriptor.
 * This value is needed by select().
 */
#if defined(BASE_SELECT_NEEDS_NFDS) && BASE_SELECT_NEEDS_NFDS!=0
static void rescan_fdset(bioqueue_t *ioqueue)
{
    bioqueue_key_t *key = ioqueue->active_list.next;
    int max = 0;

    while (key != &ioqueue->active_list) {
	if (key->fd > max)
	    max = key->fd;
	key = key->next;
    }

    ioqueue->nfds = max;
}
#else
static void rescan_fdset(bioqueue_t *ioqueue)
{
    ioqueue->nfds = FD_SETSIZE-1;
}
#endif


/*
 * bioqueue_create()
 *
 * Create select ioqueue.
 */
bstatus_t bioqueue_create( bpool_t *pool, bsize_t max_fd, bioqueue_t **p_ioqueue)
{
	bioqueue_t *ioqueue;
	block_t *lock;
	unsigned i;
	bstatus_t rc;

	/* Check that arguments are valid. */
	BASE_ASSERT_RETURN(pool != NULL && p_ioqueue != NULL && max_fd > 0 && max_fd <= BASE_IOQUEUE_MAX_HANDLES,  BASE_EINVAL);

	/* Check that size of bioqueue_op_key_t is sufficient */
	BASE_ASSERT_RETURN(sizeof(bioqueue_op_key_t)-sizeof(void*) >= sizeof(union operation_key), BASE_EBUG);

	/* Create and init common ioqueue stuffs */
	ioqueue = BASE_POOL_ALLOC_T(pool, bioqueue_t);
	ioqueue_init(ioqueue);

	ioqueue->max = (unsigned)max_fd;
	ioqueue->count = 0;
	BASE_FD_ZERO(&ioqueue->rfdset);
	BASE_FD_ZERO(&ioqueue->wfdset);
#if BASE_HAS_TCP
	BASE_FD_ZERO(&ioqueue->xfdset);
#endif
	blist_init(&ioqueue->active_list);

	rescan_fdset(ioqueue);

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
	for (i=0; i<max_fd; ++i)
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

	/* Create and init ioqueue mutex */
	rc = block_create_simple_mutex(pool, "ioq%p", &lock);
	if (rc != BASE_SUCCESS)
		return rc;

	rc = bioqueue_set_lock(ioqueue, lock, BASE_TRUE);
	if (rc != BASE_SUCCESS)
		return rc;

	BASE_INFO("select() I/O Queue created (%p)", ioqueue);

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

	block_acquire(ioqueue->lock);

#if BASE_IOQUEUE_HAS_SAFE_UNREG
	/* Destroy reference counters */
	key = ioqueue->active_list.next;

	while (key != &ioqueue->active_list)
	{
		block_destroy(key->lock);
		key = key->next;
	}

	key = ioqueue->closing_list.next;
	while (key != &ioqueue->closing_list)
	{
		block_destroy(key->lock);
		key = key->next;
	}

	key = ioqueue->free_list.next;
	while (key != &ioqueue->free_list)
	{
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
 * Register socket handle to ioqueue.
 */
bstatus_t bioqueue_register_sock2(bpool_t *pool, bioqueue_t *ioqueue, bsock_t sock,
	bgrp_lock_t *grp_lock, void *user_data, const bioqueue_callback *cb, bioqueue_key_t **p_key)
{
	bioqueue_key_t *key = NULL;
#if (defined(BASE_WIN32) && BASE_WIN32!=0 || defined(BASE_WIN64) && BASE_WIN64 != 0 || defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE!=0 )
//#if  (BASE_WIN32|| BASE_WIN64 )
	u_long value;
#else
	buint32_t value;
#endif
	bstatus_t rc = BASE_SUCCESS;

	BASE_ASSERT_RETURN(pool && ioqueue && sock != BASE_INVALID_SOCKET && cb && p_key, BASE_EINVAL);

	/* On platforms with fd_set containing fd bitmap such as *nix family,
	* avoid potential memory corruption caused by select() when given
	* an fd that is higher than FD_SETSIZE.
	*/
	if (sizeof(fd_set) < FD_SETSIZE && sock >= FD_SETSIZE)
	{
		BASE_ERROR("Failed to register socket to ioqueue because socket fd is too big (fd=%d/FD_SETSIZE=%d)", sock, FD_SETSIZE);
		return BASE_ETOOBIG;
	}

	block_acquire(ioqueue->lock);

	if (ioqueue->count >= ioqueue->max)
	{
		rc = BASE_ETOOMANY;
		goto on_return;
	}

	/* If safe unregistration (BASE_IOQUEUE_HAS_SAFE_UNREG) is used, get
	* the key from the free list. Otherwise allocate a new one. 
	*/
#if BASE_IOQUEUE_HAS_SAFE_UNREG

	/* Scan closing_keys first to let them come back to free_list */
	scan_closing_keys(ioqueue);

	bassert(!blist_empty(&ioqueue->free_list));
	if (blist_empty(&ioqueue->free_list))
	{
		rc = BASE_ETOOMANY;
		goto on_return;
	}

	key = ioqueue->free_list.next;
	blist_erase(key);
#else
	key = (bioqueue_key_t*)bpool_zalloc(pool, sizeof(bioqueue_key_t));
#endif

	rc = ioqueue_init_key(pool, ioqueue, key, sock, grp_lock, user_data, cb);
	if (rc != BASE_SUCCESS)
	{
		key = NULL;
		goto on_return;
	}

	/* Set socket to nonblocking. */
	value = 1;
#if defined(BASE_WIN32) && BASE_WIN32!=0 || \
	defined(BASE_WIN64) && BASE_WIN64 != 0 || \
	defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE!=0
	if (ioctlsocket(sock, FIONBIO, &value))
#else
	if (ioctl(sock, FIONBIO, &value))
#endif
	{
		rc = bget_netos_error();
		goto on_return;
	}


	/* Put in active list. */
	blist_insert_before(&ioqueue->active_list, key);
	++ioqueue->count;

	/* Rescan fdset to get max descriptor */
	rescan_fdset(ioqueue);

on_return:
	/* On error, socket may be left in non-blocking mode. */
	if (rc != BASE_SUCCESS)
	{
		if (key && key->grp_lock)
			bgrp_lock_dec_ref_dbg(key->grp_lock, "ioqueue", 0);
	}

	*p_key = key;
	block_release(ioqueue->lock);

	return rc;
}

bstatus_t bioqueue_register_sock( bpool_t *pool, bioqueue_t *ioqueue, bsock_t sock, 
	void *user_data, const bioqueue_callback *cb, bioqueue_key_t **p_key)
{
    return bioqueue_register_sock2(pool, ioqueue, sock, NULL, user_data, cb, p_key);
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

	if (key->ref_count == 0)
	{
		bassert(key->closing == 1);
		bgettickcount(&key->free_time);
		key->free_time.msec += BASE_IOQUEUE_KEY_FREE_DELAY;
		btime_val_normalize(&key->free_time);

		blist_erase(key);
		blist_push_back(&key->ioqueue->closing_list, key);
		/* Rescan fdset to get max descriptor */
		rescan_fdset(key->ioqueue);
	}

	bmutex_unlock(key->ioqueue->ref_cnt_mutex);
	block_release(key->ioqueue->lock);
}
#endif


/*
 * bioqueue_unregister()
 * Unregister handle from ioqueue.
 */
bstatus_t bioqueue_unregister( bioqueue_key_t *key)
{
	bioqueue_t *ioqueue;

	BASE_ASSERT_RETURN(key, BASE_EINVAL);

	ioqueue = key->ioqueue;

	/* Lock the key to make sure no callback is simultaneously modifying
	* the key. We need to lock the key before ioqueue here to prevent
	* deadlock.
	*/
	bioqueue_lock_key(key);

	/* Best effort to avoid double key-unregistration */
	if (IS_CLOSING(key))
	{
		bioqueue_unlock_key(key);
		return BASE_SUCCESS;
	}

	/* Also lock ioqueue */
	block_acquire(ioqueue->lock);

	/* Avoid "negative" ioqueue count */
	if (ioqueue->count > 0)
	{
		--ioqueue->count;
	}
	else
	{
		/* If this happens, very likely there is double unregistration of a key.*/
		bassert(!"Bad ioqueue count in key unregistration!");
		BASE_CRIT("Bad ioqueue count in key unregistration!");
	}

#if !BASE_IOQUEUE_HAS_SAFE_UNREG
	/* Ticket #520, key will be erased more than once */
	blist_erase(key);
#endif
	BASE_FD_CLR(key->fd, &ioqueue->rfdset);
	BASE_FD_CLR(key->fd, &ioqueue->wfdset);
#if BASE_HAS_TCP
	BASE_FD_CLR(key->fd, &ioqueue->xfdset);
#endif

	/* Close socket. */
	if (key->fd != BASE_INVALID_SOCKET)
	{
		bsock_close(key->fd);
		key->fd = BASE_INVALID_SOCKET;
	}

	/* Clear callback */
	key->cb.on_accept_complete = NULL;
	key->cb.on_connect_complete = NULL;
	key->cb.on_read_complete = NULL;
	key->cb.on_write_complete = NULL;

	/* Must release ioqueue lock first before decrementing counter, to
	* prevent deadlock.
	*/
	block_release(ioqueue->lock);

#if BASE_IOQUEUE_HAS_SAFE_UNREG
	/* Mark key is closing. */
	key->closing = 1;

	/* Decrement counter. */
	decrement_counter(key);

	/* Done. */
	if (key->grp_lock)
	{
		/* just dec_ref and unlock. we will set grp_lock to NULL elsewhere */
		bgrp_lock_t *grp_lock = key->grp_lock;
		// Don't set grp_lock to NULL otherwise the other thread
		// will crash. Just leave it as dangling pointer, but this
		// should be safe
		//key->grp_lock = NULL;
		bgrp_lock_dec_ref_dbg(grp_lock, "ioqueue", 0);
		bgrp_lock_release(grp_lock);
	}
	else
	{
		bioqueue_unlock_key(key);
	}
#else
	if (key->grp_lock)
	{
		/* set grp_lock to NULL and unlock */
		bgrp_lock_t *grp_lock = key->grp_lock;
		// Don't set grp_lock to NULL otherwise the other thread
		// will crash. Just leave it as dangling pointer, but this
		// should be safe
		//key->grp_lock = NULL;
		bgrp_lock_dec_ref_dbg(grp_lock, "ioqueue", 0);
		bgrp_lock_release(grp_lock);
	}
	else
	{
		bioqueue_unlock_key(key);
	}

	block_destroy(key->lock);
#endif

	return BASE_SUCCESS;
}


/* This supposed to check whether the fd_set values are consistent
 * with the operation currently set in each key.
 */
#if VALIDATE_FD_SET
static void validate_sets(const bioqueue_t *ioqueue, const bfd_set_t *rfdset, const bfd_set_t *wfdset, const bfd_set_t *xfdset)
{
	bioqueue_key_t *key;

	/*
	* This basicly would not work anymore.
	* We need to lock key before performing the check, but we can't do
	* so because we're holding ioqueue mutex. If we acquire key's mutex
	* now, the will cause deadlock.
	*/
	bassert(0);

	key = ioqueue->active_list.next;
	while (key != &ioqueue->active_list)
	{
		if (!blist_empty(&key->read_list)
#if defined(BASE_HAS_TCP) && BASE_HAS_TCP != 0
			|| !blist_empty(&key->accept_list)
#endif
		) 
		{
			bassert(BASE_FD_ISSET(key->fd, rfdset));
		}
		else
		{
			bassert(BASE_FD_ISSET(key->fd, rfdset) == 0);
		}

		if (!blist_empty(&key->write_list)
#if defined(BASE_HAS_TCP) && BASE_HAS_TCP != 0
			|| key->connecting
#endif
		)
		{
			bassert(BASE_FD_ISSET(key->fd, wfdset));
		}
		else
		{
			bassert(BASE_FD_ISSET(key->fd, wfdset) == 0);
		}
#if defined(BASE_HAS_TCP) && BASE_HAS_TCP != 0
		if (key->connecting)
		{
			bassert(BASE_FD_ISSET(key->fd, xfdset));
		}
		else
		{
			bassert(BASE_FD_ISSET(key->fd, xfdset) == 0);
		}
#endif /* BASE_HAS_TCP */

		key = key->next;
	}
}
#endif	/* VALIDATE_FD_SET */


/* ioqueue_remove_from_set()
 * This function is called from ioqueue_dispatch_event() to instruct
 * the ioqueue to remove the specified descriptor from ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_remove_from_set( bioqueue_t *ioqueue, bioqueue_key_t *key, enum ioqueue_event_type event_type)
{
	block_acquire(ioqueue->lock);

	if (event_type == READABLE_EVENT)
		BASE_FD_CLR((bsock_t)key->fd, &ioqueue->rfdset);
	else if (event_type == WRITEABLE_EVENT)
		BASE_FD_CLR((bsock_t)key->fd, &ioqueue->wfdset);
#if defined(BASE_HAS_TCP) && BASE_HAS_TCP!=0
	else if (event_type == EXCEPTION_EVENT)
		BASE_FD_CLR((bsock_t)key->fd, &ioqueue->xfdset);
#endif
	else
		bassert(0);

	block_release(ioqueue->lock);
}

/*
 * ioqueue_add_to_set()
 * This function is called from bioqueue_recv(), bioqueue_send() etc
 * to instruct the ioqueue to add the specified handle to ioqueue's descriptor
 * set for the specified event.
 */
static void ioqueue_add_to_set( bioqueue_t *ioqueue, bioqueue_key_t *key, enum ioqueue_event_type event_type )
{
	block_acquire(ioqueue->lock);

	if (event_type == READABLE_EVENT)
		BASE_FD_SET((bsock_t)key->fd, &ioqueue->rfdset);
	else if (event_type == WRITEABLE_EVENT)
		BASE_FD_SET((bsock_t)key->fd, &ioqueue->wfdset);
#if defined(BASE_HAS_TCP) && BASE_HAS_TCP!=0
	else if (event_type == EXCEPTION_EVENT)
		BASE_FD_SET((bsock_t)key->fd, &ioqueue->xfdset);
#endif
	else
		bassert(0);

	block_release(ioqueue->lock);
}

#if BASE_IOQUEUE_HAS_SAFE_UNREG
/* Scan closing keys to be put to free list again */
static void scan_closing_keys(bioqueue_t *ioqueue)
{
	btime_val now;
	bioqueue_key_t *h;

	bgettickcount(&now);
	h = ioqueue->closing_list.next;

	while (h != &ioqueue->closing_list)
	{
		bioqueue_key_t *next = h->next;

		bassert(h->closing != 0);

		if (BASE_TIME_VAL_GTE(now, h->free_time))
		{
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

#if (defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0 )

static bstatus_t replace_udp_sock(bioqueue_key_t *h)
{
	enum flags
	{
		HAS_PEER_ADDR = 1,
		HAS_QOS = 2
	};

	bsock_t old_sock, new_sock = BASE_INVALID_SOCKET;
	bsockaddr local_addr, rem_addr;
	int val, addr_len;
	bfd_set_t *fds[3];
	unsigned i, fds_cnt, flags=0;
	bqos_params qos_params;
	unsigned msec;
	bstatus_t status;

	block_acquire(h->ioqueue->lock);

	old_sock = h->fd;

	fds_cnt = 0;
	fds[fds_cnt++] = &h->ioqueue->rfdset;
	fds[fds_cnt++] = &h->ioqueue->wfdset;
#if BASE_HAS_TCP
	fds[fds_cnt++] = &h->ioqueue->xfdset;
#endif

	/* Can only replace UDP socket */
	bassert(h->fd_type == bSOCK_DGRAM());

	BASE_INFO("Attempting to replace UDP socket %d", old_sock);

	for (msec=20; (msec<1000 && status != BASE_SUCCESS);msec<1000? msec=msec*2 : 1000)
	{
		if (msec > 20)
		{
			BASE_WARN("Retry to replace UDP socket %d", old_sock);
			bthreadSleepMs(msec);
		}

		if (old_sock != BASE_INVALID_SOCKET)
		{
			/* Investigate the old socket */

			addr_len = sizeof(local_addr);
			status = bsock_getsockname(old_sock, &local_addr, &addr_len);
			if (status != BASE_SUCCESS)
			{
				BASE_PERROR(5,(THIS_FILE, status, "Error get socket name"));
				continue;
			}

			addr_len = sizeof(rem_addr);
			status = bsock_getpeername(old_sock, &rem_addr, &addr_len);
			if (status != BASE_SUCCESS)
			{
				BASE_PERROR(5,(THIS_FILE, status, "Error get peer name"));
			}
			else
			{
				flags |= HAS_PEER_ADDR;
			}

			status = bsock_get_qos_params(old_sock, &qos_params);
			if (status == BASE_STATUS_FROM_OS(EBADF) ||	status == BASE_STATUS_FROM_OS(EINVAL))
			{
				BASE_PERROR(5,(THIS_FILE, status, "Error get qos param"));
				continue;
			}

			if (status != BASE_SUCCESS)
			{
				BASE_PERROR(5,(THIS_FILE, status, "Error get qos param"));
			}
			else
			{
				flags |= HAS_QOS;
			}

			/* We're done with the old socket, close it otherwise we'll get error in bind()	*/
			status = bsock_close(old_sock);
			if (status != BASE_SUCCESS)
			{
				BASE_PERROR(5,(THIS_FILE, status, "Error closing socket"));
			}

			old_sock = BASE_INVALID_SOCKET;
		}

		/* Prepare the new socket */
		status = bsock_socket(local_addr.addr.sa_family, BASE_SOCK_DGRAM, 0, &new_sock);
		if (status != BASE_SUCCESS)
		{
			BASE_PERROR(5,(THIS_FILE, status, "Error create socket"));
			continue;
		}

		/* Even after the socket is closed, we'll still get "Address in use"
		* errors, so force it with SO_REUSEADDR
		*/
		val = 1;
		status = bsock_setsockopt(new_sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
		if (status == BASE_STATUS_FROM_OS(EBADF) ||	status == BASE_STATUS_FROM_OS(EINVAL))
		{
			BASE_PERROR(5,(THIS_FILE, status, "Error set socket option"));
			continue;
		}

		/* The loop is silly, but what else can we do? */
		addr_len = bsockaddr_get_len(&local_addr);
		for (msec=20; msec<1000 ; msec<1000? msec=msec*2 : 1000)
		{
			status = bsock_bind(new_sock, &local_addr, addr_len);
			if (status != BASE_STATUS_FROM_OS(EADDRINUSE))
				break;

			BASE_INFO("Address is still in use, retrying..");
			bthreadSleepMs(msec);
		}

		if (status != BASE_SUCCESS)
			continue;

		if (flags & HAS_QOS)
		{
			status = bsock_set_qos_params(new_sock, &qos_params);
			if (status == BASE_STATUS_FROM_OS(EINVAL))
			{
				BASE_PERROR(5,(THIS_FILE, status, "Error set qos param"));
				continue;
			}
		}

		if (flags & HAS_PEER_ADDR)
		{
			status = bsock_connect(new_sock, &rem_addr, addr_len);
			if (status != BASE_SUCCESS)
			{
				BASE_PERROR(5,(THIS_FILE, status, "Error connect socket"));
				continue;
			}
		}
	}

	if (status != BASE_SUCCESS)
		goto on_error;

	/* Set socket to nonblocking. */
	val = 1;
#if defined(BASE_WIN32) && BASE_WIN32!=0 || \
	defined(BASE_WIN64) && BASE_WIN64 != 0 || \
	defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE!=0
	if (ioctlsocket(new_sock, FIONBIO, &val))
#else
	if (ioctl(new_sock, FIONBIO, &val))
#endif
	{
		status = bget_netos_error();
		goto on_error;
	}

	/* Replace the occurrence of old socket with new socket in the
	* fd sets.
	*/
	for (i=0; i<fds_cnt; ++i)
	{
		if (BASE_FD_ISSET(h->fd, fds[i]))
		{
			BASE_FD_CLR(h->fd, fds[i]);
			BASE_FD_SET(new_sock, fds[i]);
		}
	}

	/* And finally replace the fd in the key */
	h->fd = new_sock;

	BASE_INFO("UDP has been replaced successfully!");

	block_release(h->ioqueue->lock);

return BASE_SUCCESS;

on_error:
	if (new_sock != BASE_INVALID_SOCKET)
		bsock_close(new_sock);
	if (old_sock != BASE_INVALID_SOCKET)
		bsock_close(old_sock);

	/* Clear the occurrence of old socket in the fd sets. */
	for (i=0; i<fds_cnt; ++i)
	{
		if (BASE_FD_ISSET(h->fd, fds[i]))
		{
			BASE_FD_CLR(h->fd, fds[i]);
		}
	}

	h->fd = BASE_INVALID_SOCKET;
	BASE_PERROR(1,(THIS_FILE, status, "Error replacing socket %d", old_sock));
	block_release(h->ioqueue->lock);
	return BASE_ESOCKETSTOP;
}
#endif


/*
 * bioqueue_poll()
 *
 * Few things worth written:
 *
 *  - we used to do only one callback called per poll, but it didn't go
 *    very well. The reason is because on some situation, the write 
 *    callback gets called all the time, thus doesn't give the read
 *    callback to get called. This happens, for example, when user
 *    submit write operation inside the write callback.
 *    As the result, we changed the behaviour so that now multiple
 *    callbacks are called in a single poll. It should be fast too,
 *    just that we need to be carefull with the ioqueue data structs.
 *
 *  - to guarantee preemptiveness etc, the poll function must strictly
 *    work on fd_set copy of the ioqueue (not the original one).
 */
int bioqueue_poll( bioqueue_t *ioqueue, const btime_val *timeout)
{
	bfd_set_t rfdset, wfdset, xfdset;
	int nfds;
	int i, count, event_cnt, processed_cnt;
	bioqueue_key_t *h;
	enum { MAX_EVENTS = BASE_IOQUEUE_MAX_CAND_EVENTS };

	struct event
	{
		bioqueue_key_t				*key;
		enum ioqueue_event_type		event_type;
	}event[MAX_EVENTS];

	BASE_ASSERT_RETURN(ioqueue, -BASE_EINVAL);

	/* Lock ioqueue before making fd_set copies */
	block_acquire(ioqueue->lock);

	/* We will only do select() when there are sockets to be polled.
	* Otherwise select() will return error.
	*/
	if (BASE_FD_COUNT(&ioqueue->rfdset)==0 && BASE_FD_COUNT(&ioqueue->wfdset)==0 
#if defined(BASE_HAS_TCP) && BASE_HAS_TCP!=0
		&& BASE_FD_COUNT(&ioqueue->xfdset)==0
#endif
	)
	{
#if BASE_IOQUEUE_HAS_SAFE_UNREG
		scan_closing_keys(ioqueue);
#endif
		block_release(ioqueue->lock);
		MTRACE("     poll: no fd is set");
		if (timeout)
			bthreadSleepMs(BASE_TIME_VAL_MSEC(*timeout));
		return 0;
	}

	/* Copy ioqueue's bfd_set_t to local variables. */
	bmemcpy(&rfdset, &ioqueue->rfdset, sizeof(bfd_set_t));
	bmemcpy(&wfdset, &ioqueue->wfdset, sizeof(bfd_set_t));
#if BASE_HAS_TCP
	bmemcpy(&xfdset, &ioqueue->xfdset, sizeof(bfd_set_t));
#else
	BASE_FD_ZERO(&xfdset);
#endif

#if VALIDATE_FD_SET
	validate_sets(ioqueue, &rfdset, &wfdset, &xfdset);
#endif

	nfds = ioqueue->nfds;

	/* Unlock ioqueue before select(). */
	block_release(ioqueue->lock);

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	count = 0;
	__try {
#endif

	count = bsock_select(nfds+1, &rfdset, &wfdset, &xfdset, timeout);

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	/* Ignore Invalid Handle Exception raised by select().*/
	}
	__except (GetExceptionCode() == STATUS_INVALID_HANDLE ?
	EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH) {
	}
#endif    

	if (count == 0)
		return 0;	
	else if (count < 0)
		return -bget_netos_error();

	/* Scan descriptor sets for event and add the events in the event
	* array to be processed later in this function. We do this so that
	* events can be processed in parallel without holding ioqueue lock.
	*/
	block_acquire(ioqueue->lock);

	event_cnt = 0;

	/* Scan for writable sockets first to handle piggy-back data
	* coming with accept().
	*/
	for (h = ioqueue->active_list.next; h != &ioqueue->active_list && event_cnt < MAX_EVENTS; h = h->next)
	{
		if (h->fd == BASE_INVALID_SOCKET)
			continue;

		if ( (key_has_pending_write(h) || key_has_pending_connect(h)) && BASE_FD_ISSET(h->fd, &wfdset) && !IS_CLOSING(h))
		{
#if BASE_IOQUEUE_HAS_SAFE_UNREG
			increment_counter(h);
#endif
			event[event_cnt].key = h;
			event[event_cnt].event_type = WRITEABLE_EVENT;
			++event_cnt;
		}

		/* Scan for readable socket. */
		if ((key_has_pending_read(h) || key_has_pending_accept(h))	&& BASE_FD_ISSET(h->fd, &rfdset) && !IS_CLOSING(h) && event_cnt < MAX_EVENTS)
		{
#if BASE_IOQUEUE_HAS_SAFE_UNREG
			increment_counter(h);
#endif
			event[event_cnt].key = h;
			event[event_cnt].event_type = READABLE_EVENT;
			++event_cnt;
		}

#if BASE_HAS_TCP
		if (key_has_pending_connect(h) && BASE_FD_ISSET(h->fd, &xfdset) && !IS_CLOSING(h) && event_cnt < MAX_EVENTS)
		{
#if BASE_IOQUEUE_HAS_SAFE_UNREG
			increment_counter(h);
#endif
			event[event_cnt].key = h;
			event[event_cnt].event_type = EXCEPTION_EVENT;
			++event_cnt;
		}
#endif
	}

	for (i=0; i<event_cnt; ++i)
	{
		if (event[i].key->grp_lock)
			bgrp_lock_add_ref_dbg(event[i].key->grp_lock, "ioqueue", 0);
	}

	BASE_RACE_ME(5);

	block_release(ioqueue->lock);

	BASE_RACE_ME(5);

	processed_cnt = 0;

	/* Now process all events. The dispatch functions will take care
	* of locking in each of the key
	*/
	for (i=0; i<event_cnt; ++i)
	{
		/* Just do not exceed BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL */
		if (processed_cnt < BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL)
		{
			switch (event[i].event_type)
			{
				case READABLE_EVENT:
					if (ioqueue_dispatch_read_event(ioqueue, event[i].key))
						++processed_cnt;
					break;

				case WRITEABLE_EVENT:
					if (ioqueue_dispatch_write_event(ioqueue, event[i].key))
						++processed_cnt;
					break;

				case EXCEPTION_EVENT:
					if (ioqueue_dispatch_exception_event(ioqueue, event[i].key))
						++processed_cnt;
					break;

				case NO_EVENT:
					bassert(!"Invalid event!");
					break;
			}
		}

#if BASE_IOQUEUE_HAS_SAFE_UNREG
		decrement_counter(event[i].key);
#endif

		if (event[i].key->grp_lock)
			bgrp_lock_dec_ref_dbg(event[i].key->grp_lock, "ioqueue", 0);
	}

//	MTRACE("     poll: count=%d events=%d processed=%d", count, event_cnt, processed_cnt);

	return processed_cnt;
}

