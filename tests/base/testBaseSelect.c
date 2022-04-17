/* 
 *
 */
#include "testBaseTest.h"

/**
 * \page page_baselib_select_test Test: Socket Select()
 *
 * This file provides implementation of \b select_test(). It tests the
 * functionality of the bsock_select() API.
 *
 */


#if INCLUDE_SELECT_TEST

#include <baseSock.h>
#include <baseSockSelect.h>
#include <baseLog.h>
#include <baseString.h>
#include <baseAssert.h>
#include <baseOs.h>
#include <baseErrno.h>

enum
{
	READ_FDS,
	WRITE_FDS,
	EXCEPT_FDS
};

#define UDP_PORT    51232

/*
 * do_select()
 *
 * Perform bsock_select() and find out which sockets
 * are signalled.
 */    
static int do_select( bsock_t sock1, bsock_t sock2, int setcount[])
{
    bfd_set_t fds[3];
    btime_val timeout;
    int i, n;
    
    for (i=0; i<3; ++i) {
        BASE_FD_ZERO(&fds[i]);
        BASE_FD_SET(sock1, &fds[i]);
        BASE_FD_SET(sock2, &fds[i]);
        setcount[i] = 0;
    }

    timeout.sec = 1;
    timeout.msec = 0;

    n = bsock_select(BASE_IOQUEUE_MAX_HANDLES, &fds[0], &fds[1], &fds[2], &timeout);
    if (n < 0)
        return n;
    if (n == 0)
        return 0;

    for (i=0; i<3; ++i) {
        if (BASE_FD_ISSET(sock1, &fds[i]))
            setcount[i]++;
        if (BASE_FD_ISSET(sock2, &fds[i]))
	    setcount[i]++;
    }

    return n;
}

/*
 * select_test()
 *
 * Test main entry.
 */
int select_test()
{
    bsock_t udp1=BASE_INVALID_SOCKET, udp2=BASE_INVALID_SOCKET;
    bsockaddr_in udp_addr;
    int status;
    int setcount[3];
    bstr_t s;
    const char data[] = "hello";
    const int datalen = 5;
    bssize_t sent, received;
    char buf[10];
    bstatus_t rc;

    BASE_INFO("...Testing simple UDP select()");
    
    // Create two UDP sockets.
    rc = bsock_socket( bAF_INET(), bSOCK_DGRAM(), 0, &udp1);
    if (rc != BASE_SUCCESS) {
        app_perror("...error: unable to create socket", rc);
	status=-10; goto on_return;
    }
    rc = bsock_socket( bAF_INET(), bSOCK_DGRAM(), 0, &udp2);
    if (udp2 == BASE_INVALID_SOCKET) {
        app_perror("...error: unable to create socket", rc);
	status=-20; goto on_return;
    }

    // Bind one of the UDP socket.
    bbzero(&udp_addr, sizeof(udp_addr));
    udp_addr.sin_family = bAF_INET();
    udp_addr.sin_port = UDP_PORT;
    udp_addr.sin_addr = binet_addr(bcstr(&s, "127.0.0.1"));

    if (bsock_bind(udp2, &udp_addr, sizeof(udp_addr))) {
	status=-30; goto on_return;
    }

    // Send data.
    sent = datalen;
    rc = bsock_sendto(udp1, data, &sent, 0, &udp_addr, sizeof(udp_addr));
    if (rc != BASE_SUCCESS || sent != datalen) {
        app_perror("...error: sendto() error", rc);
	status=-40; goto on_return;
    }

    // Sleep a bit. See http://trac.extsip.org/repos/ticket/890
    bthreadSleepMs(10);

    // Check that socket is marked as reable.
    // Note that select() may also report that sockets are writable.
    status = do_select(udp1, udp2, setcount);
    if (status < 0) {
	char errbuf[128];
        extStrError(bget_netos_error(), errbuf, sizeof(errbuf));
	BASE_CRIT("...error: %s", errbuf);
	status=-50; goto on_return;
    }
    if (status == 0) {
	status=-60; goto on_return;
    }

    if (setcount[READ_FDS] != 1) {
	status=-70; goto on_return;
    }
    if (setcount[WRITE_FDS] != 0) {
	if (setcount[WRITE_FDS] == 2) {
	    BASE_INFO("...info: system reports writable sockets");
	} else {
	    status=-80; goto on_return;
	}
    } else {
	BASE_INFO("...info: system doesn't report writable sockets");
    }
    if (setcount[EXCEPT_FDS] != 0) {
	status=-90; goto on_return;
    }

    // Read the socket to clear readable sockets.
    received = sizeof(buf);
    rc = bsock_recv(udp2, buf, &received, 0);
    if (rc != BASE_SUCCESS || received != 5) {
	status=-100; goto on_return;
    }
    
    status = 0;

    // Test timeout on the read part.
    // This won't necessarily return zero, as select() may report that
    // sockets are writable.
    setcount[0] = setcount[1] = setcount[2] = 0;
    status = do_select(udp1, udp2, setcount);
    if (status != 0 && status != setcount[WRITE_FDS]) {
	BASE_ERROR("...error: expecting timeout but got %d sks set", status);
	BASE_ERROR("          rdset: %d, wrset: %d, exset: %d", setcount[0], setcount[1], setcount[2]);
	status = -110; goto on_return;
    }
    if (setcount[READ_FDS] != 0) {
	BASE_ERROR("...error: readable socket not expected");
	status = -120; goto on_return;
    }

    status = 0;

on_return:
    if (udp1 != BASE_INVALID_SOCKET)
	bsock_close(udp1);
    if (udp2 != BASE_INVALID_SOCKET)
	bsock_close(udp2);
    return status;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_select_test;
#endif	/* INCLUDE_SELECT_TEST */


