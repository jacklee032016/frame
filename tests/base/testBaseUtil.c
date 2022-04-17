/* 
 *
 */
#include "testBaseTest.h"
#include <libBase.h>

void app_perror(const char *msg, bstatus_t rc)
{
	char errbuf[BASE_ERR_MSG_SIZE];

	BASE_CHECK_STACK();

	extStrError(rc, errbuf, sizeof(errbuf));
	BASE_ERROR( "%s: [bstatus_t=%d] %s", msg, rc, errbuf);
}

#define SERVER 0
#define CLIENT 1

bstatus_t app_socket(int family, int type, int proto, int port, bsock_t *ptr_sock)
{
	bsockaddr_in addr;
	bsock_t sock;
	bstatus_t rc;

	rc = bsock_socket(family, type, proto, &sock);
	if (rc != BASE_SUCCESS)
		return rc;

	bbzero(&addr, sizeof(addr));
	addr.sin_family = (buint16_t)family;
	addr.sin_port = (short)(port!=-1 ? bhtons((buint16_t)port) : 0);
	
	rc = bsock_bind(sock, &addr, sizeof(addr));
	if (rc != BASE_SUCCESS)
		return rc;

#if BASE_HAS_TCP
	if (type == bSOCK_STREAM())
	{
		rc = bsock_listen(sock, 5);
		if (rc != BASE_SUCCESS)
			return rc;
	}
#endif

	*ptr_sock = sock;
	return BASE_SUCCESS;
}

bstatus_t app_socketpair(int family, int type, int protocol, bsock_t *serverfd, bsock_t *clientfd)
{
	int i;
	static unsigned short port = 11000;
	bsockaddr_in addr;
	bstr_t s;
	bstatus_t rc = 0;
	bsock_t sock[2];

	/* Create both sockets. */
	for (i=0; i<2; ++i)
	{
		rc = bsock_socket(family, type, protocol, &sock[i]);
		if (rc != BASE_SUCCESS)
		{
			if (i==1)
				bsock_close(sock[0]);
			return rc;
		}
	}

	/* Retry bind */
	bbzero(&addr, sizeof(addr));
	addr.sin_family = bAF_INET();
	for (i=0; i<5; ++i)
	{
		addr.sin_port = bhtons(port++);
		rc = bsock_bind(sock[SERVER], &addr, sizeof(addr));
		if (rc == BASE_SUCCESS)
			break;
	}

	if (rc != BASE_SUCCESS)
		goto on_error;

	/* For TCP, listen the socket. */
#if BASE_HAS_TCP
	if (type == bSOCK_STREAM())
	{
		rc = bsock_listen(sock[SERVER], BASE_SOMAXCONN);
		if (rc != BASE_SUCCESS)
			goto on_error;
	}
#endif

	/* Connect client socket. */
	addr.sin_addr = binet_addr(bcstr(&s, "127.0.0.1"));
	rc = bsock_connect(sock[CLIENT], &addr, sizeof(addr));
	if (rc != BASE_SUCCESS)
		goto on_error;

	/* For TCP, must accept(), and get the new socket. */
#if BASE_HAS_TCP
	if (type == bSOCK_STREAM())
	{
		bsock_t newserver;

		rc = bsock_accept(sock[SERVER], &newserver, NULL, NULL);
		if (rc != BASE_SUCCESS)
			goto on_error;

		/* Replace server socket with new socket. */
		bsock_close(sock[SERVER]);
		sock[SERVER] = newserver;
	}
#endif

	*serverfd = sock[SERVER];
	*clientfd = sock[CLIENT];

	return rc;

on_error:
	for (i=0; i<2; ++i)
		bsock_close(sock[i]);
	return rc;
}

