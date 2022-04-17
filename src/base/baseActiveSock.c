/* 
 *
 */
#include <baseActiveSock.h>
#include <compat/socket.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseLog.h>
#include <basePool.h>
#include <baseSock.h>
#include <baseString.h>

#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
    BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
#   include <CFNetwork/CFNetwork.h>

    static bbool_t ios_bg_support = BASE_TRUE;
#endif

#define BASE_ACTIVESOCK_MAX_LOOP	    50


enum read_type
{
	TYPE_NONE,
	TYPE_RECV,
	TYPE_RECV_FROM
};

enum shutdown_dir
{
    SHUT_NONE = 0,
    SHUT_RX = 1,
    SHUT_TX = 2
};

struct read_op
{
    bioqueue_op_key_t	 op_key;
    buint8_t		*pkt;
    unsigned		 max_size;
    bsize_t		 size;
    bsockaddr		 src_addr;
    int			 src_addr_len;
};

struct accept_op
{
    bioqueue_op_key_t	 op_key;
    bsock_t		 new_sock;
    bsockaddr		 rem_addr;
    int			 rem_addr_len;
};

struct send_data
{
    buint8_t		*data;
    bssize_t		 len;
    bssize_t		 sent;
    unsigned		 flags;
};

struct _bactivesock_t
{
    bioqueue_key_t	*key;
    bbool_t		 stream_oriented;
    bbool_t		 whole_data;
    bioqueue_t	*ioqueue;
    void		*user_data;
    unsigned		 async_count;
    unsigned	 	 shutdown;
    unsigned		 max_loop;
    bactivesock_cb	 cb;
#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
    BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
    int			 bg_setting;
    bsock_t		 sock;
    CFReadStreamRef	 readStream;
#endif
    
    unsigned		 err_counter;
    bstatus_t		 last_err;

    struct send_data	 send_data;

    struct read_op	*read_op;
    buint32_t		 read_flags;
    enum read_type	 read_type;

    struct accept_op	*accept_op;
};


static void ioqueue_on_read_complete(bioqueue_key_t *key, 
				     bioqueue_op_key_t *op_key, 
				     bssize_t bytes_read);
static void ioqueue_on_write_complete(bioqueue_key_t *key, 
				      bioqueue_op_key_t *op_key,
				      bssize_t bytes_sent);
#if BASE_HAS_TCP
static void ioqueue_on_accept_complete(bioqueue_key_t *key, 
				       bioqueue_op_key_t *op_key,
				       bsock_t sock, 
				       bstatus_t status);
static void ioqueue_on_connect_complete(bioqueue_key_t *key, 
					bstatus_t status);
#endif

void bactivesock_cfg_default(bactivesock_cfg *cfg)
{
    bbzero(cfg, sizeof(*cfg));
    cfg->async_cnt = 1;
    cfg->concurrency = -1;
    cfg->whole_data = BASE_TRUE;
}

#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
    BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
static void activesock_destroy_iphone_os_stream(bactivesock_t *asock)
{
    if (asock->readStream) {
	CFReadStreamClose(asock->readStream);
	CFRelease(asock->readStream);
	asock->readStream = NULL;
    }
}

static void activesock_create_iphone_os_stream(bactivesock_t *asock)
{
    if (ios_bg_support && asock->bg_setting && asock->stream_oriented) {
	activesock_destroy_iphone_os_stream(asock);

	CFStreamCreatePairWithSocket(kCFAllocatorDefault, asock->sock,
				     &asock->readStream, NULL);

	if (!asock->readStream ||
	    CFReadStreamSetProperty(asock->readStream,
				    kCFStreamNetworkServiceType,
				    kCFStreamNetworkServiceTypeVoIP)
	    != TRUE ||
	    CFReadStreamOpen(asock->readStream) != TRUE)
	{
	    BASE_CRIT( "Failed to configure TCP transport for VoIP "
		      "usage. Usage of THIS particular TCP transport in "
		      "background mode will not be supported.");

	    
	    activesock_destroy_iphone_os_stream(asock);
	}
    }
}


void bactivesock_set_iphone_os_bg(bactivesock_t *asock,
					    int val)
{
    asock->bg_setting = val;
    if (asock->bg_setting)
	activesock_create_iphone_os_stream(asock);
    else
	activesock_destroy_iphone_os_stream(asock);
}

void bactivesock_enable_iphone_os_bg(bbool_t val)
{
    ios_bg_support = val;
}
#endif

bstatus_t bactivesock_create( bpool_t *pool,	bsock_t sock, int sock_type,
	const bactivesock_cfg *opt, bioqueue_t *ioqueue, const bactivesock_cb *cb, void *user_data,	bactivesock_t **p_asock)
{
	bactivesock_t *asock;
	bioqueue_callback ioq_cb;
	bstatus_t status;

	BASE_ASSERT_RETURN(pool && ioqueue && cb && p_asock, BASE_EINVAL);
	BASE_ASSERT_RETURN(sock!=0 && sock!=BASE_INVALID_SOCKET, BASE_EINVAL);
	BASE_ASSERT_RETURN(sock_type==bSOCK_STREAM() ||	sock_type==bSOCK_DGRAM(), BASE_EINVAL);
	BASE_ASSERT_RETURN(!opt || opt->async_cnt >= 1, BASE_EINVAL);

	asock = BASE_POOL_ZALLOC_T(pool, bactivesock_t);
	asock->ioqueue = ioqueue;
	asock->stream_oriented = (sock_type == bSOCK_STREAM());
	asock->async_count = (opt? opt->async_cnt : 1);
	asock->whole_data = (opt? opt->whole_data : 1);
	asock->max_loop = BASE_ACTIVESOCK_MAX_LOOP;
	asock->user_data = user_data;
	bmemcpy(&asock->cb, cb, sizeof(*cb));

	bbzero(&ioq_cb, sizeof(ioq_cb));
	ioq_cb.on_read_complete = &ioqueue_on_read_complete;
	ioq_cb.on_write_complete = &ioqueue_on_write_complete;
#if BASE_HAS_TCP
	ioq_cb.on_connect_complete = &ioqueue_on_connect_complete;
	ioq_cb.on_accept_complete = &ioqueue_on_accept_complete;
#endif

	status = bioqueue_register_sock2(pool, ioqueue, sock, (opt? opt->grp_lock : NULL), asock, &ioq_cb, &asock->key);
	if (status != BASE_SUCCESS)
	{
		bactivesock_close(asock);
		return status;
	}

	if (asock->whole_data)
	{/* Must disable concurrency otherwise there is a race condition */
		bioqueue_set_concurrency(asock->key, 0);
	}
	else if (opt && opt->concurrency >= 0)
	{
		bioqueue_set_concurrency(asock->key, opt->concurrency);
	}

#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
	asock->sock = sock;
	asock->bg_setting = BASE_ACTIVESOCK_TCP_IPHONE_OS_BG;
#endif

	*p_asock = asock;
	return BASE_SUCCESS;
}


bstatus_t bactivesock_create_udp( bpool_t *pool,	const bsockaddr *addr, const bactivesock_cfg *opt,
	bioqueue_t *ioqueue, const bactivesock_cb *cb, void *user_data, bactivesock_t **p_asock,bsockaddr *bound_addr)
{
	bsock_t sock_fd;
	bsockaddr default_addr;
	bstatus_t status;

	if (addr == NULL)
	{
		bsockaddr_init(bAF_INET(), &default_addr, NULL, 0);
		addr = &default_addr;
	}

	status = bsock_socket(addr->addr.sa_family, bSOCK_DGRAM(), 0, &sock_fd);
	if (status != BASE_SUCCESS)
	{
		return status;
	}

	status = bsock_bind(sock_fd, addr, bsockaddr_get_len(addr));
	if (status != BASE_SUCCESS)
	{
		bsock_close(sock_fd);
		return status;
	}

	status = bactivesock_create(pool, sock_fd, bSOCK_DGRAM(), opt, ioqueue, cb, user_data, p_asock);
	if (status != BASE_SUCCESS)
	{
		bsock_close(sock_fd);
		return status;
	}

	if (bound_addr)
	{
		int addr_len = sizeof(*bound_addr);
		status = bsock_getsockname(sock_fd, bound_addr, &addr_len);
		if (status != BASE_SUCCESS)
		{
			bactivesock_close(*p_asock);
			return status;
		}
	}

	return BASE_SUCCESS;
}


/* not destroy ??? */
bstatus_t bactivesock_close(bactivesock_t *asock)
{
	bioqueue_key_t *key;
	bbool_t unregister = BASE_FALSE;

	BASE_ASSERT_RETURN(asock, BASE_EINVAL);
	asock->shutdown = SHUT_RX | SHUT_TX;

	/* Avoid double unregistration on the key */
	key = asock->key;
	if (key)
	{
		bioqueue_lock_key(key);
		unregister = (asock->key != NULL);
		asock->key = NULL;
		bioqueue_unlock_key(key);
	}

	if (unregister)
	{
		bioqueue_unregister(key);

#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
		activesock_destroy_iphone_os_stream(asock);
#endif	
	}
	return BASE_SUCCESS;
}


bstatus_t bactivesock_set_user_data( bactivesock_t *asock, void *user_data)
{
    BASE_ASSERT_RETURN(asock, BASE_EINVAL);
    asock->user_data = user_data;
    return BASE_SUCCESS;
}


void* bactivesock_get_user_data(bactivesock_t *asock)
{
    BASE_ASSERT_RETURN(asock, NULL);
    return asock->user_data;
}


bstatus_t bactivesock_start_read(bactivesock_t *asock,
					     bpool_t *pool,
					     unsigned buff_size,
					     buint32_t flags)
{
    void **readbuf;
    unsigned i;

    BASE_ASSERT_RETURN(asock && pool && buff_size, BASE_EINVAL);

    readbuf = (void**) bpool_calloc(pool, asock->async_count, 
				      sizeof(void*));

    for (i=0; i<asock->async_count; ++i) {
	readbuf[i] = bpool_alloc(pool, buff_size);
    }

    return bactivesock_start_read2(asock, pool, buff_size, readbuf, flags);
}


bstatus_t bactivesock_start_read2( bactivesock_t *asock,
					       bpool_t *pool,
					       unsigned buff_size,
					       void *readbuf[],
					       buint32_t flags)
{
    unsigned i;
    bstatus_t status;

    BASE_ASSERT_RETURN(asock && pool && buff_size, BASE_EINVAL);
    BASE_ASSERT_RETURN(asock->read_type == TYPE_NONE, BASE_EINVALIDOP);
    BASE_ASSERT_RETURN(asock->read_op == NULL, BASE_EINVALIDOP);

    asock->read_op = (struct read_op*)
		     bpool_calloc(pool, asock->async_count, 
				    sizeof(struct read_op));
    asock->read_type = TYPE_RECV;
    asock->read_flags = flags;

    for (i=0; i<asock->async_count; ++i) {
	struct read_op *r = &asock->read_op[i];
	bssize_t size_to_read;

	r->pkt = (buint8_t*)readbuf[i];
	size_to_read = r->max_size = buff_size;

	status = bioqueue_recv(asock->key, &r->op_key, r->pkt, &size_to_read,
				 BASE_IOQUEUE_ALWAYS_ASYNC | flags);
	BASE_ASSERT_RETURN(status != BASE_SUCCESS, BASE_EBUG);

	if (status != BASE_EPENDING)
	    return status;
    }

    return BASE_SUCCESS;
}


bstatus_t bactivesock_start_recvfrom(bactivesock_t *asock,
						 bpool_t *pool,
						 unsigned buff_size,
						 buint32_t flags)
{
    void **readbuf;
    unsigned i;

    BASE_ASSERT_RETURN(asock && pool && buff_size, BASE_EINVAL);

    readbuf = (void**) bpool_calloc(pool, asock->async_count, 
				      sizeof(void*));

    for (i=0; i<asock->async_count; ++i) {
	readbuf[i] = bpool_alloc(pool, buff_size);
    }

    return bactivesock_start_recvfrom2(asock, pool, buff_size, 
					 readbuf, flags);
}


bstatus_t bactivesock_start_recvfrom2( bactivesock_t *asock, bpool_t *pool, unsigned buff_size, void *readbuf[], buint32_t flags)
{
	unsigned i;
	bstatus_t status;

	BASE_ASSERT_RETURN(asock && pool && buff_size, BASE_EINVAL);
	
	if(asock->read_type != TYPE_NONE)
		{
		BASE_ERROR("read type: %d", asock->read_type);
		}
	BASE_ASSERT_RETURN(asock->read_type == TYPE_NONE, BASE_EINVALIDOP);

	asock->read_op = (struct read_op*)
	bpool_calloc(pool, asock->async_count, sizeof(struct read_op));
	asock->read_type = TYPE_RECV_FROM;
	asock->read_flags = flags;

	for (i=0; i<asock->async_count; ++i)
	{
		struct read_op *r = &asock->read_op[i];
		bssize_t size_to_read;

		r->pkt = (buint8_t*) readbuf[i];
		size_to_read = r->max_size = buff_size;
		r->src_addr_len = sizeof(r->src_addr);

		status = bioqueue_recvfrom(asock->key, &r->op_key, r->pkt, &size_to_read,  BASE_IOQUEUE_ALWAYS_ASYNC | flags, &r->src_addr, &r->src_addr_len);
		BASE_ASSERT_RETURN(status != BASE_SUCCESS, BASE_EBUG);
//		status = bioqueue_recvfrom(asock->key, &r->op_key, r->pkt, &size_to_read,  flags, &r->src_addr, &r->src_addr_len);

		if (status != BASE_EPENDING)
			return status;
	}

	return BASE_SUCCESS;
}


static void ioqueue_on_read_complete(bioqueue_key_t *key, 
				     bioqueue_op_key_t *op_key, 
				     bssize_t bytes_read)
{
    bactivesock_t *asock;
    struct read_op *r = (struct read_op*)op_key;
    unsigned loop = 0;
    bstatus_t status;

    asock = (bactivesock_t*) bioqueue_get_user_data(key);

    /* Ignore if we've been shutdown */
    if (asock->shutdown & SHUT_RX)
	return;

    do {
	unsigned flags;

	if (bytes_read > 0) {
	    /*
	     * We've got new data.
	     */
	    bsize_t remainder;
	    bbool_t ret;

	    /* Append this new data to existing data. If socket is stream 
	     * oriented, user might have left some data in the buffer. 
	     * Otherwise if socket is datagram there will be nothing in 
	     * existing packet hence the packet will contain only the new
	     * packet.
	     */
	    r->size += bytes_read;

	    /* Set default remainder to zero */
	    remainder = 0;

	    /* And return value to TRUE */
	    ret = BASE_TRUE;

	    /* Notify callback */
	    if (asock->read_type == TYPE_RECV && asock->cb.on_data_read) {
		ret = (*asock->cb.on_data_read)(asock, r->pkt, r->size,
						BASE_SUCCESS, &remainder);
	    } else if (asock->read_type == TYPE_RECV_FROM && 
		       asock->cb.on_data_recvfrom) 
	    {
		ret = (*asock->cb.on_data_recvfrom)(asock, r->pkt, r->size,
						    &r->src_addr, 
						    r->src_addr_len,
						    BASE_SUCCESS);
	    }

	    /* If callback returns false, we have been destroyed! */
	    if (!ret)
		return;

	    /* Only stream oriented socket may leave data in the packet */
	    if (asock->stream_oriented) {
		r->size = remainder;
	    } else {
		r->size = 0;
	    }

	} else if (bytes_read <= 0 &&
		   -bytes_read != BASE_STATUS_FROM_OS(OSERR_EWOULDBLOCK) &&
		   -bytes_read != BASE_STATUS_FROM_OS(OSERR_EINPROGRESS) && 
		   (asock->stream_oriented ||
		    -bytes_read != BASE_STATUS_FROM_OS(OSERR_ECONNRESET))) 
	{
	    bsize_t remainder;
	    bbool_t ret;

	    if (bytes_read == 0) {
		/* For stream/connection oriented socket, this means the 
		 * connection has been closed. For datagram sockets, it means
		 * we've received datagram with zero length.
		 */
		if (asock->stream_oriented)
		    status = BASE_EEOF;
		else
		    status = BASE_SUCCESS;
	    } else {
		/* This means we've got an error. If this is stream/connection
		 * oriented, it means connection has been closed. For datagram
		 * sockets, it means we've got some error (e.g. EWOULDBLOCK).
		 */
		status = (bstatus_t)-bytes_read;
	    }

	    /* Set default remainder to zero */
	    remainder = 0;

	    /* And return value to TRUE */
	    ret = BASE_TRUE;

	    /* Notify callback */
	    if (asock->read_type == TYPE_RECV && asock->cb.on_data_read) {
		/* For connection oriented socket, we still need to report 
		 * the remainder data (if any) to the user to let user do 
		 * processing with the remainder data before it closes the
		 * connection.
		 * If there is no remainder data, set the packet to NULL.
		 */

		/* Shouldn't set the packet to NULL, as there may be active 
		 * socket user, such as SSL socket, that needs to have access
		 * to the read buffer packet.
		 */
		//ret = (*asock->cb.on_data_read)(asock, (r->size? r->pkt:NULL),
		//				r->size, status, &remainder);
		ret = (*asock->cb.on_data_read)(asock, r->pkt, r->size,
						status, &remainder);

	    } else if (asock->read_type == TYPE_RECV_FROM && 
		       asock->cb.on_data_recvfrom) 
	    {
		/* This would always be datagram oriented hence there's 
		 * nothing in the packet. We can't be sure if there will be
		 * anything useful in the source_addr, so just put NULL
		 * there too.
		 */
		/* In some scenarios, status may be BASE_SUCCESS. The upper 
		 * layer application may not expect the callback to be called
		 * with successful status and NULL data, so lets not call the
		 * callback if the status is BASE_SUCCESS.
		 */
		if (status != BASE_SUCCESS ) {
		    ret = (*asock->cb.on_data_recvfrom)(asock, NULL, 0,
							NULL, 0, status);
		}
	    }

	    /* If callback returns false, we have been destroyed! */
	    if (!ret)
		return;

	    /* Also stop further read if we've been shutdown */
	    if (asock->shutdown & SHUT_RX)
		return;

	    /* Only stream oriented socket may leave data in the packet */
	    if (asock->stream_oriented) {
		r->size = remainder;
	    } else {
		r->size = 0;
	    }
	}

	/* Read next data. We limit ourselves to processing max_loop immediate
	 * data, so when the loop counter has exceeded this value, force the
	 * read()/recvfrom() to return pending operation to allow the program
	 * to do other jobs.
	 */
	bytes_read = r->max_size - r->size;
	flags = asock->read_flags;
	if (++loop >= asock->max_loop)
	    flags |= BASE_IOQUEUE_ALWAYS_ASYNC;

	if (asock->read_type == TYPE_RECV) {
	    status = bioqueue_recv(key, op_key, r->pkt + r->size, 
				     &bytes_read, flags);
	} else {
	    r->src_addr_len = sizeof(r->src_addr);
	    status = bioqueue_recvfrom(key, op_key, r->pkt + r->size,
				         &bytes_read, flags,
					 &r->src_addr, &r->src_addr_len);
	}

	if (status == BASE_SUCCESS) {
	    /* Immediate data */
	    ;
	} else if (status != BASE_EPENDING && status != BASE_ECANCELLED) {
	    /* Error */
	    bytes_read = -status;
	} else {
	    break;
	}
    } while (1);

}


static bstatus_t send_remaining(bactivesock_t *asock, 
				  bioqueue_op_key_t *send_key)
{
    struct send_data *sd = (struct send_data*)send_key->activesock_data;
    bstatus_t status;

    do {
	bssize_t size;

	size = sd->len - sd->sent;
	status = bioqueue_send(asock->key, send_key, 
				 sd->data+sd->sent, &size, sd->flags);
	if (status != BASE_SUCCESS) {
	    /* Pending or error */
	    break;
	}

	sd->sent += size;
	if (sd->sent == sd->len) {
	    /* The whole data has been sent. */
	    return BASE_SUCCESS;
	}

    } while (sd->sent < sd->len);

    return status;
}


bstatus_t bactivesock_send( bactivesock_t *asock, bioqueue_op_key_t *send_key, const void *data, bssize_t *size, unsigned flags)
{
	BASE_ASSERT_RETURN(asock && send_key && data && size, BASE_EINVAL);

	if (asock->shutdown & SHUT_TX)
		return BASE_EINVALIDOP;

	send_key->activesock_data = NULL;

	if (asock->whole_data)
	{
		bssize_t whole;
		bstatus_t status;

		whole = *size;

		status = bioqueue_send(asock->key, send_key, data, size, flags);
		if (status != BASE_SUCCESS)
		{
			/* Pending or error */
			return status;
		}

		if (*size == whole)
		{/* The whole data has been sent. */
			return BASE_SUCCESS;
		}

		/* Data was partially sent */
		asock->send_data.data = (buint8_t*)data;
		asock->send_data.len = whole;
		asock->send_data.sent = *size;
		asock->send_data.flags = flags;
		send_key->activesock_data = &asock->send_data;

		/* Try again */
		status = send_remaining(asock, send_key);
		if (status == BASE_SUCCESS)
		{
			*size = whole;
		}
		return status;

	}
	else
	{
		return bioqueue_send(asock->key, send_key, data, size, flags);
	}
}


bstatus_t bactivesock_sendto( bactivesock_t *asock, bioqueue_op_key_t *send_key, const void *data, bssize_t *size,
	unsigned flags, const bsockaddr_t *addr, int addr_len)
{
	BASE_ASSERT_RETURN(asock && send_key && data && size && addr && addr_len, BASE_EINVAL);

	if (asock->shutdown & SHUT_TX)
		return BASE_EINVALIDOP;

	return bioqueue_sendto(asock->key, send_key, data, size, flags, addr, addr_len);
}


static void ioqueue_on_write_complete(bioqueue_key_t *key, 
				      bioqueue_op_key_t *op_key,
				      bssize_t bytes_sent)
{
    bactivesock_t *asock;

    asock = (bactivesock_t*) bioqueue_get_user_data(key);

    /* Ignore if we've been shutdown. This may cause data to be partially
     * sent even when 'wholedata' was requested if the OS only sent partial
     * buffer.
     */
    if (asock->shutdown & SHUT_TX)
	return;

    if (bytes_sent > 0 && op_key->activesock_data) {
	/* whole_data is requested. Make sure we send all the data */
	struct send_data *sd = (struct send_data*)op_key->activesock_data;

	sd->sent += bytes_sent;
	if (sd->sent == sd->len) {
	    /* all has been sent */
	    bytes_sent = sd->sent;
	    op_key->activesock_data = NULL;
	} else {
	    /* send remaining data */
	    bstatus_t status;

	    status = send_remaining(asock, op_key);
	    if (status == BASE_EPENDING)
		return;
	    else if (status == BASE_SUCCESS)
		bytes_sent = sd->sent;
	    else
		bytes_sent = -status;

	    op_key->activesock_data = NULL;
	}
    } 

    if (asock->cb.on_data_sent) {
	bbool_t ret;

	ret = (*asock->cb.on_data_sent)(asock, op_key, bytes_sent);

	/* If callback returns false, we have been destroyed! */
	if (!ret)
	    return;
    }
}

#if BASE_HAS_TCP
bstatus_t bactivesock_start_accept(bactivesock_t *asock, bpool_t *pool)
{
	unsigned i;

	BASE_ASSERT_RETURN(asock, BASE_EINVAL);
	BASE_ASSERT_RETURN(asock->accept_op==NULL, BASE_EINVALIDOP);

	/* Ignore if we've been shutdown */
	if (asock->shutdown)
		return BASE_EINVALIDOP;

	asock->accept_op = (struct accept_op*)
	bpool_calloc(pool, asock->async_count,
	sizeof(struct accept_op));
	for (i=0; i<asock->async_count; ++i)
	{
		struct accept_op *a = &asock->accept_op[i];
		bstatus_t status;

		do
		{
			a->new_sock = BASE_INVALID_SOCKET;
			a->rem_addr_len = sizeof(a->rem_addr);

			status = bioqueue_accept(asock->key, &a->op_key, &a->new_sock, NULL, &a->rem_addr, &a->rem_addr_len);
			if (status == BASE_SUCCESS)
			{
				/* We've got immediate connection. Not sure if it's a good
				* idea to call the callback now (probably application will
				* not be prepared to process it), so lets just silently
				* close the socket.
				*/
				bsock_close(a->new_sock);
			}
		} while (status == BASE_SUCCESS);

		if (status != BASE_EPENDING)
		{
			return status;
		}
	}

	return BASE_SUCCESS;
}


static void ioqueue_on_accept_complete(bioqueue_key_t *key, bioqueue_op_key_t *op_key, bsock_t new_sock,bstatus_t status)
{
	bactivesock_t *asock = (bactivesock_t*) bioqueue_get_user_data(key);
	struct accept_op *accept_op = (struct accept_op*) op_key;

	BASE_UNUSED_ARG(new_sock);

	/* Ignore if we've been shutdown */
	if (asock->shutdown)
		return;

	do
	{
		if (status == asock->last_err && status != BASE_SUCCESS)
		{
			asock->err_counter++;
			if (asock->err_counter >= BASE_ACTIVESOCK_MAX_CONSECUTIVE_ACCEPT_ERROR)
			{
				BASE_ERROR("Received %d consecutive errors: %d for the accept()"
					" operation, stopping further ioqueue accepts.", asock->err_counter, asock->last_err);

				if ((status == BASE_STATUS_FROM_OS(OSERR_EWOULDBLOCK)) && (asock->cb.on_accept_complete2)) 
				{
					(*asock->cb.on_accept_complete2)(asock, accept_op->new_sock, &accept_op->rem_addr, accept_op->rem_addr_len, BASE_ESOCKETSTOP);
				}
				return;
			}
		}
		else
		{
			asock->err_counter = 0;
			asock->last_err = status;
		}

		if (status==BASE_SUCCESS && (asock->cb.on_accept_complete2 || asock->cb.on_accept_complete))
		{
			bbool_t ret;

			/* Notify callback */
			if (asock->cb.on_accept_complete2)
			{
				ret = (*asock->cb.on_accept_complete2)(asock, accept_op->new_sock, &accept_op->rem_addr, accept_op->rem_addr_len, status);
			}
			else
			{
				ret = (*asock->cb.on_accept_complete)(asock, accept_op->new_sock, &accept_op->rem_addr, accept_op->rem_addr_len);
			}

			/* If callback returns false, we have been destroyed! */
			if (!ret)
				return;

#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
	BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
			activesock_create_iphone_os_stream(asock);
#endif
		}
		else if (status==BASE_SUCCESS)
		{
			/* Application doesn't handle the new socket, we need to 
			* close it to avoid resource leak.
			*/
			bsock_close(accept_op->new_sock);
		}

		/* Don't start another accept() if we've been shutdown */
		if (asock->shutdown)
			return;

		/* Prepare next accept() */
		accept_op->new_sock = BASE_INVALID_SOCKET;
		accept_op->rem_addr_len = sizeof(accept_op->rem_addr);

		status = bioqueue_accept(asock->key, op_key, &accept_op->new_sock, NULL, &accept_op->rem_addr, &accept_op->rem_addr_len);
	} while (status != BASE_EPENDING && status != BASE_ECANCELLED);
}


bstatus_t bactivesock_start_connect( bactivesock_t *asock, bpool_t *pool, const bsockaddr_t *remaddr, int addr_len)
{
	BASE_UNUSED_ARG(pool);

	if (asock->shutdown)
		return BASE_EINVALIDOP;

	return bioqueue_connect(asock->key, remaddr, addr_len);
}

static void ioqueue_on_connect_complete(bioqueue_key_t *key, 
					bstatus_t status)
{
    bactivesock_t *asock = (bactivesock_t*) bioqueue_get_user_data(key);

    /* Ignore if we've been shutdown */
    if (asock->shutdown)
	return;

    if (asock->cb.on_connect_complete) {
	bbool_t ret;

	ret = (*asock->cb.on_connect_complete)(asock, status);

	if (!ret) {
	    /* We've been destroyed */
	    return;
	}
	
#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
    BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
	activesock_create_iphone_os_stream(asock);
#endif
	
    }
}
#endif	/* BASE_HAS_TCP */

