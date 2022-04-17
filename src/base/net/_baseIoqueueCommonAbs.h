/* 
 *
 */

/* _baseIoqueueCommonAbs.h
 *
 * This file contains private declarations for abstracting various 
 * event polling/dispatching mechanisms (e.g. select, poll, epoll) 
 * to the ioqueue. 
 */

#include <baseList.h>

/*
 * The select ioqueue relies on socket functions (bsock_xxx()) to return
 * the correct error code.
 */
#if BASE_RETURN_OS_ERROR(100) != BASE_STATUS_FROM_OS(100)
#   error "Proper error reporting must be enabled for ioqueue to work!"
#endif


struct generic_operation
{
    BASE_DECL_LIST_MEMBER(struct generic_operation);
    bioqueue_operation_e  op;
};

struct read_operation
{
    BASE_DECL_LIST_MEMBER(struct read_operation);
    bioqueue_operation_e  op;

    void		   *buf;
    bsize_t		    size;
    unsigned                flags;
    bsockaddr_t	   *rmt_addr;
    int			   *rmt_addrlen;
};

struct write_operation
{
    BASE_DECL_LIST_MEMBER(struct write_operation);
    bioqueue_operation_e  op;

    char		   *buf;
    bsize_t		    size;
    bssize_t              written;
    unsigned                flags;
    bsockaddr_in	    rmt_addr;
    int			    rmt_addrlen;
};

struct accept_operation
{
    BASE_DECL_LIST_MEMBER(struct accept_operation);
    bioqueue_operation_e  op;

    bsock_t              *accept_fd;
    bsockaddr_t	   *local_addr;
    bsockaddr_t	   *rmt_addr;
    int			   *addrlen;
};

union operation_key
{
    struct generic_operation generic_op;
    struct read_operation    read;
    struct write_operation   write;
#if BASE_HAS_TCP
    struct accept_operation  accept;
#endif
};

#if BASE_IOQUEUE_HAS_SAFE_UNREG
#   define UNREG_FIELDS			\
	unsigned	    ref_count;	\
	bbool_t	    closing;	\
	btime_val	    free_time;	\
	
#else
#   define UNREG_FIELDS
#endif

#define DECLARE_COMMON_KEY                          \
    BASE_DECL_LIST_MEMBER(struct bioqueue_key_t);   \
    bioqueue_t           *ioqueue;                \
    bgrp_lock_t 	   *grp_lock;		    \
    block_t              *lock;                   \
    bbool_t		    inside_callback;	    \
    bbool_t		    destroy_requested;	    \
    bbool_t		    allow_concurrent;	    \
    bsock_t		    fd;                     \
    int                     fd_type;                \
    void		   *user_data;              \
    bioqueue_callback	    cb;                     \
    int                     connecting;             \
    struct read_operation   read_list;              \
    struct write_operation  write_list;             \
    struct accept_operation accept_list;	    \
    UNREG_FIELDS


#define DECLARE_COMMON_IOQUEUE                      \
    block_t          *lock;                       \
    bbool_t           auto_delete_lock;	    \
    bbool_t		default_concurrency;


enum ioqueue_event_type
{
    NO_EVENT,
    READABLE_EVENT,
    WRITEABLE_EVENT,
    EXCEPTION_EVENT,
};

static void ioqueue_add_to_set( bioqueue_t *ioqueue,
                                bioqueue_key_t *key,
                                enum ioqueue_event_type event_type );
static void ioqueue_remove_from_set( bioqueue_t *ioqueue,
                                     bioqueue_key_t *key, 
                                     enum ioqueue_event_type event_type);

