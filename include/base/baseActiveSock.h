/* 
 *
 */
#ifndef __BASE_ASYNCSOCK_H__
#define __BASE_ASYNCSOCK_H__

#include <baseIoQueue.h>
#include <baseSock.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup BASE_ACTIVESOCK Active socket I/O
 * @brief Active socket performs active operations on socket.
 * @ingroup BASE_IO
 * @{
 *
 * Active socket is a higher level abstraction to the ioqueue. It provides
 * automation to socket operations which otherwise would have to be done
 * manually by the applications. For example with socket recv(), recvfrom(),
 * and accept() operations, application only needs to invoke these
 * operation once, and it will be notified whenever data or incoming TCP
 * connection (in the case of accept()) arrives.
 */

/* This opaque structure describes the active socket.  */
typedef struct _bactivesock_t bactivesock_t;

/**
 * This structure contains the callbacks to be called by the active socket.
 */
typedef struct _bactivesock_cb
{
    /**
     * This callback is called when a data arrives as the result of bactivesock_start_read().
     *
     * @param asock	The active socket.
     * @param data	The buffer containing the new data, if any. If 
     *			the status argument is non-BASE_SUCCESS, this 
     *			argument may be NULL.
     * @param size	The length of data in the buffer.
     * @param status	The status of the read operation. This may contain
     *			non-BASE_SUCCESS for example when the TCP connection
     *			has been closed. In this case, the buffer may
     *			contain left over data from previous callback which
     *			the application may want to process.
     * @param remainder	If application wishes to leave some data in the 
     *			buffer (common for TCP applications), it should 
     *			move the remainder data to the front part of the 
     *			buffer and set the remainder length here. The value
     *			of this parameter will be ignored for datagram
     *			sockets.
     *
     * @return		BASE_TRUE if further read is desired, and BASE_FALSE 
     *			when application no longer wants to receive data.
     *			Application may destroy the active socket in the
     *			callback and return BASE_FALSE here.
     */
    bbool_t (*on_data_read)(bactivesock_t *asock,
			      void *data,
			      bsize_t size,
			      bstatus_t status,
			      bsize_t *remainder);
    /**
     * This callback is called when a packet arrives as the result of
     * bactivesock_start_recvfrom().
     *
     * @param asock	The active socket.
     * @param data	The buffer containing the packet, if any. If 
     *			the status argument is non-BASE_SUCCESS, this 
     *			argument will be set to NULL.
     * @param size	The length of packet in the buffer. If 
     *			the status argument is non-BASE_SUCCESS, this 
     *			argument will be set to zero.
     * @param src_addr	Source address of the packet.
     * @param addr_len	Length of the source address.
     * @param status	This contains
     *
     * @return		BASE_TRUE if further read is desired, and BASE_FALSE 
     *			when application no longer wants to receive data.
     *			Application may destroy the active socket in the
     *			callback and return BASE_FALSE here.
     */
    bbool_t (*on_data_recvfrom)(bactivesock_t *asock,
				  void *data,
				  bsize_t size,
				  const bsockaddr_t *src_addr,
				  int addr_len,
				  bstatus_t status);

    /**
     * This callback is called when data has been sent.
     *
     * @param asock	The active socket.
     * @param send_key	Key associated with the send operation.
     * @param sent	If value is positive non-zero it indicates the
     *			number of data sent. When the value is negative,
     *			it contains the error code which can be retrieved
     *			by negating the value (i.e. status=-sent).
     *
     * @return		Application may destroy the active socket in the
     *			callback and return BASE_FALSE here.
     */
    bbool_t (*on_data_sent)(bactivesock_t *asock,
			      bioqueue_op_key_t *send_key,
			      bssize_t sent);

    /**
     * This callback is called when new connection arrives as the result
     * of bactivesock_start_accept(). If the status of accept operation is
     * needed use on_accept_complete2 instead of this callback.
     *
     * @param asock	The active socket.
     * @param newsock	The new incoming socket.
     * @param src_addr	The source address of the connection.
     * @param addr_len	Length of the source address.
     *
     * @return		BASE_TRUE if further accept() is desired, and BASE_FALSE
     *			when application no longer wants to accept incoming
     *			connection. Application may destroy the active socket
     *			in the callback and return BASE_FALSE here.
     */
    bbool_t (*on_accept_complete)(bactivesock_t *asock,
				    bsock_t newsock,
				    const bsockaddr_t *src_addr,
				    int src_addr_len);

    /**
     * This callback is called when new connection arrives as the result
     * of bactivesock_start_accept().
     *
     * @param asock	The active socket.
     * @param newsock	The new incoming socket.
     * @param src_addr	The source address of the connection.
     * @param addr_len	Length of the source address.
     * @param status	The status of the accept operation. This may contain
     *			non-BASE_SUCCESS for example when the TCP listener is in
     *			bad state for example on iOS platform after the
     *			application waking up from background.
     *
     * @return		BASE_TRUE if further accept() is desired, and BASE_FALSE
     *			when application no longer wants to accept incoming
     *			connection. Application may destroy the active socket
     *			in the callback and return BASE_FALSE here.
     */
    bbool_t (*on_accept_complete2)(bactivesock_t *asock,
				     bsock_t newsock,
				     const bsockaddr_t *src_addr,
				     int src_addr_len, 
				     bstatus_t status);

    /**
     * This callback is called when pending connect operation has been completed.
     *
     * @param asock	The active  socket.
     * @param status	The connection result. If connection has been
     *			successfully established, the status will contain
     *			BASE_SUCCESS.
     *
     * @return		Application may destroy the active socket in the
     *			callback and return BASE_FALSE here. 
     */
    bbool_t (*on_connect_complete)(bactivesock_t *asock, bstatus_t status);

} bactivesock_cb;


/**
 * Settings that can be given during active socket creation. Application
 * must initialize this structure with #bactivesock_cfg_default().
 */
typedef struct _bactivesock_cfg
{
    /**
     * Optional group lock to be assigned to the ioqueue key.
     */
    bgrp_lock_t *grp_lock;

    /**
     * Number of concurrent asynchronous operations that is to be supported
     * by the active socket. This value only affects socket receive and
     * accept operations -- the active socket will issue one or more 
     * asynchronous read and accept operations based on the value of this
     * field. Setting this field to more than one will allow more than one
     * incoming data or incoming connections to be processed simultaneously
     * on multiprocessor systems, when the ioqueue is polled by more than
     * one threads.
     *
     * The default value is 1.
     */
    unsigned async_cnt;

    /**
     * The ioqueue concurrency to be forced on the socket when it is 
     * registered to the ioqueue. See #bioqueue_set_concurrency() for more
     * info about ioqueue concurrency.
     *
     * When this value is -1, the concurrency setting will not be forced for
     * this socket, and the socket will inherit the concurrency setting of 
     * the ioqueue. When this value is zero, the active socket will disable
     * concurrency for the socket. When this value is +1, the active socket
     * will enable concurrency for the socket.
     *
     * The default value is -1.
     */
    int concurrency;

    /**
     * If this option is specified, the active socket will make sure that
     * asynchronous send operation with stream oriented socket will only
     * call the callback after all data has been sent. This means that the
     * active socket will automatically resend the remaining data until
     * all data has been sent.
     *
     * Please note that when this option is specified, it is possible that
     * error is reported after partial data has been sent. Also setting
     * this will disable the ioqueue concurrency for the socket.
     *
     * Default value is 1.
     */
    bbool_t whole_data;

} bactivesock_cfg;


/**
 * Initialize the active socket configuration with the default values.
 *
 * @param cfg		The configuration to be initialized.
 */
void bactivesock_cfg_default(bactivesock_cfg *cfg);


/**
 * Create the active socket for the specified socket. This will register
 * the socket to the specified ioqueue. 
 *
 * @param pool		Pool to allocate memory from.
 * @param sock		The socket handle.
 * @param sock_type	Specify socket type, either bSOCK_DGRAM() or
 *			bSOCK_STREAM(). The active socket needs this
 *			information to handle connection closure for
 *			connection oriented sockets.
 * @param ioqueue	The ioqueue to use.
 * @param opt		Optional settings. When this setting is not specifed,
 *			the default values will be used.
 * @param cb		Pointer to structure containing application
 *			callbacks.
 * @param user_data	Arbitrary user data to be associated with this
 *			active socket.
 * @param p_asock	Pointer to receive the active socket instance.
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t bactivesock_create(bpool_t *pool,
					  bsock_t sock,
					  int sock_type,
					  const bactivesock_cfg *opt,
					  bioqueue_t *ioqueue,
					  const bactivesock_cb *cb,
					  void *user_data,
					  bactivesock_t **p_asock);

/**
 * Create UDP socket descriptor, bind it to the specified address, and 
 * create the active socket for the socket descriptor.
 *
 * @param pool		Pool to allocate memory from.
 * @param addr		Specifies the address family of the socket and the
 *			address where the socket should be bound to. If
 *			this argument is NULL, then AF_INET is assumed and
 *			the socket will be bound to any addresses and port.
 * @param opt		Optional settings. When this setting is not specifed,
 *			the default values will be used.
 * @param cb		Pointer to structure containing application callbacks.
 * @param user_data	Arbitrary user data to be associated with this active socket.
 * @param p_asock	Pointer to receive the active socket instance.
 * @param bound_addr	If this argument is specified, it will be filled with the bound address on return.
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t bactivesock_create_udp(bpool_t *pool, const bsockaddr *addr, const bactivesock_cfg *opt,
	bioqueue_t *ioqueue, const bactivesock_cb *cb, void *user_data, bactivesock_t **p_asock, bsockaddr *bound_addr);

/**
 * Close the active socket. This will unregister the socket from the
 * ioqueue and ultimately close the socket.
 *
 * @param asock	    The active socket.
 *
 * @return	    BASE_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
bstatus_t bactivesock_close(bactivesock_t *asock);

#if (defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
     BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0) || \
     defined(DOXYGEN)
/**
 * Set iPhone OS background mode setting. Setting to 1 will enable TCP
 * active socket to receive incoming data when application is in the
 * background. Setting to 0 will disable it. Default value of this
 * setting is BASE_ACTIVESOCK_TCP_IPHONE_OS_BG.
 *
 * This API is only available if BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT
 * is set to non-zero.
 *
 * @param asock	    The active socket.
 * @param val	    The value of background mode setting.
 *
 */
void bactivesock_set_iphone_os_bg(bactivesock_t *asock, int val);

/**
 * Enable/disable support for iPhone OS background mode. This setting
 * will apply globally and will affect any active sockets created
 * afterwards, if you want to change the setting for a particular
 * active socket, use #bactivesock_set_iphone_os_bg() instead.
 * By default, this setting is enabled.
 *
 * This API is only available if BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT
 * is set to non-zero.
 *
 * @param val	    The value of global background mode setting.
 *
 */
void bactivesock_enable_iphone_os_bg(bbool_t val);
#endif

/**
 * Associate arbitrary data with the active socket. Application may
 * inspect this data in the callbacks and associate it with higher
 * level processing.
 *
 * @param asock	    The active socket.
 * @param user_data The user data to be associated with the active
 *		    socket.
 *
 * @return	    BASE_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
bstatus_t bactivesock_set_user_data(bactivesock_t *asock, void *user_data);

/**
 * Retrieve the user data previously associated with this active socket.
 *
 * @param asock	    The active socket.
 *
 * @return	    The user data.
 */
void* bactivesock_get_user_data(bactivesock_t *asock);


/**
 * Starts read operation on this active socket. This function will create
 * \a async_cnt number of buffers (the \a async_cnt parameter was given
 * in \a bactivesock_create() function) where each buffer is \a buff_size
 * long. The buffers are allocated from the specified \a pool. Once the 
 * buffers are created, it then issues \a async_cnt number of asynchronous
 * \a recv() operations to the socket and returns back to caller. Incoming
 * data on the socket will be reported back to application via the 
 * \a on_data_read() callback.
 *
 * Application only needs to call this function once to initiate read
 * operations. Further read operations will be done automatically by the
 * active socket when \a on_data_read() callback returns non-zero. 
 *
 * @param asock	    The active socket.
 * @param pool	    Pool used to allocate buffers for incoming data.
 * @param buff_size The size of each buffer, in bytes.
 * @param flags	    Flags to be given to bioqueue_recv().
 *
 * @return	    BASE_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
bstatus_t bactivesock_start_read(bactivesock_t *asock,
					      bpool_t *pool,
					      unsigned buff_size,
					      buint32_t flags);

/**
 * Same as #bactivesock_start_read(), except that the application
 * supplies the buffers for the read operation so that the acive socket
 * does not have to allocate the buffers.
 *
 * @param asock	    The active socket.
 * @param pool	    Pool used to allocate buffers for incoming data.
 * @param buff_size The size of each buffer, in bytes.
 * @param readbuf   Array of packet buffers, each has buff_size size.
 * @param flags	    Flags to be given to bioqueue_recv().
 *
 * @return	    BASE_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
bstatus_t bactivesock_start_read2(bactivesock_t *asock,
					       bpool_t *pool,
					       unsigned buff_size,
					       void *readbuf[],
					       buint32_t flags);

/**
 * Same as bactivesock_start_read(), except that this function is used
 * only for datagram sockets, and it will trigger \a on_data_recvfrom()
 * callback instead.
 *
 * @param asock	    The active socket.
 * @param pool	    Pool used to allocate buffers for incoming data.
 * @param buff_size The size of each buffer, in bytes.
 * @param flags	    Flags to be given to bioqueue_recvfrom().
 *
 * @return	    BASE_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
bstatus_t bactivesock_start_recvfrom(bactivesock_t *asock,
						  bpool_t *pool,
						  unsigned buff_size,
						  buint32_t flags);

/**
 * Same as #bactivesock_start_recvfrom() except that the recvfrom() 
 * operation takes the buffer from the argument rather than creating
 * new ones.
 *
 * @param asock	    The active socket.
 * @param pool	    Pool used to allocate buffers for incoming data.
 * @param buff_size The size of each buffer, in bytes.
 * @param readbuf   Array of packet buffers, each has buff_size size.
 * @param flags	    Flags to be given to bioqueue_recvfrom().
 *
 * @return	    BASE_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
bstatus_t bactivesock_start_recvfrom2(bactivesock_t *asock,
						   bpool_t *pool,
						   unsigned buff_size,
						   void *readbuf[],
						   buint32_t flags);

/**
 * Send data using the socket.
 *
 * @param asock	    The active socket.
 * @param send_key  The operation key to send the data, which is useful
 *		    if application wants to submit multiple pending
 *		    send operations and want to track which exact data 
 *		    has been sent in the \a on_data_sent() callback.
 * @param data	    The data to be sent. This data must remain valid
 *		    until the data has been sent.
 * @param size	    The size of the data.
 * @param flags	    Flags to be given to bioqueue_send().
 *
 *
 * @return	    BASE_SUCCESS if data has been sent immediately, or
 *		    BASE_EPENDING if data cannot be sent immediately. In
 *		    this case the \a on_data_sent() callback will be
 *		    called when data is actually sent. Any other return
 *		    value indicates error condition.
 */
bstatus_t bactivesock_send(bactivesock_t *asock,
					bioqueue_op_key_t *send_key,
					const void *data,
					bssize_t *size,
					unsigned flags);

/**
 * Send datagram using the socket.
 *
 * @param asock	    The active socket.
 * @param send_key  The operation key to send the data, which is useful
 *		    if application wants to submit multiple pending
 *		    send operations and want to track which exact data 
 *		    has been sent in the \a on_data_sent() callback.
 * @param data	    The data to be sent. This data must remain valid
 *		    until the data has been sent.
 * @param size	    The size of the data.
 * @param flags	    Flags to be given to bioqueue_send().
 * @param addr	    The destination address.
 * @param addr_len  The length of the address.
 *
 * @return	    BASE_SUCCESS if data has been sent immediately, or
 *		    BASE_EPENDING if data cannot be sent immediately. In
 *		    this case the \a on_data_sent() callback will be
 *		    called when data is actually sent. Any other return
 *		    value indicates error condition.
 */
bstatus_t bactivesock_sendto(bactivesock_t *asock,
					  bioqueue_op_key_t *send_key,
					  const void *data,
					  bssize_t *size,
					  unsigned flags,
					  const bsockaddr_t *addr,
					  int addr_len);

#if BASE_HAS_TCP
/**
 * Starts asynchronous socket accept() operations on this active socket. 
 * Application must bind the socket before calling this function. This 
 * function will issue \a async_cnt number of asynchronous \a accept() 
 * operations to the socket and returns back to caller. Incoming
 * connection on the socket will be reported back to application via the
 * \a on_accept_complete() callback.
 *
 * Application only needs to call this function once to initiate accept()
 * operations. Further accept() operations will be done automatically by 
 * the active socket when \a on_accept_complete() callback returns non-zero.
 *
 * @param asock	    The active socket.
 * @param pool	    Pool used to allocate some internal data for the operation.
 *
 * @return	    BASE_SUCCESS if the operation has been successful,
 *		    or the appropriate error code on failure.
 */
bstatus_t bactivesock_start_accept(bactivesock_t *asock, bpool_t *pool);

/**
 * Starts asynchronous socket connect() operation for this socket. Once
 * the connection is done (either successfully or not), the 
 * \a on_connect_complete() callback will be called.
 *
 * @param asock	    The active socket.
 * @param pool	    The pool to allocate some internal data for the operation.
 * @param remaddr   Remote address.
 * @param addr_len  Length of the remote address.
 *
 * @return	    BASE_SUCCESS if connection can be established immediately,
 *		    or BASE_EPENDING if connection cannot be established 
 *		    immediately. In this case the \a on_connect_complete()
 *		    callback will be called when connection is complete. 
 *		    Any other return value indicates error condition.
 */
bstatus_t bactivesock_start_connect(bactivesock_t *asock, bpool_t *pool, const bsockaddr_t *remaddr, int addr_len);


#endif


#ifdef __cplusplus
}
#endif


#endif

