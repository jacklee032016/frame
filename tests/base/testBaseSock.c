/* 
 *
 */
#include <libBase.h>
#include "testBaseTest.h"


/**
 * \page page_baselib_sock_test Test: Socket
 *
 * This file provides implementation of \b sock_test(). It tests the
 * various aspects of the socket API.
 *
 * \section sock_test_scope_sec Scope of the Test
 *
 * The scope of the test:
 *  - verify the validity of the address structs.
 *  - verify that address manipulation API works.
 *  - simple socket creation and destruction.
 *  - simple socket send/recv and sendto/recvfrom.
 *  - UDP connect()
 *  - send/recv big data.
 *  - all for both UDP and TCP.
 *
 * The APIs tested in this test:
 *  - binet_aton()
 *  - binet_ntoa()
 *  - binet_pton()  (only if IPv6 is enabled)
 *  - binet_ntop()  (only if IPv6 is enabled)
 *  - bgethostname()
 *  - bsock_socket()
 *  - bsock_close()
 *  - bsock_send()
 *  - bsock_sendto()
 *  - bsock_recv()
 *  - bsock_recvfrom()
 *  - bsock_bind()
 *  - bsock_connect()
 *  - bsock_listen()
 *  - bsock_accept()
 *  - bgethostbyname()
 *
 */

#if INCLUDE_SOCK_TEST

#define UDP_PORT	51234
#define TCP_PORT        (UDP_PORT+10)
#define BIG_DATA_LEN	8192
#define ADDRESS		"127.0.0.1"

static char bigdata[BIG_DATA_LEN];
static char bigbuffer[BIG_DATA_LEN];

/* Macro for checking the value of "sin_len" member of sockaddr
 * (it must always be zero).
 */
#if defined(BASE_SOCKADDR_HAS_LEN) && BASE_SOCKADDR_HAS_LEN!=0
#   define CHECK_SA_ZERO_LEN(addr, ret) \
	if (((baddr_hdr*)(addr))->sa_zero_len != 0) \
	    return ret
#else
#   define CHECK_SA_ZERO_LEN(addr, ret)
#endif


static int format_test(void)
{
	bstr_t s = bstr(ADDRESS);
	unsigned char *p;
	bin_addr addr;
	char zero[64];
	bsockaddr_in addr2;
	const bstr_t *hostname;
	const unsigned char A[] = {127, 0, 0, 1};

	BASE_INFO(TEST_LEVEL_CASE"format_test()");

	/* binet_aton() */
	if (binet_aton(&s, &addr) != 1)
		return -10;

	/* Check the result. */
	p = (unsigned char*)&addr;
	if (p[0]!=A[0] || p[1]!=A[1] || p[2]!=A[2] || p[3]!=A[3])
	{
		BASE_ERROR("  error: mismatched address. p0=%d, p1=%d, p2=%d, p3=%d", 
			p[0] & 0xFF, p[1] & 0xFF, p[2] & 0xFF, p[3] & 0xFF);
		return -15;
	}

	/* binet_ntoa() */
	p = (unsigned char*) binet_ntoa(addr);
	if (!p)
		return -20;

	if (bstrcmp2(&s, (char*)p) != 0)
		return -22;

#if defined(BASE_HAS_IPV6) && BASE_HAS_IPV6!=0
	/* binet_pton() */
	/* binet_ntop() */
	{
		const bstr_t s_ipv4 = bstr("127.0.0.1");
		const bstr_t s_ipv6 = bstr("fe80::2ff:83ff:fe7c:8b42");
		char buf_ipv4[BASE_INET_ADDRSTRLEN];
		char buf_ipv6[BASE_INET6_ADDRSTRLEN];
		bin_addr ipv4;
		bin6_addr ipv6;

		if (binet_pton(bAF_INET(), &s_ipv4, &ipv4) != BASE_SUCCESS)
			return -24;

		p = (unsigned char*)&ipv4;
		if (p[0]!=A[0] || p[1]!=A[1] || p[2]!=A[2] || p[3]!=A[3]) {
			return -25;
		}

		if (binet_pton(bAF_INET6(), &s_ipv6, &ipv6) != BASE_SUCCESS)
			return -26;

		p = (unsigned char*)&ipv6;
		if (p[0] != 0xfe || p[1] != 0x80 || p[2] != 0 || p[3] != 0 ||
		p[4] != 0 || p[5] != 0 || p[6] != 0 || p[7] != 0 ||
		p[8] != 0x02 || p[9] != 0xff || p[10] != 0x83 || p[11] != 0xff ||
		p[12]!=0xfe || p[13]!=0x7c || p[14] != 0x8b || p[15]!=0x42)
		{
			return -27;
		}

		if (binet_ntop(bAF_INET(), &ipv4, buf_ipv4, sizeof(buf_ipv4)) != BASE_SUCCESS)
			return -28;
		if (bstricmp2(&s_ipv4, buf_ipv4) != 0)
			return -29;

		if (binet_ntop(bAF_INET6(), &ipv6, buf_ipv6, sizeof(buf_ipv6)) != BASE_SUCCESS)
			return -30;
		if (bstricmp2(&s_ipv6, buf_ipv6) != 0)
			return -31;
	}

#endif	/* BASE_HAS_IPV6 */

	/* Test that bsockaddr_in_init() initialize the whole structure, including sin_zero.
	*/
	bsockaddr_in_init(&addr2, 0, 1000);
	bbzero(zero, sizeof(zero));
	if (bmemcmp(addr2.sin_zero, zero, sizeof(addr2.sin_zero)) != 0)
		return -35;

	/* bgethostname() */
	hostname = bgethostname();
	if (!hostname || !hostname->ptr || !hostname->slen)
		return -40;

	BASE_INFO(TEST_LEVEL_RESULT"hostname is '%.*s'", (int)hostname->slen, hostname->ptr);

	/* bgethostaddr() */

	/* Various constants */
#if !defined(BASE_SYMBIAN) || BASE_SYMBIAN==0
	if (bAF_INET()==0xFFFF) return -5500;
	if (bAF_INET6()==0xFFFF) return -5501;

	/* 0xFFFF could be a valid SOL_SOCKET (e.g: on some Win or Mac) */
	//if (BASE_SOL_SOCKET==0xFFFF) return -5503;

	if (bSOL_IP()==0xFFFF) return -5502;
	if (bSOL_TCP()==0xFFFF) return -5510;
	if (bSOL_UDP()==0xFFFF) return -5520;
	if (bSOL_IPV6()==0xFFFF) return -5530;

	if (bSO_TYPE()==0xFFFF) return -5540;
	if (bSO_RCVBUF()==0xFFFF) return -5550;
	if (bSO_SNDBUF()==0xFFFF) return -5560;
	if (bTCP_NODELAY()==0xFFFF) return -5570;
	if (bSO_REUSEADDR()==0xFFFF) return -5580;

	if (bMSG_OOB()==0xFFFF) return -5590;
	if (bMSG_PEEK()==0xFFFF) return -5600;
#endif

	return 0;
}

static int parse_test(void)
{
#define IPv4	1
#define IPv6	2

	struct test_t {
		const char  *input;
		int	     result_af;
		const char  *result_ip;
		buint16_t  result_port;
	};

	struct test_t valid_tests[] = 
	{
		/* IPv4 */
		{ "10.0.0.1:80", IPv4, "10.0.0.1", 80},
		{ "10.0.0.1", IPv4, "10.0.0.1", 0},
		{ "10.0.0.1:", IPv4, "10.0.0.1", 0},
		{ "10.0.0.1:0", IPv4, "10.0.0.1", 0},
		{ ":80", IPv4, "0.0.0.0", 80},
		{ ":", IPv4, "0.0.0.0", 0},
#if !BASE_SYMBIAN
		{ "localhost", IPv4, "127.0.0.1", 0},
		{ "localhost:", IPv4, "127.0.0.1", 0},
		{ "localhost:80", IPv4, "127.0.0.1", 80},
#endif

#if defined(BASE_HAS_IPV6) && BASE_HAS_IPV6
		{ "fe::01:80", IPv6, "fe::01:80", 0},
		{ "[fe::01]:80", IPv6, "fe::01", 80},
		{ "fe::01", IPv6, "fe::01", 0},
		{ "[fe::01]", IPv6, "fe::01", 0},
		{ "fe::01:", IPv6, "fe::01", 0},
		{ "[fe::01]:", IPv6, "fe::01", 0},
		{ "::", IPv6, "::0", 0},
		{ "[::]", IPv6, "::", 0},
		{ ":::", IPv6, "::", 0},
		{ "[::]:", IPv6, "::", 0},
		{ ":::80", IPv6, "::", 80},
		{ "[::]:80", IPv6, "::", 80},
#endif
	};
	
	struct test_t invalid_tests[] = 
	{
		/* IPv4 */
		{ "10.0.0.1:abcd", IPv4},   /* port not numeric */
		{ "10.0.0.1:-1", IPv4},	    /* port contains illegal character */
		{ "10.0.0.1:123456", IPv4}, /* port too big	*/
		//this actually is fine on my Mac OS 10.9
		//it will be resolved with gethostbyname() and something is returned!
		//{ "1.2.3.4.5:80", IPv4},    /* invalid IP */
		{ "10:0:80", IPv4},	    /* hostname has colon */

#if defined(BASE_HAS_IPV6) && BASE_HAS_IPV6
		{ "[fe::01]:abcd", IPv6},   /* port not numeric */
		{ "[fe::01]:-1", IPv6},	    /* port contains illegal character */
		{ "[fe::01]:123456", IPv6}, /* port too big	*/
		{ "fe::01:02::03:04:80", IPv6},	    /* invalid IP */
		{ "[fe::01:02::03:04]:80", IPv6},   /* invalid IP */
		{ "[fe:01", IPv6},	    /* Unterminated bracket */
#endif
	};

	unsigned i;

	BASE_INFO(TEST_LEVEL_CASE"IP address parsing");

	for (i=0; i<BASE_ARRAY_SIZE(valid_tests); ++i)
	{
		bstatus_t status;
		bstr_t input;
		bsockaddr addr, result;

		switch (valid_tests[i].result_af)
		{
			case IPv4:
				valid_tests[i].result_af = bAF_INET();
				break;

			case IPv6:
				valid_tests[i].result_af = bAF_INET6();
				break;
			
			default:
				bassert(!"Invalid AF!");
				continue;
		}

		/* Try parsing with BASE_AF_UNSPEC */
		status = bsockaddr_parse(bAF_UNSPEC(), 0, bcstr(&input, valid_tests[i].input), &addr);
		if (status != BASE_SUCCESS)
		{
			BASE_CRIT(".... failed when parsing %s (i=%d)", valid_tests[i].input, i);
			return -10;
		}

		/* Check "sin_len" member of parse result */
		CHECK_SA_ZERO_LEN(&addr, -20);

		/* Build the correct result */
		status = bsockaddr_init(valid_tests[i].result_af, &result, bcstr(&input, valid_tests[i].result_ip), valid_tests[i].result_port);
		if (status != BASE_SUCCESS)
		{
			BASE_CRIT(".... error building IP address %s", valid_tests[i].input);
			return -30;
		}

		/* Compare the result */
		if (bsockaddr_cmp(&addr, &result) != 0)
		{
			BASE_CRIT(".... parsed result mismatched for %s", valid_tests[i].input);
			return -40;
		}

		/* Parse again with the specified af */
		status = bsockaddr_parse(valid_tests[i].result_af, 0, bcstr(&input, valid_tests[i].input), &addr);
		if (status != BASE_SUCCESS)
		{
			BASE_CRIT(".... failed when parsing %s", valid_tests[i].input);
			return -50;
		}

		/* Check "sin_len" member of parse result */
		CHECK_SA_ZERO_LEN(&addr, -55);

		/* Compare the result again */
		if (bsockaddr_cmp(&addr, &result) != 0)
		{
			BASE_CRIT(".... parsed result mismatched for %s", valid_tests[i].input);
			return -60;
		}
	}

	for (i=0; i<BASE_ARRAY_SIZE(invalid_tests); ++i)
	{
		bstatus_t status;
		bstr_t input;
		bsockaddr addr;

		switch (invalid_tests[i].result_af)
		{
			case IPv4:
				invalid_tests[i].result_af = bAF_INET();
				break;
			case IPv6:
				invalid_tests[i].result_af = bAF_INET6();
				break;
			default:
				bassert(!"Invalid AF!");
				continue;
		}

		/* Try parsing with BASE_AF_UNSPEC */
		status = bsockaddr_parse(bAF_UNSPEC(), 0, bcstr(&input, invalid_tests[i].input), &addr);
		if (status == BASE_SUCCESS)
		{
			BASE_CRIT(".... expecting failure when parsing %s", invalid_tests[i].input);
			return -100;
		}
	}

	return 0;
}

static int purity_test(void)
{
	BASE_INFO("...purity_test()");

#if defined(BASE_SOCKADDR_HAS_LEN) && BASE_SOCKADDR_HAS_LEN!=0
	/* Check on "sin_len" member of sockaddr */
	{
		const bstr_t str_ip = {"1.1.1.1", 7};
		bsockaddr addr[16];
		baddrinfo ai[16];
		unsigned cnt;
		bstatus_t rc;

TRACE();
		/* benum_ip_interface() */
		cnt = BASE_ARRAY_SIZE(addr);
		rc = benum_ip_interface(bAF_UNSPEC(), &cnt, addr);
		if (rc == BASE_SUCCESS)
		{
			while (cnt--)
				CHECK_SA_ZERO_LEN(&addr[cnt], -10);
		}

		/* bgethostip() on IPv4 */
		rc = bgethostip(bAF_INET(), &addr[0]);
		if (rc == BASE_SUCCESS)
			CHECK_SA_ZERO_LEN(&addr[0], -20);

		/* bgethostip() on IPv6 */
		rc = bgethostip(bAF_INET6(), &addr[0]);
		if (rc == BASE_SUCCESS)
			CHECK_SA_ZERO_LEN(&addr[0], -30);

		/* bgetdefaultipinterface() on IPv4 */
		rc = bgetdefaultipinterface(bAF_INET(), &addr[0]);
		if (rc == BASE_SUCCESS)
			CHECK_SA_ZERO_LEN(&addr[0], -40);

		/* bgetdefaultipinterface() on IPv6 */
		rc = bgetdefaultipinterface(bAF_INET6(), &addr[0]);
		if (rc == BASE_SUCCESS)
			CHECK_SA_ZERO_LEN(&addr[0], -50);

		/* bgetaddrinfo() on a host name */
		cnt = BASE_ARRAY_SIZE(ai);
		rc = bgetaddrinfo(bAF_UNSPEC(), bgethostname(), &cnt, ai);
		if (rc == BASE_SUCCESS) {
			while (cnt--)
			CHECK_SA_ZERO_LEN(&ai[cnt].ai_addr, -60);
		}

		/* bgetaddrinfo() on an IP address */
		cnt = BASE_ARRAY_SIZE(ai);
		rc = bgetaddrinfo(bAF_UNSPEC(), &str_ip, &cnt, ai);
		if (rc == BASE_SUCCESS) {
			while (cnt--)
				CHECK_SA_ZERO_LEN(&ai[cnt].ai_addr, -70);
		}
	}
#endif

	return 0;
}

static int simple_sock_test(void)
{
	int types[2];
	bsock_t sock;
	int i;
	bstatus_t rc = BASE_SUCCESS;

	types[0] = bSOCK_STREAM();
	types[1] = bSOCK_DGRAM();

	BASE_INFO("...simple_sock_test()");

	for (i=0; i<(int)(sizeof(types)/sizeof(types[0])); ++i)
	{
		rc = bsock_socket(bAF_INET(), types[i], 0, &sock);
		if (rc != BASE_SUCCESS)
		{
			app_perror("...error: unable to create socket", rc);
			break;
		}
		else
		{
			rc = bsock_close(sock);
			if (rc != 0)
			{
				app_perror("...error: close socket", rc);
				break;
			}
		}
	}
	return rc;
}


static int send_recv_test(int sock_type, bsock_t ss, bsock_t cs,
	bsockaddr_in *dstaddr, bsockaddr_in *srcaddr, int addrlen)
{
	enum { DATA_LEN = 16 };
	char senddata[DATA_LEN+4], recvdata[DATA_LEN+4];
	bssize_t sent, received, total_received;
	bstatus_t rc;

	MTRACE( "....create_random_string()");
	bcreate_random_string(senddata, DATA_LEN);
	senddata[DATA_LEN-1] = '\0';

	/*
	* Test send/recv small data.
	*/
	MTRACE( "....sendto()");
	if (dstaddr)
	{
		sent = DATA_LEN;
		rc = bsock_sendto(cs, senddata, &sent, 0, dstaddr, addrlen);
		if (rc != BASE_SUCCESS || sent != DATA_LEN)
		{
			app_perror("...sendto error", rc);
			rc = -140; goto on_error;
		}
	}
	else
	{
		sent = DATA_LEN;
		rc = bsock_send(cs, senddata, &sent, 0);
		if (rc != BASE_SUCCESS || sent != DATA_LEN)
		{
			app_perror("...send error", rc);
			rc = -145; goto on_error;
		}
	}

	MTRACE( "....recv()");
	if (srcaddr)
	{
		bsockaddr_in addr;
		int srclen = sizeof(addr);

		bbzero(&addr, sizeof(addr));

		received = DATA_LEN;
		rc = bsock_recvfrom(ss, recvdata, &received, 0, &addr, &srclen);
		if (rc != BASE_SUCCESS || received != DATA_LEN)
		{
			app_perror("...recvfrom error", rc);
			rc = -150; goto on_error;
		}
		
		if (srclen != addrlen)
			return -151;
		
		if (bsockaddr_cmp(&addr, srcaddr) != 0)
		{
			char srcaddr_str[32], addr_str[32];
			strcpy(srcaddr_str, binet_ntoa(srcaddr->sin_addr));
			strcpy(addr_str, binet_ntoa(addr.sin_addr));
			BASE_ERROR( "...error: src address mismatch (original=%s, recvfrom addr=%s)", srcaddr_str, addr_str);
			return -152;
		}

	}
	else
	{
		/* Repeat recv() until all data is received.
		* This applies only for non-UDP of course, since for UDP
		* we would expect all data to be received in one packet.
		*/
		total_received = 0;
		do
		{
			received = DATA_LEN-total_received;
			rc = bsock_recv(ss, recvdata+total_received, &received, 0);
			if (rc != BASE_SUCCESS) {
				app_perror("...recv error", rc);
				rc = -155; goto on_error;
			}
			
			if (received <= 0) {
				BASE_ERROR("...error: socket has closed! (received=%d)", received);
				rc = -156; goto on_error;
			}
			
			if (received != DATA_LEN-total_received)
			{
				if (sock_type != bSOCK_STREAM())
				{
					BASE_ERROR( "...error: expecting %u bytes, got %u bytes", DATA_LEN-total_received, received);
					rc = -157; goto on_error;
				}
			}
			total_received += received;
		} while (total_received < DATA_LEN);
	}

	MTRACE( "....memcmp()");
	if (bmemcmp(senddata, recvdata, DATA_LEN) != 0)
	{
		BASE_ERROR("...error: received data mismatch (got:'%s' expecting:'%s'",  recvdata, senddata);
		rc = -160; goto on_error;
	}

	/*
	* Test send/recv big data.
	*/
	MTRACE( "....sendto()");
	if (dstaddr)
	{
		sent = BIG_DATA_LEN;
		rc = bsock_sendto(cs, bigdata, &sent, 0, dstaddr, addrlen);
		if (rc != BASE_SUCCESS || sent != BIG_DATA_LEN)
		{
			app_perror("...sendto error", rc);
			rc = -161; goto on_error;
		}
	}
	else
	{
		sent = BIG_DATA_LEN;
		rc = bsock_send(cs, bigdata, &sent, 0);
		if (rc != BASE_SUCCESS || sent != BIG_DATA_LEN)
		{
			app_perror("...send error", rc);
			rc = -165; goto on_error;
		}
	}

	MTRACE("....recv()");

	/* Repeat recv() until all data is received.
	* This applies only for non-UDP of course, since for UDP
	* we would expect all data to be received in one packet.
	*/
	total_received = 0;
	do 
	{
		received = BIG_DATA_LEN-total_received;
		rc = bsock_recv(ss, bigbuffer+total_received, &received, 0);
		if (rc != BASE_SUCCESS)
		{
			app_perror("...recv error", rc);
			rc = -170; goto on_error;
		}
		
		if (received <= 0) {
			BASE_ERROR( "...error: socket has closed! (received=%d)", received);
			rc = -173; goto on_error;
		}
		
		if (received != BIG_DATA_LEN-total_received)
		{
			if (sock_type != bSOCK_STREAM())
			{
				BASE_ERROR("...error: expecting %u bytes, got %u bytes", BIG_DATA_LEN-total_received, received);
				rc = -176; goto on_error;
			}
		}
		total_received += received;
	} while (total_received < BIG_DATA_LEN);

	MTRACE("....memcmp()");
	if (bmemcmp(bigdata, bigbuffer, BIG_DATA_LEN) != 0)
	{
		BASE_INFO("...error: received data has been altered!");
		rc = -180; goto on_error;
	}

	rc = 0;

on_error:
	return rc;
}

static int udp_test(void)
{
    bsock_t cs = BASE_INVALID_SOCKET, ss = BASE_INVALID_SOCKET;
    bsockaddr_in dstaddr, srcaddr;
    bstr_t s;
    bstatus_t rc = 0, retval;

    BASE_INFO("...udp_test()");

    rc = bsock_socket(bAF_INET(), bSOCK_DGRAM(), 0, &ss);
    if (rc != 0) {
	app_perror("...error: unable to create socket", rc);
	return -100;
    }

    rc = bsock_socket(bAF_INET(), bSOCK_DGRAM(), 0, &cs);
    if (rc != 0)
	return -110;

    /* Bind server socket. */
    bbzero(&dstaddr, sizeof(dstaddr));
    dstaddr.sin_family = bAF_INET();
    dstaddr.sin_port = bhtons(UDP_PORT);
    dstaddr.sin_addr = binet_addr(bcstr(&s, ADDRESS));
    
    if ((rc=bsock_bind(ss, &dstaddr, sizeof(dstaddr))) != 0) {
	app_perror("...bind error udp:"ADDRESS, rc);
	rc = -120; goto on_error;
    }

    /* Bind client socket. */
    bbzero(&srcaddr, sizeof(srcaddr));
    srcaddr.sin_family = bAF_INET();
    srcaddr.sin_port = bhtons(UDP_PORT-1);
    srcaddr.sin_addr = binet_addr(bcstr(&s, ADDRESS));

    if ((rc=bsock_bind(cs, &srcaddr, sizeof(srcaddr))) != 0) {
	app_perror("...bind error", rc);
	rc = -121; goto on_error;
    }
	    
    /* Test send/recv, with sendto */
    rc = send_recv_test(bSOCK_DGRAM(), ss, cs, &dstaddr, NULL, 
                        sizeof(dstaddr));
    if (rc != 0)
	goto on_error;

    /* Test send/recv, with sendto and recvfrom */
    rc = send_recv_test(bSOCK_DGRAM(), ss, cs, &dstaddr, 
                        &srcaddr, sizeof(dstaddr));
    if (rc != 0)
	goto on_error;

    /* Disable this test on Symbian since UDP connect()/send() failed
     * with S60 3rd edition (including MR2).
     * See http://www.extsip.org/trac/ticket/264
     */    
#if !defined(BASE_SYMBIAN) || BASE_SYMBIAN==0
    /* connect() the sockets. */
    rc = bsock_connect(cs, &dstaddr, sizeof(dstaddr));
    if (rc != 0) {
	app_perror("...connect() error", rc);
	rc = -122; goto on_error;
    }

    /* Test send/recv with send() */
    rc = send_recv_test(bSOCK_DGRAM(), ss, cs, NULL, NULL, 0);
    if (rc != 0)
	goto on_error;

    /* Test send/recv with send() and recvfrom */
    rc = send_recv_test(bSOCK_DGRAM(), ss, cs, NULL, &srcaddr, 
                        sizeof(srcaddr));
    if (rc != 0)
	goto on_error;
#endif

on_error:
    retval = rc;
    if (cs != BASE_INVALID_SOCKET) {
        rc = bsock_close(cs);
        if (rc != BASE_SUCCESS) {
            app_perror("...error in closing socket", rc);
            return -1000;
        }
    }
    if (ss != BASE_INVALID_SOCKET) {
        rc = bsock_close(ss);
        if (rc != BASE_SUCCESS) {
            app_perror("...error in closing socket", rc);
            return -1010;
        }
    }

    return retval;
}

static int tcp_test(void)
{
	bsock_t cs, ss;
	bstatus_t rc = 0, retval;

	BASE_INFO("...tcp_test()");

	rc = app_socketpair(bAF_INET(), bSOCK_STREAM(), 0, &ss, &cs);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...error: app_socketpair():", rc);
		return -2000;
	}

	/* Test send/recv with send() and recv() */
	retval = send_recv_test(bSOCK_STREAM(), ss, cs, NULL, NULL, 0);
	rc = bsock_close(cs);
	if (rc != BASE_SUCCESS) {
		app_perror("...error in closing socket", rc);
		return -2000;
	}

	rc = bsock_close(ss);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...error in closing socket", rc);
		return -2010;
	}

	return retval;
}

static int ioctl_test(void)
{
    return 0;
}

static int gethostbyname_test(void)
{
	bstr_t host;
	bhostent he;
	bstatus_t status;

	BASE_INFO(TEST_LEVEL_CASE"gethostbyname_test()");

	/* Testing bgethostbyname() with invalid host */
	host = bstr("an-invalid-host-name");
	status = bgethostbyname(&host, &he);

	/* Must return failure! */
	if (status == BASE_SUCCESS)
		return -20100;

//	BASE_INFO(TEST_LEVEL_RESULT"hostbyname '%.*s'", host.slen, host.ptr);
	return 0;
}

#if 0
#include "../os_symbian.h"
static int connect_test()
{
	RSocketServ rSockServ;
	RSocket rSock;
	TInetAddr inetAddr;
	TRequestStatus reqStatus;
	char buffer[16];
	TPtrC8 data((const TUint8*)buffer, (TInt)sizeof(buffer));
 	int rc;
	
	rc = rSockServ.Connect();
	if (rc != KErrNone)
		return rc;
	
	rc = rSock.Open(rSockServ, KAfInet, KSockDatagram, KProtocolInetUdp);
    	if (rc != KErrNone) 
    	{    		
    		rSockServ.Close();
    		return rc;
    	}
   	
    	inetAddr.Init(KAfInet);
    	inetAddr.Input(_L("127.0.0.1"));
    	inetAddr.SetPort(80);
    	
    	rSock.Connect(inetAddr, reqStatus);
    	User::WaitForRequest(reqStatus);

    	if (reqStatus != KErrNone) {
		rSock.Close();
    		rSockServ.Close();
		return rc;
    	}
    
    	rSock.Send(data, 0, reqStatus);
    	User::WaitForRequest(reqStatus);
    	
    	if (reqStatus!=KErrNone) {
    		rSock.Close();
    		rSockServ.Close();
    		return rc;
    	}
    	
    	rSock.Close();
    	rSockServ.Close();
	return KErrNone;
}
#endif

int testBaseSock(void)
{
	int rc;

	bcreate_random_string(bigdata, BIG_DATA_LEN);

	// Enable this to demonstrate the error witn S60 3rd Edition MR2
#if 0
	rc = connect_test();
	if (rc != 0)
	return rc;
#endif

	rc = format_test();
	if (rc != 0)
		return rc;

	rc = parse_test();
	if (rc != 0)
		return rc;

	rc = purity_test();
	if (rc != 0)
		return rc;

	rc = gethostbyname_test();
	if (rc != 0)
		return rc;

	rc = simple_sock_test();
	if (rc != 0)
		return rc;

	rc = ioctl_test();
	if (rc != 0)
		return rc;

	rc = udp_test();
	if (rc != 0)
		return rc;

	rc = tcp_test();
	if (rc != 0)
		return rc;

	return 0;
}


#else
int dummy_sock_test;
#endif

