/* 
 *
 */
#ifndef __UTIL_HTTP_CLIENT_H__
#define __UTIL_HTTP_CLIENT_H__

/**
 * @brief Simple HTTP Client
 */
#include <baseActiveSock.h>
#include <utilTypes.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_HTTP_CLIENT Simple HTTP Client
 * @ingroup BASE_PROTOCOLS
 * @{
 * This contains a simple HTTP client implementation.
 * Some known limitations: 
 * - Does not support chunked Transfer-Encoding.
 */

/**
 * This opaque structure describes the http request.
 */
typedef struct bhttp_req bhttp_req;

/**
 * Defines the maximum number of elements in a bhttp_headers
 * structure.
 */
#define BASE_HTTP_HEADER_SIZE 32

/**
 * HTTP header representation.
 */
typedef struct bhttp_header_elmt
{
    bstr_t name;	/**< Header name */
    bstr_t value;	/**< Header value */
} bhttp_header_elmt;

/**
 * This structure describes http request/response headers.
 * Application should call #bhttp_headers_add_elmt() to
 * add a header field.
 */
typedef struct bhttp_headers
{
    /**< Number of header fields */
    unsigned     count;

    /** Header elements/fields */
    bhttp_header_elmt header[BASE_HTTP_HEADER_SIZE];
} bhttp_headers;

/**
 * Structure to save HTTP authentication credential.
 */
typedef struct bhttp_auth_cred
{
    /**
     * Specify specific authentication schemes to be responded. Valid values
     * are "basic" and "digest". If this field is not set, any authentication
     * schemes will be responded.
     *
     * Default is empty.
     */
    bstr_t	scheme;

    /**
     * Specify specific authentication realm to be responded. If this field
     * is set, only 401/407 response with matching realm will be responded.
     * If this field is not set, any realms will be responded.
     *
     * Default is empty.
     */
    bstr_t	realm;

    /**
     * Specify authentication username.
     *
     * Default is empty.
     */
    bstr_t	username;

    /**
     * The type of password in \a data field. Currently only 0 is
     * supported, meaning the \a data contains plain-text password.
     *
     * Default is 0.
     */
    unsigned	data_type;

    /**
     * Specify authentication password. The encoding of the password depends
     * on the value of \a data_type field above.
     *
     * Default is empty.
     */
    bstr_t	data;

} bhttp_auth_cred;


/**
 * Parameters that can be given during http request creation. Application
 * must initialize this structure with #bhttp_req_param_default().
 */
typedef struct bhttp_req_param 
{
    /** 
     * The address family of the URL.
     *  Default is bAF_INET().
     */
    int             addr_family;

    /** 
     * The HTTP request method.
     * Default is GET.
     */
    bstr_t        method;

    /** 
     * The HTTP protocol version ("1.0" or "1.1").
     * Default is "1.0".
     */
    bstr_t        version;

    /** 
     * HTTP request operation timeout.
     * Default is BASE_HTTP_DEFAULT_TIMEOUT.
     */
    btime_val     timeout;

    /** 
     * User-defined data.
     * Default is NULL.
     */
    void            *user_data;

    /** 
     * HTTP request headers.
     * Default is empty.
     */
    bhttp_headers headers;

    /**
      * This structure describes the http request body. If application
      * specifies the data to send, the data must remain valid until 
      * the HTTP request is sent. Alternatively, application can choose
      * to specify total_size as the total data size to send instead
      * while leaving the data NULL (and its size 0). In this case,
      * HTTP request will then call on_send_data() callback once it is 
      * ready to send the request body. This will be useful if 
      * application does not wish to load the data into the buffer at 
      * once.
      * 
      * Default is empty.
      */
    struct bhttp_reqdata
    {
        void       *data;          /**< Request body data */
        bsize_t  size;           /**< Request body size */
        bsize_t  total_size;     /**< If total_size > 0, data */
                                   /**< will be provided later  */
    } reqdata;

    /**
     * Authentication credential needed to respond to 401/407 response.
     */
    bhttp_auth_cred	auth_cred;

    /**
     * Optional source port range to use when binding the socket.
     * This can be used if the source port needs to be within a certain range
     * for instance due to strict firewall settings. The port used will be
     * randomized within the range.
     *
     * Note that if authentication is configured, the authentication response
     * will be a new transaction
     *
     * Default is 0 (The OS will select the source port automatically)
     */
    buint16_t		source_port_range_start;

    /**
     * Optional source port range to use when binding.
     * The size of the port restriction range
     *
     * Default is 0 (The OS will select the source port automatically))
     */
    buint16_t		source_port_range_size;

    /**
     * Max number of retries if binding to a port fails.
     * Note that this does not adress the scenario where a request times out
     * or errors. This needs to be taken care of by the on_complete callback.
     *
     * Default is 3
     */
    buint16_t		max_retries;

} bhttp_req_param;

/**
 * HTTP authentication challenge, parsed from WWW-Authenticate header.
 */
typedef struct bhttp_auth_chal
{
    bstr_t	scheme;		/**< Auth scheme.		*/
    bstr_t	realm;		/**< Realm for the challenge.	*/
    bstr_t	domain;		/**< Domain.			*/
    bstr_t	nonce;		/**< Nonce challenge.		*/
    bstr_t	opaque;		/**< Opaque value.		*/
    int		stale;		/**< Stale parameter.		*/
    bstr_t	algorithm;	/**< Algorithm parameter.	*/
    bstr_t	qop;		/**< Quality of protection.	*/
} bhttp_auth_chal;

/**
 * This structure describes HTTP response.
 */
typedef struct bhttp_resp
{
    bstr_t        version;        /**< HTTP version of the server */
    buint16_t     status_code;    /**< Status code of the request */
    bstr_t        reason;         /**< Reason phrase */
    bhttp_headers headers;        /**< Response headers */
    bhttp_auth_chal auth_chal;    /**< Parsed WWW-Authenticate header, if
				         any. */
    bint32_t      content_length; /**< The value of content-length header
					 field. -1 if not specified. */
    void            *data;          /**< Data received */
    bsize_t       size;           /**< Data size */
} bhttp_resp;

/**
 * This structure describes HTTP URL.
 */
typedef struct bhttp_url
{
    bstr_t	username;	    /**< Username part */
    bstr_t	passwd;		    /**< Password part */
    bstr_t    protocol;           /**< Protocol used */
    bstr_t    host;               /**< Host name */
    buint16_t port;               /**< Port number */
    bstr_t    path;               /**< Path */
} bhttp_url;

/**
 * This structure describes the callbacks to be called by the HTTP request.
 */
typedef struct bhttp_req_callback
{
    /**
     * This callback is called when a complete HTTP response header 
     * is received.
     *
     * @param http_req	The http request.
     * @param resp	The response of the request.
     */
    void (*on_response)(bhttp_req *http_req, const bhttp_resp *resp);

    /**
     * This callback is called when the HTTP request is ready to send
     * its request body. Application may wish to use this callback if
     * it wishes to load the data at a later time or if it does not 
     * wish to load the whole data into memory. In order for this
     * callback to be called, application MUST set http_req_param.total_size
     * to a value greater than 0.
     *
     * @param http_req	The http request.
     * @param data	Pointer to the data that will be sent. Application
     *                  must set the pointer to the current data chunk/segment
     *                  to be sent. Data must remain valid until the next 
     *                  on_send_data() callback or for the last segment,
     *                  until it is sent.
     * @param size	Pointer to the data size that will be sent.
     */
    void (*on_send_data)(bhttp_req *http_req,
                         void **data, bsize_t *size);

    /**
     * This callback is called when a segment of response body data
     * arrives. If this callback is specified (i.e. not NULL), the
     * on_complete() callback will be called with zero-length data
     * (within the response parameter), hence the application must 
     * store and manage its own data buffer, otherwise the 
     * on_complete() callback will be called with the response 
     * parameter containing the complete data. 
     * 
     * @param http_req	The http request.
     * @param data	The buffer containing the data.
     * @param size	The length of data in the buffer.
     */
    void (*on_data_read)(bhttp_req *http_req,
                         void *data, bsize_t size);

    /**
     * This callback is called when the HTTP request is completed.
     * If the callback on_data_read() is specified, the variable
     * response->data will be set to NULL, otherwise it will
     * contain the complete data. Response data is allocated from
     * bhttp_req's internal memory pool so the data remain valid
     * as long as bhttp_req is not destroyed and application does
     * not start a new request.
     *
     * If no longer required, application may choose to destroy 
     * bhttp_req immediately by calling #bhttp_req_destroy() inside 
     * the callback.
     *
     * @param http_req	The http request.
     * @param status	The status of the request operation. BASE_SUCCESS
     *                  if the operation completed successfully
     *                  (connection-wise). To check the server's 
     *                  status-code response to the HTTP request, 
     *                  application should check resp->status_code instead.
     * @param resp	The response of the corresponding request. If 
     *			the status argument is non-BASE_SUCCESS, this 
     *			argument will be set to NULL.
     */
    void (*on_complete)(bhttp_req *http_req,
                        bstatus_t status,
                        const bhttp_resp *resp);

} bhttp_req_callback;


/**
 * Initialize the http request parameters with the default values.
 *
 * @param param		The parameter to be initialized.
 */
void bhttp_req_param_default(bhttp_req_param *param);

/**
 * Add a header element/field. Application MUST make sure that 
 * name and val pointer remains valid until the HTTP request is sent.
 *
 * @param headers	The headers.
 * @param name	        The header field name.
 * @param value	        The header field value.
 *
 * @return	        BASE_SUCCESS if the operation has been successful,
 *		        or the appropriate error code on failure.
 */
bstatus_t bhttp_headers_add_elmt(bhttp_headers *headers, 
                                              bstr_t *name, 
                                              bstr_t *val);

/** 
 * The same as #bhttp_headers_add_elmt() with char * as
 * its parameters. Application MUST make sure that name and val pointer 
 * remains valid until the HTTP request is sent.
 *
 * @param headers	The headers.
 * @param name	        The header field name.
 * @param value	        The header field value.
 *
 * @return	        BASE_SUCCESS if the operation has been successful,
 *		        or the appropriate error code on failure.
 */
bstatus_t bhttp_headers_add_elmt2(bhttp_headers *headers, 
                                               char *name, char *val);

/**
 * Parse a http URL into its components.
 *
 * @param url	        The URL to be parsed.
 * @param hurl	        Pointer to receive the parsed result.
 *
 * @return	        BASE_SUCCESS if the operation has been successful,
 *		        or the appropriate error code on failure.
 */
bstatus_t bhttp_req_parse_url(const bstr_t *url, 
                                           bhttp_url *hurl);

/**
 * Create the HTTP request.
 *
 * @param pool		Pool to use. HTTP request will use the pool's factory
 *                      to allocate its own memory pool.
 * @param url		HTTP URL request.
 * @param timer	        The timer to use.
 * @param ioqueue	The ioqueue to use.
 * @param param		Optional parameters. When this parameter is not 
 *                      specifed (NULL), the default values will be used.
 * @param hcb		Pointer to structure containing application
 *			callbacks.
 * @param http_req	Pointer to receive the http request instance.
 *
 * @return		BASE_SUCCESS if the operation has been successful,
 *			or the appropriate error code on failure.
 */
bstatus_t bhttp_req_create(bpool_t *pool,
                                        const bstr_t *url,
					btimer_heap_t *timer,
					bioqueue_t *ioqueue,
                                        const bhttp_req_param *param,
                                        const bhttp_req_callback *hcb,
                                        bhttp_req **http_req);

/**
 * Set the timeout of the HTTP request operation. Note that if the 
 * HTTP request is currently running, the timeout will only affect 
 * subsequent request operations.
 *
 * @param http_req  The http request.
 * @param timeout   Timeout value for HTTP request operation.
 */
void bhttp_req_set_timeout(bhttp_req *http_req,
                                      const btime_val* timeout);

/**
 * Starts an asynchronous HTTP request to the URL specified.
 *
 * @param http_req  The http request.
 *
 * @return
 *  - BASE_SUCCESS    if success
 *  - non-zero      which indicates the appropriate error code.
 */
bstatus_t bhttp_req_start(bhttp_req *http_req);

/**
 * Cancel the asynchronous HTTP request. 
 *
 * @param http_req  The http request.
 * @param notify    If non-zero, the on_complete() callback will be 
 *                  called with status BASE_ECANCELLED to notify that 
 *                  the query has been cancelled.
 *
 * @return
 *  - BASE_SUCCESS    if success
 *  - non-zero      which indicates the appropriate error code.
 */
bstatus_t bhttp_req_cancel(bhttp_req *http_req,
                                        bbool_t notify);

/**
 * Destroy the http request.
 *
 * @param http_req	The http request to be destroyed.
 *
 * @return              BASE_SUCCESS if success.
 */
bstatus_t bhttp_req_destroy(bhttp_req *http_req);

/**
 * Find out whether the http request is running.
 *
 * @param http_req      The http request.
 *
 * @return	        BASE_TRUE if a request is pending, or
 *		        BASE_FALSE if idle
 */
bbool_t bhttp_req_is_running(const bhttp_req *http_req);

/**
 * Retrieve the user data previously associated with this http
 * request.
 *
 * @param http_req  The http request.
 *
 * @return	    The user data.
 */
void * bhttp_req_get_user_data(bhttp_req *http_req);

BASE_END_DECL


#endif

