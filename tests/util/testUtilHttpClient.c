/* 
 *
 */


#include "testUtilTest.h"

#if INCLUDE_HTTP_CLIENT_TEST

//#define VERBOSE
#define STR_PREC(s) (int)s.slen, s.ptr
#define USE_LOCAL_SERVER

#include <libBase.h>
#include <libUtil.h>

#define ACTION_REPLY	0
#define ACTION_IGNORE	-1

static struct server_t
{
    bsock_t	     sock;
    buint16_t	     port;
    bthread_t	    *thread;

    /* Action:
     *	0:    reply with the response in resp.
     * -1:    ignore query (to simulate timeout).
     * other: reply with that error
     */
    int		    action;
    bbool_t       send_content_length;
    unsigned	    data_size;
    unsigned        buf_size;
} g_server;

static bbool_t thread_quit;
static btimer_heap_t *timer_heap;
static bioqueue_t *ioqueue;
static bpool_t *pool;
static bhttp_req *http_req;
static bbool_t test_cancel = BASE_FALSE;
static bsize_t total_size;
static bsize_t send_size = 0;
static bstatus_t sstatus;
static bsockaddr_in addr;
static int counter = 0;

static int server_thread(void *p)
{
    struct server_t *srv = (struct server_t*)p;
    char *pkt = (char*)bpool_alloc(pool, srv->buf_size);
    bsock_t newsock = BASE_INVALID_SOCKET;

    while (!thread_quit) {
	bssize_t pkt_len;
	int rc;
        bfd_set_t rset;
	btime_val timeout = {0, 500};

	while (!thread_quit) {
	    BASE_FD_ZERO(&rset);
	    BASE_FD_SET(srv->sock, &rset);
	    rc = bsock_select((int)srv->sock+1, &rset, NULL, NULL, &timeout);
	    if (rc != 1) {
		continue;
	    }

	    rc = bsock_accept(srv->sock, &newsock, NULL, NULL);
	    if (rc == BASE_SUCCESS) {
		break;
	    }
	}

	if (thread_quit)
	    break;

	while (!thread_quit) {
            BASE_FD_ZERO(&rset);
            BASE_FD_SET(newsock, &rset);
            rc = bsock_select((int)newsock+1, &rset, NULL, NULL, &timeout);
            if (rc != 1) {
        	BASE_INFO("client timeout");
                continue;
            }

            pkt_len = srv->buf_size;
            rc = bsock_recv(newsock, pkt, &pkt_len, 0);
            if (rc == BASE_SUCCESS) {
                break;
            }
        }

	if (thread_quit)
	    break;

	/* Simulate network RTT */
	bthreadSleepMs(50);

	if (srv->action == ACTION_IGNORE) {
	    continue;
	} else if (srv->action == ACTION_REPLY) {
            bsize_t send_len = 0;
	    unsigned ctr = 0;
            bansi_sprintf(pkt, "HTTP/1.0 200 OK\r\n");
            if (srv->send_content_length) {
                bansi_sprintf(pkt + bansi_strlen(pkt), 
                                "Content-Length: %d\r\n",
                                srv->data_size);
            }
            bansi_sprintf(pkt + bansi_strlen(pkt), "\r\n");
            pkt_len = bansi_strlen(pkt);
            rc = bsock_send(newsock, pkt, &pkt_len, 0);
            if (rc != BASE_SUCCESS) {
        	bsock_close(newsock);
        	continue;
            }
            while (send_len < srv->data_size) {
                pkt_len = srv->data_size - send_len;
                if (pkt_len > (signed)srv->buf_size)
                    pkt_len = srv->buf_size;
		send_len += pkt_len;
                bcreate_random_string(pkt, pkt_len);
                bansi_sprintf(pkt, "\nPacket: %d", ++ctr);
                pkt[bansi_strlen(pkt)] = '\n';
		rc = bsock_send(newsock, pkt, &pkt_len, 0);
		if (rc != BASE_SUCCESS)
		    break;
            }
            bsock_close(newsock);
	}
    }

    return 0;
}

static void on_data_read(bhttp_req *hreq, void *data, bsize_t size)
{
    BASE_UNUSED_ARG(hreq);
    BASE_UNUSED_ARG(data);

    BASE_INFO( "\nData received: %d bytes", size);
    if (size > 0) {
#ifdef VERBOSE
        printf("%.*s\n", (int)size, (char *)data);
#endif
    }
}

static void on_send_data(bhttp_req *hreq,
                         void **data, bsize_t *size)
{
    char *sdata;
    bsize_t sendsz = 8397;

    BASE_UNUSED_ARG(hreq);

    if (send_size + sendsz > total_size) {
        sendsz = total_size - send_size;
    }
    send_size += sendsz;

    sdata = (char*)bpool_alloc(pool, sendsz);
    bcreate_random_string(sdata, sendsz);
    bansi_sprintf(sdata, "\nSegment #%d\n", ++counter);
    *data = sdata;
    *size = sendsz;

    BASE_INFO("\nSending data progress: %d out of %d bytes", send_size, total_size);
}


static void on_complete(bhttp_req *hreq, bstatus_t status,
                        const bhttp_resp *resp)
{
    BASE_UNUSED_ARG(hreq);

    if (status == BASE_ECANCELLED) {
        BASE_INFO("Request cancelled");
        return;
    } else if (status == BASE_ETIMEDOUT) {
        BASE_INFO("Request timed out!");
        return;
    } else if (status != BASE_SUCCESS) {
        BASE_ERROR("Error %d", status);
        return;
    }
    BASE_INFO("\nData completed: %d bytes", resp->size);
    if (resp->size > 0 && resp->data) {
#ifdef VERBOSE
        printf("%.*s\n", (int)resp->size, (char *)resp->data);
#endif
    }
}

static void on_response(bhttp_req *hreq, const bhttp_resp *resp)
{
    bsize_t i;

    BASE_UNUSED_ARG(hreq);
    BASE_UNUSED_ARG(resp);
    BASE_UNUSED_ARG(i);

#ifdef VERBOSE
    printf("%.*s, %d, %.*s\n", STR_PREC(resp->version),
           resp->status_code, STR_PREC(resp->reason));
    for (i = 0; i < resp->headers.count; i++) {
        printf("%.*s : %.*s\n", 
               STR_PREC(resp->headers.header[i].name),
               STR_PREC(resp->headers.header[i].value));
    }
#endif

    if (test_cancel) {
	/* Need to delay closing the client socket here, otherwise the
	 * server will get SIGPIPE when sending response.
	 */
	bthreadSleepMs(100);
        bhttp_req_cancel(hreq, BASE_TRUE);
        test_cancel = BASE_FALSE;
    }
}


bstatus_t parse_url(const char *url, bhttp_url *hurl)
{
    bstr_t surl;
    bstatus_t status;

    bcstr(&surl, url);
    status = bhttp_req_parse_url(&surl, hurl);
#ifdef VERBOSE
    if (!status) {
        printf("URL: %s\nProtocol: %.*s\nHost: %.*s\nPort: %d\nPath: %.*s\n\n",
               url, STR_PREC(hurl->protocol), STR_PREC(hurl->host),
               hurl->port, STR_PREC(hurl->path));
    } else {
    }
#endif
    return status;
}

static int parse_url_test()
{
    struct test_data
    {
	char *url;
	bstatus_t result;
	const char *username;
	const char *passwd;
	const char *host;
	int port;
	const char *path;
    } test_data[] =
    {
	/* Simple URL without '/' in the end */
        {"http://www.extsip.org", BASE_SUCCESS, "", "", "www.extsip.org", 80, "/"},

        /* Simple URL with port number but without '/' in the end */
        {"http://extsip.org:8080", BASE_SUCCESS, "", "", "extsip.org", 8080, "/"},

        /* URL with path */
        {"http://127.0.0.1:280/Joomla/index.php?option=com_content&task=view&id=5&Itemid=6",
        	BASE_SUCCESS, "", "", "127.0.0.1", 280,
        	"/Joomla/index.php?option=com_content&task=view&id=5&Itemid=6"},

	/* URL with port and path */
	{"http://extsip.org:81/about-us/", BASE_SUCCESS, "", "", "extsip.org", 81, "/about-us/"},

	/* unsupported protocol */
	{"ftp://www.extsip.org", BASE_ENOTSUP, "", "", "", 80, ""},

	/* invalid format */
	{"http:/extsip.org/about-us/", UTIL_EHTTPINURL, "", "", "", 80, ""},

	/* invalid port number */
	{"http://extsip.org:xyz/", UTIL_EHTTPINPORT, "", "", "", 80, ""},

	/* with username and password */
	{"http://user:pass@extsip.org", BASE_SUCCESS, "user", "pass", "extsip.org", 80, "/"},

	/* password only*/
	{"http://:pass@extsip.org", BASE_SUCCESS, "", "pass", "extsip.org", 80, "/"},

	/* user only*/
	{"http://user:@extsip.org", BASE_SUCCESS, "user", "", "extsip.org", 80, "/"},

	/* empty username and passwd*/
	{"http://:@extsip.org", BASE_SUCCESS, "", "", "extsip.org", 80, "/"},

	/* '@' character in username and path */
	{"http://user@extsip.org/@", BASE_SUCCESS, "user", "", "extsip.org", 80, "/@"},

	/* '@' character in path */
	{"http://extsip.org/@", BASE_SUCCESS, "", "", "extsip.org", 80, "/@"},

	/* '@' character in path */
	{"http://extsip.org/one@", BASE_SUCCESS, "", "", "extsip.org", 80, "/one@"},

	/* Invalid URL */
	{"http://:", BASE_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http://@", BASE_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http", BASE_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http:/", BASE_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http://", BASE_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http:///", BASE_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http://@/", BASE_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http:///@", BASE_EINVAL, "", "", "", 0, ""},

	/* Invalid URL */
	{"http://:::", BASE_EINVAL, "", "", "", 0, ""},
    };
    unsigned i;

    for (i=0; i<BASE_ARRAY_SIZE(test_data); ++i) {
	struct test_data *ptd;
	bhttp_url hurl;
	bstatus_t status;

	ptd = &test_data[i];

	BASE_INFO(".. %s", ptd->url);
	status = parse_url(ptd->url, &hurl);

	if (status != ptd->result) {
	    BASE_ERROR("%d", status);
	    return -11;
	}
	if (status != BASE_SUCCESS)
	    continue;
	if (bstrcmp2(&hurl.username, ptd->username))
	    return -12;
	if (bstrcmp2(&hurl.passwd, ptd->passwd))
	    return -13;
	if (bstrcmp2(&hurl.host, ptd->host))
	    return -14;
	if (hurl.port != ptd->port)
	    return -15;
	if (bstrcmp2(&hurl.path, ptd->path))
	    return -16;
    }

    return 0;
}

/* 
 * GET request scenario 1: using on_response() and on_data_read()
 * Server replies with content-length. Application cancels the 
 * request upon receiving the response, then start it again.
 */
int http_client_test1()
{
    bstr_t url;
    bhttp_req_callback hcb;
    bhttp_req_param param;
    char urlbuf[80];

    bbzero(&hcb, sizeof(hcb));
    hcb.on_complete = &on_complete;
    hcb.on_data_read = &on_data_read;
    hcb.on_response = &on_response;
    bhttp_req_param_default(&param);

    /* Create pool, timer, and ioqueue */
    pool = bpool_create(mem, NULL, 8192, 4096, NULL);
    if (btimer_heap_create(pool, 16, &timer_heap))
        return -31;
    if (bioqueue_create(pool, 16, &ioqueue))
        return -32;

#ifdef USE_LOCAL_SERVER

    thread_quit = BASE_FALSE;
    g_server.action = ACTION_REPLY;
    g_server.send_content_length = BASE_TRUE;
    g_server.data_size = 2970;
    g_server.buf_size = 1024;

    sstatus = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, 
                             &g_server.sock);
    if (sstatus != BASE_SUCCESS)
        return -41;

    bsockaddr_in_init(&addr, NULL, 0);

    sstatus = bsock_bind(g_server.sock, &addr, sizeof(addr));
    if (sstatus != BASE_SUCCESS)
        return -43;

    {
	bsockaddr_in addr2;
	int addr_len = sizeof(addr2);
	sstatus = bsock_getsockname(g_server.sock, &addr2, &addr_len);
	if (sstatus != BASE_SUCCESS)
	    return -44;
	g_server.port = bsockaddr_in_get_port(&addr2);
	bansi_snprintf(urlbuf, sizeof(urlbuf),
			 "http://127.0.0.1:%d/about-us/",
			 g_server.port);
	url = bstr(urlbuf);
    }

    sstatus = bsock_listen(g_server.sock, 8);
    if (sstatus != BASE_SUCCESS)
        return -45;

    sstatus = bthreadCreate(pool, NULL, &server_thread, &g_server,
                               0, 0, &g_server.thread);
    if (sstatus != BASE_SUCCESS)
        return -47;

#else
    bcstr(&url, "http://www.teluu.com/about-us/");
#endif

    if (bhttp_req_create(pool, &url, timer_heap, ioqueue, 
                           &param, &hcb, &http_req))
        return -33;

    test_cancel = BASE_TRUE;
    if (bhttp_req_start(http_req))
        return -35;

    while (bhttp_req_is_running(http_req)) {
        btime_val delay = {0, 50};
	bioqueue_poll(ioqueue, &delay);
	btimer_heap_poll(timer_heap, NULL);
    }

    if (bhttp_req_start(http_req))
        return -37;

    while (bhttp_req_is_running(http_req)) {
        btime_val delay = {0, 50};
	bioqueue_poll(ioqueue, &delay);
	btimer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    thread_quit = BASE_TRUE;
    bthreadJoin(g_server.thread);
    bsock_close(g_server.sock);
#endif

    bhttp_req_destroy(http_req);
    bioqueue_destroy(ioqueue);
    btimer_heap_destroy(timer_heap);
    bpool_release(pool);

    return BASE_SUCCESS;
}

/* 
 * GET request scenario 2: using on_complete() to get the 
 * complete data. Server does not reply with content-length.
 * Request timed out, application sets a longer timeout, then
 * then restart the request.
 */
int http_client_test2()
{
    bstr_t url;
    bhttp_req_callback hcb;
    bhttp_req_param param;
    btime_val timeout;
    char urlbuf[80];

    bbzero(&hcb, sizeof(hcb));
    hcb.on_complete = &on_complete;
    hcb.on_response = &on_response;
    bhttp_req_param_default(&param);

    /* Create pool, timer, and ioqueue */
    pool = bpool_create(mem, NULL, 8192, 4096, NULL);
    if (btimer_heap_create(pool, 16, &timer_heap))
        return -41;
    if (bioqueue_create(pool, 16, &ioqueue))
        return -42;

#ifdef USE_LOCAL_SERVER

    bcstr(&url, "http://127.0.0.1:380");
    param.timeout.sec = 0;
    param.timeout.msec = 2000;

    thread_quit = BASE_FALSE;
    g_server.action = ACTION_IGNORE;
    g_server.send_content_length = BASE_FALSE;
    g_server.data_size = 4173;
    g_server.buf_size = 1024;

    sstatus = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, 
                             &g_server.sock);
    if (sstatus != BASE_SUCCESS)
        return -41;

    bsockaddr_in_init(&addr, NULL, 0);

    sstatus = bsock_bind(g_server.sock, &addr, sizeof(addr));
    if (sstatus != BASE_SUCCESS)
        return -43;

    {
	bsockaddr_in addr2;
	int addr_len = sizeof(addr2);
	sstatus = bsock_getsockname(g_server.sock, &addr2, &addr_len);
	if (sstatus != BASE_SUCCESS)
	    return -44;
	g_server.port = bsockaddr_in_get_port(&addr2);
	bansi_snprintf(urlbuf, sizeof(urlbuf),
			 "http://127.0.0.1:%d",
			 g_server.port);
	url = bstr(urlbuf);
    }

    sstatus = bsock_listen(g_server.sock, 8);
    if (sstatus != BASE_SUCCESS)
        return -45;

    sstatus = bthreadCreate(pool, NULL, &server_thread, &g_server,
                               0, 0, &g_server.thread);
    if (sstatus != BASE_SUCCESS)
        return -47;

#else
    bcstr(&url, "http://www.google.com.sg");
    param.timeout.sec = 0;
    param.timeout.msec = 50;
#endif

    bhttp_headers_add_elmt2(&param.headers, (char*)"Accept",
			     (char*)"image/gif, image/x-xbitmap, image/jpeg, "
				    "image/extpeg, application/x-ms-application,"
				    " application/vnd.ms-xpsdocument, "
			            "application/xaml+xml, "
			            "application/x-ms-xbap, "
			            "application/x-shockwave-flash, "
			            "application/vnd.ms-excel, "
			            "application/vnd.ms-powerpoint, "
			            "application/msword, */*");
    bhttp_headers_add_elmt2(&param.headers, (char*)"Accept-Language",
	                      (char*)"en-sg");
    bhttp_headers_add_elmt2(&param.headers, (char*)"User-Agent",
                              (char*)"Mozilla/4.0 (compatible; MSIE 7.0; "
                                     "Windows NT 6.0; SLCC1; "
                                     ".NET CLR 2.0.50727; "
                                     ".NET CLR 3.0.04506)");
    if (bhttp_req_create(pool, &url, timer_heap, ioqueue, 
                           &param, &hcb, &http_req))
        return -43;

    if (bhttp_req_start(http_req))
        return -45;

    while (bhttp_req_is_running(http_req)) {
        btime_val delay = {0, 50};
	bioqueue_poll(ioqueue, &delay);
	btimer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    g_server.action = ACTION_REPLY;
#endif

    timeout.sec = 0; timeout.msec = 10000;
    bhttp_req_set_timeout(http_req, &timeout);
    if (bhttp_req_start(http_req))
        return -47;

    while (bhttp_req_is_running(http_req)) {
        btime_val delay = {0, 50};
	bioqueue_poll(ioqueue, &delay);
	btimer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    thread_quit = BASE_TRUE;
    bthreadJoin(g_server.thread);
    bsock_close(g_server.sock);
#endif

    bhttp_req_destroy(http_req);
    bioqueue_destroy(ioqueue);
    btimer_heap_destroy(timer_heap);
    bpool_release(pool);

    return BASE_SUCCESS;
}

/*
 * PUT request scenario 1: sending the whole data at once
 */
int http_client_test_put1()
{
    bstr_t url;
    bhttp_req_callback hcb;
    bhttp_req_param param;
    char *data;
    int length = 3875;
    char urlbuf[80];

    bbzero(&hcb, sizeof(hcb));
    hcb.on_complete = &on_complete;
    hcb.on_data_read = &on_data_read;
    hcb.on_response = &on_response;

    /* Create pool, timer, and ioqueue */
    pool = bpool_create(mem, NULL, 8192, 4096, NULL);
    if (btimer_heap_create(pool, 16, &timer_heap))
        return -51;
    if (bioqueue_create(pool, 16, &ioqueue))
        return -52;

#ifdef USE_LOCAL_SERVER
    thread_quit = BASE_FALSE;
    g_server.action = ACTION_REPLY;
    g_server.send_content_length = BASE_TRUE;
    g_server.data_size = 0;
    g_server.buf_size = 4096;

    sstatus = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, 
                             &g_server.sock);
    if (sstatus != BASE_SUCCESS)
        return -41;

    bsockaddr_in_init(&addr, NULL, 0);

    sstatus = bsock_bind(g_server.sock, &addr, sizeof(addr));
    if (sstatus != BASE_SUCCESS)
        return -43;

    {
	bsockaddr_in addr2;
	int addr_len = sizeof(addr2);
	sstatus = bsock_getsockname(g_server.sock, &addr2, &addr_len);
	if (sstatus != BASE_SUCCESS)
	    return -44;
	g_server.port = bsockaddr_in_get_port(&addr2);
	bansi_snprintf(urlbuf, sizeof(urlbuf),
			 "http://127.0.0.1:%d/test/test.txt",
			 g_server.port);
	url = bstr(urlbuf);
    }

    sstatus = bsock_listen(g_server.sock, 8);
    if (sstatus != BASE_SUCCESS)
        return -45;

    sstatus = bthreadCreate(pool, NULL, &server_thread, &g_server,
                               0, 0, &g_server.thread);
    if (sstatus != BASE_SUCCESS)
        return -47;

#else
    bcstr(&url, "http://127.0.0.1:280/test/test.txt");

#endif

    bhttp_req_param_default(&param);
    bstrset2(&param.method, (char*)"PUT");
    data = (char*)bpool_alloc(pool, length);
    bcreate_random_string(data, length);
    bansi_sprintf(data, "PUT test\n");
    param.reqdata.data = data;
    param.reqdata.size = length;
    if (bhttp_req_create(pool, &url, timer_heap, ioqueue, 
                           &param, &hcb, &http_req))
        return -53;

    if (bhttp_req_start(http_req))
        return -55;

    while (bhttp_req_is_running(http_req)) {
        btime_val delay = {0, 50};
	bioqueue_poll(ioqueue, &delay);
	btimer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    thread_quit = BASE_TRUE;
    bthreadJoin(g_server.thread);
    bsock_close(g_server.sock);
#endif

    bhttp_req_destroy(http_req);
    bioqueue_destroy(ioqueue);
    btimer_heap_destroy(timer_heap);
    bpool_release(pool);

    return BASE_SUCCESS;
}

/*
 * PUT request scenario 2: using on_send_data() callback to
 * sending the data in chunks
 */
int http_client_test_put2()
{
    bstr_t url;
    bhttp_req_callback hcb;
    bhttp_req_param param;
    char urlbuf[80];

    bbzero(&hcb, sizeof(hcb));
    hcb.on_complete = &on_complete;
    hcb.on_send_data = &on_send_data;
    hcb.on_data_read = &on_data_read;
    hcb.on_response = &on_response;

    /* Create pool, timer, and ioqueue */
    pool = bpool_create(mem, NULL, 8192, 4096, NULL);
    if (btimer_heap_create(pool, 16, &timer_heap))
        return -51;
    if (bioqueue_create(pool, 16, &ioqueue))
        return -52;

#ifdef USE_LOCAL_SERVER
    thread_quit = BASE_FALSE;
    g_server.action = ACTION_REPLY;
    g_server.send_content_length = BASE_TRUE;
    g_server.data_size = 0;
    g_server.buf_size = 16384;

    sstatus = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, 
                             &g_server.sock);
    if (sstatus != BASE_SUCCESS)
        return -41;

    bsockaddr_in_init(&addr, NULL, 0);

    sstatus = bsock_bind(g_server.sock, &addr, sizeof(addr));
    if (sstatus != BASE_SUCCESS)
        return -43;

    {
	bsockaddr_in addr2;
	int addr_len = sizeof(addr2);
	sstatus = bsock_getsockname(g_server.sock, &addr2, &addr_len);
	if (sstatus != BASE_SUCCESS)
	    return -44;
	g_server.port = bsockaddr_in_get_port(&addr2);
	bansi_snprintf(urlbuf, sizeof(urlbuf),
			 "http://127.0.0.1:%d/test/test2.txt",
			 g_server.port);
	url = bstr(urlbuf);
    }

    sstatus = bsock_listen(g_server.sock, 8);
    if (sstatus != BASE_SUCCESS)
        return -45;

    sstatus = bthreadCreate(pool, NULL, &server_thread, &g_server,
                               0, 0, &g_server.thread);
    if (sstatus != BASE_SUCCESS)
        return -47;

#else
    bcstr(&url, "http://127.0.0.1:280/test/test2.txt");

#endif

    bhttp_req_param_default(&param);
    bstrset2(&param.method, (char*)"PUT");
    total_size = 15383;
    send_size = 0;
    param.reqdata.total_size = total_size;
    if (bhttp_req_create(pool, &url, timer_heap, ioqueue, 
                           &param, &hcb, &http_req))
        return -53;

    if (bhttp_req_start(http_req))
        return -55;

    while (bhttp_req_is_running(http_req)) {
        btime_val delay = {0, 50};
	bioqueue_poll(ioqueue, &delay);
	btimer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    thread_quit = BASE_TRUE;
    bthreadJoin(g_server.thread);
    bsock_close(g_server.sock);
#endif

    bhttp_req_destroy(http_req);
    bioqueue_destroy(ioqueue);
    btimer_heap_destroy(timer_heap);
    bpool_release(pool);

    return BASE_SUCCESS;
}

int http_client_test_delete()
{
    bstr_t url;
    bhttp_req_callback hcb;
    bhttp_req_param param;
    char urlbuf[80];

    bbzero(&hcb, sizeof(hcb));
    hcb.on_complete = &on_complete;
    hcb.on_response = &on_response;

    /* Create pool, timer, and ioqueue */
    pool = bpool_create(mem, NULL, 8192, 4096, NULL);
    if (btimer_heap_create(pool, 16, &timer_heap))
        return -61;
    if (bioqueue_create(pool, 16, &ioqueue))
        return -62;

#ifdef USE_LOCAL_SERVER
    thread_quit = BASE_FALSE;
    g_server.action = ACTION_REPLY;
    g_server.send_content_length = BASE_TRUE;
    g_server.data_size = 0;
    g_server.buf_size = 1024;

    sstatus = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, 
                             &g_server.sock);
    if (sstatus != BASE_SUCCESS)
        return -41;

    bsockaddr_in_init(&addr, NULL, 0);

    sstatus = bsock_bind(g_server.sock, &addr, sizeof(addr));
    if (sstatus != BASE_SUCCESS)
        return -43;

    {
	bsockaddr_in addr2;
	int addr_len = sizeof(addr2);
	sstatus = bsock_getsockname(g_server.sock, &addr2, &addr_len);
	if (sstatus != BASE_SUCCESS)
	    return -44;
	g_server.port = bsockaddr_in_get_port(&addr2);
	bansi_snprintf(urlbuf, sizeof(urlbuf),
			 "http://127.0.0.1:%d/test/test2.txt",
			 g_server.port);
	url = bstr(urlbuf);
    }

    sstatus = bsock_listen(g_server.sock, 8);
    if (sstatus != BASE_SUCCESS)
        return -45;

    sstatus = bthreadCreate(pool, NULL, &server_thread, &g_server,
                               0, 0, &g_server.thread);
    if (sstatus != BASE_SUCCESS)
        return -47;

#else
    bcstr(&url, "http://127.0.0.1:280/test/test2.txt");
#endif

    bhttp_req_param_default(&param);
    bstrset2(&param.method, (char*)"DELETE");
    if (bhttp_req_create(pool, &url, timer_heap, ioqueue, 
                           &param, &hcb, &http_req))
        return -63;

    if (bhttp_req_start(http_req))
        return -65;

    while (bhttp_req_is_running(http_req)) {
        btime_val delay = {0, 50};
	bioqueue_poll(ioqueue, &delay);
	btimer_heap_poll(timer_heap, NULL);
    }

#ifdef USE_LOCAL_SERVER
    thread_quit = BASE_TRUE;
    bthreadJoin(g_server.thread);
    bsock_close(g_server.sock);
#endif

    bhttp_req_destroy(http_req);
    bioqueue_destroy(ioqueue);
    btimer_heap_destroy(timer_heap);
    bpool_release(pool);

    return BASE_SUCCESS;
}

int http_client_test()
{
	int rc;

	BASE_INFO("..Testing URL parsing");
	rc = parse_url_test();
	if (rc)
		return rc;

	BASE_INFO("..Testing GET request scenario 1");
	rc = http_client_test1();
	if (rc)
		return rc;

	BASE_INFO("..Testing GET request scenario 2");
	rc = http_client_test2();
	if (rc)
		return rc;

	BASE_INFO("..Testing PUT request scenario 1");
	rc = http_client_test_put1();
	if (rc)
		return rc;

	BASE_INFO("..Testing PUT request scenario 2");
	rc = http_client_test_put2();
	if (rc)
		return rc;

	BASE_INFO("..Testing DELETE request");
	rc = http_client_test_delete();
	if (rc)
		return rc;

	return BASE_SUCCESS;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_http_client_test;
#endif

