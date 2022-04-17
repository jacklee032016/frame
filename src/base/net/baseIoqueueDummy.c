/* 
 *
 */
#include <baseIoQueue.h>
#include <baseOs.h>
#include <baseLog.h>
#include <baseList.h>
#include <basePool.h>
#include <baseString.h>
#include <baseAssert.h>
#include <baseSock.h>
#include <baseErrno.h>


#define BASE_IOQUEUE_IS_READ_OP(op)   \
	((op & BASE_IOQUEUE_OP_READ)  || (op & BASE_IOQUEUE_OP_RECV_FROM))
#define BASE_IOQUEUE_IS_WRITE_OP(op)  \
	((op & BASE_IOQUEUE_OP_WRITE) || (op & BASE_IOQUEUE_OP_SEND_TO))


#if BASE_HAS_TCP
#  define BASE_IOQUEUE_IS_ACCEPT_OP(op)	(op & BASE_IOQUEUE_OP_ACCEPT)
#  define BASE_IOQUEUE_IS_CONNECT_OP(op)	(op & BASE_IOQUEUE_OP_CONNECT)
#else
#  define BASE_IOQUEUE_IS_ACCEPT_OP(op)	0
#  define BASE_IOQUEUE_IS_CONNECT_OP(op)	0
#endif

#if defined(BASE_LIB_DEBUG) && BASE_LIB_DEBUG != 0
#  define VALIDATE_FD_SET		1
#else
#  define VALIDATE_FD_SET		0
#endif

struct bioqueue_key_t
{
    BASE_DECL_LIST_MEMBER(struct bioqueue_key_t)
    bsock_t		    fd;
    bioqueue_operation_e  op;
    void		   *user_data;
    bioqueue_callback	    cb;
};

struct bioqueue_t
{
};

bstatus_t bioqueue_create( bpool_t *pool, 
				       bsize_t max_fd,
				       int max_threads,
				       bioqueue_t **ptr_ioqueue)
{
    return BASE_ENOTSUP;
}

bstatus_t bioqueue_destroy(bioqueue_t *ioque)
{
    return BASE_ENOTSUP;
}

bstatus_t bioqueue_set_lock( bioqueue_t *ioque, 
					 block_t *lock,
					 bbool_t auto_delete )
{
    return BASE_ENOTSUP;
}

bstatus_t bioqueue_register_sock( bpool_t *pool,
					      bioqueue_t *ioque,
					      bsock_t sock,
					      void *user_data,
					      const bioqueue_callback *cb,
					      bioqueue_key_t **ptr_key)
{
    return BASE_ENOTSUP;
}

bstatus_t bioqueue_unregister( bioqueue_t *ioque,
					   bioqueue_key_t *key)
{
    return BASE_ENOTSUP;
}

void* bioqueue_get_user_data( bioqueue_key_t *key )
{
    return NULL;
}


int bioqueue_poll( bioqueue_t *ioque, const btime_val *timeout)
{
    return -1;
}

bstatus_t bioqueue_read( bioqueue_t *ioque,
				     bioqueue_key_t *key,
				     void *buffer,
				     bsize_t buflen)
{
    return -1;
}

bstatus_t bioqueue_recv( bioqueue_t *ioque,
				     bioqueue_key_t *key,
				     void *buffer,
				     bsize_t buflen,
				     unsigned flags)
{
    return -1;
}

bstatus_t bioqueue_recvfrom( bioqueue_t *ioque,
					 bioqueue_key_t *key,
					 void *buffer,
					 bsize_t buflen,
					 unsigned flags,
					 bsockaddr_t *addr,
					 int *addrlen)
{
    return -1;
}

bstatus_t bioqueue_write( bioqueue_t *ioque,
				      bioqueue_key_t *key,
				      const void *data,
				      bsize_t datalen)
{
    return -1;
}

bstatus_t bioqueue_send( bioqueue_t *ioque,
				     bioqueue_key_t *key,
				     const void *data,
				     bsize_t datalen,
				     unsigned flags)
{
    return -1;
}

bstatus_t bioqueue_sendto( bioqueue_t *ioque,
				       bioqueue_key_t *key,
				       const void *data,
				       bsize_t datalen,
				       unsigned flags,
				       const bsockaddr_t *addr,
				       int addrlen)
{
    return -1;
}

#if BASE_HAS_TCP
/*
 * Initiate overlapped accept() operation.
 */
bstatus_t bioqueue_accept( bioqueue_t *ioqueue,
				       bioqueue_key_t *key,
				       bsock_t *new_sock,
				       bsockaddr_t *local,
				       bsockaddr_t *remote,
				      int *addrlen)
{
    return -1;
}

/*
 * Initiate overlapped connect() operation (well, it's non-blocking actually,
 * since there's no overlapped version of connect()).
 */
bstatus_t bioqueue_connect( bioqueue_t *ioqueue,
					bioqueue_key_t *key,
					const bsockaddr_t *addr,
					int addrlen )
{
    return -1;
}
#endif
