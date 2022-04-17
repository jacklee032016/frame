/* 
 * this file is included by other 3 files: epoll, select, UWP
 */

/*
 * _baseIoqueueCommonAbs.c
 *
 * This contains common functionalities to emulate proactor pattern with
 * various event dispatching mechanisms (e.g. select, epoll).
 *
 * This file will be included by the appropriate ioqueue implementation.
 * This file is NOT supposed to be compiled as stand-alone source.
 */

#define PENDING_RETRY	2

static void ioqueue_init( bioqueue_t *ioqueue )
{
    ioqueue->lock = NULL;
    ioqueue->auto_delete_lock = 0;
    ioqueue->default_concurrency = BASE_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY;
}

static bstatus_t ioqueue_destroy(bioqueue_t *ioqueue)
{
    if (ioqueue->auto_delete_lock && ioqueue->lock ) {
	block_release(ioqueue->lock);
        return block_destroy(ioqueue->lock);
    }
    
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

    if (ioqueue->auto_delete_lock && ioqueue->lock) {
        block_destroy(ioqueue->lock);
    }

    ioqueue->lock = lock;
    ioqueue->auto_delete_lock = auto_delete;

    return BASE_SUCCESS;
}

static bstatus_t ioqueue_init_key( bpool_t *pool,
                                     bioqueue_t *ioqueue,
                                     bioqueue_key_t *key,
                                     bsock_t sock,
                                     bgrp_lock_t *grp_lock,
                                     void *user_data,
                                     const bioqueue_callback *cb)
{
    bstatus_t rc;
    int optlen;

    BASE_UNUSED_ARG(pool);

    key->ioqueue = ioqueue;
    key->fd = sock;
    key->user_data = user_data;
    blist_init(&key->read_list);
    blist_init(&key->write_list);
#if BASE_HAS_TCP
    blist_init(&key->accept_list);
    key->connecting = 0;
#endif

    /* Save callback. */
    bmemcpy(&key->cb, cb, sizeof(bioqueue_callback));

#if BASE_IOQUEUE_HAS_SAFE_UNREG
    /* Set initial reference count to 1 */
    bassert(key->ref_count == 0);
    ++key->ref_count;

    key->closing = 0;
#endif

    rc = bioqueue_set_concurrency(key, ioqueue->default_concurrency);
    if (rc != BASE_SUCCESS)
	return rc;

    /* Get socket type. When socket type is datagram, some optimization
     * will be performed during send to allow parallel send operations.
     */
    optlen = sizeof(key->fd_type);
    rc = bsock_getsockopt(sock, bSOL_SOCKET(), bSO_TYPE(),
                            &key->fd_type, &optlen);
    if (rc != BASE_SUCCESS)
        key->fd_type = bSOCK_STREAM();

    /* Create mutex for the key. */
#if !BASE_IOQUEUE_HAS_SAFE_UNREG
    rc = block_create_simple_mutex(pool, NULL, &key->lock);
    if (rc != BASE_SUCCESS)
	return rc;
#endif

    /* Group lock */
    key->grp_lock = grp_lock;
    if (key->grp_lock) {
	bgrp_lock_add_ref_dbg(key->grp_lock, "ioqueue", 0);
    }
    
    return BASE_SUCCESS;
}

/*
 * bioqueue_get_user_data()
 *
 * Obtain value associated with a key.
 */
void* bioqueue_get_user_data( bioqueue_key_t *key )
{
    BASE_ASSERT_RETURN(key != NULL, NULL);
    return key->user_data;
}

/*
 * bioqueue_set_user_data()
 */
bstatus_t bioqueue_set_user_data( bioqueue_key_t *key,
                                              void *user_data,
                                              void **old_data)
{
    BASE_ASSERT_RETURN(key, BASE_EINVAL);

    if (old_data)
        *old_data = key->user_data;
    key->user_data = user_data;

    return BASE_SUCCESS;
}

BASE_INLINE_SPECIFIER int key_has_pending_write(bioqueue_key_t *key)
{
    return !blist_empty(&key->write_list);
}

BASE_INLINE_SPECIFIER int key_has_pending_read(bioqueue_key_t *key)
{
    return !blist_empty(&key->read_list);
}

BASE_INLINE_SPECIFIER int key_has_pending_accept(bioqueue_key_t *key)
{
#if BASE_HAS_TCP
    return !blist_empty(&key->accept_list);
#else
    BASE_UNUSED_ARG(key);
    return 0;
#endif
}

BASE_INLINE_SPECIFIER int key_has_pending_connect(bioqueue_key_t *key)
{
    return key->connecting;
}


#if BASE_IOQUEUE_HAS_SAFE_UNREG
#   define IS_CLOSING(key)  (key->closing)
#else
#   define IS_CLOSING(key)  (0)
#endif


/*
 * ioqueue_dispatch_event()
 *
 * Report occurence of an event in the key to be processed by the
 * framework.
 */
bbool_t ioqueue_dispatch_write_event( bioqueue_t *ioqueue,
				        bioqueue_key_t *h)
{
    bstatus_t rc;

    /* Try lock the key. */
    rc = bioqueue_trylock_key(h);
    if (rc != BASE_SUCCESS) {
	return BASE_FALSE;
    }

    if (IS_CLOSING(h)) {
	bioqueue_unlock_key(h);
	return BASE_TRUE;
    }

#if defined(BASE_HAS_TCP) && BASE_HAS_TCP!=0
    if (h->connecting) {
	/* Completion of connect() operation */
	bstatus_t status;
	bbool_t has_lock;

	/* Clear operation. */
	h->connecting = 0;

        ioqueue_remove_from_set(ioqueue, h, WRITEABLE_EVENT);
        ioqueue_remove_from_set(ioqueue, h, EXCEPTION_EVENT);


#if (defined(BASE_HAS_SO_ERROR) && BASE_HAS_SO_ERROR!=0)
	/* from connect(2): 
	 * On Linux, use getsockopt to read the SO_ERROR option at
	 * level SOL_SOCKET to determine whether connect() completed
	 * successfully (if SO_ERROR is zero).
	 */
	{
	  int value;
	  int vallen = sizeof(value);
	  int gs_rc = bsock_getsockopt(h->fd, SOL_SOCKET, SO_ERROR, 
					 &value, &vallen);
	  if (gs_rc != 0) {
	    /* Argh!! What to do now??? 
	     * Just indicate that the socket is connected. The
	     * application will get error as soon as it tries to use
	     * the socket to send/receive.
	     */
	      status = BASE_SUCCESS;
	  } else {
	      status = BASE_STATUS_FROM_OS(value);
	  }
 	}
#elif (defined(BASE_WIN32) && BASE_WIN32!=0) || (defined(BASE_WIN64) && BASE_WIN64!=0) 
	status = BASE_SUCCESS; /* success */
#else
	/* Excellent information in D.J. Bernstein page:
	 * http://cr.yp.to/docs/connect.html
	 *
	 * Seems like the most portable way of detecting connect()
	 * failure is to call getpeername(). If socket is connected,
	 * getpeername() will return 0. If the socket is not connected,
	 * it will return ENOTCONN, and read(fd, &ch, 1) will produce
	 * the right errno through error slippage. This is a combination
	 * of suggestions from Douglas C. Schmidt and Ken Keys.
	 */
	{
	    struct sockaddr_in addr;
	    int addrlen = sizeof(addr);

	    status = bsock_getpeername(h->fd, (struct sockaddr*)&addr,
				         &addrlen);
	}
#endif

        /* Unlock; from this point we don't need to hold key's mutex
	 * (unless concurrency is disabled, which in this case we should
	 * hold the mutex while calling the callback) */
	if (h->allow_concurrent) {
	    /* concurrency may be changed while we're in the callback, so
	     * save it to a flag.
	     */
	    has_lock = BASE_FALSE;
	    bioqueue_unlock_key(h);
	} else {
	    has_lock = BASE_TRUE;
	}

	/* Call callback. */
        if (h->cb.on_connect_complete && !IS_CLOSING(h))
	    (*h->cb.on_connect_complete)(h, status);

	/* Unlock if we still hold the lock */
	if (has_lock) {
	    bioqueue_unlock_key(h);
	}

        /* Done. */

    } else 
#endif /* BASE_HAS_TCP */
    if (key_has_pending_write(h)) {
	/* Socket is writable. */
        struct write_operation *write_op;
        bssize_t sent;
        bstatus_t send_rc = BASE_SUCCESS;

        /* Get the first in the queue. */
        write_op = h->write_list.next;

        /* For datagrams, we can remove the write_op from the list
         * so that send() can work in parallel.
         */
        if (h->fd_type == bSOCK_DGRAM()) {
            blist_erase(write_op);

            if (blist_empty(&h->write_list))
                ioqueue_remove_from_set(ioqueue, h, WRITEABLE_EVENT);

        }

        /* Send the data. 
         * Unfortunately we must do this while holding key's mutex, thus
         * preventing parallel write on a single key.. :-((
         */
        sent = write_op->size - write_op->written;
        if (write_op->op == BASE_IOQUEUE_OP_SEND) {
            send_rc = bsock_send(h->fd, write_op->buf+write_op->written,
                                   &sent, write_op->flags);
	    /* Can't do this. We only clear "op" after we're finished sending
	     * the whole buffer.
	     */
	    //write_op->op = 0;
        } else if (write_op->op == BASE_IOQUEUE_OP_SEND_TO) {
	    int retry = 2;
	    while (--retry >= 0) {
		send_rc = bsock_sendto(h->fd, 
					 write_op->buf+write_op->written,
					 &sent, write_op->flags,
					 &write_op->rmt_addr, 
					 write_op->rmt_addrlen);
#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	    BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
		/* Special treatment for dead UDP sockets here, see ticket #1107 */
		if (send_rc==BASE_STATUS_FROM_OS(EPIPE) && !IS_CLOSING(h) &&
		    h->fd_type==bSOCK_DGRAM())
		{
		    BASE_PERROR(4,(THIS_FILE, send_rc,
				 "Send error for socket %d, retrying",
				 h->fd));
		    send_rc = replace_udp_sock(h);
		    continue;
		}
#endif
		break;
	    }

	    /* Can't do this. We only clear "op" after we're finished sending
	     * the whole buffer.
	     */
	    //write_op->op = 0;
        } else {
            bassert(!"Invalid operation type!");
	    write_op->op = BASE_IOQUEUE_OP_NONE;
            send_rc = BASE_EBUG;
        }

        if (send_rc == BASE_SUCCESS) {
            write_op->written += sent;
        } else {
            bassert(send_rc > 0);
            write_op->written = -send_rc;
        }

        /* Are we finished with this buffer? */
        if (send_rc!=BASE_SUCCESS || 
            write_op->written == (bssize_t)write_op->size ||
            h->fd_type == bSOCK_DGRAM()) 
        {
	    bbool_t has_lock;

	    write_op->op = BASE_IOQUEUE_OP_NONE;

            if (h->fd_type != bSOCK_DGRAM()) {
                /* Write completion of the whole stream. */
                blist_erase(write_op);

                /* Clear operation if there's no more data to send. */
                if (blist_empty(&h->write_list))
                    ioqueue_remove_from_set(ioqueue, h, WRITEABLE_EVENT);

            }

	    /* Unlock; from this point we don't need to hold key's mutex
	     * (unless concurrency is disabled, which in this case we should
	     * hold the mutex while calling the callback) */
	    if (h->allow_concurrent) {
		/* concurrency may be changed while we're in the callback, so
		 * save it to a flag.
		 */
		has_lock = BASE_FALSE;
		bioqueue_unlock_key(h);
		BASE_RACE_ME(5);
	    } else {
		has_lock = BASE_TRUE;
	    }

	    /* Call callback. */
            if (h->cb.on_write_complete && !IS_CLOSING(h)) {
	        (*h->cb.on_write_complete)(h, 
                                           (bioqueue_op_key_t*)write_op,
                                           write_op->written);
            }

	    if (has_lock) {
		bioqueue_unlock_key(h);
	    }

        } else {
            bioqueue_unlock_key(h);
        }

        /* Done. */
    } else {
        /*
         * This is normal; execution may fall here when multiple threads
         * are signalled for the same event, but only one thread eventually
         * able to process the event.
         */
	bioqueue_unlock_key(h);

	return BASE_FALSE;
    }

    return BASE_TRUE;
}

bbool_t ioqueue_dispatch_read_event( bioqueue_t *ioqueue, 		       bioqueue_key_t *h )
{
	bstatus_t rc;

	/* Try lock the key. */
	rc = bioqueue_trylock_key(h);
	if (rc != BASE_SUCCESS)
	{
		return BASE_FALSE;
	}

	if (IS_CLOSING(h))
	{
		bioqueue_unlock_key(h);
		return BASE_TRUE;
	}

#   if BASE_HAS_TCP
	if (!blist_empty(&h->accept_list))
	{
		struct accept_operation *accept_op;
		bbool_t has_lock;

		/* Get one accept operation from the list. */
		accept_op = h->accept_list.next;
		blist_erase(accept_op);
		accept_op->op = BASE_IOQUEUE_OP_NONE;

		/* Clear bit in fdset if there is no more pending accept */
		if (blist_empty(&h->accept_list))
			ioqueue_remove_from_set(ioqueue, h, READABLE_EVENT);

		rc=bsock_accept(h->fd, accept_op->accept_fd, accept_op->rmt_addr, accept_op->addrlen);
		if (rc==BASE_SUCCESS && accept_op->local_addr)
		{
			rc = bsock_getsockname(*accept_op->accept_fd, accept_op->local_addr, accept_op->addrlen);
		}

		/* Unlock; from this point we don't need to hold key's mutex
		* (unless concurrency is disabled, which in this case we should
		* hold the mutex while calling the callback) */
		if (h->allow_concurrent)
		{
			/* concurrency may be changed while we're in the callback, so
			* save it to a flag.
			*/
			has_lock = BASE_FALSE;
			bioqueue_unlock_key(h);
			BASE_RACE_ME(5);
		}
		else
		{
			has_lock = BASE_TRUE;
		}

		/* Call callback. */
		if (h->cb.on_accept_complete && !IS_CLOSING(h))
		{
			(*h->cb.on_accept_complete)(h, (bioqueue_op_key_t*)accept_op, *accept_op->accept_fd, rc);
		}

		if (has_lock)
		{
			bioqueue_unlock_key(h);
		}
	}
	else
#endif
	if (key_has_pending_read(h))
	{
		struct read_operation *read_op;
		bssize_t bytes_read;
		bbool_t has_lock;

		/* Get one pending read operation from the list. */
		read_op = h->read_list.next;
		blist_erase(read_op);

		/* Clear fdset if there is no pending read. */
		if (blist_empty(&h->read_list))
			ioqueue_remove_from_set(ioqueue, h, READABLE_EVENT);

		bytes_read = read_op->size;

		if (read_op->op == BASE_IOQUEUE_OP_RECV_FROM)
		{
			read_op->op = BASE_IOQUEUE_OP_NONE;
			rc = bsock_recvfrom(h->fd, read_op->buf, &bytes_read, read_op->flags, read_op->rmt_addr, read_op->rmt_addrlen);
		}
		else if (read_op->op == BASE_IOQUEUE_OP_RECV)
		{
			read_op->op = BASE_IOQUEUE_OP_NONE;
			rc = bsock_recv(h->fd, read_op->buf, &bytes_read, read_op->flags);
		}
		else
		{
			bassert(read_op->op == BASE_IOQUEUE_OP_READ);
			read_op->op = BASE_IOQUEUE_OP_NONE;
			/*
			* User has specified bioqueue_read().
			* On Win32, we should do ReadFile(). But because we got
			* here because of select() anyway, user must have put a
			* socket descriptor on h->fd, which in this case we can
			* just call bsock_recv() instead of ReadFile().
			* On Unix, user may put a file in h->fd, so we'll have
			* to call read() here.
			* This may not compile on systems which doesn't have 
			* read(). That's why we only specify BASE_LINUX here so
			* that error is easier to catch.
			*/
#if defined(BASE_WIN32) && BASE_WIN32 != 0 || \
	defined(BASE_WIN64) && BASE_WIN64 != 0 || \
	defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE != 0
			rc = bsock_recv(h->fd, read_op->buf, &bytes_read, read_op->flags);
			//rc = ReadFile((HANDLE)h->fd, read_op->buf, read_op->size,
			//              &bytes_read, NULL);
#elif (defined(BASE_HAS_UNISTD_H) && BASE_HAS_UNISTD_H != 0)
			bytes_read = read(h->fd, read_op->buf, bytes_read);
			rc = (bytes_read >= 0) ? BASE_SUCCESS : bget_os_error();
#else
#error "Implement read() for this platform!"
#endif
		}

		if (rc != BASE_SUCCESS)
		{
#if (defined(BASE_WIN32) && BASE_WIN32 != 0) || \
	(defined(BASE_WIN64) && BASE_WIN64 != 0) 
			/* On Win32, for UDP, WSAECONNRESET on the receive side 
			* indicates that previous sending has triggered ICMP Port 
			* Unreachable message.
			* But we wouldn't know at this point which one of previous 
			* key that has triggered the error, since UDP socket can
			* be shared!
			* So we'll just ignore it!
			*/

			if (rc == BASE_STATUS_FROM_OS(WSAECONNRESET))
			{
			//BASE_INFO("Ignored ICMP port unreach. on key=%p", h);
			}
#endif

			/* In any case we would report this to caller. */
			bytes_read = -rc;

#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
	/* Special treatment for dead UDP sockets here, see ticket #1107 */
			if (rc == BASE_STATUS_FROM_OS(ENOTCONN) && !IS_CLOSING(h) && h->fd_type==bSOCK_DGRAM())
			{
				rc = replace_udp_sock(h);
				if (rc != BASE_SUCCESS)
				{
					bytes_read = -rc;
				}
			}
#endif
		}

		/* Unlock; from this point we don't need to hold key's mutex
		* (unless concurrency is disabled, which in this case we should
		* hold the mutex while calling the callback) */
		if (h->allow_concurrent)
		{
			/* concurrency may be changed while we're in the callback, so
			* save it to a flag.
			*/
			has_lock = BASE_FALSE;
			bioqueue_unlock_key(h);
			BASE_RACE_ME(5);
		}
		else
		{
			has_lock = BASE_TRUE;
		}

		/* Call callback. */
		if (h->cb.on_read_complete && !IS_CLOSING(h))
		{
			(*h->cb.on_read_complete)(h, (bioqueue_op_key_t*)read_op, bytes_read);
		}

		if (has_lock)
		{
			bioqueue_unlock_key(h);
		}

	}
	else
	{
		/*
		* This is normal; execution may fall here when multiple threads
		* are signalled for the same event, but only one thread eventually
		* able to process the event.
		*/
		bioqueue_unlock_key(h);

		return BASE_FALSE;
	}

	return BASE_TRUE;
}


bbool_t ioqueue_dispatch_exception_event( bioqueue_t *ioqueue,
					    bioqueue_key_t *h )
{
    bbool_t has_lock;
    bstatus_t rc;

    /* Try lock the key. */
    rc = bioqueue_trylock_key(h);
    if (rc != BASE_SUCCESS) {
	return BASE_FALSE;
    }

    if (!h->connecting) {
        /* It is possible that more than one thread was woken up, thus
         * the remaining thread will see h->connecting as zero because
         * it has been processed by other thread.
         */
	bioqueue_unlock_key(h);
	return BASE_TRUE;
    }

    if (IS_CLOSING(h)) {
	bioqueue_unlock_key(h);
	return BASE_TRUE;
    }

    /* Clear operation. */
    h->connecting = 0;

    ioqueue_remove_from_set(ioqueue, h, WRITEABLE_EVENT);
    ioqueue_remove_from_set(ioqueue, h, EXCEPTION_EVENT);

    /* Unlock; from this point we don't need to hold key's mutex
     * (unless concurrency is disabled, which in this case we should
     * hold the mutex while calling the callback) */
    if (h->allow_concurrent) {
	/* concurrency may be changed while we're in the callback, so
	 * save it to a flag.
	 */
	has_lock = BASE_FALSE;
	bioqueue_unlock_key(h);
	BASE_RACE_ME(5);
    } else {
	has_lock = BASE_TRUE;
    }

    /* Call callback. */
    if (h->cb.on_connect_complete && !IS_CLOSING(h)) {
	bstatus_t status = -1;
#if (defined(BASE_HAS_SO_ERROR) && BASE_HAS_SO_ERROR!=0)
	int value;
	int vallen = sizeof(value);
	int gs_rc = bsock_getsockopt(h->fd, SOL_SOCKET, SO_ERROR, 
				       &value, &vallen);
	if (gs_rc == 0) {
	    status = BASE_RETURN_OS_ERROR(value);
	}
#endif

	(*h->cb.on_connect_complete)(h, status);
    }

    if (has_lock) {
	bioqueue_unlock_key(h);
    }

    return BASE_TRUE;
}

/*
 * bioqueue_recv()
 *
 * Start asynchronous recv() from the socket.
 */
bstatus_t bioqueue_recv(  bioqueue_key_t *key,
                                      bioqueue_op_key_t *op_key,
				      void *buffer,
				      bssize_t *length,
				      unsigned flags )
{
    struct read_operation *read_op;

    BASE_ASSERT_RETURN(key && op_key && buffer && length, BASE_EINVAL);
    BASE_CHECK_STACK();

    /* Check if key is closing (need to do this first before accessing
     * other variables, since they might have been destroyed. See ticket
     * #469).
     */
    if (IS_CLOSING(key))
	return BASE_ECANCELLED;

    read_op = (struct read_operation*)op_key;
    BASE_ASSERT_RETURN(read_op->op == BASE_IOQUEUE_OP_NONE, BASE_EPENDING);
    read_op->op = BASE_IOQUEUE_OP_NONE;

    /* Try to see if there's data immediately available. 
     */
    if ((flags & BASE_IOQUEUE_ALWAYS_ASYNC) == 0) {
	bstatus_t status;
	bssize_t size;

	size = *length;
	status = bsock_recv(key->fd, buffer, &size, flags);
	if (status == BASE_SUCCESS) {
	    /* Yes! Data is available! */
	    *length = size;
	    return BASE_SUCCESS;
	} else {
	    /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
	     * the error to caller.
	     */
	    if (status != BASE_STATUS_FROM_OS(BASE_BLOCKING_ERROR_VAL))
		return status;
	}
    }

    flags &= ~(BASE_IOQUEUE_ALWAYS_ASYNC);

    /*
     * No data is immediately available.
     * Must schedule asynchronous operation to the ioqueue.
     */
    read_op->op = BASE_IOQUEUE_OP_RECV;
    read_op->buf = buffer;
    read_op->size = *length;
    read_op->flags = flags;

    bioqueue_lock_key(key);
    /* Check again. Handle may have been closed after the previous check
     * in multithreaded app. If we add bad handle to the set it will
     * corrupt the ioqueue set. See #913
     */
    if (IS_CLOSING(key)) {
	bioqueue_unlock_key(key);
	return BASE_ECANCELLED;
    }
    blist_insert_before(&key->read_list, read_op);
    ioqueue_add_to_set(key->ioqueue, key, READABLE_EVENT);
    bioqueue_unlock_key(key);

    return BASE_EPENDING;
}

/*
 * bioqueue_recvfrom()
 *
 * Start asynchronous recvfrom() from the socket.
 */
bstatus_t bioqueue_recvfrom( bioqueue_key_t *key,
                                         bioqueue_op_key_t *op_key,
				         void *buffer,
				         bssize_t *length,
                                         unsigned flags,
				         bsockaddr_t *addr,
				         int *addrlen)
{
    struct read_operation *read_op;

    BASE_ASSERT_RETURN(key && op_key && buffer && length, BASE_EINVAL);
    BASE_CHECK_STACK();

    /* Check if key is closing. */
    if (IS_CLOSING(key))
	return BASE_ECANCELLED;

    read_op = (struct read_operation*)op_key;
    BASE_ASSERT_RETURN(read_op->op == BASE_IOQUEUE_OP_NONE, BASE_EPENDING);
    read_op->op = BASE_IOQUEUE_OP_NONE;

    /* Try to see if there's data immediately available. 
     */
    if ((flags & BASE_IOQUEUE_ALWAYS_ASYNC) == 0) {
	bstatus_t status;
	bssize_t size;

	size = *length;
	status = bsock_recvfrom(key->fd, buffer, &size, flags,
				  addr, addrlen);
	if (status == BASE_SUCCESS) {
	    /* Yes! Data is available! */
	    *length = size;
	    return BASE_SUCCESS;
	} else {
	    /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
	     * the error to caller.
	     */
	    if (status != BASE_STATUS_FROM_OS(BASE_BLOCKING_ERROR_VAL))
		return status;
	}
    }

    flags &= ~(BASE_IOQUEUE_ALWAYS_ASYNC);

    /*
     * No data is immediately available.
     * Must schedule asynchronous operation to the ioqueue.
     */
    read_op->op = BASE_IOQUEUE_OP_RECV_FROM;
    read_op->buf = buffer;
    read_op->size = *length;
    read_op->flags = flags;
    read_op->rmt_addr = addr;
    read_op->rmt_addrlen = addrlen;

    bioqueue_lock_key(key);
    /* Check again. Handle may have been closed after the previous check
     * in multithreaded app. If we add bad handle to the set it will
     * corrupt the ioqueue set. See #913
     */
    if (IS_CLOSING(key)) {
	bioqueue_unlock_key(key);
	return BASE_ECANCELLED;
    }
    blist_insert_before(&key->read_list, read_op);
    ioqueue_add_to_set(key->ioqueue, key, READABLE_EVENT);
    bioqueue_unlock_key(key);

    return BASE_EPENDING;
}

/*
 * bioqueue_send()
 *
 * Start asynchronous send() to the descriptor.
 */
bstatus_t bioqueue_send( bioqueue_key_t *key,
                                     bioqueue_op_key_t *op_key,
			             const void *data,
			             bssize_t *length,
                                     unsigned flags)
{
    struct write_operation *write_op;
    bstatus_t status;
    unsigned retry;
    bssize_t sent;

    BASE_ASSERT_RETURN(key && op_key && data && length, BASE_EINVAL);
    BASE_CHECK_STACK();

    /* Check if key is closing. */
    if (IS_CLOSING(key))
	return BASE_ECANCELLED;

    /* We can not use BASE_IOQUEUE_ALWAYS_ASYNC for socket write. */
    flags &= ~(BASE_IOQUEUE_ALWAYS_ASYNC);

    /* Fast track:
     *   Try to send data immediately, only if there's no pending write!
     * Note:
     *  We are speculating that the list is empty here without properly
     *  acquiring ioqueue's mutex first. This is intentional, to maximize
     *  performance via parallelism.
     *
     *  This should be safe, because:
     *      - by convention, we require caller to make sure that the
     *        key is not unregistered while other threads are invoking
     *        an operation on the same key.
     *      - blist_empty() is safe to be invoked by multiple threads,
     *        even when other threads are modifying the list.
     */
    if (blist_empty(&key->write_list)) {
        /*
         * See if data can be sent immediately.
         */
        sent = *length;
        status = bsock_send(key->fd, data, &sent, flags);
        if (status == BASE_SUCCESS) {
            /* Success! */
            *length = sent;
            return BASE_SUCCESS;
        } else {
            /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
             * the error to caller.
             */
            if (status != BASE_STATUS_FROM_OS(BASE_BLOCKING_ERROR_VAL)) {
                return status;
            }
        }
    }

    /*
     * Schedule asynchronous send.
     */
    write_op = (struct write_operation*)op_key;

    /* Spin if write_op has pending operation */
    for (retry=0; write_op->op != 0 && retry<PENDING_RETRY; ++retry)
	bthreadSleepMs(0);

    /* Last chance */
    if (write_op->op) {
	/* Unable to send packet because there is already pending write in the
	 * write_op. We could not put the operation into the write_op
	 * because write_op already contains a pending operation! And
	 * we could not send the packet directly with send() either,
	 * because that will break the order of the packet. So we can
	 * only return error here.
	 *
	 * This could happen for example in multithreads program,
	 * where polling is done by one thread, while other threads are doing
	 * the sending only. If the polling thread runs on lower priority
	 * than the sending thread, then it's possible that the pending
	 * write flag is not cleared in-time because clearing is only done
	 * during polling. 
	 *
	 * Aplication should specify multiple write operation keys on
	 * situation like this.
	 */
	//bassert(!"ioqueue: there is pending operation on this key!");
	return BASE_EBUSY;
    }

    write_op->op = BASE_IOQUEUE_OP_SEND;
    write_op->buf = (char*)data;
    write_op->size = *length;
    write_op->written = 0;
    write_op->flags = flags;
    
    bioqueue_lock_key(key);
    /* Check again. Handle may have been closed after the previous check
     * in multithreaded app. If we add bad handle to the set it will
     * corrupt the ioqueue set. See #913
     */
    if (IS_CLOSING(key)) {
	bioqueue_unlock_key(key);
	return BASE_ECANCELLED;
    }
    blist_insert_before(&key->write_list, write_op);
    ioqueue_add_to_set(key->ioqueue, key, WRITEABLE_EVENT);
    bioqueue_unlock_key(key);

    return BASE_EPENDING;
}


/*
 * bioqueue_sendto()
 *
 * Start asynchronous write() to the descriptor.
 */
bstatus_t bioqueue_sendto( bioqueue_key_t *key,
                                       bioqueue_op_key_t *op_key,
			               const void *data,
			               bssize_t *length,
                                       buint32_t flags,
			               const bsockaddr_t *addr,
			               int addrlen)
{
    struct write_operation *write_op;
    unsigned retry;
    bbool_t restart_retry = BASE_FALSE;
    bstatus_t status;
    bssize_t sent;

    BASE_ASSERT_RETURN(key && op_key && data && length, BASE_EINVAL);
    BASE_CHECK_STACK();

#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	    BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
retry_on_restart:
#else
    BASE_UNUSED_ARG(restart_retry);
#endif
    /* Check if key is closing. */
    if (IS_CLOSING(key))
	return BASE_ECANCELLED;

    /* We can not use BASE_IOQUEUE_ALWAYS_ASYNC for socket write */
    flags &= ~(BASE_IOQUEUE_ALWAYS_ASYNC);

    /* Fast track:
     *   Try to send data immediately, only if there's no pending write!
     * Note:
     *  We are speculating that the list is empty here without properly
     *  acquiring ioqueue's mutex first. This is intentional, to maximize
     *  performance via parallelism.
     *
     *  This should be safe, because:
     *      - by convention, we require caller to make sure that the
     *        key is not unregistered while other threads are invoking
     *        an operation on the same key.
     *      - blist_empty() is safe to be invoked by multiple threads,
     *        even when other threads are modifying the list.
     */
    if (blist_empty(&key->write_list)) {
        /*
         * See if data can be sent immediately.
         */
        sent = *length;
        status = bsock_sendto(key->fd, data, &sent, flags, addr, addrlen);
        if (status == BASE_SUCCESS) {
            /* Success! */
            *length = sent;
            return BASE_SUCCESS;
        } else {
            /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
             * the error to caller.
             */
            if (status != BASE_STATUS_FROM_OS(BASE_BLOCKING_ERROR_VAL)) {
#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	    BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
		/* Special treatment for dead UDP sockets here, see ticket #1107 */
		if (status == BASE_STATUS_FROM_OS(EPIPE) && !IS_CLOSING(key) &&
		    key->fd_type == bSOCK_DGRAM())
		{
		    if (!restart_retry) {
			BASE_PERROR(4, (THIS_FILE, status,
				      "Send error for socket %d, retrying",
				      key->fd));
			replace_udp_sock(key);
			restart_retry = BASE_TRUE;
			goto retry_on_restart;
		    } else {
			status = BASE_ESOCKETSTOP;
		    }
		}
#endif

                return status;
            }
        }
    }

    /*
     * Check that address storage can hold the address parameter.
     */
    BASE_ASSERT_RETURN(addrlen <= (int)sizeof(bsockaddr_in), BASE_EBUG);

    /*
     * Schedule asynchronous send.
     */
    write_op = (struct write_operation*)op_key;
    
    /* Spin if write_op has pending operation */
    for (retry=0; write_op->op != 0 && retry<PENDING_RETRY; ++retry)
	bthreadSleepMs(0);

    /* Last chance */
    if (write_op->op) {
	/* Unable to send packet because there is already pending write on the
	 * write_op. We could not put the operation into the write_op
	 * because write_op already contains a pending operation! And
	 * we could not send the packet directly with sendto() either,
	 * because that will break the order of the packet. So we can
	 * only return error here.
	 *
	 * This could happen for example in multithreads program,
	 * where polling is done by one thread, while other threads are doing
	 * the sending only. If the polling thread runs on lower priority
	 * than the sending thread, then it's possible that the pending
	 * write flag is not cleared in-time because clearing is only done
	 * during polling. 
	 *
	 * Aplication should specify multiple write operation keys on
	 * situation like this.
	 */
	//bassert(!"ioqueue: there is pending operation on this key!");
	return BASE_EBUSY;
    }

    write_op->op = BASE_IOQUEUE_OP_SEND_TO;
    write_op->buf = (char*)data;
    write_op->size = *length;
    write_op->written = 0;
    write_op->flags = flags;
    bmemcpy(&write_op->rmt_addr, addr, addrlen);
    write_op->rmt_addrlen = addrlen;
    
    bioqueue_lock_key(key);
    /* Check again. Handle may have been closed after the previous check
     * in multithreaded app. If we add bad handle to the set it will
     * corrupt the ioqueue set. See #913
     */
    if (IS_CLOSING(key)) {
	bioqueue_unlock_key(key);
	return BASE_ECANCELLED;
    }
    blist_insert_before(&key->write_list, write_op);
    ioqueue_add_to_set(key->ioqueue, key, WRITEABLE_EVENT);
    bioqueue_unlock_key(key);

    return BASE_EPENDING;
}

#if BASE_HAS_TCP
/*
 * Initiate overlapped accept() operation.
 */
bstatus_t bioqueue_accept( bioqueue_key_t *key,
                                       bioqueue_op_key_t *op_key,
			               bsock_t *new_sock,
			               bsockaddr_t *local,
			               bsockaddr_t *remote,
			               int *addrlen)
{
    struct accept_operation *accept_op;
    bstatus_t status;

    /* check parameters. All must be specified! */
    BASE_ASSERT_RETURN(key && op_key && new_sock, BASE_EINVAL);

    /* Check if key is closing. */
    if (IS_CLOSING(key))
	return BASE_ECANCELLED;

    accept_op = (struct accept_operation*)op_key;
    BASE_ASSERT_RETURN(accept_op->op == BASE_IOQUEUE_OP_NONE, BASE_EPENDING);
    accept_op->op = BASE_IOQUEUE_OP_NONE;

    /* Fast track:
     *  See if there's new connection available immediately.
     */
    if (blist_empty(&key->accept_list)) {
        status = bsock_accept(key->fd, new_sock, remote, addrlen);
        if (status == BASE_SUCCESS) {
            /* Yes! New connection is available! */
            if (local && addrlen) {
                status = bsock_getsockname(*new_sock, local, addrlen);
                if (status != BASE_SUCCESS) {
                    bsock_close(*new_sock);
                    *new_sock = BASE_INVALID_SOCKET;
                    return status;
                }
            }
            return BASE_SUCCESS;
        } else {
            /* If error is not EWOULDBLOCK (or EAGAIN on Linux), report
             * the error to caller.
             */
            if (status != BASE_STATUS_FROM_OS(BASE_BLOCKING_ERROR_VAL)) {
                return status;
            }
        }
    }

    /*
     * No connection is available immediately.
     * Schedule accept() operation to be completed when there is incoming
     * connection available.
     */
    accept_op->op = BASE_IOQUEUE_OP_ACCEPT;
    accept_op->accept_fd = new_sock;
    accept_op->rmt_addr = remote;
    accept_op->addrlen= addrlen;
    accept_op->local_addr = local;

    bioqueue_lock_key(key);
    /* Check again. Handle may have been closed after the previous check
     * in multithreaded app. If we add bad handle to the set it will
     * corrupt the ioqueue set. See #913
     */
    if (IS_CLOSING(key)) {
	bioqueue_unlock_key(key);
	return BASE_ECANCELLED;
    }
    blist_insert_before(&key->accept_list, accept_op);
    ioqueue_add_to_set(key->ioqueue, key, READABLE_EVENT);
    bioqueue_unlock_key(key);

    return BASE_EPENDING;
}

/*
 * Initiate overlapped connect() operation (well, it's non-blocking actually,
 * since there's no overlapped version of connect()).
 */
bstatus_t bioqueue_connect( bioqueue_key_t *key,
					const bsockaddr_t *addr,
					int addrlen )
{
    bstatus_t status;
    
    /* check parameters. All must be specified! */
    BASE_ASSERT_RETURN(key && addr && addrlen, BASE_EINVAL);

    /* Check if key is closing. */
    if (IS_CLOSING(key))
	return BASE_ECANCELLED;

    /* Check if socket has not been marked for connecting */
    if (key->connecting != 0)
        return BASE_EPENDING;
    
    status = bsock_connect(key->fd, addr, addrlen);
    if (status == BASE_SUCCESS) {
	/* Connected! */
	return BASE_SUCCESS;
    } else {
	if (status == BASE_STATUS_FROM_OS(BASE_BLOCKING_CONNECT_ERROR_VAL)) {
	    /* Pending! */
	    bioqueue_lock_key(key);
	    /* Check again. Handle may have been closed after the previous 
	     * check in multithreaded app. See #913
	     */
	    if (IS_CLOSING(key)) {
		bioqueue_unlock_key(key);
		return BASE_ECANCELLED;
	    }
	    key->connecting = BASE_TRUE;
            ioqueue_add_to_set(key->ioqueue, key, WRITEABLE_EVENT);
            ioqueue_add_to_set(key->ioqueue, key, EXCEPTION_EVENT);
            bioqueue_unlock_key(key);
	    return BASE_EPENDING;
	} else {
	    /* Error! */
	    return status;
	}
    }
}
#endif	/* BASE_HAS_TCP */


void bioqueue_op_key_init( bioqueue_op_key_t *op_key, bsize_t size )
{
	bbzero(op_key, size);
}


/*
 * bioqueue_is_pending()
 */
bbool_t bioqueue_is_pending( bioqueue_key_t *key,
                                         bioqueue_op_key_t *op_key )
{
    struct generic_operation *op_rec;

    BASE_UNUSED_ARG(key);

    op_rec = (struct generic_operation*)op_key;
    return op_rec->op != 0;
}


/*
 * bioqueue_post_completion()
 */
bstatus_t bioqueue_post_completion( bioqueue_key_t *key,
                                                bioqueue_op_key_t *op_key,
                                                bssize_t bytes_status )
{
    struct generic_operation *op_rec;

    /*
     * Find the operation key in all pending operation list to
     * really make sure that it's still there; then call the callback.
     */
    bioqueue_lock_key(key);

    /* Find the operation in the pending read list. */
    op_rec = (struct generic_operation*)key->read_list.next;
    while (op_rec != (void*)&key->read_list) {
        if (op_rec == (void*)op_key) {
            blist_erase(op_rec);
            op_rec->op = BASE_IOQUEUE_OP_NONE;
            bioqueue_unlock_key(key);

            if (key->cb.on_read_complete)
            	(*key->cb.on_read_complete)(key, op_key, bytes_status);
            return BASE_SUCCESS;
        }
        op_rec = op_rec->next;
    }

    /* Find the operation in the pending write list. */
    op_rec = (struct generic_operation*)key->write_list.next;
    while (op_rec != (void*)&key->write_list) {
        if (op_rec == (void*)op_key) {
            blist_erase(op_rec);
            op_rec->op = BASE_IOQUEUE_OP_NONE;
            bioqueue_unlock_key(key);

            if (key->cb.on_write_complete)
            	(*key->cb.on_write_complete)(key, op_key, bytes_status);
            return BASE_SUCCESS;
        }
        op_rec = op_rec->next;
    }

    /* Find the operation in the pending accept list. */
    op_rec = (struct generic_operation*)key->accept_list.next;
    while (op_rec != (void*)&key->accept_list) {
        if (op_rec == (void*)op_key) {
            blist_erase(op_rec);
            op_rec->op = BASE_IOQUEUE_OP_NONE;
            bioqueue_unlock_key(key);

            if (key->cb.on_accept_complete) {
            	(*key->cb.on_accept_complete)(key, op_key, 
                                              BASE_INVALID_SOCKET,
                                              (bstatus_t)bytes_status);
            }
            return BASE_SUCCESS;
        }
        op_rec = op_rec->next;
    }

    bioqueue_unlock_key(key);
    
    return BASE_EINVALIDOP;
}

bstatus_t bioqueue_set_default_concurrency( bioqueue_t *ioqueue,
							bbool_t allow)
{
    BASE_ASSERT_RETURN(ioqueue != NULL, BASE_EINVAL);
    ioqueue->default_concurrency = allow;
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
    if (key->grp_lock)
	return bgrp_lock_acquire(key->grp_lock);
    else
	return block_acquire(key->lock);
}

bstatus_t bioqueue_trylock_key(bioqueue_key_t *key)
{
    if (key->grp_lock)
	return bgrp_lock_tryacquire(key->grp_lock);
    else
	return block_tryacquire(key->lock);
}

bstatus_t bioqueue_unlock_key(bioqueue_key_t *key)
{
    if (key->grp_lock)
	return bgrp_lock_release(key->grp_lock);
    else
	return block_release(key->lock);
}


