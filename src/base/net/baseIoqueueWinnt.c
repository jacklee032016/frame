/* 
 *
 */
#include <baseIoQueue.h>
#include <baseOs.h>
#include <baseLock.h>
#include <basePool.h>
#include <baseString.h>
#include <baseSock.h>
#include <baseArray.h>
#include <baseLog.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <compat/socket.h>


#if defined(BASE_HAS_WINSOCK2_H) && BASE_HAS_WINSOCK2_H != 0
#  include <winsock2.h>
#elif defined(BASE_HAS_WINSOCK_H) && BASE_HAS_WINSOCK_H != 0
#  include <winsock.h>
#endif

#if defined(BASE_HAS_MSWSOCK_H) && BASE_HAS_MSWSOCK_H != 0
#  include <mswsock.h>
#endif


/* The address specified in AcceptEx() must be 16 more than the size of
 * SOCKADDR (source: MSDN).
 */
#define ACCEPT_ADDR_LEN	    (sizeof(bsockaddr_in)+16)

typedef struct generic_overlapped
{
    WSAOVERLAPPED	   overlapped;
    bioqueue_operation_e operation;
} generic_overlapped;

/*
 * OVERLAPPPED structure for send and receive.
 */
typedef struct ioqueue_overlapped
{
    WSAOVERLAPPED	   overlapped;
    bioqueue_operation_e operation;
    WSABUF		   wsabuf;
    bsockaddr_in         dummy_addr;
    int                    dummy_addrlen;
} ioqueue_overlapped;

#if BASE_HAS_TCP
/*
 * OVERLAP structure for accept.
 */
typedef struct ioqueue_accept_rec
{
    WSAOVERLAPPED	    overlapped;
    bioqueue_operation_e  operation;
    bsock_t		    newsock;
    bsock_t		   *newsock_ptr;
    int			   *addrlen;
    void		   *remote;
    void		   *local;
    char		    accept_buf[2 * ACCEPT_ADDR_LEN];
} ioqueue_accept_rec;
#endif

/*
 * Structure to hold pending operation key.
 */
union operation_key
{
    generic_overlapped      generic;
    ioqueue_overlapped      overlapped;
#if BASE_HAS_TCP
    ioqueue_accept_rec      accept;
#endif
};

/* Type of handle in the key. */
enum handle_type
{
    HND_IS_UNKNOWN,
    HND_IS_FILE,
    HND_IS_SOCKET,
};

enum { POST_QUIT_LEN = 0xFFFFDEADUL };

/*
 * Structure for individual socket.
 */
struct bioqueue_key_t
{
    BASE_DECL_LIST_MEMBER(struct bioqueue_key_t);

    bioqueue_t       *ioqueue;
    HANDLE		hnd;
    void	       *user_data;
    enum handle_type    hnd_type;
    bioqueue_callback	cb;
    bbool_t		allow_concurrent;

#if BASE_HAS_TCP
    int			connecting;
#endif

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    batomic_t	       *ref_count;
    bbool_t		closing;
    btime_val		free_time;
    bmutex_t	       *mutex;
#endif

};

/*
 * IO Queue structure.
 */
struct bioqueue_t
{
    HANDLE	      iocp;
    block_t        *lock;
    bbool_t         auto_delete_lock;
    bbool_t	      default_concurrency;

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    bioqueue_key_t  active_list;
    bioqueue_key_t  free_list;
    bioqueue_key_t  closing_list;
#endif

    /* These are to keep track of connecting sockets */
#if BASE_HAS_TCP
    unsigned	      event_count;
    HANDLE	      event_pool[MAXIMUM_WAIT_OBJECTS+1];
    unsigned	      connecting_count;
    HANDLE	      connecting_handles[MAXIMUM_WAIT_OBJECTS+1];
    bioqueue_key_t *connecting_keys[MAXIMUM_WAIT_OBJECTS+1];
#endif
};


#if BASE_IOQUEUE_HAS_SAFE_UNREG
/* Prototype */
static void scan_closing_keys(bioqueue_t *ioqueue);
#endif


#if BASE_HAS_TCP
/*
 * Process the socket when the overlapped accept() completed.
 */
static void ioqueue_on_accept_complete(bioqueue_key_t *key,
                                       ioqueue_accept_rec *accept_overlapped)
{
    struct sockaddr *local;
    struct sockaddr *remote;
    int locallen, remotelen;
    bstatus_t status;

    BASE_CHECK_STACK();

    /* On WinXP or later, use SO_UPDATE_ACCEPT_CONTEXT so that socket 
     * addresses can be obtained with getsockname() and getpeername().
     */
    status = setsockopt(accept_overlapped->newsock, SOL_SOCKET,
                        SO_UPDATE_ACCEPT_CONTEXT, 
                        (char*)&key->hnd, 
                        sizeof(SOCKET));
    /* SO_UPDATE_ACCEPT_CONTEXT is for WinXP or later.
     * So ignore the error status.
     */

    /* Operation complete immediately. */
    if (accept_overlapped->addrlen) {
	GetAcceptExSockaddrs( accept_overlapped->accept_buf,
			      0, 
			      ACCEPT_ADDR_LEN,
			      ACCEPT_ADDR_LEN,
			      &local,
			      &locallen,
			      &remote,
			      &remotelen);
	if (*accept_overlapped->addrlen >= locallen) {
	    if (accept_overlapped->local)
		bmemcpy(accept_overlapped->local, local, locallen);
	    if (accept_overlapped->remote)
		bmemcpy(accept_overlapped->remote, remote, locallen);
	} else {
	    if (accept_overlapped->local)
		bbzero(accept_overlapped->local, 
			 *accept_overlapped->addrlen);
	    if (accept_overlapped->remote)
		bbzero(accept_overlapped->remote, 
			 *accept_overlapped->addrlen);
	}

	*accept_overlapped->addrlen = locallen;
    }
    if (accept_overlapped->newsock_ptr)
        *accept_overlapped->newsock_ptr = accept_overlapped->newsock;
    accept_overlapped->operation = 0;
}

static void erase_connecting_socket( bioqueue_t *ioqueue, unsigned pos)
{
    bioqueue_key_t *key = ioqueue->connecting_keys[pos];
    HANDLE hEvent = ioqueue->connecting_handles[pos];

    /* Remove key from array of connecting handles. */
    barray_erase(ioqueue->connecting_keys, sizeof(key),
		   ioqueue->connecting_count, pos);
    barray_erase(ioqueue->connecting_handles, sizeof(HANDLE),
		   ioqueue->connecting_count, pos);
    --ioqueue->connecting_count;

    /* Disassociate the socket from the event. */
    WSAEventSelect((bsock_t)key->hnd, hEvent, 0);

    /* Put event object to pool. */
    if (ioqueue->event_count < MAXIMUM_WAIT_OBJECTS) {
	ioqueue->event_pool[ioqueue->event_count++] = hEvent;
    } else {
	/* Shouldn't happen. There should be no more pending connections
	 * than max. 
	 */
	bassert(0);
	CloseHandle(hEvent);
    }

}

/*
 * Poll for the completion of non-blocking connect().
 * If there's a completion, the function return the key of the completed
 * socket, and 'result' argument contains the connect() result. If connect()
 * succeeded, 'result' will have value zero, otherwise will have the error
 * code.
 */
static int check_connecting( bioqueue_t *ioqueue )
{
    if (ioqueue->connecting_count) {
	int i, count;
	struct 
	{
	    bioqueue_key_t *key;
	    bstatus_t	      status;
	} events[BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL-1];

	block_acquire(ioqueue->lock);
	for (count=0; count<BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL-1; ++count) {
	    DWORD result;

	    result = WaitForMultipleObjects(ioqueue->connecting_count,
					    ioqueue->connecting_handles,
					    FALSE, 0);
	    if (result >= WAIT_OBJECT_0 && 
		result < WAIT_OBJECT_0+ioqueue->connecting_count) 
	    {
		WSANETWORKEVENTS net_events;

		/* Got completed connect(). */
		unsigned pos = result - WAIT_OBJECT_0;
		events[count].key = ioqueue->connecting_keys[pos];

		/* See whether connect has succeeded. */
		WSAEnumNetworkEvents((bsock_t)events[count].key->hnd, 
				     ioqueue->connecting_handles[pos], 
				     &net_events);
		events[count].status = 
		    BASE_STATUS_FROM_OS(net_events.iErrorCode[FD_CONNECT_BIT]);

		/* Erase socket from pending connect. */
		erase_connecting_socket(ioqueue, pos);
	    } else {
		/* No more events */
		break;
	    }
	}
	block_release(ioqueue->lock);

	/* Call callbacks. */
	for (i=0; i<count; ++i) {
	    if (events[i].key->cb.on_connect_complete) {
		events[i].key->cb.on_connect_complete(events[i].key, 
						      events[i].status);
	    }
	}

	return count;
    }

    return 0;
    
}
#endif

/*
 * bioqueue_name()
 */
const char* bioqueue_name(void)
{
    return "iocp";
}

/*
 * bioqueue_create()
 */
bstatus_t bioqueue_create( bpool_t *pool, 
				       bsize_t max_fd,
				       bioqueue_t **p_ioqueue)
{
    bioqueue_t *ioqueue;
    unsigned i;
    bstatus_t rc;

    BASE_UNUSED_ARG(max_fd);
    BASE_ASSERT_RETURN(pool && p_ioqueue, BASE_EINVAL);

    rc = sizeof(union operation_key);

    /* Check that sizeof(bioqueue_op_key_t) makes sense. */
    BASE_ASSERT_RETURN(sizeof(bioqueue_op_key_t)-sizeof(void*) >= 
                     sizeof(union operation_key), BASE_EBUG);

    /* Create IOCP */
    ioqueue = bpool_zalloc(pool, sizeof(*ioqueue));
    ioqueue->iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (ioqueue->iocp == NULL)
	return BASE_RETURN_OS_ERROR(GetLastError());

    /* Create IOCP mutex */
    rc = block_create_recursive_mutex(pool, NULL, &ioqueue->lock);
    if (rc != BASE_SUCCESS) {
	CloseHandle(ioqueue->iocp);
	return rc;
    }

    ioqueue->auto_delete_lock = BASE_TRUE;
    ioqueue->default_concurrency = BASE_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY;

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /*
     * Create and initialize key pools.
     */
    blist_init(&ioqueue->active_list);
    blist_init(&ioqueue->free_list);
    blist_init(&ioqueue->closing_list);

    /* Preallocate keys according to max_fd setting, and put them
     * in free_list.
     */
    for (i=0; i<max_fd; ++i) {
	bioqueue_key_t *key;

	key = bpool_alloc(pool, sizeof(bioqueue_key_t));

	rc = batomic_create(pool, 0, &key->ref_count);
	if (rc != BASE_SUCCESS) {
	    key = ioqueue->free_list.next;
	    while (key != &ioqueue->free_list) {
		batomic_destroy(key->ref_count);
		bmutex_destroy(key->mutex);
		key = key->next;
	    }
	    CloseHandle(ioqueue->iocp);
	    return rc;
	}

	rc = bmutex_create_recursive(pool, "ioqkey", &key->mutex);
	if (rc != BASE_SUCCESS) {
	    batomic_destroy(key->ref_count);
	    key = ioqueue->free_list.next;
	    while (key != &ioqueue->free_list) {
		batomic_destroy(key->ref_count);
		bmutex_destroy(key->mutex);
		key = key->next;
	    }
	    CloseHandle(ioqueue->iocp);
	    return rc;
	}

	blist_push_back(&ioqueue->free_list, key);
    }
#endif

    *p_ioqueue = ioqueue;

    BASE_INFO("WinNT IOCP I/O Queue created (%p)", ioqueue);
    return BASE_SUCCESS;
}

/*
 * bioqueue_destroy()
 */
bstatus_t bioqueue_destroy( bioqueue_t *ioqueue )
{
#if BASE_HAS_TCP
    unsigned i;
#endif
    bioqueue_key_t *key;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(ioqueue, BASE_EINVAL);

    block_acquire(ioqueue->lock);

#if BASE_HAS_TCP
    /* Destroy events in the pool */
    for (i=0; i<ioqueue->event_count; ++i) {
	CloseHandle(ioqueue->event_pool[i]);
    }
    ioqueue->event_count = 0;
#endif

    if (CloseHandle(ioqueue->iocp) != TRUE)
	return BASE_RETURN_OS_ERROR(GetLastError());

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Destroy reference counters */
    key = ioqueue->active_list.next;
    while (key != &ioqueue->active_list) {
	batomic_destroy(key->ref_count);
	bmutex_destroy(key->mutex);
	key = key->next;
    }

    key = ioqueue->closing_list.next;
    while (key != &ioqueue->closing_list) {
	batomic_destroy(key->ref_count);
	bmutex_destroy(key->mutex);
	key = key->next;
    }

    key = ioqueue->free_list.next;
    while (key != &ioqueue->free_list) {
	batomic_destroy(key->ref_count);
	bmutex_destroy(key->mutex);
	key = key->next;
    }
#endif

    if (ioqueue->auto_delete_lock)
        block_destroy(ioqueue->lock);

    return BASE_SUCCESS;
}


bstatus_t bioqueue_set_default_concurrency(bioqueue_t *ioqueue,
						       bbool_t allow)
{
    BASE_ASSERT_RETURN(ioqueue != NULL, BASE_EINVAL);
    ioqueue->default_concurrency = allow;
    return BASE_SUCCESS;
}

/*
 * bioqueue_set_lock()
 */
bstatus_t bioqueue_set_lock( bioqueue_t *ioqueue, 
					 block_t *lock,
					 bbool_t auto_delete )
{
    BASE_ASSERT_RETURN(ioqueue && lock, BASE_EINVAL);

    if (ioqueue->auto_delete_lock) {
        block_destroy(ioqueue->lock);
    }

    ioqueue->lock = lock;
    ioqueue->auto_delete_lock = auto_delete;

    return BASE_SUCCESS;
}

/*
 * bioqueue_register_sock()
 */
bstatus_t bioqueue_register_sock( bpool_t *pool,
					      bioqueue_t *ioqueue,
					      bsock_t sock,
					      void *user_data,
					      const bioqueue_callback *cb,
					      bioqueue_key_t **key )
{
    HANDLE hioq;
    bioqueue_key_t *rec;
    u_long value;
    int rc;

    BASE_ASSERT_RETURN(pool && ioqueue && cb && key, BASE_EINVAL);

    block_acquire(ioqueue->lock);

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Scan closing list first to release unused keys.
     * Must do this with lock acquired.
     */
    scan_closing_keys(ioqueue);

    /* If safe unregistration is used, then get the key record from
     * the free list.
     */
    if (blist_empty(&ioqueue->free_list)) {
	block_release(ioqueue->lock);
	return BASE_ETOOMANY;
    }

    rec = ioqueue->free_list.next;
    blist_erase(rec);

    /* Set initial reference count to 1 */
    bassert(batomic_get(rec->ref_count) == 0);
    batomic_inc(rec->ref_count);

    rec->closing = 0;

#else
    rec = bpool_zalloc(pool, sizeof(bioqueue_key_t));
#endif

    /* Build the key for this socket. */
    rec->ioqueue = ioqueue;
    rec->hnd = (HANDLE)sock;
    rec->hnd_type = HND_IS_SOCKET;
    rec->user_data = user_data;
    bmemcpy(&rec->cb, cb, sizeof(bioqueue_callback));

    /* Set concurrency for this handle */
    rc = bioqueue_set_concurrency(rec, ioqueue->default_concurrency);
    if (rc != BASE_SUCCESS) {
	block_release(ioqueue->lock);
	return rc;
    }

#if BASE_HAS_TCP
    rec->connecting = 0;
#endif

    /* Set socket to nonblocking. */
    value = 1;
    rc = ioctlsocket(sock, FIONBIO, &value);
    if (rc != 0) {
	block_release(ioqueue->lock);
        return BASE_RETURN_OS_ERROR(WSAGetLastError());
    }

    /* Associate with IOCP */
    hioq = CreateIoCompletionPort((HANDLE)sock, ioqueue->iocp, (DWORD)rec, 0);
    if (!hioq) {
	block_release(ioqueue->lock);
	return BASE_RETURN_OS_ERROR(GetLastError());
    }

    *key = rec;

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    blist_push_back(&ioqueue->active_list, rec);
#endif

    block_release(ioqueue->lock);

    return BASE_SUCCESS;
}


/*
 * bioqueue_get_user_data()
 */
void* bioqueue_get_user_data( bioqueue_key_t *key )
{
    BASE_ASSERT_RETURN(key, NULL);
    return key->user_data;
}

/*
 * bioqueue_set_user_data()
 */
bstatus_t bioqueue_set_user_data( bioqueue_key_t *key,
                                              void *user_data,
                                              void **old_data )
{
    BASE_ASSERT_RETURN(key, BASE_EINVAL);
    
    if (old_data)
        *old_data = key->user_data;

    key->user_data = user_data;
    return BASE_SUCCESS;
}


#if BASE_IOQUEUE_HAS_SAFE_UNREG
/* Decrement the key's reference counter, and when the counter reach zero,
 * destroy the key.
 */
static void decrement_counter(bioqueue_key_t *key)
{
    if (batomic_dec_and_get(key->ref_count) == 0) {

	block_acquire(key->ioqueue->lock);

	bassert(key->closing == 1);
	bgettickcount(&key->free_time);
	key->free_time.msec += BASE_IOQUEUE_KEY_FREE_DELAY;
	btime_val_normalize(&key->free_time);

	blist_erase(key);
	blist_push_back(&key->ioqueue->closing_list, key);

	block_release(key->ioqueue->lock);
    }
}
#endif

/*
 * Poll the I/O Completion Port, execute callback, 
 * and return the key and bytes transferred of the last operation.
 */
static bbool_t poll_iocp( HANDLE hIocp, DWORD dwTimeout, 
			    bssize_t *p_bytes, bioqueue_key_t **p_key )
{
    DWORD dwBytesTransferred, dwKey;
    generic_overlapped *pOv;
    bioqueue_key_t *key;
    bssize_t size_status = -1;
    BOOL rcGetQueued;

    /* Poll for completion status. */
    rcGetQueued = GetQueuedCompletionStatus(hIocp, &dwBytesTransferred,
					    &dwKey, (OVERLAPPED**)&pOv, 
					    dwTimeout);

    /* The return value is:
     * - nonzero if event was dequeued.
     * - zero and pOv==NULL if no event was dequeued.
     * - zero and pOv!=NULL if event for failed I/O was dequeued.
     */
    if (pOv) {
	bbool_t has_lock;

	/* Event was dequeued for either successfull or failed I/O */
	key = (bioqueue_key_t*)dwKey;
	size_status = dwBytesTransferred;

	/* Report to caller regardless */
	if (p_bytes)
	    *p_bytes = size_status;
	if (p_key)
	    *p_key = key;

#if BASE_IOQUEUE_HAS_SAFE_UNREG
	/* We shouldn't call callbacks if key is quitting. */
	if (key->closing)
	    return BASE_TRUE;

	/* If concurrency is disabled, lock the key 
	 * (and save the lock status to local var since app may change
	 * concurrency setting while in the callback) */
	if (key->allow_concurrent == BASE_FALSE) {
	    bmutex_lock(key->mutex);
	    has_lock = BASE_TRUE;
	} else {
	    has_lock = BASE_FALSE;
	}

	/* Now that we get the lock, check again that key is not closing */
	if (key->closing) {
	    if (has_lock) {
		bmutex_unlock(key->mutex);
	    }
	    return BASE_TRUE;
	}

	/* Increment reference counter to prevent this key from being
	 * deleted
	 */
	batomic_inc(key->ref_count);
#else
	BASE_UNUSED_ARG(has_lock);
#endif

	/* Carry out the callback */
	switch (pOv->operation) {
	case BASE_IOQUEUE_OP_READ:
	case BASE_IOQUEUE_OP_RECV:
	case BASE_IOQUEUE_OP_RECV_FROM:
            pOv->operation = 0;
            if (key->cb.on_read_complete)
	        key->cb.on_read_complete(key, (bioqueue_op_key_t*)pOv, 
                                         size_status);
	    break;
	case BASE_IOQUEUE_OP_WRITE:
	case BASE_IOQUEUE_OP_SEND:
	case BASE_IOQUEUE_OP_SEND_TO:
            pOv->operation = 0;
            if (key->cb.on_write_complete)
	        key->cb.on_write_complete(key, (bioqueue_op_key_t*)pOv, 
                                                size_status);
	    break;
#if BASE_HAS_TCP
	case BASE_IOQUEUE_OP_ACCEPT:
	    /* special case for accept. */
	    ioqueue_on_accept_complete(key, (ioqueue_accept_rec*)pOv);
            if (key->cb.on_accept_complete) {
                ioqueue_accept_rec *accept_rec = (ioqueue_accept_rec*)pOv;
		bstatus_t status = BASE_SUCCESS;
		bsock_t newsock;

		newsock = accept_rec->newsock;
		accept_rec->newsock = BASE_INVALID_SOCKET;

		if (newsock == BASE_INVALID_SOCKET) {
		    int dwError = WSAGetLastError();
		    if (dwError == 0) dwError = OSERR_ENOTCONN;
		    status = BASE_RETURN_OS_ERROR(dwError);
		}

	        key->cb.on_accept_complete(key, (bioqueue_op_key_t*)pOv,
                                           newsock, status);
		
            }
	    break;
	case BASE_IOQUEUE_OP_CONNECT:
#endif
	case BASE_IOQUEUE_OP_NONE:
	    bassert(0);
	    break;
	}

#if BASE_IOQUEUE_HAS_SAFE_UNREG
	decrement_counter(key);
	if (has_lock)
	    bmutex_unlock(key->mutex);
#endif

	return BASE_TRUE;
    }

    /* No event was queued. */
    return BASE_FALSE;
}

/*
 * bioqueue_unregister()
 */
bstatus_t bioqueue_unregister( bioqueue_key_t *key )
{
    unsigned i;
    bbool_t has_lock;
    enum { RETRY = 10 };

    BASE_ASSERT_RETURN(key, BASE_EINVAL);

#if BASE_HAS_TCP
    if (key->connecting) {
	unsigned pos;
        bioqueue_t *ioqueue;

        ioqueue = key->ioqueue;

	/* Erase from connecting_handles */
	block_acquire(ioqueue->lock);
	for (pos=0; pos < ioqueue->connecting_count; ++pos) {
	    if (ioqueue->connecting_keys[pos] == key) {
		erase_connecting_socket(ioqueue, pos);
		break;
	    }
	}
	key->connecting = 0;
	block_release(ioqueue->lock);
    }
#endif

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Mark key as closing before closing handle. */
    key->closing = 1;

    /* If concurrency is disabled, wait until the key has finished
     * processing the callback
     */
    if (key->allow_concurrent == BASE_FALSE) {
	bmutex_lock(key->mutex);
	has_lock = BASE_TRUE;
    } else {
	has_lock = BASE_FALSE;
    }
#else
    BASE_UNUSED_ARG(has_lock);
#endif
    
    /* Close handle (the only way to disassociate handle from IOCP). 
     * We also need to close handle to make sure that no further events
     * will come to the handle.
     */
    /* Update 2008/07/18 (http://trac.extsip.org/repos/ticket/575):
     *  - It seems that CloseHandle() in itself does not actually close
     *    the socket (i.e. it will still appear in "netstat" output). Also
     *    if we only use CloseHandle(), an "Invalid Handle" exception will
     *    be raised in WSACleanup().
     *  - MSDN documentation says that CloseHandle() must be called after 
     *    closesocket() call (see
     *    http://msdn.microsoft.com/en-us/library/ms724211(VS.85).aspx).
     *    But turns out that this will raise "Invalid Handle" exception
     *    in debug mode.
     *  So because of this, we replaced CloseHandle() with closesocket()
     *  instead. These was tested on WinXP SP2.
     */
    //CloseHandle(key->hnd);
    bsock_close((bsock_t)key->hnd);

    /* Reset callbacks */
    key->cb.on_accept_complete = NULL;
    key->cb.on_connect_complete = NULL;
    key->cb.on_read_complete = NULL;
    key->cb.on_write_complete = NULL;

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Even after handle is closed, I suspect that IOCP may still try to
     * do something with the handle, causing memory corruption when pool
     * debugging is enabled.
     *
     * Forcing context switch seems to have fixed that, but this is quite
     * an ugly solution..
     *
     * Update 2008/02/13:
     *	This should not happen if concurrency is disallowed for the key.
     *  So at least application has a solution for this (i.e. by disallowing
     *  concurrency in the key).
     */
    //This will loop forever if unregistration is done on the callback.
    //Doing this with RETRY I think should solve the IOCP setting the 
    //socket signalled, without causing the deadlock.
    //while (batomic_get(key->ref_count) != 1)
    //	bthreadSleepMs(0);
    for (i=0; batomic_get(key->ref_count) != 1 && i<RETRY; ++i)
	bthreadSleepMs(0);

    /* Decrement reference counter to destroy the key. */
    decrement_counter(key);

    if (has_lock)
	bmutex_unlock(key->mutex);
#endif

    return BASE_SUCCESS;
}

#if BASE_IOQUEUE_HAS_SAFE_UNREG
/* Scan the closing list, and put pending closing keys to free list.
 * Must do this with ioqueue mutex held.
 */
static void scan_closing_keys(bioqueue_t *ioqueue)
{
    if (!blist_empty(&ioqueue->closing_list)) {
	btime_val now;
	bioqueue_key_t *key;

	bgettickcount(&now);
	
	/* Move closing keys to free list when they've finished the closing
	 * idle time.
	 */
	key = ioqueue->closing_list.next;
	while (key != &ioqueue->closing_list) {
	    bioqueue_key_t *next = key->next;

	    bassert(key->closing != 0);

	    if (BASE_TIME_VAL_GTE(now, key->free_time)) {
		blist_erase(key);
		blist_push_back(&ioqueue->free_list, key);
	    }
	    key = next;
	}
    }
}
#endif

/*
 * bioqueue_poll()
 *
 * Poll for events.
 */
int bioqueue_poll( bioqueue_t *ioqueue, const btime_val *timeout)
{
    DWORD dwMsec;
#if BASE_HAS_TCP
    int connect_count = 0;
#endif
    int event_count = 0;

    BASE_ASSERT_RETURN(ioqueue, -BASE_EINVAL);

    /* Calculate miliseconds timeout for GetQueuedCompletionStatus */
    dwMsec = timeout ? timeout->sec*1000 + timeout->msec : INFINITE;

    /* Poll for completion status. */
    event_count = poll_iocp(ioqueue->iocp, dwMsec, NULL, NULL);

#if BASE_HAS_TCP
    /* Check the connecting array, only when there's no activity. */
    if (event_count == 0) {
	connect_count = check_connecting(ioqueue);
	if (connect_count > 0)
	    event_count += connect_count;
    }
#endif

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Check the closing keys only when there's no activity and when there are
     * pending closing keys.
     */
    if (event_count == 0 && !blist_empty(&ioqueue->closing_list)) {
	block_acquire(ioqueue->lock);
	scan_closing_keys(ioqueue);
	block_release(ioqueue->lock);
    }
#endif

    /* Return number of events. */
    return event_count;
}

/*
 * bioqueue_recv()
 *
 * Initiate overlapped WSARecv() operation.
 */
bstatus_t bioqueue_recv(  bioqueue_key_t *key,
                                      bioqueue_op_key_t *op_key,
				      void *buffer,
				      bssize_t *length,
				      buint32_t flags )
{
    /*
     * Ideally we should just call bioqueue_recvfrom() with NULL addr and
     * addrlen here. But unfortunately it generates EINVAL... :-(
     *  -bennylp
     */
    int rc;
    DWORD bytesRead;
    DWORD dwFlags = 0;
    union operation_key *op_key_rec;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(key && op_key && buffer && length, BASE_EINVAL);

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Check key is not closing */
    if (key->closing)
	return BASE_ECANCELLED;
#endif

    op_key_rec = (union operation_key*)op_key->internal__;
    op_key_rec->overlapped.wsabuf.buf = buffer;
    op_key_rec->overlapped.wsabuf.len = *length;

    dwFlags = flags;
    
    /* Try non-overlapped received first to see if data is
     * immediately available.
     */
    if ((flags & BASE_IOQUEUE_ALWAYS_ASYNC) == 0) {
	rc = WSARecv((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1,
		     &bytesRead, &dwFlags, NULL, NULL);
	if (rc == 0) {
	    *length = bytesRead;
	    return BASE_SUCCESS;
	} else {
	    DWORD dwError = WSAGetLastError();
	    if (dwError != WSAEWOULDBLOCK) {
		*length = -1;
		return BASE_RETURN_OS_ERROR(dwError);
	    }
	}
    }

    dwFlags &= ~(BASE_IOQUEUE_ALWAYS_ASYNC);

    /*
     * No immediate data available.
     * Register overlapped Recv() operation.
     */
    bbzero( &op_key_rec->overlapped.overlapped, 
              sizeof(op_key_rec->overlapped.overlapped));
    op_key_rec->overlapped.operation = BASE_IOQUEUE_OP_RECV;

    rc = WSARecv((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1, 
                  &bytesRead, &dwFlags, 
		  &op_key_rec->overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
        if (dwStatus!=WSA_IO_PENDING) {
            *length = -1;
            return BASE_STATUS_FROM_OS(dwStatus);
        }
    }

    /* Pending operation has been scheduled. */
    return BASE_EPENDING;
}

/*
 * bioqueue_recvfrom()
 *
 * Initiate overlapped RecvFrom() operation.
 */
bstatus_t bioqueue_recvfrom( bioqueue_key_t *key,
                                         bioqueue_op_key_t *op_key,
					 void *buffer,
					 bssize_t *length,
                                         buint32_t flags,
					 bsockaddr_t *addr,
					 int *addrlen)
{
    int rc;
    DWORD bytesRead;
    DWORD dwFlags = 0;
    union operation_key *op_key_rec;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(key && op_key && buffer, BASE_EINVAL);

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Check key is not closing */
    if (key->closing)
	return BASE_ECANCELLED;
#endif

    op_key_rec = (union operation_key*)op_key->internal__;
    op_key_rec->overlapped.wsabuf.buf = buffer;
    op_key_rec->overlapped.wsabuf.len = *length;

    dwFlags = flags;
    
    /* Try non-overlapped received first to see if data is
     * immediately available.
     */
    if ((flags & BASE_IOQUEUE_ALWAYS_ASYNC) == 0) {
	rc = WSARecvFrom((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1,
			 &bytesRead, &dwFlags, addr, addrlen, NULL, NULL);
	if (rc == 0) {
	    *length = bytesRead;
	    return BASE_SUCCESS;
	} else {
	    DWORD dwError = WSAGetLastError();
	    if (dwError != WSAEWOULDBLOCK) {
		*length = -1;
		return BASE_RETURN_OS_ERROR(dwError);
	    }
	}
    }

    dwFlags &= ~(BASE_IOQUEUE_ALWAYS_ASYNC);

    /*
     * No immediate data available.
     * Register overlapped Recv() operation.
     */
    bbzero( &op_key_rec->overlapped.overlapped, 
              sizeof(op_key_rec->overlapped.overlapped));
    op_key_rec->overlapped.operation = BASE_IOQUEUE_OP_RECV;

    rc = WSARecvFrom((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1, 
                     &bytesRead, &dwFlags, addr, addrlen,
		     &op_key_rec->overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
        if (dwStatus!=WSA_IO_PENDING) {
            *length = -1;
            return BASE_STATUS_FROM_OS(dwStatus);
        }
    } 
    
    /* Pending operation has been scheduled. */
    return BASE_EPENDING;
}

/*
 * bioqueue_send()
 *
 * Initiate overlapped Send operation.
 */
bstatus_t bioqueue_send(  bioqueue_key_t *key,
                                      bioqueue_op_key_t *op_key,
				      const void *data,
				      bssize_t *length,
				      buint32_t flags )
{
    return bioqueue_sendto(key, op_key, data, length, flags, NULL, 0);
}


/*
 * bioqueue_sendto()
 *
 * Initiate overlapped SendTo operation.
 */
bstatus_t bioqueue_sendto( bioqueue_key_t *key,
                                       bioqueue_op_key_t *op_key,
				       const void *data,
				       bssize_t *length,
                                       buint32_t flags,
				       const bsockaddr_t *addr,
				       int addrlen)
{
    int rc;
    DWORD bytesWritten;
    DWORD dwFlags;
    union operation_key *op_key_rec;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(key && op_key && data, BASE_EINVAL);

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Check key is not closing */
    if (key->closing)
	return BASE_ECANCELLED;
#endif

    op_key_rec = (union operation_key*)op_key->internal__;

    /*
     * First try blocking write.
     */
    op_key_rec->overlapped.wsabuf.buf = (void*)data;
    op_key_rec->overlapped.wsabuf.len = *length;

    dwFlags = flags;

    if ((flags & BASE_IOQUEUE_ALWAYS_ASYNC) == 0) {
	rc = WSASendTo((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1,
		       &bytesWritten, dwFlags, addr, addrlen,
		       NULL, NULL);
	if (rc == 0) {
	    *length = bytesWritten;
	    return BASE_SUCCESS;
	} else {
	    DWORD dwStatus = WSAGetLastError();
	    if (dwStatus != WSAEWOULDBLOCK) {
		*length = -1;
		return BASE_RETURN_OS_ERROR(dwStatus);
	    }
	}
    }

    dwFlags &= ~(BASE_IOQUEUE_ALWAYS_ASYNC);

    /*
     * Data can't be sent immediately.
     * Schedule asynchronous WSASend().
     */
    bbzero( &op_key_rec->overlapped.overlapped, 
              sizeof(op_key_rec->overlapped.overlapped));
    op_key_rec->overlapped.operation = BASE_IOQUEUE_OP_SEND;

    rc = WSASendTo((SOCKET)key->hnd, &op_key_rec->overlapped.wsabuf, 1,
                   &bytesWritten,  dwFlags, addr, addrlen,
		   &op_key_rec->overlapped.overlapped, NULL);
    if (rc == SOCKET_ERROR) {
	DWORD dwStatus = WSAGetLastError();
        if (dwStatus!=WSA_IO_PENDING)
            return BASE_STATUS_FROM_OS(dwStatus);
    }

    /* Asynchronous operation successfully submitted. */
    return BASE_EPENDING;
}

#if BASE_HAS_TCP

/*
 * bioqueue_accept()
 *
 * Initiate overlapped accept() operation.
 */
bstatus_t bioqueue_accept( bioqueue_key_t *key,
                                       bioqueue_op_key_t *op_key,
			               bsock_t *new_sock,
			               bsockaddr_t *local,
			               bsockaddr_t *remote,
			               int *addrlen)
{
    BOOL rc;
    DWORD bytesReceived;
    bstatus_t status;
    union operation_key *op_key_rec;
    SOCKET sock;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(key && op_key && new_sock, BASE_EINVAL);

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Check key is not closing */
    if (key->closing)
	return BASE_ECANCELLED;
#endif

    /*
     * See if there is a new connection immediately available.
     */
    sock = WSAAccept((SOCKET)key->hnd, remote, addrlen, NULL, 0);
    if (sock != INVALID_SOCKET) {
        /* Yes! New socket is available! */
	if (local && addrlen) {
	    int status;

	    /* On WinXP or later, use SO_UPDATE_ACCEPT_CONTEXT so that socket 
	     * addresses can be obtained with getsockname() and getpeername().
	     */
	    status = setsockopt(sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
				(char*)&key->hnd, sizeof(SOCKET));
	    /* SO_UPDATE_ACCEPT_CONTEXT is for WinXP or later.
	     * So ignore the error status.
	     */

	    status = getsockname(sock, local, addrlen);
	    if (status != 0) {
		DWORD dwError = WSAGetLastError();
		closesocket(sock);
		return BASE_RETURN_OS_ERROR(dwError);
	    }
	}

        *new_sock = sock;
        return BASE_SUCCESS;

    } else {
        DWORD dwError = WSAGetLastError();
        if (dwError != WSAEWOULDBLOCK) {
            return BASE_RETURN_OS_ERROR(dwError);
        }
    }

    /*
     * No connection is immediately available.
     * Must schedule an asynchronous operation.
     */
    op_key_rec = (union operation_key*)op_key->internal__;
    
    status = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, 
                            &op_key_rec->accept.newsock);
    if (status != BASE_SUCCESS)
	return status;

    op_key_rec->accept.operation = BASE_IOQUEUE_OP_ACCEPT;
    op_key_rec->accept.addrlen = addrlen;
    op_key_rec->accept.local = local;
    op_key_rec->accept.remote = remote;
    op_key_rec->accept.newsock_ptr = new_sock;
    bbzero( &op_key_rec->accept.overlapped, 
	      sizeof(op_key_rec->accept.overlapped));

    rc = AcceptEx( (SOCKET)key->hnd, (SOCKET)op_key_rec->accept.newsock,
		   op_key_rec->accept.accept_buf,
		   0, ACCEPT_ADDR_LEN, ACCEPT_ADDR_LEN,
		   &bytesReceived,
		   &op_key_rec->accept.overlapped );

    if (rc == TRUE) {
	ioqueue_on_accept_complete(key, &op_key_rec->accept);
	return BASE_SUCCESS;
    } else {
	DWORD dwStatus = WSAGetLastError();
	if (dwStatus!=WSA_IO_PENDING)
            return BASE_STATUS_FROM_OS(dwStatus);
    }

    /* Asynchronous Accept() has been submitted. */
    return BASE_EPENDING;
}


/*
 * bioqueue_connect()
 *
 * Initiate overlapped connect() operation (well, it's non-blocking actually,
 * since there's no overlapped version of connect()).
 */
bstatus_t bioqueue_connect( bioqueue_key_t *key,
					const bsockaddr_t *addr,
					int addrlen )
{
    HANDLE hEvent;
    bioqueue_t *ioqueue;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(key && addr && addrlen, BASE_EINVAL);

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Check key is not closing */
    if (key->closing)
	return BASE_ECANCELLED;
#endif

    /* Initiate connect() */
    if (connect((bsock_t)key->hnd, addr, addrlen) != 0) {
	DWORD dwStatus;
	dwStatus = WSAGetLastError();
        if (dwStatus != WSAEWOULDBLOCK) {
	    return BASE_RETURN_OS_ERROR(dwStatus);
	}
    } else {
	/* Connect has completed immediately! */
	return BASE_SUCCESS;
    }

    ioqueue = key->ioqueue;

    /* Add to the array of connecting socket to be polled */
    block_acquire(ioqueue->lock);

    if (ioqueue->connecting_count >= MAXIMUM_WAIT_OBJECTS) {
	block_release(ioqueue->lock);
	return BASE_ETOOMANYCONN;
    }

    /* Get or create event object. */
    if (ioqueue->event_count) {
	hEvent = ioqueue->event_pool[ioqueue->event_count - 1];
	--ioqueue->event_count;
    } else {
	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (hEvent == NULL) {
	    DWORD dwStatus = GetLastError();
	    block_release(ioqueue->lock);
	    return BASE_STATUS_FROM_OS(dwStatus);
	}
    }

    /* Mark key as connecting.
     * We can't use array index since key can be removed dynamically. 
     */
    key->connecting = 1;

    /* Associate socket events to the event object. */
    if (WSAEventSelect((bsock_t)key->hnd, hEvent, FD_CONNECT) != 0) {
	CloseHandle(hEvent);
	block_release(ioqueue->lock);
	return BASE_RETURN_OS_ERROR(WSAGetLastError());
    }

    /* Add to array. */
    ioqueue->connecting_keys[ ioqueue->connecting_count ] = key;
    ioqueue->connecting_handles[ ioqueue->connecting_count ] = hEvent;
    ioqueue->connecting_count++;

    block_release(ioqueue->lock);

    return BASE_EPENDING;
}
#endif	/* #if BASE_HAS_TCP */


void bioqueue_op_key_init( bioqueue_op_key_t *op_key,
				     bsize_t size )
{
    bbzero(op_key, size);
}

bbool_t bioqueue_is_pending( bioqueue_key_t *key,
                                         bioqueue_op_key_t *op_key )
{
    BOOL rc;
    DWORD bytesTransferred;

    rc = GetOverlappedResult( key->hnd, (LPOVERLAPPED)op_key,
                              &bytesTransferred, FALSE );

    if (rc == FALSE) {
        return GetLastError()==ERROR_IO_INCOMPLETE;
    }

    return FALSE;
}


bstatus_t bioqueue_post_completion( bioqueue_key_t *key,
                                                bioqueue_op_key_t *op_key,
                                                bssize_t bytes_status )
{
    BOOL rc;

    rc = PostQueuedCompletionStatus(key->ioqueue->iocp, bytes_status,
                                    (long)key, (OVERLAPPED*)op_key );
    if (rc == FALSE) {
        return BASE_RETURN_OS_ERROR(GetLastError());
    }

    return BASE_SUCCESS;
}

bstatus_t bioqueue_set_concurrency(bioqueue_key_t *key,
					       bbool_t allow)
{
    BASE_ASSERT_RETURN(key, BASE_EINVAL);

    /* BASE_IOQUEUE_HAS_SAFE_UNREG must be enabled if concurrency is
     * disabled.
     */
    BASE_ASSERT_RETURN(allow || BASE_IOQUEUE_HAS_SAFE_UNREG, BASE_EINVAL);

    key->allow_concurrent = allow;
    return BASE_SUCCESS;
}

bstatus_t bioqueue_lock_key(bioqueue_key_t *key)
{
#if BASE_IOQUEUE_HAS_SAFE_UNREG
    return bmutex_lock(key->mutex);
#else
    BASE_ASSERT_RETURN(!"BASE_IOQUEUE_HAS_SAFE_UNREG is disabled", BASE_EINVALIDOP);
#endif
}

bstatus_t bioqueue_unlock_key(bioqueue_key_t *key)
{
#if BASE_IOQUEUE_HAS_SAFE_UNREG
    return bmutex_unlock(key->mutex);
#else
    BASE_ASSERT_RETURN(!"BASE_IOQUEUE_HAS_SAFE_UNREG is disabled", BASE_EINVALIDOP);
#endif
}

