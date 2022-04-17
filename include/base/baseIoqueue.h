/*
 *
 */
#ifndef __BASE_IOQUEUE_H__
#define __BASE_IOQUEUE_H__

/**
 * @brief I/O Dispatching Mechanism
 */

#include <_baseTypes.h>

#ifdef __cplusplus
	extern "C" {
#endif

/**
 * @defgroup BASE_IO Input/Output
 * @brief Input/Output
 * @ingroup BASE_OS
 *
 * This section contains API building blocks to perform network I/O and communications. If provides:  - @ref BASE_SOCK
 *\n
 *    A highly portable socket abstraction, runs on all kind of
 *    network APIs such as standard BSD socket, Windows socket, Linux
 *    \b kernel socket, PalmOS networking API, etc.
 *
 *  - @ref baddr_resolve
 *\n
 *    Portable address resolution, which implements #bgethostbyname().
 *
 *  - @ref BASE_SOCK_SELECT
 *\n
 *    A portable \a select() like API (#bsock_select()) which can be implemented with various back-ends.
 *
 *  - @ref BASE_IOQUEUE
 *\n
 *    Framework for dispatching network events.
 *
 * For more information see the modules below.
 */

/**
 * @defgroup BASE_IOQUEUE IOQueue: I/O Event Dispatching with Proactor Pattern
 * @ingroup BASE_IO
 * @{
 *
 * I/O Queue provides API for performing asynchronous I/O operations. It
 * conforms to proactor pattern, which allows application to submit an
 * asynchronous operation and to be notified later when the operation has
 * completed.
 *
 * The I/O Queue can work on both socket and file descriptors. For 
 * asynchronous file operations however, one must make sure that the correct
 * file I/O back-end is used, because not all file I/O back-end can be
 * used with the ioqueue. Please see \ref BASE_FILE_IO for more details.
 *
 * The framework works natively in platforms where asynchronous operation API
 * exists, such as in Windows NT with IoCompletionPort/IOCP. In other 
 * platforms, the I/O queue abstracts the operating system's event poll API
 * to provide semantics similar to IoCompletionPort with minimal penalties
 * (i.e. per ioqueue and per handle mutex protection).
 *
 * The I/O queue provides more than just unified abstraction. It also:
 *  - makes sure that the operation uses the most effective way to utilize
 *    the underlying mechanism, to achieve the maximum theoritical
 *    throughput possible on a given platform.
 *  - choose the most efficient mechanism for event polling on a given
 *    platform.
 *
 * Currently, the I/O Queue is implemented using:
 *  - <tt><b>select()</b></tt>, as the common denominator, but the least 
 *    efficient. Also the number of descriptor is limited to 
 *    \c BASE_IOQUEUE_MAX_HANDLES (which by default is 64).
 *  - <tt><b>/dev/epoll</b></tt> on Linux (user mode and kernel mode), 
 *    a much faster replacement for select() on Linux (and more importantly
 *    doesn't have limitation on number of descriptors).
 *  - <b>I/O Completion ports</b> on Windows NT/2000/XP, which is the most 
 *    efficient way to dispatch events in Windows NT based OSes, and most 
 *    importantly, it doesn't have the limit on how many handles to monitor.
 *    And it works with files (not only sockets) as well.
 *
 *
 * \section bioqueue_concurrency_sec Concurrency Rules
 *
 * The ioqueue has been fine tuned to allow multiple threads to poll the
 * handles simultaneously, to maximize scalability when the application is
 * running on multiprocessor systems. When more than one threads are polling
 * the ioqueue and there are more than one handles are signaled, more than
 * one threads will execute the callback simultaneously to serve the events.
 * These parallel executions are completely safe when the events happen for
 * two different handles.
 *
 * However, with multithreading, care must be taken when multiple events 
 * happen on the same handle, or when event is happening on a handle (and 
 * the callback is being executed) and application is performing 
 * unregistration to the handle at the same time.
 *
 * The treatments of above scenario differ according to the concurrency
 * setting that are applied to the handle.
 *
 * \subsection bioq_concur_set Concurrency Settings for Handles
 *
 * Concurrency can be set on per handle (key) basis, by using
 * #bioqueue_set_concurrency() function. The default key concurrency value 
 * for the handle is inherited from the key concurrency setting of the ioqueue, 
 * and the key concurrency setting for the ioqueue can be changed by using
 * #bioqueue_set_default_concurrency(). The default key concurrency setting 
 * for ioqueue itself is controlled by compile time setting
 * BASE_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY.
 *
 * Note that this key concurrency setting only controls whether multiple
 * threads are allowed to operate <b>on the same key</b> at the same time. 
 * The ioqueue itself always allows multiple threads to enter the ioqeuue at 
 * the same time, and also simultaneous callback calls to <b>differrent 
 * keys</b> is always allowed regardless to the key concurrency setting.
 *
 * \subsection bioq_parallel Parallel Callback Executions for the Same Handle
 *
 * Note that when key concurrency is enabled (i.e. parallel callback calls on
 * the same key is allowed; this is the default setting), the ioqueue will only
 * perform simultaneous callback executions on the same key when the key has
 * invoked multiple pending operations. This could be done for example by
 * calling #bioqueue_recvfrom() more than once on the same key, each with
 * the same key but different operation key (bioqueue_op_key_t). With this
 * scenario, when multiple packets arrive on the key at the same time, more
 * than one threads may execute the callback simultaneously, each with the
 * same key but different operation key.
 *
 * When there is only one pending operation on the key (e.g. there is only one
 * #bioqueue_recvfrom() invoked on the key), then events occuring to the
 * same key will be queued by the ioqueue, thus no simultaneous callback calls
 * will be performed.
 *
 * \subsection bioq_allow_concur Concurrency is Enabled (Default Value)
 *
 * The default setting for the ioqueue is to allow multiple threads to
 * execute callbacks for the same handle/key. This setting is selected to
 * promote good performance and scalability for application.
 *
 * However this setting has a major drawback with regard to synchronization,
 * and application MUST carefully follow the following guidelines to ensure 
 * that parallel access to the key does not cause problems:
 *
 *  - Always note that callback may be called simultaneously for the same
 *    key.
 *  - <b>Care must be taken when unregistering a key</b> from the
 *    ioqueue. Application must take care that when one thread is issuing
 *    an unregistration, other thread is not simultaneously invoking the
 *    callback <b>to the same key</b>.
 *\n
 *    This happens because the ioqueue functions are working with a pointer
 *    to the key, and there is a possible race condition where the pointer
 *    has been rendered invalid by other threads before the ioqueue has a
 *    chance to acquire mutex on it.
 *
 * \subsection bioq_disallow_concur Concurrency is Disabled
 *
 * Alternatively, application may disable key concurrency to make 
 * synchronization easier. As noted above, there are three ways to control
 * key concurrency setting:
 *  - by controlling on per handle/key basis, with #bioqueue_set_concurrency().
 *  - by changing default key concurrency setting on the ioqueue, with
 *    #bioqueue_set_default_concurrency().
 *  - by changing the default concurrency on compile time, by declaring
 *    BASE_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY macro to zero in your config_site.h
 *
 * \section bioqeuue_examples_sec Examples
 *
 * For some examples on how to use the I/O Queue, please see:
 *
 *  - \ref page_baselib_ioqueue_tcp_test
 *  - \ref page_baselib_ioqueue_udp_test
 *  - \ref page_baselib_ioqueue_perf_test
 */


/**
 * This structure describes operation specific key to be submitted to
 * I/O Queue when performing the asynchronous operation. This key will
 * be returned to the application when completion callback is called.
 *
 * Application normally wants to attach it's specific data in the
 * \c user_data field so that it can keep track of which operation has
 * completed when the callback is called. Alternatively, application can
 * also extend this struct to include its data, because the pointer that
 * is returned in the completion callback will be exactly the same as
 * the pointer supplied when the asynchronous function is called.
 */
typedef struct _bioqueue_op_key_t
{ 
	void		*internal__[32];           /**< Internal I/O Queue data.   */
	void		*activesock_data;	    /**< Active socket data.	    */
	void		*user_data;                /**< Application data.          */
} bioqueue_op_key_t;

/**
 * This structure describes the callbacks to be called when I/O operation completes.
 */
typedef struct bioqueue_callback
{
    /**
     * This callback is called when #bioqueue_recv or #bioqueue_recvfrom
     * completes.
     *
     * @param key	    The key.
     * @param op_key        Operation key.
     * @param bytes_read    >= 0 to indicate the amount of data read, 
     *                      otherwise negative value containing the error
     *                      code. To obtain the bstatus_t error code, use
     *                      (bstatus_t code = -bytes_read).
     */
    void (*on_read_complete)(bioqueue_key_t *key, 
                             bioqueue_op_key_t *op_key, 
                             bssize_t bytes_read);

    /**
     * This callback is called when #bioqueue_send or #bioqueue_sendto
     * completes.
     *
     * @param key	    The key.
     * @param op_key        Operation key.
     * @param bytes_sent    >= 0 to indicate the amount of data written, 
     *                      otherwise negative value containing the error
     *                      code. To obtain the bstatus_t error code, use
     *                      (bstatus_t code = -bytes_sent).
     */
    void (*on_write_complete)(bioqueue_key_t *key, 
                              bioqueue_op_key_t *op_key, 
                              bssize_t bytes_sent);

    /**
     * This callback is called when #bioqueue_accept completes.
     *
     * @param key	    The key.
     * @param op_key        Operation key.
     * @param sock          Newly connected socket.
     * @param status	    Zero if the operation completes successfully.
     */
    void (*on_accept_complete)(bioqueue_key_t *key, bioqueue_op_key_t *op_key, bsock_t sock, bstatus_t status);

    /**
     * This callback is called when #bioqueue_connect completes.
     *
     * @param key	    The key.
     * @param status	    BASE_SUCCESS if the operation completes successfully.
     */
    void (*on_connect_complete)(bioqueue_key_t *key, bstatus_t status);
} bioqueue_callback;


/**
 * Types of pending I/O Queue operation. This enumeration is only used
 * internally within the ioqueue.
 */
typedef enum bioqueue_operation_e
{
	BASE_IOQUEUE_OP_NONE		= 0,	/**< No operation.          */
	BASE_IOQUEUE_OP_READ		= 1,	/**< read() operation.      */
	BASE_IOQUEUE_OP_RECV          = 2,    /**< recv() operation.      */
	BASE_IOQUEUE_OP_RECV_FROM	= 4,	/**< recvfrom() operation.  */
	BASE_IOQUEUE_OP_WRITE		= 8,	/**< write() operation.     */
	BASE_IOQUEUE_OP_SEND          = 16,   /**< send() operation.      */
	BASE_IOQUEUE_OP_SEND_TO	= 32,	/**< sendto() operation.    */
#if defined(BASE_HAS_TCP) && BASE_HAS_TCP != 0
	BASE_IOQUEUE_OP_ACCEPT	= 64,	/**< accept() operation.    */
	BASE_IOQUEUE_OP_CONNECT	= 128	/**< connect() operation.   */
#endif	/* BASE_HAS_TCP */
} bioqueue_operation_e;


/**
 * This macro specifies the maximum number of events that can be
 * processed by the ioqueue on a single poll cycle, on implementation
 * that supports it. The value is only meaningfull when specified
 * during  build.
 */
#ifndef BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL
#   define BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL     (16)
#endif


/**
 * This macro specifies the maximum event candidates collected by each
 * polling thread to be able to reach maximum number of processed events
 * (i.e: BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL) in each poll cycle.
 * An event candidate will be dispatched to application as event unless
 * it is already being dispatched by other polling thread. So in order to
 * anticipate such race condition, each poll operation should collects its
 * event candidates more than BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL, the
 * recommended value is (BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL *
 * number of polling threads).
 *
 * The value is only meaningfull when specified during  build and
 * is only effective on multiple polling threads environment.
 */
#if !defined(BASE_IOQUEUE_MAX_CAND_EVENTS) || \
    BASE_IOQUEUE_MAX_CAND_EVENTS < BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL
#   undef  BASE_IOQUEUE_MAX_CAND_EVENTS
#   define BASE_IOQUEUE_MAX_CAND_EVENTS	BASE_IOQUEUE_MAX_EVENTS_IN_SINGLE_POLL
#endif


/**
 * When this flag is specified in ioqueue's recv() or send() operations,
 * the ioqueue will always mark the operation as asynchronous.
 */
#define BASE_IOQUEUE_ALWAYS_ASYNC	    ((buint32_t)1 << (buint32_t)31)

/**
 * Return the name of the ioqueue implementation.
 * @return		Implementation name.
 */
const char* bioqueue_name(void);


/**
 * Create a new I/O Queue framework.
 * @param pool		The pool to allocate the I/O queue structure. 
 * @param max_fd	The maximum number of handles to be supported, which 
 *			should not exceed BASE_IOQUEUE_MAX_HANDLES.
 * @param ioqueue	Pointer to hold the newly created I/O Queue.
 * @return		BASE_SUCCESS on success.
 */
bstatus_t bioqueue_create( bpool_t *pool, bsize_t max_fd, bioqueue_t **ioqueue);

/**
 * Destroy the I/O queue.
 * @param ioque	        The I/O Queue to be destroyed.
 * @return              BASE_SUCCESS if success.
 */
bstatus_t bioqueue_destroy( bioqueue_t *ioque );

/**
 * Set the lock object to be used by the I/O Queue. This function can only
 * be called right after the I/O queue is created, before any handle is
 * registered to the I/O queue.
 *
 * Initially the I/O queue is created with non-recursive mutex protection. 
 * Applications can supply alternative lock to be used by calling this 
 * function.
 *
 * @param ioque         The ioqueue instance.
 * @param lock          The lock to be used by the ioqueue.
 * @param auto_delete   In non-zero, the lock will be deleted by the ioqueue.
 *
 * @return              BASE_SUCCESS or the appropriate error code.
 */
bstatus_t bioqueue_set_lock( bioqueue_t *ioque, block_t *lock, bbool_t auto_delete );

/**
 * Set default concurrency policy for this ioqueue. If this function is not
 * called, the default concurrency policy for the ioqueue is controlled by 
 * compile time setting BASE_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY.
 *
 * Note that changing the concurrency setting to the ioqueue will only affect
 * subsequent key registrations. To modify the concurrency setting for
 * individual key, use #bioqueue_set_concurrency().
 *
 * @param ioqueue	The ioqueue instance.
 * @param allow		Non-zero to allow concurrent callback calls, or BASE_FALSE to disallow it.
 *
 * @return		BASE_SUCCESS on success or the appropriate error code.
 */
bstatus_t bioqueue_set_default_concurrency(bioqueue_t *ioqueue, bbool_t allow);

/**
 * Register a socket to the I/O queue framework. 
 * When a socket is registered to the IOQueue, it may be modified to use
 * non-blocking IO. If it is modified, there is no guarantee that this 
 * modification will be restored after the socket is unregistered.
 *
 * @param pool	    To allocate the resource for the specified handle, 
 *		    which must be valid until the handle/key is unregistered 
 *		    from I/O Queue.
 * @param ioque	    The I/O Queue.
 * @param sock	    The socket.
 * @param user_data User data to be associated with the key, which can be retrieved later.
 * @param cb	    Callback to be called when I/O operation completes. 
 * @param key       Pointer to receive the key to be associated with this socket. Subsequent I/O queue operation will need this key.
 *
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t bioqueue_register_sock( bpool_t *pool, bioqueue_t *ioque, bsock_t sock, void *user_data, const bioqueue_callback *cb, bioqueue_key_t **key );

/**
 * Variant of bioqueue_register_sock() with additional group lock parameter.
 * If group lock is set for the key, the key will add the reference counter
 * when the socket is registered and decrease it when it is destroyed.
 */
bstatus_t bioqueue_register_sock2(bpool_t *pool, bioqueue_t *ioque, bsock_t sock, bgrp_lock_t *grp_lock, void *user_data, const bioqueue_callback *cb, bioqueue_key_t **key );

/**
 * Unregister from the I/O Queue framework. Caller must make sure that
 * the key doesn't have any pending operations before calling this function,
 * by calling #bioqueue_is_pending() for all previously submitted
 * operations except asynchronous connect, and if necessary call
 * #bioqueue_post_completion() to cancel the pending operations.
 *
 * Note that asynchronous connect operation will automatically be 
 * cancelled during the unregistration.
 *
 * Also note that when I/O Completion Port backend is used, application
 * MUST close the handle immediately after unregistering the key. This is
 * because there is no unregistering API for IOCP. The only way to
 * unregister the handle from IOCP is to close the handle.
 *
 * @param key	    The key that was previously obtained from registration.
 *
 * @return          BASE_SUCCESS on success or the error code.
 *
 * @see bioqueue_is_pending
 */
bstatus_t bioqueue_unregister( bioqueue_key_t *key );


/**
 * Get user data associated with an ioqueue key.
 * @param key	    The key that was previously obtained from registration.
 * @return          The user data associated with the descriptor, or NULL 
 *                  on error or if no data is associated with the key during
 *                  registration.
 */
void* bioqueue_get_user_data( bioqueue_key_t *key );

/**
 * Set or change the user data to be associated with the file descriptor or handle or socket descriptor.
 *
 * @param key	    The key that was previously obtained from registration.
 * @param user_data User data to be associated with the descriptor.
 * @param old_data  Optional parameter to retrieve the old user data.
 *
 * @return          BASE_SUCCESS on success or the error code.
 */
bstatus_t bioqueue_set_user_data( bioqueue_key_t *key, void *user_data, void **old_data);

/**
 * Configure whether the ioqueue is allowed to call the key's callback
 * concurrently/in parallel. The default concurrency setting for the key
 * is controlled by ioqueue's default concurrency value, which can be
 * changed by calling #bioqueue_set_default_concurrency().
 *
 * If concurrency is allowed for the key, it means that if there are more
 * than one pending operations complete simultaneously, more than one
 * threads may call the key's  callback at the same time. This generally
 * would promote good scalability for application, at the expense of more
 * complexity to manage the concurrent accesses in application's code.
 *
 * Alternatively application may disable the concurrent access by
 * setting the \a allow flag to false. With concurrency disabled, only
 * one thread can call the key's callback at one time.
 *
 * @param key	    The key that was previously obtained from registration.
 * @param allow	    Set this to non-zero to allow concurrent callback calls
 *		    and zero (BASE_FALSE) to disallow it.
 *
 * @return	    BASE_SUCCESS on success or the appropriate error code.
 */
bstatus_t bioqueue_set_concurrency(bioqueue_key_t *key, bbool_t allow);

/**
 * Acquire the key's mutex. When the key's concurrency is disabled, 
 * application may call this function to synchronize its operation
 * with the key's callback (i.e. this function will block until the
 * key's callback returns).
 *
 * @param key	    The key that was previously obtained from registration.
 *
 * @return	    BASE_SUCCESS on success or the appropriate error code.
 */
bstatus_t bioqueue_lock_key(bioqueue_key_t *key);

/**
 * Try to acquire the key's mutex. When the key's concurrency is disabled, 
 * application may call this function to synchronize its operation
 * with the key's callback.
 *
 * @param key	    The key that was previously obtained from registration.
 *
 * @return	    BASE_SUCCESS on success or the appropriate error code.
 */
bstatus_t bioqueue_trylock_key(bioqueue_key_t *key);

/**
 * Release the lock previously acquired with bioqueue_lock_key().
 *
 * @param key	    The key that was previously obtained from registration.
 *
 * @return	    BASE_SUCCESS on success or the appropriate error code.
 */
bstatus_t bioqueue_unlock_key(bioqueue_key_t *key);

/**
 * Initialize operation key.
 *
 * @param op_key    The operation key to be initialied.
 * @param size	    The size of the operation key.
 */
void bioqueue_op_key_init( bioqueue_op_key_t *op_key, bsize_t size );

/**
 * Check if operation is pending on the specified operation key.
 * The \c op_key must have been initialized with #bioqueue_op_key_init() 
 * or submitted as pending operation before, or otherwise the result 
 * is undefined.
 *
 * @param key       The key.
 * @param op_key    The operation key, previously submitted to any of
 *                  the I/O functions and has returned BASE_EPENDING.
 *
 * @return          Non-zero if operation is still pending.
 */
bbool_t bioqueue_is_pending( bioqueue_key_t *key, bioqueue_op_key_t *op_key );


/**
 * Post completion status to the specified operation key and call the
 * appropriate callback. When the callback is called, the number of bytes 
 * received in read/write callback or the status in accept/connect callback
 * will be set from the \c bytes_status parameter.
 *
 * @param key           The key.
 * @param op_key        Pending operation key.
 * @param bytes_status  Number of bytes or status to be set. A good value
 *                      to put here is -BASE_ECANCELLED.
 *
 * @return              BASE_SUCCESS if completion status has been successfully
 *                      sent.
 */
bstatus_t bioqueue_post_completion( bioqueue_key_t *key, bioqueue_op_key_t *op_key, bssize_t bytes_status );


#if defined(BASE_HAS_TCP) && BASE_HAS_TCP != 0
/**
 * Instruct I/O Queue to accept incoming connection on the specified 
 * listening socket. This function will return immediately (i.e. non-blocking)
 * regardless whether a connection is immediately available. If the function
 * can't complete immediately, the caller will be notified about the incoming
 * connection when it calls bioqueue_poll(). If a new connection is
 * immediately available, the function returns BASE_SUCCESS with the new
 * connection; in this case, the callback WILL NOT be called.
 *
 * @param key	    The key which registered to the server socket.
 * @param op_key    An operation specific key to be associated with the
 *                  pending operation, so that application can keep track of
 *                  which operation has been completed when the callback is
 *                  called.
 * @param new_sock  Argument which contain pointer to receive the new socket
 *                  for the incoming connection.
 * @param local	    Optional argument which contain pointer to variable to 
 *                  receive local address.
 * @param remote    Optional argument which contain pointer to variable to 
 *                  receive the remote address.
 * @param addrlen   On input, contains the length of the buffer for the
 *		    address, and on output, contains the actual length of the
 *		    address. This argument is optional.
 * @return
 *  - BASE_SUCCESS    When connection is available immediately, and the 
 *                  parameters will be updated to contain information about 
 *                  the new connection. In this case, a completion callback
 *                  WILL NOT be called.
 *  - BASE_EPENDING   If no connection is available immediately. When a new
 *                  connection arrives, the callback will be called.
 *  - non-zero      which indicates the appropriate error code.
 */
bstatus_t bioqueue_accept( bioqueue_key_t *key,
                                        bioqueue_op_key_t *op_key,
					bsock_t *new_sock,
					bsockaddr_t *local,
					bsockaddr_t *remote,
					int *addrlen );

/**
 * Initiate non-blocking socket connect. If the socket can NOT be connected
 * immediately, asynchronous connect() will be scheduled and caller will be
 * notified via completion callback when it calls bioqueue_poll(). If
 * socket is connected immediately, the function returns BASE_SUCCESS and
 * completion callback WILL NOT be called.
 *
 * @param key	    The key associated with TCP socket
 * @param addr	    The remote address.
 * @param addrlen   The remote address length.
 *
 * @return
 *  - BASE_SUCCESS    If socket is connected immediately. In this case, the
 *                  completion callback WILL NOT be called.
 *  - BASE_EPENDING   If operation is queued, or 
 *  - non-zero      Indicates the error code.
 */
bstatus_t bioqueue_connect( bioqueue_key_t *key,
					 const bsockaddr_t *addr,
					 int addrlen );

#endif	/* BASE_HAS_TCP */

/**
 * Poll the I/O Queue for completed events.
 *
 * Note: polling the ioqueue is not necessary in Symbian. Please see
 * @ref BASE_SYMBIAN_OS for more info.
 *
 * @param ioque		the I/O Queue.
 * @param timeout	polling timeout, or NULL if the thread wishes to wait indefinetely for the event.
 *
 * @return 
 *  - zero if timed out (no event).
 *  - (<0) if error occured during polling. Callback will NOT be called.
 *  - (>1) to indicate numbers of events. Callbacks have been called.
 */
int bioqueue_poll( bioqueue_t *ioque, const btime_val *timeout);


/**
 * Instruct the I/O Queue to read from the specified handle. This function
 * returns immediately (i.e. non-blocking) regardless whether some data has 
 * been transferred. If the operation can't complete immediately, caller will 
 * be notified about the completion when it calls bioqueue_poll(). If data
 * is immediately available, the function will return BASE_SUCCESS and the
 * callback WILL NOT be called.
 *
 * @param key	    The key that uniquely identifies the handle.
 * @param op_key    An operation specific key to be associated with the
 *                  pending operation, so that application can keep track of
 *                  which operation has been completed when the callback is
 *                  called. Caller must make sure that this key remains 
 *                  valid until the function completes.
 * @param buffer    The buffer to hold the read data. The caller MUST make sure
 *		    that this buffer remain valid until the framework completes
 *		    reading the handle.
 * @param length    On input, it specifies the size of the buffer. If data is
 *                  available to be read immediately, the function returns
 *                  BASE_SUCCESS and this argument will be filled with the
 *                  amount of data read. If the function is pending, caller
 *                  will be notified about the amount of data read in the
 *                  callback. This parameter can point to local variable in
 *                  caller's stack and doesn't have to remain valid for the
 *                  duration of pending operation.
 * @param flags     Recv flag. If flags has BASE_IOQUEUE_ALWAYS_ASYNC then
 *		    the function will never return BASE_SUCCESS.
 *
 * @return
 *  - BASE_SUCCESS    If immediate data has been received in the buffer. In this
 *                  case, the callback WILL NOT be called.
 *  - BASE_EPENDING   If the operation has been queued, and the callback will be
 *                  called when data has been received.
 *  - non-zero      The return value indicates the error code.
 */
bstatus_t bioqueue_recv( bioqueue_key_t *key, bioqueue_op_key_t *op_key, void *buffer, bssize_t *length, buint32_t flags );

/**
 * This function behaves similarly as #bioqueue_recv(), except that it is
 * normally called for socket, and the remote address will also be returned
 * along with the data. Caller MUST make sure that both buffer and addr
 * remain valid until the framework completes reading the data.
 *
 * @param key	    The key that uniquely identifies the handle.
 * @param op_key    An operation specific key to be associated with the
 *                  pending operation, so that application can keep track of
 *                  which operation has been completed when the callback is
 *                  called.
 * @param buffer    The buffer to hold the read data. The caller MUST make sure
 *		    that this buffer remain valid until the framework completes
 *		    reading the handle.
 * @param length    On input, it specifies the size of the buffer. If data is
 *                  available to be read immediately, the function returns
 *                  BASE_SUCCESS and this argument will be filled with the
 *                  amount of data read. If the function is pending, caller
 *                  will be notified about the amount of data read in the
 *                  callback. This parameter can point to local variable in
 *                  caller's stack and doesn't have to remain valid for the
 *                  duration of pending operation.
 * @param flags     Recv flag. If flags has BASE_IOQUEUE_ALWAYS_ASYNC then
 *		    the function will never return BASE_SUCCESS.
 * @param addr      Optional Pointer to buffer to receive the address.
 * @param addrlen   On input, specifies the length of the address buffer.
 *                  On output, it will be filled with the actual length of
 *                  the address. This argument can be NULL if \c addr is not
 *                  specified.
 *
 * @return
 *  - BASE_SUCCESS    If immediate data has been received. In this case, the 
 *		    callback must have been called before this function 
 *		    returns, and no pending operation is scheduled.
 *  - BASE_EPENDING   If the operation has been queued.
 *  - non-zero      The return value indicates the error code.
 */
bstatus_t bioqueue_recvfrom( bioqueue_key_t *key,
                                          bioqueue_op_key_t *op_key,
					  void *buffer,
					  bssize_t *length,
                                          buint32_t flags,
					  bsockaddr_t *addr,
					  int *addrlen);

/**
 * Instruct the I/O Queue to write to the handle. This function will return
 * immediately (i.e. non-blocking) regardless whether some data has been 
 * transferred. If the function can't complete immediately, the caller will
 * be notified about the completion when it calls bioqueue_poll(). If 
 * operation completes immediately and data has been transferred, the function
 * returns BASE_SUCCESS and the callback will NOT be called.
 *
 * @param key	    The key that identifies the handle.
 * @param op_key    An operation specific key to be associated with the
 *                  pending operation, so that application can keep track of
 *                  which operation has been completed when the callback is
 *                  called.
 * @param data	    The data to send. Caller MUST make sure that this buffer 
 *		    remains valid until the write operation completes.
 * @param length    On input, it specifies the length of data to send. When
 *                  data was sent immediately, this function returns BASE_SUCCESS
 *                  and this parameter contains the length of data sent. If
 *                  data can not be sent immediately, an asynchronous operation
 *                  is scheduled and caller will be notified via callback the
 *                  number of bytes sent. This parameter can point to local 
 *                  variable on caller's stack and doesn't have to remain 
 *                  valid until the operation has completed.
 * @param flags     Send flags. If flags has BASE_IOQUEUE_ALWAYS_ASYNC then
 *		    the function will never return BASE_SUCCESS.
 *
 * @return
 *  - BASE_SUCCESS    If data was immediately transferred. In this case, no
 *                  pending operation has been scheduled and the callback
 *                  WILL NOT be called.
 *  - BASE_EPENDING   If the operation has been queued. Once data base been
 *                  transferred, the callback will be called.
 *  - non-zero      The return value indicates the error code.
 */
bstatus_t bioqueue_send( bioqueue_key_t *key,
                                      bioqueue_op_key_t *op_key,
				      const void *data,
				      bssize_t *length,
				      buint32_t flags );


/**
 * Instruct the I/O Queue to write to the handle. This function will return
 * immediately (i.e. non-blocking) regardless whether some data has been 
 * transferred. If the function can't complete immediately, the caller will
 * be notified about the completion when it calls bioqueue_poll(). If 
 * operation completes immediately and data has been transferred, the function
 * returns BASE_SUCCESS and the callback will NOT be called.
 *
 * @param key	    the key that identifies the handle.
 * @param op_key    An operation specific key to be associated with the
 *                  pending operation, so that application can keep track of
 *                  which operation has been completed when the callback is
 *                  called.
 * @param data	    the data to send. Caller MUST make sure that this buffer 
 *		    remains valid until the write operation completes.
 * @param length    On input, it specifies the length of data to send. When
 *                  data was sent immediately, this function returns BASE_SUCCESS
 *                  and this parameter contains the length of data sent. If
 *                  data can not be sent immediately, an asynchronous operation
 *                  is scheduled and caller will be notified via callback the
 *                  number of bytes sent. This parameter can point to local 
 *                  variable on caller's stack and doesn't have to remain 
 *                  valid until the operation has completed.
 * @param flags     send flags. If flags has BASE_IOQUEUE_ALWAYS_ASYNC then
 *		    the function will never return BASE_SUCCESS.
 * @param addr      Optional remote address.
 * @param addrlen   Remote address length, \c addr is specified.
 *
 * @return
 *  - BASE_SUCCESS    If data was immediately written.
 *  - BASE_EPENDING   If the operation has been queued.
 *  - non-zero      The return value indicates the error code.
 */
bstatus_t bioqueue_sendto( bioqueue_key_t *key,	bioqueue_op_key_t *op_key, const void *data, bssize_t *length,
	buint32_t flags, const bsockaddr_t *addr, int addrlen);


#ifdef __cplusplus
}
#endif

#endif

