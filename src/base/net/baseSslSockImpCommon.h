/* 
 *
 */

#ifndef __SSL_SOCK_IMP_COMMON_H__
#define __SSL_SOCK_IMP_COMMON_H__

#include <baseActiveSock.h>
#include <baseTimer.h>

/*
 * SSL/TLS state enumeration.
 */
enum ssl_state {
    SSL_STATE_NULL,
    SSL_STATE_HANDSHAKING,
    SSL_STATE_ESTABLISHED,
    SSL_STATE_ERROR
};

/*
 * Internal timer types.
 */
enum timer_id
{
    TIMER_NONE,
    TIMER_HANDSHAKE_TIMEOUT,
    TIMER_CLOSE
};

/*
 * Structure of SSL socket read buffer.
 */
typedef struct read_data_t
{
    void		 *data;
    bsize_t		  len;
} read_data_t;

/*
 * Structure of SSL socket write data.
 */
typedef struct write_data_t {
    BASE_DECL_LIST_MEMBER(struct write_data_t);
    bioqueue_op_key_t	 key;
    bsize_t 	 	 record_len;
    bioqueue_op_key_t	*app_key;
    bsize_t 	 	 plain_data_len;
    bsize_t 	 	 data_len;
    unsigned		 flags;
    union {
	char		 content[1];
	const char	*ptr;
    } data;
} write_data_t;

/*
 * Structure of SSL socket write buffer (circular buffer).
 */
typedef struct send_buf_t {
    char		*buf;
    bsize_t		 max_len;    
    char		*start;
    bsize_t		 len;
} send_buf_t;

/* Circular buffer object */
typedef struct circ_buf_t {
    bssl_sock_t *owner;  /* owner of the circular buffer */
    bsize_t      cap;    /* maximum number of elements (must be power of 2) */
    bsize_t      readp;  /* index of oldest element */
    bsize_t      writep; /* index at which to write new element  */
    bsize_t      size;   /* number of elements */
    buint8_t    *buf;    /* data buffer */
    bpool_t     *pool;   /* where new allocations will take place */
} circ_buf_t;

/*
 * Secure socket structure definition.
 */
struct bssl_sock_t
{
    bpool_t		 *pool;
    bssl_sock_t	 *parent;
    bssl_sock_param	  param;
    bssl_sock_param	  newsock_param;
    bssl_cert_t	 *cert;
    
    bssl_cert_info	  local_cert_info;
    bssl_cert_info	  remote_cert_info;

    bbool_t		  is_server;
    enum ssl_state	  ssl_state;
    bioqueue_op_key_t	  handshake_op_key;
    btimer_entry	  timer;
    bstatus_t		  verify_status;

    unsigned long	  last_err;

    bsock_t		  sock;
    bactivesock_t	 *asock;

    bsockaddr		  local_addr;
    bsockaddr		  rem_addr;
    int			  addr_len;
    
    bbool_t		  read_started;
    bsize_t		  read_size;
    buint32_t		  read_flags;
    void		**asock_rbuf;
    read_data_t		 *ssock_rbuf;

    write_data_t	  write_pending;/* list of pending write to ssl */
    write_data_t	  write_pending_empty; /* cache for write_pending   */
    bbool_t		  flushing_write_pend; /* flag of flushing is ongoing*/
    send_buf_t		  send_buf;
    write_data_t 	  send_buf_pending; /* send buffer is full but some
					     * data is queuing in wbio.     */
    write_data_t	  send_pending;	/* list of pending write to network */
    block_t		 *write_mutex;	/* protect write BIO and send_buf   */

    circ_buf_t            circ_buf_input;
    block_t            *circ_buf_input_mutex;

    circ_buf_t            circ_buf_output;
    block_t            *circ_buf_output_mutex;
};


/*
 * Certificate/credential structure definition.
 */
struct bssl_cert_t
{
    bstr_t CA_file;
    bstr_t CA_path;
    bstr_t cert_file;
    bstr_t privkey_file;
    bstr_t privkey_pass;

    /* Certificate buffer. */
    bssl_cert_buffer CA_buf;
    bssl_cert_buffer cert_buf;
    bssl_cert_buffer privkey_buf;
};

/* ssl available ciphers */
static unsigned ssl_cipher_num;
static struct ssl_ciphers_t {
    bssl_cipher    id;
    const char	    *name;
} ssl_ciphers[BASE_SSL_SOCK_MAX_CIPHERS];

/* ssl available curves */
static unsigned ssl_curves_num;
static struct ssl_curves_t {
    bssl_curve    id;
    const char	    *name;
} ssl_curves[BASE_SSL_SOCK_MAX_CURVES];

/*
 *******************************************************************
 * I/O functions.
 *******************************************************************
 */

static bbool_t io_empty(bssl_sock_t *ssock, circ_buf_t *cb);
static bsize_t io_size(bssl_sock_t *ssock, circ_buf_t *cb);
static void io_read(bssl_sock_t *ssock, circ_buf_t *cb,
		    buint8_t *dst, bsize_t len);
static bstatus_t io_write(bssl_sock_t *ssock, circ_buf_t *cb,
                            const buint8_t *src, bsize_t len);

static write_data_t* alloc_send_data(bssl_sock_t *ssock, bsize_t len);
static void free_send_data(bssl_sock_t *ssock, write_data_t *wdata);
static bstatus_t flush_delayed_send(bssl_sock_t *ssock);

#ifdef SSL_SOCK_IMP_USE_CIRC_BUF
/*
 *******************************************************************
 * Circular buffer functions.
 *******************************************************************
 */

static bstatus_t circ_init(bpool_factory *factory,
                             circ_buf_t *cb, bsize_t cap);
static void circ_deinit(circ_buf_t *cb);
static bbool_t circ_empty(const circ_buf_t *cb);
static bsize_t circ_size(const circ_buf_t *cb);
static void circ_read(circ_buf_t *cb, buint8_t *dst, bsize_t len);
static bstatus_t circ_write(circ_buf_t *cb,
                              const buint8_t *src, bsize_t len);

inline static bbool_t io_empty(bssl_sock_t *ssock, circ_buf_t *cb)
{
    return circ_empty(cb);
}
inline static bsize_t io_size(bssl_sock_t *ssock, circ_buf_t *cb)
{
    return circ_size(cb);
}
inline static void io_reset(bssl_sock_t *ssock, circ_buf_t *cb) {}
inline static void io_read(bssl_sock_t *ssock, circ_buf_t *cb,
		    	   buint8_t *dst, bsize_t len)
{
    return circ_read(cb, dst, len);
}
inline static bstatus_t io_write(bssl_sock_t *ssock, circ_buf_t *cb,
                            const buint8_t *src, bsize_t len)
{
    return circ_write(cb, src, len);
}

#endif


/*
 *******************************************************************
 * The below functions must be implemented by SSL backend.
 *******************************************************************
 */

/* Allocate SSL backend struct */
static bssl_sock_t *ssl_alloc(bpool_t *pool);
/* Create and initialize new SSL context and instance */
static bstatus_t ssl_create(bssl_sock_t *ssock);
/* Destroy SSL context and instance */
static void ssl_destroy(bssl_sock_t *ssock);
/* Reset SSL socket state */
static void ssl_reset_sock_state(bssl_sock_t *ssock);

/* Ciphers and certs */
static void ssl_ciphers_populate();
static bssl_cipher ssl_get_cipher(bssl_sock_t *ssock);
static void ssl_update_certs_info(bssl_sock_t *ssock);

/* SSL session functions */
static void ssl_set_state(bssl_sock_t *ssock, bbool_t is_server);
static void ssl_set_peer_name(bssl_sock_t *ssock);

static bstatus_t ssl_do_handshake(bssl_sock_t *ssock);
static bstatus_t ssl_renegotiate(bssl_sock_t *ssock);
static bstatus_t ssl_read(bssl_sock_t *ssock, void *data, int *size);
static bstatus_t ssl_write(bssl_sock_t *ssock, const void *data,
			     bssize_t size, int *nwritten);

#endif /* __SSL_SOCK_IMP_COMMON_H__ */
       