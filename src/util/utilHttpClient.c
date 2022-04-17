/* 
 *
 */

#include <utilHttpClient.h>
#include <baseActiveSock.h>
#include <baseAssert.h>
#include <baseCtype.h>
#include <baseErrno.h>
#include <baseExcept.h>
#include <basePool.h>
#include <baseString.h>
#include <baseTimer.h>
#include <baseRand.h>
#include <utilBase64.h>
#include <utilErrno.h>
#include <utilMd5.h>
#include <utilScanner.h>
#include <utilString.h>


#define NUM_PROTOCOL            2
#define HTTP_1_0                "1.0"
#define HTTP_1_1                "1.1"
#define CONTENT_LENGTH          "Content-Length"
/* Buffer size for sending/receiving messages. */
#define BUF_SIZE                2048
/* Initial data buffer size to store the data in case content-
 * length is not specified in the server's response.
 */
#define INITIAL_DATA_BUF_SIZE   2048
#define INITIAL_POOL_SIZE       1024
#define POOL_INCREMENT_SIZE     512

enum http_protocol
{
    PROTOCOL_HTTP,
    PROTOCOL_HTTPS
};

static const char *http_protocol_names[NUM_PROTOCOL] =
{
    "HTTP",
    "HTTPS"
};

static const unsigned int http_default_port[NUM_PROTOCOL] =
{
    80,
    443
};

enum http_method
{
    HTTP_GET,
    HTTP_PUT,
    HTTP_DELETE
};

static const char *http_method_names[3] =
{
    "GET",
    "PUT",
    "DELETE"
};

enum http_state
{
    IDLE,
    CONNECTING,
    SENDING_REQUEST,
    SENDING_REQUEST_BODY,
    REQUEST_SENT,
    READING_RESPONSE,
    READING_DATA,
    READING_COMPLETE,
    ABORTING,
};

enum auth_state
{
    AUTH_NONE,		/* Not authenticating */
    AUTH_RETRYING,	/* New request with auth has been submitted */
    AUTH_DONE		/* Done retrying the request with auth. */
};

struct bhttp_req
{
    bstr_t                url;        /* Request URL */
    bhttp_url             hurl;       /* Parsed request URL */
    bsockaddr             addr;       /* The host's socket address */
    bhttp_req_param       param;      /* HTTP request parameters */
    bpool_t               *pool;      /* Pool to allocate memory from */
    btimer_heap_t         *timer;     /* Timer for timeout management */
    bioqueue_t            *ioqueue;   /* Ioqueue to use */
    bhttp_req_callback    cb;         /* Callbacks */
    bactivesock_t         *asock;     /* Active socket */
    bstatus_t             error;      /* Error status */
    bstr_t                buffer;     /* Buffer to send/receive msgs */
    enum http_state         state;      /* State of the HTTP request */
    enum auth_state	    auth_state; /* Authentication state */
    btimer_entry          timer_entry;/* Timer entry */
    bbool_t               resolved;   /* Whether URL's host is resolved */
    bhttp_resp            response;   /* HTTP response */
    bioqueue_op_key_t	    op_key;
    struct tcp_state
    {
        /* Total data sent so far if the data is sent in segments (i.e.
         * if on_send_data() is not NULL and if param.reqdata.total_size > 0)
         */
        bsize_t tot_chunk_size;
        /* Size of data to be sent (in a single activesock operation).*/
        bsize_t send_size;
        /* Data size sent so far. */
        bsize_t current_send_size;
        /* Total data received so far. */
        bsize_t current_read_size;
    } tcp_state;
};

/* Start sending the request */
static bstatus_t http_req_start_sending(bhttp_req *hreq);
/* Start reading the response */
static bstatus_t http_req_start_reading(bhttp_req *hreq);
/* End the request */
static bstatus_t http_req_end_request(bhttp_req *hreq);
/* Parse the header data and populate the header fields with the result. */
static bstatus_t http_headers_parse(char *hdata, bsize_t size, 
                                      bhttp_headers *headers);
/* Parse the response */
static bstatus_t http_response_parse(bpool_t *pool,
                                       bhttp_resp *response,
                                       void *data, bsize_t size,
                                       bsize_t *remainder);
/* Restart the request with authentication */
static void restart_req_with_auth(bhttp_req *hreq);
/* Parse authentication challenge */
static bstatus_t parse_auth_chal(bpool_t *pool, bstr_t *input,
				   bhttp_auth_chal *chal);

static buint16_t get_http_default_port(const bstr_t *protocol)
{
    int i;

    for (i = 0; i < NUM_PROTOCOL; i++) {
        if (!bstricmp2(protocol, http_protocol_names[i])) {
            return (buint16_t)http_default_port[i];
        }
    }
    return 0;
}

static const char * get_protocol(const bstr_t *protocol)
{
    int i;

    for (i = 0; i < NUM_PROTOCOL; i++) {
        if (!bstricmp2(protocol, http_protocol_names[i])) {
            return http_protocol_names[i];
        }
    }

    /* Should not happen */
    bassert(0);
    return NULL;
}


/* Syntax error handler for parser. */
static void on_syntax_error(bscanner *scanner)
{
    BASE_UNUSED_ARG(scanner);
    BASE_THROW(BASE_EINVAL);  // syntax error
}

/* Callback when connection is established to the server */
static bbool_t http_on_connect(bactivesock_t *asock,
				 bstatus_t status)
{
    bhttp_req *hreq = (bhttp_req*) bactivesock_get_user_data(asock);

    if (hreq->state == ABORTING || hreq->state == IDLE)
        return BASE_FALSE;

    if (status != BASE_SUCCESS) {
        hreq->error = status;
        bhttp_req_cancel(hreq, BASE_TRUE);
        return BASE_FALSE;
    }

    /* OK, we are connected. Start sending the request */
    hreq->state = SENDING_REQUEST;
    http_req_start_sending(hreq);
    return BASE_TRUE;
}

static bbool_t http_on_data_sent(bactivesock_t *asock,
 				   bioqueue_op_key_t *op_key,
				   bssize_t sent)
{
    bhttp_req *hreq = (bhttp_req*) bactivesock_get_user_data(asock);

    BASE_UNUSED_ARG(op_key);

    if (hreq->state == ABORTING || hreq->state == IDLE)
        return BASE_FALSE;

    if (sent <= 0) {
        hreq->error = (sent < 0 ? (bstatus_t)-sent : UTIL_EHTTPLOST);
        bhttp_req_cancel(hreq, BASE_TRUE);
        return BASE_FALSE;
    } 

    hreq->tcp_state.current_send_size += sent;
    MTRACE("\nData sent: %d out of %d bytes", 
           hreq->tcp_state.current_send_size, hreq->tcp_state.send_size);
    if (hreq->tcp_state.current_send_size == hreq->tcp_state.send_size) {
        /* Find out whether there is a request body to send. */
        if (hreq->param.reqdata.total_size > 0 || 
            hreq->param.reqdata.size > 0) 
        {
            if (hreq->state == SENDING_REQUEST) {
                /* Start sending the request body */
                hreq->state = SENDING_REQUEST_BODY;
                hreq->tcp_state.tot_chunk_size = 0;
                bassert(hreq->param.reqdata.total_size == 0 ||
                          (hreq->param.reqdata.total_size > 0 && 
                          hreq->param.reqdata.size == 0));
            } else {
                /* Continue sending the next chunk of the request body */
                hreq->tcp_state.tot_chunk_size += hreq->tcp_state.send_size;
                if (hreq->tcp_state.tot_chunk_size == 
                    hreq->param.reqdata.total_size ||
                    hreq->param.reqdata.total_size == 0) 
                {
                    /* Finish sending all the chunks, start reading 
                     * the response.
                     */
                    hreq->state = REQUEST_SENT;
                    http_req_start_reading(hreq);
                    return BASE_TRUE;
                }
            }
            if (hreq->param.reqdata.total_size > 0 &&
                hreq->cb.on_send_data) 
            {
                /* Call the callback for the application to provide
                 * the next chunk of data to be sent.
                 */
                (*hreq->cb.on_send_data)(hreq, &hreq->param.reqdata.data, 
                                         &hreq->param.reqdata.size);
                /* Make sure the total data size given by the user does not
                 * exceed what the user originally said.
                 */
                bassert(hreq->tcp_state.tot_chunk_size + 
                          hreq->param.reqdata.size <=
                          hreq->param.reqdata.total_size);
            }
            http_req_start_sending(hreq);
        } else {
            /* No request body, proceed to reading the server's response. */
            hreq->state = REQUEST_SENT;
            http_req_start_reading(hreq);
        }
    }
    return BASE_TRUE;
}

static bbool_t http_on_data_read(bactivesock_t *asock,
				  void *data,
				  bsize_t size,
				  bstatus_t status,
				  bsize_t *remainder)
{
    bhttp_req *hreq = (bhttp_req*) bactivesock_get_user_data(asock);

    MTRACE( "\nData received: %d bytes", size);

    if (hreq->state == ABORTING || hreq->state == IDLE)
        return BASE_FALSE;

    if (hreq->state == READING_RESPONSE) {
        bstatus_t st;
        bsize_t rem;

        if (status != BASE_SUCCESS && status != BASE_EPENDING) {
            hreq->error = status;
            bhttp_req_cancel(hreq, BASE_TRUE);
            return BASE_FALSE;
        }

        /* Parse the response. */
        st = http_response_parse(hreq->pool, &hreq->response,
                                 data, size, &rem);
        if (st == UTIL_EHTTPINCHDR) {
            /* If we already use up all our buffer and still
             * hasn't received the whole header, return error
             */
            if (size == BUF_SIZE) {
                hreq->error = BASE_ETOOBIG; // response header size is too big
                bhttp_req_cancel(hreq, BASE_TRUE);
                return BASE_FALSE;
            }
            /* Keep the data if we do not get the whole response header */
            *remainder = size;
        } else {
            hreq->state = READING_DATA;
            if (st != BASE_SUCCESS) {
                /* Server replied with an invalid (or unknown) response 
                 * format. We'll just pass the whole (unparsed) response 
                 * to the user.
                 */
                hreq->response.data = data;
                hreq->response.size = size - rem;
            }

            /* If code is 401 or 407, find and parse WWW-Authenticate or
             * Proxy-Authenticate header
             */
            if (hreq->response.status_code == 401 ||
        	hreq->response.status_code == 407)
            {
        	const bstr_t STR_WWW_AUTH = { "WWW-Authenticate", 16 };
        	const bstr_t STR_PROXY_AUTH = { "Proxy-Authenticate", 18 };
        	bhttp_resp *response = &hreq->response;
        	bhttp_headers *hdrs = &response->headers;
        	unsigned i;

        	status = BASE_ENOTFOUND;
        	for (i = 0; i < hdrs->count; i++) {
        	    if (!bstricmp(&hdrs->header[i].name, &STR_WWW_AUTH) ||
        		!bstricmp(&hdrs->header[i].name, &STR_PROXY_AUTH))
        	    {
        		status = parse_auth_chal(hreq->pool,
        					 &hdrs->header[i].value,
        					 &response->auth_chal);
        		break;
        	    }
        	}

                /* Check if we should perform authentication */
                if (status == BASE_SUCCESS &&
		    hreq->auth_state == AUTH_NONE &&
		    hreq->response.auth_chal.scheme.slen &&
		    hreq->param.auth_cred.username.slen &&
		    (hreq->param.auth_cred.scheme.slen == 0 ||
		     !bstricmp(&hreq->response.auth_chal.scheme,
				 &hreq->param.auth_cred.scheme)) &&
		    (hreq->param.auth_cred.realm.slen == 0 ||
		     !bstricmp(&hreq->response.auth_chal.realm,
				 &hreq->param.auth_cred.realm))
		    )
		    {
		    /* Yes, authentication is required and we have been
		     * configured with credential.
		     */
		    restart_req_with_auth(hreq);
		    if (hreq->auth_state == AUTH_RETRYING) {
			/* We'll be resending the request with auth. This
			 * connection has been closed.
			 */
			return BASE_FALSE;
		    }
                }
            }

            /* We already received the response header, call the 
             * appropriate callback.
             */
            if (hreq->cb.on_response)
                (*hreq->cb.on_response)(hreq, &hreq->response);
            hreq->response.data = NULL;
            hreq->response.size = 0;

	    if (rem > 0 || hreq->response.content_length == 0)
		return http_on_data_read(asock, (rem == 0 ? NULL:
		   	                 (char *)data + size - rem),
				         rem, BASE_SUCCESS, NULL);
        }

        return BASE_TRUE;
    }

    if (hreq->state != READING_DATA)
	return BASE_FALSE;
    if (hreq->cb.on_data_read) {
        /* If application wishes to receive the data once available, call
         * its callback.
         */
        if (size > 0)
            (*hreq->cb.on_data_read)(hreq, data, size);
    } else {
        if (hreq->response.size == 0) {
            /* If we know the content length, allocate the data based
             * on that, otherwise we'll use initial buffer size and grow 
             * it later if necessary.
             */
            hreq->response.size = (hreq->response.content_length == -1 ? 
                                   INITIAL_DATA_BUF_SIZE : 
                                   hreq->response.content_length);
            hreq->response.data = bpool_alloc(hreq->pool, 
                                                hreq->response.size);
        }

        /* If the size of data received exceeds its current size,
         * grow the buffer by a factor of 2.
         */
        if (hreq->tcp_state.current_read_size + size > 
            hreq->response.size) 
        {
            void *olddata = hreq->response.data;

            hreq->response.data = bpool_alloc(hreq->pool, 
                                                hreq->response.size << 1);
            bmemcpy(hreq->response.data, olddata, hreq->response.size);
            hreq->response.size <<= 1;
        }

        /* Append the response data. */
        bmemcpy((char *)hreq->response.data + 
                  hreq->tcp_state.current_read_size, data, size);
    }
    hreq->tcp_state.current_read_size += size;

    /* If the total data received so far is equal to the content length
     * or if it's already EOF.
     */
    if ((hreq->response.content_length >=0 &&
	(bssize_t)hreq->tcp_state.current_read_size >=
        hreq->response.content_length) ||
        (status == BASE_EEOF && hreq->response.content_length == -1)) 
    {
	/* Finish reading */
        http_req_end_request(hreq);
        hreq->response.size = hreq->tcp_state.current_read_size;

        /* HTTP request is completed, call the callback. */
        if (hreq->cb.on_complete) {
            (*hreq->cb.on_complete)(hreq, BASE_SUCCESS, &hreq->response);
        }

        return BASE_FALSE;
    }

    /* Error status or premature EOF. */
    if ((status != BASE_SUCCESS && status != BASE_EPENDING && status != BASE_EEOF)
        || (status == BASE_EEOF && hreq->response.content_length > -1)) 
    {
        hreq->error = status;
        bhttp_req_cancel(hreq, BASE_TRUE);
        return BASE_FALSE;
    }
    
    return BASE_TRUE;
}

/* Callback to be called when query has timed out */
static void on_timeout( btimer_heap_t *timer_heap,
			struct _btimer_entry *entry)
{
    bhttp_req *hreq = (bhttp_req *) entry->user_data;

    BASE_UNUSED_ARG(timer_heap);

    /* Recheck that the request is still not completed, since there is a 
     * slight possibility of race condition (timer elapsed while at the 
     * same time response arrives).
     */
    if (hreq->state == READING_COMPLETE) {
	/* Yeah, we finish on time */
	return;
    }

    /* Invalidate id. */
    hreq->timer_entry.id = 0;

    /* Request timed out. */
    hreq->error = BASE_ETIMEDOUT;
    bhttp_req_cancel(hreq, BASE_TRUE);
}

/* Parse authentication challenge */
static bstatus_t parse_auth_chal(bpool_t *pool, bstr_t *input,
				   bhttp_auth_chal *chal)
{
    bscanner scanner;
    const bstr_t REALM_STR 	=    { "realm", 5},
    		NONCE_STR 	=    { "nonce", 5},
    		ALGORITHM_STR 	=    { "algorithm", 9 },
    		STALE_STR 	=    { "stale", 5},
    		QOP_STR 	=    { "qop", 3},
    		OPAQUE_STR 	=    { "opaque", 6};
    bstatus_t status = BASE_SUCCESS;
    BASE_USE_EXCEPTION ;

    bscan_init(&scanner, input->ptr, input->slen, BASE_SCAN_AUTOSKIP_WS,
		 &on_syntax_error);
    BASE_TRY {
	/* Get auth scheme */
	if (*scanner.curptr == '"') {
	    bscan_get_quote(&scanner, '"', '"', &chal->scheme);
	    chal->scheme.ptr++;
	    chal->scheme.slen -= 2;
	} else {
	    bscan_get_until_chr(&scanner, " \t\r\n", &chal->scheme);
	}

	/* Loop parsing all parameters */
	for (;;) {
	    const char *end_param = ", \t\r\n;";
	    bstr_t name, value;

	    /* Get pair of parameter name and value */
	    value.ptr = NULL;
	    value.slen = 0;
	    bscan_get_until_chr(&scanner, "=, \t\r\n", &name);
	    if (*scanner.curptr == '=') {
		bscan_get_char(&scanner);
		if (!bscan_is_eof(&scanner)) {
		    if (*scanner.curptr == '"' || *scanner.curptr == '\'') {
			int quote_char = *scanner.curptr;
			bscan_get_quote(&scanner, quote_char, quote_char,
					  &value);
			value.ptr++;
			value.slen -= 2;
		    } else if (!strchr(end_param, *scanner.curptr)) {
			bscan_get_until_chr(&scanner, end_param, &value);
		    }
		}
		value = bstr_unescape(pool, &value);
	    }

	    if (!bstricmp(&name, &REALM_STR)) {
		chal->realm = value;

	    } else if (!bstricmp(&name, &NONCE_STR)) {
		chal->nonce = value;

	    } else if (!bstricmp(&name, &ALGORITHM_STR)) {
		chal->algorithm = value;

	    } else if (!bstricmp(&name, &OPAQUE_STR)) {
		chal->opaque = value;

	    } else if (!bstricmp(&name, &QOP_STR)) {
		chal->qop = value;

	    } else if (!bstricmp(&name, &STALE_STR)) {
		chal->stale = value.slen &&
			      (*value.ptr != '0') &&
			      (*value.ptr != 'f') &&
			      (*value.ptr != 'F');

	    }

	    /* Eat comma */
	    if (!bscan_is_eof(&scanner) && *scanner.curptr == ',')
		bscan_get_char(&scanner);
	    else
		break;
	}

    }
    BASE_CATCH_ANY {
	status = BASE_GET_EXCEPTION();
	bbzero(chal, sizeof(*chal));
	MTRACE( "Error: parsing of auth header failed");
    }
    BASE_END;
    bscan_fini(&scanner);
    return status;
}

/* The same as #bhttp_headers_add_elmt() with char * as
 * its parameters.
 */
bstatus_t bhttp_headers_add_elmt2(bhttp_headers *headers, 
                                              char *name, char *val)
{
    bstr_t f, v;
    bcstr(&f, name);
    bcstr(&v, val);
    return bhttp_headers_add_elmt(headers, &f, &v);
}   

bstatus_t bhttp_headers_add_elmt(bhttp_headers *headers, 
                                             bstr_t *name, 
                                             bstr_t *val)
{
    BASE_ASSERT_RETURN(headers && name && val, BASE_FALSE);
    if (headers->count >= BASE_HTTP_HEADER_SIZE)
        return BASE_ETOOMANY;
    bstrassign(&headers->header[headers->count].name, name);
    bstrassign(&headers->header[headers->count++].value, val);
    return BASE_SUCCESS;
}

static bstatus_t http_response_parse(bpool_t *pool,
                                       bhttp_resp *response,
                                       void *data, bsize_t size,
                                       bsize_t *remainder)
{
    bsize_t i;
    char *cptr;
    char *end_status, *newdata;
    bscanner scanner;
    bstr_t s;
    const bstr_t STR_CONTENT_LENGTH = { CONTENT_LENGTH, 14 };
    bstatus_t status;

    BASE_USE_EXCEPTION;

    BASE_ASSERT_RETURN(response, BASE_EINVAL);
    if (size < 2)
        return UTIL_EHTTPINCHDR;
    /* Detect whether we already receive the response's status-line
     * and its headers. We're looking for a pair of CRLFs. A pair of
     * LFs is also supported although it is not RFC standard.
     */
    cptr = (char *)data;
    for (i = 1, cptr++; i < size; i++, cptr++) {
        if (*cptr == '\n') {
            if (*(cptr - 1) == '\n')
                break;
            if (*(cptr - 1) == '\r') {
                if (i >= 3 && *(cptr - 2) == '\n' && *(cptr - 3) == '\r')
                    break;
            }
        }
    }
    if (i == size)
        return UTIL_EHTTPINCHDR;
    *remainder = size - 1 - i;

    bbzero(response, sizeof(*response));
    response->content_length = -1;

    newdata = (char*) bpool_alloc(pool, i);
    bmemcpy(newdata, data, i);

    /* Parse the status-line. */
    bscan_init(&scanner, newdata, i, 0, &on_syntax_error);
    BASE_TRY {
        bscan_get_until_ch(&scanner, ' ', &response->version);
        bscan_advance_n(&scanner, 1, BASE_FALSE);
        bscan_get_until_ch(&scanner, ' ', &s);
        response->status_code = (buint16_t)bstrtoul(&s);
        bscan_advance_n(&scanner, 1, BASE_FALSE);
        bscan_get_until_ch(&scanner, '\n', &response->reason);
        if (response->reason.ptr[response->reason.slen-1] == '\r')
            response->reason.slen--;
    }
    BASE_CATCH_ANY {
        bscan_fini(&scanner);
        return BASE_GET_EXCEPTION();
    }
    BASE_END;

    end_status = scanner.curptr;
    bscan_fini(&scanner);

    /* Parse the response headers. */
    size = i - 2 - (end_status - newdata);
    if (size > 0) {
        status = http_headers_parse(end_status + 1, size,
                                    &response->headers);
    } else {
        status = BASE_SUCCESS;
    }

    /* Find content-length header field. */
    for (i = 0; i < response->headers.count; i++) {
        if (!bstricmp(&response->headers.header[i].name,
                        &STR_CONTENT_LENGTH))
        {
            response->content_length = 
                bstrtoul(&response->headers.header[i].value);
            /* If content length is zero, make sure that it is because the
             * header value is really zero and not due to parsing error.
             */
            if (response->content_length == 0) {
                if (bstrcmp2(&response->headers.header[i].value, "0")) {
                    response->content_length = -1;
                }
            }
            break;
        }
    }

    return status;
}

static bstatus_t http_headers_parse(char *hdata, bsize_t size, 
                                      bhttp_headers *headers)
{
    bscanner scanner;
    bstr_t s, s2;
    bstatus_t status;
    BASE_USE_EXCEPTION;

    BASE_ASSERT_RETURN(headers, BASE_EINVAL);

    bscan_init(&scanner, hdata, size, 0, &on_syntax_error);

    /* Parse each line of header field consisting of header field name and
     * value, separated by ":" and any number of white spaces.
     */
    BASE_TRY {
        do {
            bscan_get_until_chr(&scanner, ":\n", &s);
            if (*scanner.curptr == ':') {
                bscan_advance_n(&scanner, 1, BASE_TRUE);
                bscan_get_until_ch(&scanner, '\n', &s2);
                if (s2.ptr[s2.slen-1] == '\r')
                    s2.slen--;
                status = bhttp_headers_add_elmt(headers, &s, &s2);
                if (status != BASE_SUCCESS)
                    BASE_THROW(status);
            }
            bscan_advance_n(&scanner, 1, BASE_TRUE);
            /* Finish parsing */
            if (bscan_is_eof(&scanner))
                break;
        } while (1);
    }
    BASE_CATCH_ANY {
        bscan_fini(&scanner);
        return BASE_GET_EXCEPTION();
    }
    BASE_END;

    bscan_fini(&scanner);

    return BASE_SUCCESS;
}

void bhttp_req_param_default(bhttp_req_param *param)
{
    bassert(param);
    bbzero(param, sizeof(*param));
    param->addr_family = bAF_INET();
    bstrset2(&param->method, (char*)http_method_names[HTTP_GET]);
    bstrset2(&param->version, (char*)HTTP_1_0);
    param->timeout.msec = BASE_HTTP_DEFAULT_TIMEOUT;
    btime_val_normalize(&param->timeout);
    param->max_retries = 3;
}

/* Get the location of '@' character to indicate the end of
 * user:passwd part of an URI. If user:passwd part is not
 * present, NULL will be returned.
 */
static char *get_url_at_pos(const char *str, bsize_t len)
{
    const char *end = str + len;
    const char *p = str;

    /* skip scheme: */
    while (p!=end && *p!='/') ++p;
    if (p!=end && *p=='/') ++p;
    if (p!=end && *p=='/') ++p;
    if (p==end) return NULL;

    for (; p!=end; ++p) {
	switch (*p) {
	case '/':
	    return NULL;
	case '@':
	    return (char*)p;
	}
    }

    return NULL;
}


bstatus_t bhttp_req_parse_url(const bstr_t *url, 
                                          bhttp_url *hurl)
{
    bscanner scanner;
    bsize_t len = url->slen;
    BASE_USE_EXCEPTION;

    if (!len) return -1;
    
    bbzero(hurl, sizeof(*hurl));
    bscan_init(&scanner, url->ptr, url->slen, 0, &on_syntax_error);

    BASE_TRY {
	bstr_t s;

        /* Exhaust any whitespaces. */
        bscan_skip_whitespace(&scanner);

        /* Parse the protocol */
        bscan_get_until_ch(&scanner, ':', &s);
        if (!bstricmp2(&s, http_protocol_names[PROTOCOL_HTTP])) {
            bstrset2(&hurl->protocol,
        	       (char*)http_protocol_names[PROTOCOL_HTTP]);
        } else if (!bstricmp2(&s, http_protocol_names[PROTOCOL_HTTPS])) {
            bstrset2(&hurl->protocol,
		       (char*)http_protocol_names[PROTOCOL_HTTPS]);
        } else {
            BASE_THROW(BASE_ENOTSUP); // unsupported protocol
        }

        if (bscan_strcmp(&scanner, "://", 3)) {
            BASE_THROW(UTIL_EHTTPINURL); // no "://" after protocol name
        }
        bscan_advance_n(&scanner, 3, BASE_FALSE);

        if (get_url_at_pos(url->ptr, url->slen)) {
            /* Parse username and password */
            bscan_get_until_chr(&scanner, ":@", &hurl->username);
            if (*scanner.curptr == ':') {
        	bscan_get_char(&scanner);
        	bscan_get_until_chr(&scanner, "@", &hurl->passwd);
            } else {
        	hurl->passwd.slen = 0;
            }
            bscan_get_char(&scanner);
        }

        /* Parse the host and port number (if any) */
        bscan_get_until_chr(&scanner, ":/", &s);
        bstrassign(&hurl->host, &s);
        if (hurl->host.slen==0)
            BASE_THROW(BASE_EINVAL);
        if (bscan_is_eof(&scanner) || *scanner.curptr == '/') {
            /* No port number specified */
            /* Assume default http/https port number */
            hurl->port = get_http_default_port(&hurl->protocol);
            bassert(hurl->port > 0);
        } else {
            bscan_advance_n(&scanner, 1, BASE_FALSE);
            bscan_get_until_ch(&scanner, '/', &s);
            /* Parse the port number */
            hurl->port = (buint16_t)bstrtoul(&s);
            if (!hurl->port)
                BASE_THROW(UTIL_EHTTPINPORT); // invalid port number
        }

        if (!bscan_is_eof(&scanner)) {
            hurl->path.ptr = scanner.curptr;
            hurl->path.slen = scanner.end - scanner.curptr;
        } else {
            /* no path, append '/' */
            bcstr(&hurl->path, "/");
        }
    }
    BASE_CATCH_ANY {
        bscan_fini(&scanner);
	return BASE_GET_EXCEPTION();
    }
    BASE_END;

    bscan_fini(&scanner);
    return BASE_SUCCESS;
}

void bhttp_req_set_timeout(bhttp_req *http_req,
                                     const btime_val* timeout)
{
    bmemcpy(&http_req->param.timeout, timeout, sizeof(*timeout));
}

bstatus_t bhttp_req_create(bpool_t *pool,
                                       const bstr_t *url,
                                       btimer_heap_t *timer,
                                       bioqueue_t *ioqueue,
                                       const bhttp_req_param *param,
                                       const bhttp_req_callback *hcb,
                                       bhttp_req **http_req)
{
    bpool_t *own_pool;
    bhttp_req *hreq;
    char *at_pos;
    bstatus_t status;

    BASE_ASSERT_RETURN(pool && url && timer && ioqueue && 
                     hcb && http_req, BASE_EINVAL);

    *http_req = NULL;
    own_pool = bpool_create(pool->factory, NULL, INITIAL_POOL_SIZE, 
                              POOL_INCREMENT_SIZE, NULL);
    hreq = BASE_POOL_ZALLOC_T(own_pool, struct bhttp_req);
    if (!hreq)
        return BASE_ENOMEM;

    /* Initialization */
    hreq->pool = own_pool;
    hreq->ioqueue = ioqueue;
    hreq->timer = timer;
    hreq->asock = NULL;
    bmemcpy(&hreq->cb, hcb, sizeof(*hcb));
    hreq->state = IDLE;
    hreq->resolved = BASE_FALSE;
    hreq->buffer.ptr = NULL;
    btimer_entry_init(&hreq->timer_entry, 0, hreq, &on_timeout);

    /* Initialize parameter */
    if (param) {
        bmemcpy(&hreq->param, param, sizeof(*param));
        /* TODO: validate the param here
         * Should we validate the method as well? If yes, based on all HTTP
         * methods or based on supported methods only? For the later, one 
         * drawback would be that you can't use this if the method is not 
         * officially supported
         */
        BASE_ASSERT_RETURN(hreq->param.addr_family==bAF_UNSPEC() || 
                         hreq->param.addr_family==bAF_INET() || 
                         hreq->param.addr_family==bAF_INET6(), BASE_EAFNOTSUP);
        BASE_ASSERT_RETURN(!bstrcmp2(&hreq->param.version, HTTP_1_0) || 
                         !bstrcmp2(&hreq->param.version, HTTP_1_1), 
                         BASE_ENOTSUP); 
        btime_val_normalize(&hreq->param.timeout);
    } else {
        bhttp_req_param_default(&hreq->param);
    }

    /* Parse the URL */
    if (!bstrdup_with_null(hreq->pool, &hreq->url, url)) {
	bpool_release(hreq->pool);
        return BASE_ENOMEM;
    }
    status = bhttp_req_parse_url(&hreq->url, &hreq->hurl);
    if (status != BASE_SUCCESS) {
	bpool_release(hreq->pool);
        return status; // Invalid URL supplied
    }

    /* If URL contains username/password, move them to credential and
     * remove them from the URL.
     */
    if ((at_pos=get_url_at_pos(hreq->url.ptr, hreq->url.slen)) != NULL) {
	bstr_t tmp;
	char *user_pos = bstrchr(&hreq->url, '/');
	int removed_len;

	/* Save credential first, unescape the string */
	tmp = bstr_unescape(hreq->pool, &hreq->hurl.username);;
	bstrdup(hreq->pool, &hreq->param.auth_cred.username, &tmp);

	tmp = bstr_unescape(hreq->pool, &hreq->hurl.passwd);
	bstrdup(hreq->pool, &hreq->param.auth_cred.data, &tmp);

	hreq->hurl.username.ptr = hreq->hurl.passwd.ptr = NULL;
	hreq->hurl.username.slen = hreq->hurl.passwd.slen = 0;

	/* Remove "username:password@" from the URL */
	bassert(user_pos != 0 && user_pos < at_pos);
	user_pos += 2;
	removed_len = (int)(at_pos + 1 - user_pos);
	bmemmove(user_pos, at_pos+1, hreq->url.ptr+hreq->url.slen-at_pos-1);
	hreq->url.slen -= removed_len;

	/* Need to adjust hostname and path pointers due to memmove*/
	if (hreq->hurl.host.ptr > user_pos &&
	    hreq->hurl.host.ptr < user_pos + hreq->url.slen)
	{
	    hreq->hurl.host.ptr -= removed_len;
	}
	/* path may come from a string constant, don't shift it if so */
	if (hreq->hurl.path.ptr > user_pos &&
	    hreq->hurl.path.ptr < user_pos + hreq->url.slen)
	{
	    hreq->hurl.path.ptr -= removed_len;
	}
    }

    *http_req = hreq;
    return BASE_SUCCESS;
}

bbool_t bhttp_req_is_running(const bhttp_req *http_req)
{
   BASE_ASSERT_RETURN(http_req, BASE_FALSE);
   return (http_req->state != IDLE);
}

void* bhttp_req_get_user_data(bhttp_req *http_req)
{
    BASE_ASSERT_RETURN(http_req, NULL);
    return http_req->param.user_data;
}

static bstatus_t start_http_req(bhttp_req *http_req,
                                  bbool_t notify_on_fail)
{
    bsock_t sock = BASE_INVALID_SOCKET;
    bstatus_t status;
    bactivesock_cb asock_cb;
    int retry = 0;

    BASE_ASSERT_RETURN(http_req, BASE_EINVAL);
    /* Http request is not idle, a request was initiated before and 
     * is still in progress
     */
    BASE_ASSERT_RETURN(http_req->state == IDLE, BASE_EBUSY);

    /* Reset few things to make sure restarting works */
    http_req->error = 0;
    http_req->response.headers.count = 0;
    bbzero(&http_req->tcp_state, sizeof(http_req->tcp_state));

    if (!http_req->resolved) {
        /* Resolve the Internet address of the host */
        status = bsockaddr_init(http_req->param.addr_family, 
                                  &http_req->addr, &http_req->hurl.host,
				  http_req->hurl.port);
	if (status != BASE_SUCCESS ||
	    !bsockaddr_has_addr(&http_req->addr) ||
	    (http_req->param.addr_family==bAF_INET() &&
	     http_req->addr.ipv4.sin_addr.s_addr==BASE_INADDR_NONE))
	{
	    goto on_return;
        }
        http_req->resolved = BASE_TRUE;
    }

    status = bsock_socket(http_req->param.addr_family, bSOCK_STREAM(), 
                            0, &sock);
    if (status != BASE_SUCCESS)
        goto on_return; // error creating socket

    bbzero(&asock_cb, sizeof(asock_cb));
    asock_cb.on_data_read = &http_on_data_read;
    asock_cb.on_data_sent = &http_on_data_sent;
    asock_cb.on_connect_complete = &http_on_connect;
	
    do
    {
	bsockaddr_in bound_addr;
	buint16_t port = 0;

	/* If we are using port restriction.
	 * Get a random port within the range
	 */
	if (http_req->param.source_port_range_start != 0) {
	    port = (buint16_t)
		   (http_req->param.source_port_range_start +
		    (brand() % http_req->param.source_port_range_size));
	}

	bsockaddr_in_init(&bound_addr, NULL, port);
	status = bsock_bind(sock, &bound_addr, sizeof(bound_addr));

    } while (status != BASE_SUCCESS && (retry++ < http_req->param.max_retries));

    if (status != BASE_SUCCESS) {
	BASE_PERROR(1,(THIS_FILE, status,
		     "Unable to bind to the requested port"));
	bsock_close(sock);
	goto on_return;
    }

    // TODO: should we set whole data to 0 by default?
    // or add it in the param?
    status = bactivesock_create(http_req->pool, sock, bSOCK_STREAM(), 
                                  NULL, http_req->ioqueue,
				  &asock_cb, http_req, &http_req->asock);
    if (status != BASE_SUCCESS) {
        bsock_close(sock);
	goto on_return; // error creating activesock
    }

    /* Schedule timeout timer for the request */
    bassert(http_req->timer_entry.id == 0);
    http_req->timer_entry.id = 1;
    status = btimer_heap_schedule(http_req->timer, &http_req->timer_entry, 
                                    &http_req->param.timeout);
    if (status != BASE_SUCCESS) {
        http_req->timer_entry.id = 0;
	goto on_return; // error scheduling timer
    }

    /* Connect to host */
    http_req->state = CONNECTING;
    status = bactivesock_start_connect(http_req->asock, http_req->pool, 
                                         (bsock_t *)&(http_req->addr), 
                                         bsockaddr_get_len(&http_req->addr));
    if (status == BASE_SUCCESS) {
        http_req->state = SENDING_REQUEST;
        status =  http_req_start_sending(http_req);
        if (status != BASE_SUCCESS)
            goto on_return;
    } else if (status != BASE_EPENDING) {
        goto on_return; // error connecting
    }

    return BASE_SUCCESS;

on_return:
    http_req->error = status;
    if (notify_on_fail)
	bhttp_req_cancel(http_req, BASE_TRUE);
    else
	http_req_end_request(http_req);

    return status;
}

/* Starts an asynchronous HTTP request to the URL specified. */
bstatus_t bhttp_req_start(bhttp_req *http_req)
{
    return start_http_req(http_req, BASE_FALSE);
}

/* Respond to basic authentication challenge */
static bstatus_t auth_respond_basic(bhttp_req *hreq)
{
    /* Basic authentication:
     *      credentials       = "Basic" basic-credentials
     *      basic-credentials = base64-user-pass
     *      base64-user-pass  = <base64 [4] encoding of user-pass>
     *      user-pass         = userid ":" password
     *
     * Sample:
     *       Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==
     */
    bstr_t user_pass;
    bhttp_header_elmt *phdr;
    int len;

    /* Use send buffer to store userid ":" password */
    user_pass.ptr = hreq->buffer.ptr;
    bstrcpy(&user_pass, &hreq->param.auth_cred.username);
    bstrcat2(&user_pass, ":");
    bstrcat(&user_pass, &hreq->param.auth_cred.data);

    /* Create Authorization header */
    phdr = &hreq->param.headers.header[hreq->param.headers.count++];
    bbzero(phdr, sizeof(*phdr));
    if (hreq->response.status_code == 401)
	phdr->name = bstr("Authorization");
    else
	phdr->name = bstr("Proxy-Authorization");

    len = (int)(BASE_BASE256_TO_BASE64_LEN(user_pass.slen) + 10);
    phdr->value.ptr = (char*)bpool_alloc(hreq->pool, len);
    phdr->value.slen = 0;

    bstrcpy2(&phdr->value, "Basic ");
    len -= (int)phdr->value.slen;
    bbase64_encode((buint8_t*)user_pass.ptr, (int)user_pass.slen,
		     phdr->value.ptr + phdr->value.slen, &len);
    phdr->value.slen += len;

    return BASE_SUCCESS;
}

/** Length of digest string. */
#define MD5_STRLEN 32
/* A macro just to get rid of type mismatch between char and unsigned char */
#define MD5_APPEND(pms,buf,len)	bmd5_update(pms, (const buint8_t*)buf, \
		   (unsigned)len)

/* Transform digest to string.
 * output must be at least PJSIP_MD5STRLEN+1 bytes.
 *
 * NOTE: THE OUTPUT STRING IS NOT NULL TERMINATED!
 */
static void digest2str(const unsigned char digest[], char *output)
{
    int i;
    for (i = 0; i<16; ++i) {
	bval_to_hex_digit(digest[i], output);
	output += 2;
    }
}

static void auth_create_digest_response(bstr_t *result,
					const bhttp_auth_cred *cred,
				        const bstr_t *nonce,
				        const bstr_t *nc,
				        const bstr_t *cnonce,
				        const bstr_t *qop,
				        const bstr_t *uri,
				        const bstr_t *realm,
				        const bstr_t *method)
{
    char ha1[MD5_STRLEN];
    char ha2[MD5_STRLEN];
    unsigned char digest[16];
    bmd5_context pms;

    bassert(result->slen >= MD5_STRLEN);

    MTRACE("Begin creating digest");

    if (cred->data_type == 0) {
	/***
	 *** ha1 = MD5(username ":" realm ":" password)
	 ***/
	bmd5_init(&pms);
	MD5_APPEND( &pms, cred->username.ptr, cred->username.slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, realm->ptr, realm->slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, cred->data.ptr, cred->data.slen);
	bmd5_final(&pms, digest);

	digest2str(digest, ha1);

    } else if (cred->data_type == 1) {
	bassert(cred->data.slen == 32);
	bmemcpy( ha1, cred->data.ptr, cred->data.slen );
    } else {
	bassert(!"Invalid data_type");
    }

    MTRACE( "  ha1=%.32s", ha1);

    /***
     *** ha2 = MD5(method ":" req_uri)
     ***/
    bmd5_init(&pms);
    MD5_APPEND( &pms, method->ptr, method->slen);
    MD5_APPEND( &pms, ":", 1);
    MD5_APPEND( &pms, uri->ptr, uri->slen);
    bmd5_final(&pms, digest);
    digest2str(digest, ha2);

    MTRACE("  ha2=%.32s", ha2);

    /***
     *** When qop is not used:
     ***    response = MD5(ha1 ":" nonce ":" ha2)
     ***
     *** When qop=auth is used:
     ***    response = MD5(ha1 ":" nonce ":" nc ":" cnonce ":" qop ":" ha2)
     ***/
    bmd5_init(&pms);
    MD5_APPEND( &pms, ha1, MD5_STRLEN);
    MD5_APPEND( &pms, ":", 1);
    MD5_APPEND( &pms, nonce->ptr, nonce->slen);
    if (qop && qop->slen != 0) {
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, nc->ptr, nc->slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, cnonce->ptr, cnonce->slen);
	MD5_APPEND( &pms, ":", 1);
	MD5_APPEND( &pms, qop->ptr, qop->slen);
    }
    MD5_APPEND( &pms, ":", 1);
    MD5_APPEND( &pms, ha2, MD5_STRLEN);

    /* This is the final response digest. */
    bmd5_final(&pms, digest);

    /* Convert digest to string and store in chal->response. */
    result->slen = MD5_STRLEN;
    digest2str(digest, result->ptr);

    MTRACE("  digest=%.32s", result->ptr);
    MTRACE("Digest created");
}

/* Find out if qop offer contains "auth" token */
static bbool_t auth_has_qop( bpool_t *pool, const bstr_t *qop_offer)
{
    bstr_t qop;
    char *p;

    bstrdup_with_null( pool, &qop, qop_offer);
    p = qop.ptr;
    while (*p) {
	*p = (char)btolower(*p);
	++p;
    }

    p = qop.ptr;
    while (*p) {
	if (*p=='a' && *(p+1)=='u' && *(p+2)=='t' && *(p+3)=='h') {
	    int e = *(p+4);
	    if (e=='"' || e==',' || e==0)
		return BASE_TRUE;
	    else
		p += 4;
	} else {
	    ++p;
	}
    }

    return BASE_FALSE;
}

#define STR_PREC(s) (int)(s).slen, (s).ptr

/* Respond to digest authentication */
static bstatus_t auth_respond_digest(bhttp_req *hreq)
{
    const bhttp_auth_chal *chal = &hreq->response.auth_chal;
    const bhttp_auth_cred *cred = &hreq->param.auth_cred;
    bhttp_header_elmt *phdr;
    char digest_response_buf[MD5_STRLEN];
    int len;
    bstr_t digest_response;

    /* Check algorithm is supported. We only support MD5 */
    if (chal->algorithm.slen!=0 &&
	bstricmp2(&chal->algorithm, "MD5"))
    {
	MTRACE( "Error: Unsupported digest algorithm \"%.*s\"",  chal->algorithm.slen, chal->algorithm.ptr);
	return BASE_ENOTSUP;
    }

    /* Add Authorization header */
    phdr = &hreq->param.headers.header[hreq->param.headers.count++];
    bbzero(phdr, sizeof(*phdr));
    if (hreq->response.status_code == 401)
	phdr->name = bstr("Authorization");
    else
	phdr->name = bstr("Proxy-Authorization");

    /* Allocate space for the header */
    len = (int)(8 + /* Digest */
	  16 + hreq->param.auth_cred.username.slen + /* username= */
	  12 + chal->realm.slen + /* realm= */
	  12 + chal->nonce.slen + /* nonce= */
	  8 + hreq->hurl.path.slen + /* uri= */
	  16 + /* algorithm=MD5 */
	  16 + MD5_STRLEN + /* response= */
	  12 + /* qop=auth */
	  8 + /* nc=.. */
	  30 + /* cnonce= */
	  12 + chal->opaque.slen + /* opaque=".." */
	  0);
    phdr->value.ptr = (char*)bpool_alloc(hreq->pool, len);

    /* Configure buffer to temporarily store the digest */
    digest_response.ptr = digest_response_buf;
    digest_response.slen = MD5_STRLEN;

    if (chal->qop.slen == 0) {
	const bstr_t STR_MD5 = { "MD5", 3 };
	int max_len;

	/* Server doesn't require quality of protection. */
	auth_create_digest_response(&digest_response, cred,
				    &chal->nonce, NULL, NULL,  NULL,
				    &hreq->hurl.path, &chal->realm,
				    &hreq->param.method);

	max_len = len;
	len = bansi_snprintf(
		phdr->value.ptr, max_len,
		"Digest username=\"%.*s\", "
		"realm=\"%.*s\", "
		"nonce=\"%.*s\", "
		"uri=\"%.*s\", "
		"algorithm=%.*s, "
		"response=\"%.*s\"",
		STR_PREC(cred->username),
		STR_PREC(chal->realm),
		STR_PREC(chal->nonce),
		STR_PREC(hreq->hurl.path),
		STR_PREC(STR_MD5),
		STR_PREC(digest_response));
	if (len < 0 || len >= max_len)
	    return BASE_ETOOSMALL;
	phdr->value.slen = len;

    } else if (auth_has_qop(hreq->pool, &chal->qop)) {
	/* Server requires quality of protection.
	 * We respond with selecting "qop=auth" protection.
	 */
	const bstr_t STR_MD5 = { "MD5", 3 };
	const bstr_t qop = bstr("auth");
	const bstr_t nc = bstr("00000001");
	const bstr_t cnonce = bstr("b39971");
	int max_len;

	auth_create_digest_response(&digest_response, cred,
				    &chal->nonce, &nc, &cnonce, &qop,
				    &hreq->hurl.path, &chal->realm,
				    &hreq->param.method);
	max_len = len;
	len = bansi_snprintf(
		phdr->value.ptr, max_len,
		"Digest username=\"%.*s\", "
		"realm=\"%.*s\", "
		"nonce=\"%.*s\", "
		"uri=\"%.*s\", "
		"algorithm=%.*s, "
		"response=\"%.*s\", "
		"qop=%.*s, "
		"nc=%.*s, "
		"cnonce=\"%.*s\"",
		STR_PREC(cred->username),
		STR_PREC(chal->realm),
		STR_PREC(chal->nonce),
		STR_PREC(hreq->hurl.path),
		STR_PREC(STR_MD5),
		STR_PREC(digest_response),
		STR_PREC(qop),
		STR_PREC(nc),
		STR_PREC(cnonce));
	if (len < 0 || len >= max_len)
	    return BASE_ETOOSMALL;
	phdr->value.slen = len;

	if (chal->opaque.slen) {
	    bstrcat2(&phdr->value, ", opaque=\"");
	    bstrcat(&phdr->value, &chal->opaque);
	    bstrcat2(&phdr->value, "\"");
	}

    } else {
	/* Server requires quality protection that we don't support. */
	MTRACE( "Error: Unsupported qop offer %.*s", chal->qop.slen, chal->qop.ptr);
	return BASE_ENOTSUP;
    }

    return BASE_SUCCESS;
}


static void restart_req_with_auth(bhttp_req *hreq)
{
    bhttp_auth_chal *chal = &hreq->response.auth_chal;
    bhttp_auth_cred *cred = &hreq->param.auth_cred;
    bstatus_t status;

    if (hreq->param.headers.count >= BASE_HTTP_HEADER_SIZE) {
	MTRACE( "Error: no place to put Authorization header");
	hreq->auth_state = AUTH_DONE;
	return;
    }

    /* If credential specifies specific scheme, make sure they match */
    if (cred->scheme.slen && bstricmp(&chal->scheme, &cred->scheme)) {
	status = BASE_ENOTSUP;
	MTRACE( "Error: auth schemes mismatch");
	goto on_error;
    }

    /* If credential specifies specific realm, make sure they match */
    if (cred->realm.slen && bstricmp(&chal->realm, &cred->realm)) {
	status = BASE_ENOTSUP;
	MTRACE( "Error: auth realms mismatch");
	goto on_error;
    }

    if (!bstricmp2(&chal->scheme, "basic")) {
	status = auth_respond_basic(hreq);
    } else if (!bstricmp2(&chal->scheme, "digest")) {
	status = auth_respond_digest(hreq);
    } else {
	MTRACE( "Error: unsupported HTTP auth scheme");
	status = BASE_ENOTSUP;
    }

    if (status != BASE_SUCCESS)
	goto on_error;

    http_req_end_request(hreq);

    status = start_http_req(hreq, BASE_TRUE);
    if (status != BASE_SUCCESS)
	goto on_error;

    hreq->auth_state = AUTH_RETRYING;
    return;

on_error:
    hreq->auth_state = AUTH_DONE;
}


/* snprintf() to a bstr_t struct with an option to append the 
 * result at the back of the string.
 */
static void str_snprintf(bstr_t *s, size_t size, 
			 bbool_t append, const char *format, ...)
{
    va_list arg;
    int retval;

    va_start(arg, format);
    if (!append)
        s->slen = 0;
    size -= s->slen;
    retval = bansi_vsnprintf(s->ptr + s->slen, 
                               size, format, arg);
    s->slen += ((retval < (int)size) ? retval : size - 1);
    va_end(arg);
}

static bstatus_t http_req_start_sending(bhttp_req *hreq)
{
    bstatus_t status;
    bstr_t pkt;
    bssize_t len;
    bsize_t i;

    BASE_ASSERT_RETURN(hreq->state == SENDING_REQUEST || 
                     hreq->state == SENDING_REQUEST_BODY, BASE_EBUG);

    if (hreq->state == SENDING_REQUEST) {
        /* Prepare the request data */
        if (!hreq->buffer.ptr)
            hreq->buffer.ptr = (char*)bpool_alloc(hreq->pool, BUF_SIZE);
        bstrassign(&pkt, &hreq->buffer);
        pkt.slen = 0;
        /* Start-line */
        str_snprintf(&pkt, BUF_SIZE, BASE_TRUE, "%.*s %.*s %s/%.*s\r\n",
                     STR_PREC(hreq->param.method), 
                     STR_PREC(hreq->hurl.path),
                     get_protocol(&hreq->hurl.protocol), 
                     STR_PREC(hreq->param.version));
        /* Header field "Host" */
        str_snprintf(&pkt, BUF_SIZE, BASE_TRUE, "Host: %.*s:%d\r\n",
                     STR_PREC(hreq->hurl.host), hreq->hurl.port);
        if (!bstrcmp2(&hreq->param.method, http_method_names[HTTP_PUT])) {
            char buf[16];

            /* Header field "Content-Length" */
            butoa(hreq->param.reqdata.total_size ? 
                    (unsigned long)hreq->param.reqdata.total_size: 
                    (unsigned long)hreq->param.reqdata.size, buf);
            str_snprintf(&pkt, BUF_SIZE, BASE_TRUE, "%s: %s\r\n",
                         CONTENT_LENGTH, buf);
        }

        /* Append user-specified headers */
        for (i = 0; i < hreq->param.headers.count; i++) {
            str_snprintf(&pkt, BUF_SIZE, BASE_TRUE, "%.*s: %.*s\r\n",
                         STR_PREC(hreq->param.headers.header[i].name),
                         STR_PREC(hreq->param.headers.header[i].value));
        }
        if (pkt.slen >= BUF_SIZE - 1) {
            status = UTIL_EHTTPINSBUF;
            goto on_return;
        }

        bstrcat2(&pkt, "\r\n");
        pkt.ptr[pkt.slen] = 0;
        MTRACE( "%s", pkt.ptr);
    } else {
        pkt.ptr = (char*)hreq->param.reqdata.data;
        pkt.slen = hreq->param.reqdata.size;
    }

    /* Send the request */
    len = bstrlen(&pkt);
    bioqueue_op_key_init(&hreq->op_key, sizeof(hreq->op_key));
    hreq->tcp_state.send_size = len;
    hreq->tcp_state.current_send_size = 0;
    status = bactivesock_send(hreq->asock, &hreq->op_key, 
                                pkt.ptr, &len, 0);

    if (status == BASE_SUCCESS) {
        http_on_data_sent(hreq->asock, &hreq->op_key, len);
    } else if (status != BASE_EPENDING) {
        goto on_return; // error sending data
    }

    return BASE_SUCCESS;

on_return:
    http_req_end_request(hreq);
    return status;
}

static bstatus_t http_req_start_reading(bhttp_req *hreq)
{
    bstatus_t status;

    BASE_ASSERT_RETURN(hreq->state == REQUEST_SENT, BASE_EBUG);

    /* Receive the response */
    hreq->state = READING_RESPONSE;
    hreq->tcp_state.current_read_size = 0;
    bassert(hreq->buffer.ptr);
    status = bactivesock_start_read2(hreq->asock, hreq->pool, BUF_SIZE, 
                                       (void**)&hreq->buffer.ptr, 0);
    if (status != BASE_SUCCESS) {
        /* Error reading */
        http_req_end_request(hreq);
        return status;
    }

    return BASE_SUCCESS;
}

static bstatus_t http_req_end_request(bhttp_req *hreq)
{
    if (hreq->asock) {
	bactivesock_close(hreq->asock);
        hreq->asock = NULL;
    }

    /* Cancel query timeout timer. */
    if (hreq->timer_entry.id != 0) {
        btimer_heap_cancel(hreq->timer, &hreq->timer_entry);
        /* Invalidate id. */
        hreq->timer_entry.id = 0;
    }

    hreq->state = IDLE;

    return BASE_SUCCESS;
}

bstatus_t bhttp_req_cancel(bhttp_req *http_req,
                                       bbool_t notify)
{
    http_req->state = ABORTING;

    http_req_end_request(http_req);

    if (notify && http_req->cb.on_complete) {
        (*http_req->cb.on_complete)(http_req, (!http_req->error? 
                                    BASE_ECANCELLED: http_req->error), NULL);
    }

    return BASE_SUCCESS;
}


bstatus_t bhttp_req_destroy(bhttp_req *http_req)
{
    BASE_ASSERT_RETURN(http_req, BASE_EINVAL);
    
    /* If there is any pending request, cancel it */
    if (http_req->state != IDLE) {
        bhttp_req_cancel(http_req, BASE_FALSE);
    }

    bpool_release(http_req->pool);

    return BASE_SUCCESS;
}
