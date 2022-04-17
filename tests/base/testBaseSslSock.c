/* $Id: ssl_sock.c 5957 2019-03-25 01:33:12Z ming $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "testBaseTest.h"
#include <libBase.h>


#define CERT_DIR		    "../build/"
#if (BASE_SSL_SOCK_IMP == BASE_SSL_SOCK_IMP_DARWIN)
/* If we use Darwin SSL, use the cert in DER format. */
#   define CERT_CA_FILE		    CERT_DIR "cacert.der"
#else
#   define CERT_CA_FILE		    CERT_DIR "cacert.pem"
#endif
#define CERT_FILE		    CERT_DIR "cacert.pem"
#define CERT_PRIVKEY_FILE	    CERT_DIR "privkey.pem"
#define CERT_PRIVKEY_PASS	    ""

#define TEST_LOAD_FROM_FILES 1

#if INCLUDE_SSLSOCK_TEST

/* Global vars */
static int clients_num;

struct send_key {
    bioqueue_op_key_t	op_key;
};


static int get_cipher_list(void) {
    bstatus_t status;
    bssl_cipher ciphers[BASE_SSL_SOCK_MAX_CIPHERS];
    unsigned cipher_num;
    unsigned i;

    cipher_num = BASE_ARRAY_SIZE(ciphers);
    status = bssl_cipher_get_availables(ciphers, &cipher_num);
    if (status != BASE_SUCCESS) {
	app_perror("...FAILED to get available ciphers", status);
	return status;
    }

    BASE_INFO("...Found %u ciphers:", cipher_num);
    for (i = 0; i < cipher_num; ++i) {
	const char* st;
	st = bssl_cipher_name(ciphers[i]);
	if (st == NULL)
	    st = "[Unknown]";

	BASE_INFO("...%3u: 0x%08x=%s", i+1, ciphers[i], st);
    }

    return BASE_SUCCESS;
}


struct test_state
{
    bpool_t	   *pool;	    /* pool				    */
    bioqueue_t   *ioqueue;	    /* ioqueue				    */
    bbool_t	    is_server;	    /* server role flag			    */
    bbool_t	    is_verbose;	    /* verbose flag, e.g: cert info	    */
    bbool_t	    echo;	    /* echo received data		    */
    bstatus_t	    err;	    /* error flag			    */
    bsize_t	    sent;	    /* bytes sent			    */
    bsize_t	    recv;	    /* bytes received			    */
    buint8_t	    read_buf[256];  /* read buffer			    */
    bbool_t	    done;	    /* test done flag			    */
    char	   *send_str;	    /* data to send once connected	    */
    bsize_t	    send_str_len;   /* send data length			    */
    bbool_t	    check_echo;	    /* flag to compare sent & echoed data   */
    const char	   *check_echo_ptr; /* pointer/cursor for comparing data    */
    struct send_key send_key;	    /* send op key			    */
};

static void dump_ssl_info(const bssl_sock_info *si)
{
    const char *tmp_st;

    /* Print cipher name */
    tmp_st = bssl_cipher_name(si->cipher);
    if (tmp_st == NULL)
	tmp_st = "[Unknown]";
    BASE_INFO(".....Cipher: %s", tmp_st);

    /* Print remote certificate info and verification result */
    if (si->remote_cert_info && si->remote_cert_info->subject.info.slen) 
    {
	char buf[2048];
	const char *verif_msgs[32];
	unsigned verif_msg_cnt;

	/* Dump remote TLS certificate info */
	BASE_INFO(".....Remote certificate info:");
	bssl_cert_info_dump(si->remote_cert_info, "  ", buf, sizeof(buf));
	BASE_INFO("\n%s", buf);

	/* Dump remote TLS certificate verification result */
	verif_msg_cnt = BASE_ARRAY_SIZE(verif_msgs);
	bssl_cert_get_verify_status_strings(si->verify_status,
					      verif_msgs, &verif_msg_cnt);
	BASE_INFO(".....Remote certificate verification result: %s",  (verif_msg_cnt == 1? verif_msgs[0]:""));
	if (verif_msg_cnt > 1) {
	    unsigned i;
	    for (i = 0; i < verif_msg_cnt; ++i)
		BASE_INFO("..... - %s", verif_msgs[i]);
	}
    }
}


static bbool_t ssl_on_connect_complete(bssl_sock_t *ssock,
					 bstatus_t status)
{
    struct test_state *st = (struct test_state*) 
		    	    bssl_sock_get_user_data(ssock);
    void *read_buf[1];
    bssl_sock_info info;
    char buf1[64], buf2[64];

    if (status != BASE_SUCCESS) {
	app_perror("...ERROR ssl_on_connect_complete()", status);
	goto on_return;
    }

    status = bssl_sock_get_info(ssock, &info);
    if (status != BASE_SUCCESS) {
	app_perror("...ERROR bssl_sock_get_info()", status);
	goto on_return;
    }

    bsockaddr_print((bsockaddr_t*)&info.local_addr, buf1, sizeof(buf1), 1);
    bsockaddr_print((bsockaddr_t*)&info.remote_addr, buf2, sizeof(buf2), 1);
    BASE_INFO("...Connected %s -> %s!", buf1, buf2);

    if (st->is_verbose)
	dump_ssl_info(&info);

    /* Start reading data */
    read_buf[0] = st->read_buf;
    status = bssl_sock_start_read2(ssock, st->pool, sizeof(st->read_buf), (void**)read_buf, 0);
    if (status != BASE_SUCCESS) {
	app_perror("...ERROR bssl_sock_start_read2()", status);
	goto on_return;
    }

    /* Start sending data */
    while (st->sent < st->send_str_len) {
	bssize_t size;

	size = st->send_str_len - st->sent;
	status = bssl_sock_send(ssock, (bioqueue_op_key_t*)&st->send_key, 
				  st->send_str + st->sent, &size, 0);
	if (status != BASE_SUCCESS && status != BASE_EPENDING) {
	    app_perror("...ERROR bssl_sock_send()", status);
	    goto on_return;
	}

	if (status == BASE_SUCCESS)
	    st->sent += size;
	else
	    break;
    }

on_return:
    st->err = status;

    if (st->err != BASE_SUCCESS) {
	bssl_sock_close(ssock);
	clients_num--;
	return BASE_FALSE;
    }

    return BASE_TRUE;
}


static bbool_t ssl_on_accept_complete(bssl_sock_t *ssock,
					bssl_sock_t *newsock,
					const bsockaddr_t *src_addr,
					int src_addr_len)
{
    struct test_state *parent_st = (struct test_state*) 
				   bssl_sock_get_user_data(ssock);
    struct test_state *st;
    void *read_buf[1];
    bssl_sock_info info;
    char buf[64];
    bstatus_t status;

    BASE_UNUSED_ARG(src_addr_len);

    /* Duplicate parent test state to newly accepted test state */
    st = (struct test_state*)bpool_zalloc(parent_st->pool, sizeof(struct test_state));
    *st = *parent_st;
    bssl_sock_set_user_data(newsock, st);

    status = bssl_sock_get_info(newsock, &info);
    if (status != BASE_SUCCESS) {
	app_perror("...ERROR bssl_sock_get_info()", status);
	goto on_return;
    }

    bsockaddr_print(src_addr, buf, sizeof(buf), 1);
    BASE_INFO("...Accepted connection from %s", buf);

    if (st->is_verbose)
	dump_ssl_info(&info);

    /* Start reading data */
    read_buf[0] = st->read_buf;
    status = bssl_sock_start_read2(newsock, st->pool, sizeof(st->read_buf), (void**)read_buf, 0);
    if (status != BASE_SUCCESS) {
	app_perror("...ERROR bssl_sock_start_read2()", status);
	goto on_return;
    }

    /* Start sending data */
    while (st->sent < st->send_str_len) {
	bssize_t size;

	size = st->send_str_len - st->sent;
	status = bssl_sock_send(newsock, (bioqueue_op_key_t*)&st->send_key, 
				  st->send_str + st->sent, &size, 0);
	if (status != BASE_SUCCESS && status != BASE_EPENDING) {
	    app_perror("...ERROR bssl_sock_send()", status);
	    goto on_return;
	}

	if (status == BASE_SUCCESS)
	    st->sent += size;
	else
	    break;
    }

on_return:
    st->err = status;

    if (st->err != BASE_SUCCESS) {
	bssl_sock_close(newsock);
	return BASE_FALSE;
    }

    return BASE_TRUE;
}

static bbool_t ssl_on_data_read(bssl_sock_t *ssock,
				  void *data,
				  bsize_t size,
				  bstatus_t status,
				  bsize_t *remainder)
{
    struct test_state *st = (struct test_state*) 
			     bssl_sock_get_user_data(ssock);

    BASE_UNUSED_ARG(remainder);
    BASE_UNUSED_ARG(data);

    if (size > 0) {
	bsize_t consumed;

	/* Set random remainder */
	*remainder = brand() % 100;

	/* Apply zero remainder if:
	 * - remainder is less than size, or
	 * - connection closed/error
	 * - echo/check_eco set
	 */
	if (*remainder > size || status != BASE_SUCCESS || st->echo || st->check_echo)
	    *remainder = 0;

	consumed = size - *remainder;
	st->recv += consumed;

	//printf("%.*s", consumed, (char*)data);

	bmemmove(data, (char*)data + consumed, *remainder);

	/* Echo data when specified to */
	if (st->echo) {
	    bssize_t size_ = consumed;
	    status = bssl_sock_send(ssock, (bioqueue_op_key_t*)&st->send_key, data, &size_, 0);
	    if (status != BASE_SUCCESS && status != BASE_EPENDING) {
		app_perror("...ERROR bssl_sock_send()", status);
		goto on_return;
	    }

	    if (status == BASE_SUCCESS)
		st->sent += size_;
	}

	/* Verify echoed data when specified to */
	if (st->check_echo) {
	    if (!st->check_echo_ptr)
		st->check_echo_ptr = st->send_str;

	    if (bmemcmp(st->check_echo_ptr, data, consumed)) {
		status = BASE_EINVAL;
		app_perror("...ERROR echoed data not exact", status);
		goto on_return;
	    }
	    st->check_echo_ptr += consumed;

	    /* Echo received completely */
	    if (st->send_str_len == st->recv) {
		bssl_sock_info info;
		char buf[64];

		status = bssl_sock_get_info(ssock, &info);
		if (status != BASE_SUCCESS) {
		    app_perror("...ERROR bssl_sock_get_info()", status);
		    goto on_return;
		}

		bsockaddr_print((bsockaddr_t*)&info.local_addr, buf, sizeof(buf), 1);
		BASE_INFO("...%s successfully recv %d bytes echo", buf, st->recv);
		st->done = BASE_TRUE;
	    }
	}
    }

    if (status != BASE_SUCCESS) {
	if (status == BASE_EEOF) {
	    status = BASE_SUCCESS;
	    st->done = BASE_TRUE;
	} else {
	    app_perror("...ERROR ssl_on_data_read()", status);
	}
    }

on_return:
    st->err = status;

    if (st->err != BASE_SUCCESS || st->done) {
	bssl_sock_close(ssock);
	if (!st->is_server)
	    clients_num--;
	return BASE_FALSE;
    }

    return BASE_TRUE;
}

static bbool_t ssl_on_data_sent(bssl_sock_t *ssock,
				  bioqueue_op_key_t *op_key,
				  bssize_t sent)
{
    struct test_state *st = (struct test_state*)
			     bssl_sock_get_user_data(ssock);
    BASE_UNUSED_ARG(op_key);

    if (sent < 0) {
	st->err = (bstatus_t)-sent;
    } else {
	st->sent += sent;

	/* Send more if any */
	while (st->sent < st->send_str_len) {
	    bssize_t size;
	    bstatus_t status;

	    size = st->send_str_len - st->sent;
	    status = bssl_sock_send(ssock, (bioqueue_op_key_t*)&st->send_key, 
				      st->send_str + st->sent, &size, 0);
	    if (status != BASE_SUCCESS && status != BASE_EPENDING) {
		app_perror("...ERROR bssl_sock_send()", status);
		st->err = status;
		break;
	    }

	    if (status == BASE_SUCCESS)
		st->sent += size;
	    else
		break;
	}
    }

    if (st->err != BASE_SUCCESS) {
	bssl_sock_close(ssock);
	if (!st->is_server)
	    clients_num--;
	return BASE_FALSE;
    }

    return BASE_TRUE;
}

#define HTTP_SERVER_ADDR	"trac.extsip.org"
#define HTTP_SERVER_PORT	443
#define HTTP_REQ		"GET https://" HTTP_SERVER_ADDR "/ HTTP/1.0\r\n\r\n";

static int https_client_test(unsigned ms_timeout)
{
    bpool_t *pool = NULL;
    bioqueue_t *ioqueue = NULL;
    btimer_heap_t *timer = NULL;
    bssl_sock_t *ssock = NULL;
    bssl_sock_param param;
    bstatus_t status;
    struct test_state state = {0};
    bsockaddr local_addr, rem_addr;
    bstr_t tmp_st;

    pool = bpool_create(mem, "https_get", 256, 256, NULL);

    status = bioqueue_create(pool, 4, &ioqueue);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    status = btimer_heap_create(pool, 4, &timer);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    state.pool = pool;
    state.send_str = HTTP_REQ;
    state.send_str_len = bansi_strlen(state.send_str);
    state.is_verbose = BASE_TRUE;

    bssl_sock_param_default(&param);
    param.cb.on_connect_complete = &ssl_on_connect_complete;
    param.cb.on_data_read = &ssl_on_data_read;
    param.cb.on_data_sent = &ssl_on_data_sent;
    param.ioqueue = ioqueue;
    param.user_data = &state;
    param.server_name = bstr((char*)HTTP_SERVER_ADDR);
    param.timer_heap = timer;
    param.timeout.sec = 0;
    param.timeout.msec = ms_timeout;
    param.proto = BASE_SSL_SOCK_PROTO_SSL23;
    btime_val_normalize(&param.timeout);

    status = bssl_sock_create(pool, &param, &ssock);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    bsockaddr_init(BASE_AF_INET, &local_addr, bstrset2(&tmp_st, "0.0.0.0"), 0);
    bsockaddr_init(BASE_AF_INET, &rem_addr, bstrset2(&tmp_st, HTTP_SERVER_ADDR), HTTP_SERVER_PORT);
    status = bssl_sock_start_connect(ssock, pool, &local_addr, &rem_addr, sizeof(rem_addr));
    if (status == BASE_SUCCESS) {
	ssl_on_connect_complete(ssock, BASE_SUCCESS);
    } else if (status == BASE_EPENDING) {
	status = BASE_SUCCESS;
    } else {
	goto on_return;
    }

    /* Wait until everything has been sent/received */
    while (state.err == BASE_SUCCESS && !state.done) {
#ifdef BASE_SYMBIAN
	bsymbianos_poll(-1, 1000);
#else
	btime_val delay = {0, 100};
	bioqueue_poll(ioqueue, &delay);
	btimer_heap_poll(timer, &delay);
#endif
    }

    if (state.err) {
	status = state.err;
	goto on_return;
    }

    BASE_INFO("...Done!");
    BASE_INFO(".....Sent/recv: %d/%d bytes", state.sent, state.recv);

on_return:
    if (ssock && !state.err && !state.done) 
	bssl_sock_close(ssock);
    if (ioqueue)
	bioqueue_destroy(ioqueue);
    if (timer)
	btimer_heap_destroy(timer);
    if (pool)
	bpool_release(pool);

    return status;
}

#if !(defined(TEST_LOAD_FROM_FILES) && TEST_LOAD_FROM_FILES==1) 
static bstatus_t load_cert_to_buf(bpool_t *pool, const bstr_t *file_name,
				    bssl_cert_buffer *buf)
{
    bstatus_t status;
    bOsHandle_t fd = 0;
    bssize_t size = (bssize_t)bfile_size(file_name->ptr);

    status = bfile_open(pool, file_name->ptr, BASE_O_RDONLY, &fd);
    if (status != BASE_SUCCESS)
	return status;

    buf->ptr = (char*)bpool_zalloc(pool, size+1);
    status = bfile_read(fd, buf->ptr, &size);
    buf->slen = size;

    bfile_close(fd);
    fd = NULL;
    return status;
}
#endif

static int echo_test(bssl_sock_proto srv_proto, bssl_sock_proto cli_proto,
		     bssl_cipher srv_cipher, bssl_cipher cli_cipher,
		     bbool_t req_client_cert, bbool_t client_provide_cert)
{
    bpool_t *pool = NULL;
    bioqueue_t *ioqueue = NULL;
    bssl_sock_t *ssock_serv = NULL;
    bssl_sock_t *ssock_cli = NULL;
    bssl_sock_param param;
    struct test_state state_serv = { 0 };
    struct test_state state_cli = { 0 };
    bsockaddr addr, listen_addr;
    bssl_cipher ciphers[1];
    bssl_cert_t *cert = NULL;
    bstatus_t status;

    pool = bpool_create(mem, "ssl_echo", 256, 256, NULL);

    status = bioqueue_create(pool, 4, &ioqueue);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    bssl_sock_param_default(&param);
    param.cb.on_accept_complete = &ssl_on_accept_complete;
    param.cb.on_connect_complete = &ssl_on_connect_complete;
    param.cb.on_data_read = &ssl_on_data_read;
    param.cb.on_data_sent = &ssl_on_data_sent;
    param.ioqueue = ioqueue;
    param.ciphers = ciphers;

    /* Init default bind address */
    {
	bstr_t tmp_st;
	bsockaddr_init(BASE_AF_INET, &addr, bstrset2(&tmp_st, "127.0.0.1"), 0);
    }

    /* === SERVER === */
    param.proto = srv_proto;
    param.user_data = &state_serv;
    param.ciphers_num = (srv_cipher == -1)? 0 : 1;
    param.require_client_cert = req_client_cert;
    ciphers[0] = srv_cipher;

    state_serv.pool = pool;
    state_serv.echo = BASE_TRUE;
    state_serv.is_server = BASE_TRUE;
    state_serv.is_verbose = BASE_TRUE;

    status = bssl_sock_create(pool, &param, &ssock_serv);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Set server cert */
    {
	bstr_t ca_file = bstr(CERT_CA_FILE);
	bstr_t cert_file = bstr(CERT_FILE);
	bstr_t privkey_file = bstr(CERT_PRIVKEY_FILE);
	bstr_t privkey_pass = bstr(CERT_PRIVKEY_PASS);

#if (defined(TEST_LOAD_FROM_FILES) && TEST_LOAD_FROM_FILES==1)
	status = bssl_cert_load_from_files(pool, &ca_file, &cert_file, 
					     &privkey_file, &privkey_pass,
					     &cert);
#else
	bssl_cert_buffer ca_buf, cert_buf, privkey_buf;

	status = load_cert_to_buf(pool, &ca_file, &ca_buf);
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}

	status = load_cert_to_buf(pool, &cert_file, &cert_buf);
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}

	status = load_cert_to_buf(pool, &privkey_file, &privkey_buf);
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}

	status = bssl_cert_load_from_buffer(pool, &ca_buf, &cert_buf,
					      &privkey_buf, &privkey_pass, 
					      &cert);
#endif
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}

	status = bssl_sock_set_certificate(ssock_serv, pool, cert);
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}
    }

    status = bssl_sock_start_accept(ssock_serv, pool, &addr, bsockaddr_get_len(&addr));
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Get listener address */
    {
	bssl_sock_info info;

	bssl_sock_get_info(ssock_serv, &info);
	bsockaddr_cp(&listen_addr, &info.local_addr);
    }

    /* === CLIENT === */
    param.proto = cli_proto;
    param.user_data = &state_cli;
    param.ciphers_num = (cli_cipher == -1)? 0 : 1;
    ciphers[0] = cli_cipher;

    state_cli.pool = pool;
    state_cli.check_echo = BASE_TRUE;
    state_cli.is_verbose = BASE_TRUE;

    {
	btime_val now;

	bgettimeofday(&now);
	bsrand((unsigned)now.sec);
	state_cli.send_str_len = (brand() % 5 + 1) * 1024 + brand() % 1024;
    }
    state_cli.send_str = (char*)bpool_alloc(pool, state_cli.send_str_len);
    {
	unsigned i;
	for (i = 0; i < state_cli.send_str_len; ++i)
	    state_cli.send_str[i] = (char)(brand() % 256);
    }

    status = bssl_sock_create(pool, &param, &ssock_cli);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Set cert for client */
    {

	if (!client_provide_cert) {
	    bstr_t ca_file = bstr(CERT_CA_FILE);
	    bstr_t null_str = bstr("");

#if (defined(TEST_LOAD_FROM_FILES) && TEST_LOAD_FROM_FILES==1)
	    status = bssl_cert_load_from_files(pool, &ca_file, &null_str, 
						 &null_str, &null_str, &cert);
#else
	    bssl_cert_buffer null_buf, ca_buf;

	    null_buf.slen = 0;

	    status = load_cert_to_buf(pool, &ca_file, &ca_buf);
	    if (status != BASE_SUCCESS) {
		goto on_return;
	    }

	    status = bssl_cert_load_from_buffer(pool, &ca_buf, &null_buf,
						  &null_buf, &null_str, &cert);
#endif
	    if (status != BASE_SUCCESS) {
		goto on_return;
	    }

	}

	status = bssl_sock_set_certificate(ssock_cli, pool, cert);
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}
    }

    status = bssl_sock_start_connect(ssock_cli, pool, &addr, &listen_addr, bsockaddr_get_len(&addr));
    if (status == BASE_SUCCESS) {
	ssl_on_connect_complete(ssock_cli, BASE_SUCCESS);
    } else if (status == BASE_EPENDING) {
	status = BASE_SUCCESS;
    } else {
	goto on_return;
    }

    /* Wait until everything has been sent/received or error */
    while (!state_serv.err && !state_cli.err && !state_serv.done && !state_cli.done)
    {
#ifdef BASE_SYMBIAN
	bsymbianos_poll(-1, 1000);
#else
	btime_val delay = {0, 100};
	bioqueue_poll(ioqueue, &delay);
#endif
    }

    /* Clean up sockets */
    {
	btime_val delay = {0, 100};
	while (bioqueue_poll(ioqueue, &delay) > 0);
    }

    if (state_serv.err || state_cli.err) {
	if (state_serv.err != BASE_SUCCESS)
	    status = state_serv.err;
	else
	    status = state_cli.err;

	goto on_return;
    }

    BASE_INFO("...Done!");
    BASE_INFO(".....Sent/recv: %d/%d bytes", state_cli.sent, state_cli.recv);

on_return:
    if (ssock_serv)
	bssl_sock_close(ssock_serv);
    if (ssock_cli && !state_cli.err && !state_cli.done) 
	bssl_sock_close(ssock_cli);
    if (ioqueue)
	bioqueue_destroy(ioqueue);
    if (pool)
	bpool_release(pool);

    return status;
}


static bbool_t asock_on_data_read(bactivesock_t *asock,
				    void *data,
				    bsize_t size,
				    bstatus_t status,
				    bsize_t *remainder)
{
    struct test_state *st = (struct test_state*)
			     bactivesock_get_user_data(asock);

    BASE_UNUSED_ARG(data);
    BASE_UNUSED_ARG(size);
    BASE_UNUSED_ARG(remainder);

    if (status != BASE_SUCCESS) {
	if (status == BASE_EEOF) {
	    status = BASE_SUCCESS;
	    st->done = BASE_TRUE;
	} else {
	    app_perror("...ERROR asock_on_data_read()", status);
	}
    }

    st->err = status;

    if (st->err != BASE_SUCCESS || st->done) {
	bactivesock_close(asock);
	if (!st->is_server)
	    clients_num--;
	return BASE_FALSE;
    }

    return BASE_TRUE;
}


static bbool_t asock_on_connect_complete(bactivesock_t *asock,
					   bstatus_t status)
{
    struct test_state *st = (struct test_state*)
			     bactivesock_get_user_data(asock);

    if (status == BASE_SUCCESS) {
	void *read_buf[1];

	/* Start reading data */
	read_buf[0] = st->read_buf;
	status = bactivesock_start_read2(asock, st->pool, sizeof(st->read_buf), (void**)read_buf, 0);
	if (status != BASE_SUCCESS) {
	    app_perror("...ERROR bssl_sock_start_read2()", status);
	}
    }

    st->err = status;

    if (st->err != BASE_SUCCESS) {
	bactivesock_close(asock);
	if (!st->is_server)
	    clients_num--;
	return BASE_FALSE;
    }

    return BASE_TRUE;
}

static bbool_t asock_on_accept_complete(bactivesock_t *asock,
					  bsock_t newsock,
					  const bsockaddr_t *src_addr,
					  int src_addr_len)
{
    struct test_state *st;
    void *read_buf[1];
    bactivesock_t *new_asock;
    bactivesock_cb asock_cb = { 0 };
    bstatus_t status;

    BASE_UNUSED_ARG(src_addr);
    BASE_UNUSED_ARG(src_addr_len);

    st = (struct test_state*) bactivesock_get_user_data(asock);

    asock_cb.on_data_read = &asock_on_data_read;
    status = bactivesock_create(st->pool, newsock, bSOCK_STREAM(), NULL, 
				  st->ioqueue, &asock_cb, st, &new_asock);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Start reading data */
    read_buf[0] = st->read_buf;
    status = bactivesock_start_read2(new_asock, st->pool, 
				       sizeof(st->read_buf), 
				       (void**)read_buf, 0);
    if (status != BASE_SUCCESS) {
	app_perror("...ERROR bssl_sock_start_read2()", status);
    }

on_return:
    st->err = status;

    if (st->err != BASE_SUCCESS)
	bactivesock_close(new_asock);

    return BASE_TRUE;
}


/* Raw TCP socket try to connect to SSL socket server, once
 * connection established, it will just do nothing, SSL socket
 * server should be able to close the connection after specified
 * timeout period (set ms_timeout to 0 to disable timer).
 */
static int client_non_ssl(unsigned ms_timeout)
{
    bpool_t *pool = NULL;
    bioqueue_t *ioqueue = NULL;
    btimer_heap_t *timer = NULL;
    bssl_sock_t *ssock_serv = NULL;
    bactivesock_t *asock_cli = NULL;
    bactivesock_cb asock_cb = { 0 };
    bsock_t sock = BASE_INVALID_SOCKET;
    bssl_sock_param param;
    struct test_state state_serv = { 0 };
    struct test_state state_cli = { 0 };
    bsockaddr listen_addr;
    bssl_cert_t *cert = NULL;
    bstatus_t status;

    pool = bpool_create(mem, "ssl_accept_raw_tcp", 256, 256, NULL);

    status = bioqueue_create(pool, 4, &ioqueue);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    status = btimer_heap_create(pool, 4, &timer);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Set cert */
    {
	bstr_t ca_file = bstr(CERT_CA_FILE);
	bstr_t cert_file = bstr(CERT_FILE);
	bstr_t privkey_file = bstr(CERT_PRIVKEY_FILE);
	bstr_t privkey_pass = bstr(CERT_PRIVKEY_PASS);

#if (BASE_SSL_SOCK_IMP == BASE_SSL_SOCK_IMP_DARWIN)
	BASE_WARN( "Darwin SSL requires the private key to be "
		       "inside the Keychain. So double click on "
		       "the file libBase/build/privkey.p12 to "
		       "place it in the Keychain. "
		       "The password is \"extsip\".");
#endif

#if (defined(TEST_LOAD_FROM_FILES) && TEST_LOAD_FROM_FILES==1)
	status = bssl_cert_load_from_files(pool, &ca_file, &cert_file, 
					     &privkey_file, &privkey_pass,
					     &cert);
#else
	bssl_cert_buffer ca_buf, cert_buf, privkey_buf;

	status = load_cert_to_buf(pool, &ca_file, &ca_buf);
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}

	status = load_cert_to_buf(pool, &cert_file, &cert_buf);
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}

	status = load_cert_to_buf(pool, &privkey_file, &privkey_buf);
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}

	status = bssl_cert_load_from_buffer(pool, &ca_buf, &cert_buf,
					      &privkey_buf, &privkey_pass, 
					      &cert);
#endif
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}
    }

    bssl_sock_param_default(&param);
    param.cb.on_accept_complete = &ssl_on_accept_complete;
    param.cb.on_data_read = &ssl_on_data_read;
    param.cb.on_data_sent = &ssl_on_data_sent;
    param.ioqueue = ioqueue;
    param.timer_heap = timer;
    param.timeout.sec = 0;
    param.timeout.msec = ms_timeout;
    btime_val_normalize(&param.timeout);

    /* SERVER */
    param.user_data = &state_serv;
    state_serv.pool = pool;
    state_serv.is_server = BASE_TRUE;
    state_serv.is_verbose = BASE_TRUE;

    status = bssl_sock_create(pool, &param, &ssock_serv);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    status = bssl_sock_set_certificate(ssock_serv, pool, cert);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Init bind address */
    {
	bstr_t tmp_st;
	bsockaddr_init(BASE_AF_INET, &listen_addr, bstrset2(&tmp_st, "127.0.0.1"), 0);
    }

    status = bssl_sock_start_accept(ssock_serv, pool, &listen_addr, bsockaddr_get_len(&listen_addr));
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Update listener address */
    {
	bssl_sock_info info;

	bssl_sock_get_info(ssock_serv, &info);
	bsockaddr_cp(&listen_addr, &info.local_addr);
    }

    /* CLIENT */
    state_cli.pool = pool;
    status = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, &sock);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    asock_cb.on_connect_complete = &asock_on_connect_complete;
    asock_cb.on_data_read = &asock_on_data_read;
    status = bactivesock_create(pool, sock, bSOCK_STREAM(), NULL, 
				  ioqueue, &asock_cb, &state_cli, &asock_cli);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    status = bactivesock_start_connect(asock_cli, pool, (bsockaddr_t*)&listen_addr, 
					 bsockaddr_get_len(&listen_addr));
    if (status == BASE_SUCCESS) {
	asock_on_connect_complete(asock_cli, BASE_SUCCESS);
    } else if (status == BASE_EPENDING) {
	status = BASE_SUCCESS;
    } else {
	goto on_return;
    }

    /* Wait until everything has been sent/received or error */
    while (!state_serv.err && !state_cli.err && !state_serv.done && !state_cli.done)
    {
#ifdef BASE_SYMBIAN
	bsymbianos_poll(-1, 1000);
#else
	btime_val delay = {0, 100};
	bioqueue_poll(ioqueue, &delay);
	btimer_heap_poll(timer, &delay);
#endif
    }

    if (state_serv.err || state_cli.err) {
	if (state_serv.err != BASE_SUCCESS)
	    status = state_serv.err;
	else
	    status = state_cli.err;

	goto on_return;
    }

    BASE_INFO("...Done!");

on_return:
    if (ssock_serv)
	bssl_sock_close(ssock_serv);
    if (asock_cli && !state_cli.err && !state_cli.done)
	bactivesock_close(asock_cli);
    if (timer)
	btimer_heap_destroy(timer);
    if (ioqueue)
	bioqueue_destroy(ioqueue);
    if (pool)
	bpool_release(pool);

    return status;
}


/* SSL socket try to connect to raw TCP socket server, once
 * connection established, SSL socket will try to perform SSL
 * handshake. SSL client socket should be able to close the
 * connection after specified timeout period (set ms_timeout to 
 * 0 to disable timer).
 */
static int server_non_ssl(unsigned ms_timeout)
{
    bpool_t *pool = NULL;
    bioqueue_t *ioqueue = NULL;
    btimer_heap_t *timer = NULL;
    bactivesock_t *asock_serv = NULL;
    bssl_sock_t *ssock_cli = NULL;
    bactivesock_cb asock_cb = { 0 };
    bsock_t sock = BASE_INVALID_SOCKET;
    bssl_sock_param param;
    struct test_state state_serv = { 0 };
    struct test_state state_cli = { 0 };
    bsockaddr addr, listen_addr;
    bstatus_t status;

    pool = bpool_create(mem, "ssl_connect_raw_tcp", 256, 256, NULL);

    status = bioqueue_create(pool, 4, &ioqueue);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    status = btimer_heap_create(pool, 4, &timer);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* SERVER */
    state_serv.pool = pool;
    state_serv.ioqueue = ioqueue;

    status = bsock_socket(bAF_INET(), bSOCK_STREAM(), 0, &sock);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Init bind address */
    {
	bstr_t tmp_st;
	bsockaddr_init(BASE_AF_INET, &listen_addr, bstrset2(&tmp_st, "127.0.0.1"), 0);
    }

    status = bsock_bind(sock, (bsockaddr_t*)&listen_addr, 
			  bsockaddr_get_len((bsockaddr_t*)&listen_addr));
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    status = bsock_listen(sock, BASE_SOMAXCONN);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    asock_cb.on_accept_complete = &asock_on_accept_complete;
    status = bactivesock_create(pool, sock, bSOCK_STREAM(), NULL, 
				  ioqueue, &asock_cb, &state_serv, &asock_serv);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    status = bactivesock_start_accept(asock_serv, pool);
    if (status != BASE_SUCCESS)
	goto on_return;

    /* Update listener address */
    {
	int addr_len;

	addr_len = sizeof(listen_addr);
	bsock_getsockname(sock, (bsockaddr_t*)&listen_addr, &addr_len);
    }

    /* CLIENT */
    bssl_sock_param_default(&param);
    param.cb.on_connect_complete = &ssl_on_connect_complete;
    param.cb.on_data_read = &ssl_on_data_read;
    param.cb.on_data_sent = &ssl_on_data_sent;
    param.ioqueue = ioqueue;
    param.timer_heap = timer;
    param.timeout.sec = 0;
    param.timeout.msec = ms_timeout;
    btime_val_normalize(&param.timeout);
    param.user_data = &state_cli;

    state_cli.pool = pool;
    state_cli.is_server = BASE_FALSE;
    state_cli.is_verbose = BASE_TRUE;

    status = bssl_sock_create(pool, &param, &ssock_cli);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Init default bind address */
    {
	bstr_t tmp_st;
	bsockaddr_init(BASE_AF_INET, &addr, bstrset2(&tmp_st, "127.0.0.1"), 0);
    }

    status = bssl_sock_start_connect(ssock_cli, pool, 
				       (bsockaddr_t*)&addr, 
				       (bsockaddr_t*)&listen_addr, 
				       bsockaddr_get_len(&listen_addr));
    if (status != BASE_EPENDING) {
	goto on_return;
    }

    /* Wait until everything has been sent/received or error */
    while ((!state_serv.err && !state_serv.done) || (!state_cli.err && !state_cli.done))
    {
#ifdef BASE_SYMBIAN
	bsymbianos_poll(-1, 1000);
#else
	btime_val delay = {0, 100};
	bioqueue_poll(ioqueue, &delay);
	btimer_heap_poll(timer, &delay);
#endif
    }

    if (state_serv.err || state_cli.err) {
	if (state_cli.err != BASE_SUCCESS)
	    status = state_cli.err;
	else
	    status = state_serv.err;

	goto on_return;
    }

    BASE_INFO("...Done!");

on_return:
    if (asock_serv)
	bactivesock_close(asock_serv);
    if (ssock_cli && !state_cli.err && !state_cli.done)
	bssl_sock_close(ssock_cli);
    if (timer)
	btimer_heap_destroy(timer);
    if (ioqueue)
	bioqueue_destroy(ioqueue);
    if (pool)
	bpool_release(pool);

    return status;
}


/* Test will perform multiple clients trying to connect to single server.
 * Once SSL connection established, echo test will be performed.
 */
static int perf_test(unsigned clients, unsigned ms_handshake_timeout)
{
    bpool_t *pool = NULL;
    bioqueue_t *ioqueue = NULL;
    btimer_heap_t *timer = NULL;
    bssl_sock_t *ssock_serv = NULL;
    bssl_sock_t **ssock_cli = NULL;
    bssl_sock_param param;
    struct test_state state_serv = { 0 };
    struct test_state *state_cli = NULL;
    bsockaddr addr, listen_addr;
    bssl_cert_t *cert = NULL;
    bstatus_t status;
    unsigned i, cli_err = 0;
    bsize_t tot_sent = 0, tot_recv = 0;
    btime_val start;

    pool = bpool_create(mem, "ssl_perf", 256, 256, NULL);

    status = bioqueue_create(pool, BASE_IOQUEUE_MAX_HANDLES, &ioqueue);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    status = btimer_heap_create(pool, BASE_IOQUEUE_MAX_HANDLES, &timer);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Set cert */
    {
	bstr_t ca_file = bstr(CERT_CA_FILE);
	bstr_t cert_file = bstr(CERT_FILE);
	bstr_t privkey_file = bstr(CERT_PRIVKEY_FILE);
	bstr_t privkey_pass = bstr(CERT_PRIVKEY_PASS);

#if (defined(TEST_LOAD_FROM_FILES) && TEST_LOAD_FROM_FILES==1)
	status = bssl_cert_load_from_files(pool, &ca_file, &cert_file, 
					     &privkey_file, &privkey_pass,
					     &cert);
#else
	bssl_cert_buffer ca_buf, cert_buf, privkey_buf;

	status = load_cert_to_buf(pool, &ca_file, &ca_buf);
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}

	status = load_cert_to_buf(pool, &cert_file, &cert_buf);
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}

	status = load_cert_to_buf(pool, &privkey_file, &privkey_buf);
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}

	status = bssl_cert_load_from_buffer(pool, &ca_buf, &cert_buf,
					      &privkey_buf, &privkey_pass, 
					      &cert);
#endif
	if (status != BASE_SUCCESS) {
	    goto on_return;
	}
    }

    bssl_sock_param_default(&param);
    param.cb.on_accept_complete = &ssl_on_accept_complete;
    param.cb.on_connect_complete = &ssl_on_connect_complete;
    param.cb.on_data_read = &ssl_on_data_read;
    param.cb.on_data_sent = &ssl_on_data_sent;
    param.ioqueue = ioqueue;
    param.timer_heap = timer;
    param.timeout.sec = 0;
    param.timeout.msec = ms_handshake_timeout;
    btime_val_normalize(&param.timeout);

    /* Init default bind address */
    {
	bstr_t tmp_st;
	bsockaddr_init(BASE_AF_INET, &addr, bstrset2(&tmp_st, "127.0.0.1"), 0);
    }

    /* SERVER */
    param.user_data = &state_serv;

    state_serv.pool = pool;
    state_serv.echo = BASE_TRUE;
    state_serv.is_server = BASE_TRUE;

    status = bssl_sock_create(pool, &param, &ssock_serv);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    status = bssl_sock_set_certificate(ssock_serv, pool, cert);
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    status = bssl_sock_start_accept(ssock_serv, pool, &addr, bsockaddr_get_len(&addr));
    if (status != BASE_SUCCESS) {
	goto on_return;
    }

    /* Get listening address for clients to connect to */
    {
	bssl_sock_info info;
	char buf[64];

	bssl_sock_get_info(ssock_serv, &info);
	bsockaddr_cp(&listen_addr, &info.local_addr);

	bsockaddr_print((bsockaddr_t*)&listen_addr, buf, sizeof(buf), 1);
	BASE_INFO("...Listener ready at %s", buf);
    }


    /* CLIENTS */
    clients_num = clients;
    param.timeout.sec = 0;
    param.timeout.msec = 0;

    /* Init random seed */
    {
	btime_val now;

	bgettimeofday(&now);
	bsrand((unsigned)now.sec);
    }

    /* Allocate SSL socket pointers and test state */
    ssock_cli = (bssl_sock_t**)bpool_calloc(pool, clients, sizeof(bssl_sock_t*));
    state_cli = (struct test_state*)bpool_calloc(pool, clients, sizeof(struct test_state));

    /* Get start timestamp */
    bgettimeofday(&start);

    /* Setup clients */
    for (i = 0; i < clients; ++i) {
	param.user_data = &state_cli[i];

	state_cli[i].pool = pool;
	state_cli[i].check_echo = BASE_TRUE;
	state_cli[i].send_str_len = (brand() % 5 + 1) * 1024 + brand() % 1024;
	state_cli[i].send_str = (char*)bpool_alloc(pool, state_cli[i].send_str_len);
	{
	    unsigned j;
	    for (j = 0; j < state_cli[i].send_str_len; ++j)
		state_cli[i].send_str[j] = (char)(brand() % 256);
	}

	status = bssl_sock_create(pool, &param, &ssock_cli[i]);
	if (status != BASE_SUCCESS) {
	    app_perror("...ERROR bssl_sock_create()", status);
	    cli_err++;
	    clients_num--;
	    continue;
	}

	status = bssl_sock_start_connect(ssock_cli[i], pool, &addr, &listen_addr, bsockaddr_get_len(&addr));
	if (status == BASE_SUCCESS) {
	    ssl_on_connect_complete(ssock_cli[i], BASE_SUCCESS);
	} else if (status == BASE_EPENDING) {
	    status = BASE_SUCCESS;
	} else {
	    app_perror("...ERROR bssl_sock_create()", status);
	    bssl_sock_close(ssock_cli[i]);
	    ssock_cli[i] = NULL;
	    clients_num--;
	    cli_err++;
	    continue;
	}

	/* Give chance to server to accept this client */
	{
	    unsigned n = 5;

#ifdef BASE_SYMBIAN
	    while(n && bsymbianos_poll(-1, 1000))
		n--;
#else
	    btime_val delay = {0, 100};
	    while(n && bioqueue_poll(ioqueue, &delay) > 0)
		n--;
#endif
	}
    }

    /* Wait until everything has been sent/received or error */
    while (clients_num)
    {
#ifdef BASE_SYMBIAN
	bsymbianos_poll(-1, 1000);
#else
	btime_val delay = {0, 100};
	bioqueue_poll(ioqueue, &delay);
	btimer_heap_poll(timer, &delay);
#endif
    }

    /* Clean up sockets */
    {
	btime_val delay = {0, 500};
	while (bioqueue_poll(ioqueue, &delay) > 0);
    }

    if (state_serv.err != BASE_SUCCESS) {
	status = state_serv.err;
	goto on_return;
    }

    BASE_INFO("...Done!");

    /* SSL setup and data transfer duration */
    {
	btime_val stop;
	
	bgettimeofday(&stop);
	BASE_TIME_VAL_SUB(stop, start);

	BASE_INFO(".....Setup & data transfer duration: %d.%03ds", stop.sec, stop.msec);
    }

    /* Check clients status */
    for (i = 0; i < clients; ++i) {
	if (state_cli[i].err != BASE_SUCCESS)
	    cli_err++;

	tot_sent += state_cli[1].sent;
	tot_recv += state_cli[1].recv;
    }

    BASE_INFO(".....Clients: %d (%d errors)", clients, cli_err);
    BASE_INFO(".....Total sent/recv: %d/%d bytes", tot_sent, tot_recv);

on_return:
    if (ssock_serv) 
	bssl_sock_close(ssock_serv);

    if (ssock_cli && state_cli) {
        for (i = 0; i < clients; ++i) {
	    if (ssock_cli[i] && !state_cli[i].err && !state_cli[i].done)
	        bssl_sock_close(ssock_cli[i]);
	}
    }
    if (ioqueue)
	bioqueue_destroy(ioqueue);
    if (pool)
	bpool_release(pool);

    return status;
}

#if 0 && (!defined(BASE_SYMBIAN) || BASE_SYMBIAN==0)
bstatus_t bssl_sock_ossl_test_send_buf(bpool_t *pool);
static int ossl_test_send_buf()
{
    bpool_t *pool;
    bstatus_t status;

    pool = bpool_create(mem, "send_buf", 256, 256, NULL);
    status = bssl_sock_ossl_test_send_buf(pool);
    bpool_release(pool);
    return status;
}
#else
static int ossl_test_send_buf()
{
    return 0;
}
#endif

int ssl_sock_test(void)
{
    int ret;

    BASE_INFO("..test ossl send buf");
    ret = ossl_test_send_buf();
    if (ret != 0)
	return ret;

    BASE_INFO("..get cipher list test");
    ret = get_cipher_list();
    if (ret != 0)
	return ret;

    BASE_INFO("..https client test");
    ret = https_client_test(30000);
    // Ignore test result as internet connection may not be available.
    //if (ret != 0)
	//return ret;

#ifndef BASE_SYMBIAN
   
    /* On Symbian platforms, SSL socket is implemented using CSecureSocket, 
     * and it hasn't supported server mode, so exclude the following tests,
     * which require SSL server, for now.
     */

    BASE_INFO("..echo test w/ TLSv1 and BASE_TLS_RSA_WITH_AES_256_CBC_SHA cipher");
    ret = echo_test(BASE_SSL_SOCK_PROTO_TLS1, BASE_SSL_SOCK_PROTO_TLS1, 
		    BASE_TLS_RSA_WITH_AES_256_CBC_SHA, BASE_TLS_RSA_WITH_AES_256_CBC_SHA, 
		    BASE_FALSE, BASE_FALSE);
    if (ret != 0)
	return ret;

    BASE_INFO("..echo test w/ SSLv23 and BASE_TLS_RSA_WITH_AES_256_CBC_SHA cipher");
    ret = echo_test(BASE_SSL_SOCK_PROTO_SSL23, BASE_SSL_SOCK_PROTO_SSL23, 
		    BASE_TLS_RSA_WITH_AES_256_CBC_SHA, BASE_TLS_RSA_WITH_AES_256_CBC_SHA,
		    BASE_FALSE, BASE_FALSE);
    if (ret != 0)
	return ret;

    BASE_INFO("..echo test w/ incompatible proto");
    ret = echo_test(BASE_SSL_SOCK_PROTO_TLS1, BASE_SSL_SOCK_PROTO_SSL3, 
		    BASE_TLS_RSA_WITH_DES_CBC_SHA, BASE_TLS_RSA_WITH_DES_CBC_SHA,
		    BASE_FALSE, BASE_FALSE);
    if (ret == 0)
	return BASE_EBUG;

    BASE_INFO("..echo test w/ incompatible ciphers");
    ret = echo_test(BASE_SSL_SOCK_PROTO_DEFAULT, BASE_SSL_SOCK_PROTO_DEFAULT, 
		    BASE_TLS_RSA_WITH_DES_CBC_SHA, BASE_TLS_RSA_WITH_AES_256_CBC_SHA,
		    BASE_FALSE, BASE_FALSE);
    if (ret == 0)
	return BASE_EBUG;

    BASE_INFO("..echo test w/ client cert required but not provided");
    ret = echo_test(BASE_SSL_SOCK_PROTO_DEFAULT, BASE_SSL_SOCK_PROTO_DEFAULT, 
		    BASE_TLS_RSA_WITH_AES_256_CBC_SHA, BASE_TLS_RSA_WITH_AES_256_CBC_SHA,
		    BASE_TRUE, BASE_FALSE);
    if (ret == 0)
	return BASE_EBUG;

    BASE_INFO("..echo test w/ client cert required and provided");
    ret = echo_test(BASE_SSL_SOCK_PROTO_DEFAULT, BASE_SSL_SOCK_PROTO_DEFAULT, 
		    BASE_TLS_RSA_WITH_AES_256_CBC_SHA, BASE_TLS_RSA_WITH_AES_256_CBC_SHA,
		    BASE_TRUE, BASE_TRUE);
    if (ret != 0)
	return ret;

    BASE_INFO("..performance test");
    ret = perf_test(BASE_IOQUEUE_MAX_HANDLES/2 - 1, 0);
    if (ret != 0)
	return ret;

    BASE_INFO("..client non-SSL (handshake timeout 5 secs)");
    ret = client_non_ssl(5000);
    /* BASE_TIMEDOUT won't be returned as accepted socket is deleted silently */
    if (ret != 0)
	return ret;

#endif

    BASE_INFO("..server non-SSL (handshake timeout 5 secs)");
    ret = server_non_ssl(5000);
    if (ret != BASE_ETIMEDOUT)
	return ret;

    return 0;
}

#else
/* To prevent warning about "translation unit is empty" when this test is disabled. 
 */
int dummy_ssl_sock_test;
#endif

