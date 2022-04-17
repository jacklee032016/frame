
#include <libBase.h>
#include <libUtil.h>
#include "compact.h"
#include <libCmn.h>
#include "xmSdk.h"

#include "xm.h"

enum { BUF_SIZE = 512 };


bstatus_t _sdkSocket(int family, int type, int proto, int port, bsock_t *ptr_sock)
{
	bsockaddr_in addr;
	bsock_t sock;
	bstatus_t rc;

	rc = bsock_socket(family, type, proto, &sock);
	if (rc != BASE_SUCCESS)
	{
		return rc;
	}

	bbzero(&addr, sizeof(addr));
	addr.sin_family = (buint16_t)family;
	addr.sin_port = (short)(port!=-1 ? bhtons((buint16_t)port) : 0);

	rc = bsock_bind(sock, &addr, sizeof(addr));
	if (rc != BASE_SUCCESS)
	{
		return rc;
	}

#if BASE_HAS_TCP
	if (type == bSOCK_STREAM())
	{
		rc = bsock_listen(sock, 5);
		if (rc != BASE_SUCCESS)
		{
			return rc;
		}
	}
#endif

	*ptr_sock = sock;
	return BASE_SUCCESS;
}


static int _waitSocket(bsock_t sock, unsigned msec_timeout)
{
    bfd_set_t fdset;
    btime_val timeout;

    timeout.sec = 0;
    timeout.msec = msec_timeout;
    btime_val_normalize(&timeout);

    BASE_FD_ZERO(&fdset);
    BASE_FD_SET(sock, &fdset);
    
    return bsock_select(FD_SETSIZE, &fdset, NULL, NULL, &timeout);
}

static batomic_t *_totalBytes;
static batomic_t *_timeoutCounter;
static batomic_t *_invalidCounter;

#if 0
static cJSON *_dataConnInput(struct DATA_CONN *dataConn, void *buf, int size)
{
	cJSON *cmdObjs = cJSON_Parse((char *)buf );
	
	if( cmdObjs == NULL)
	{/*Wrong JSON format */
		DATA_CONN_ERR(dataConn, IPCMD_ERR_JSON_CORRUPTED,"JSON of command is wrong:received JSON: '%s'", (char *)buf) ;
		return NULL;
	}

	return cmdObjs;
}
#endif

char *authId = "{ \"EncryptType\" : \"NONE\", " \
		"\"LoginType\" : \"DVRIP-Web\", "\
		"\"PassWord\" : \"\",  " \
		"\"UserName\" : \"admin\"  }" ;

int	constructClientRequest(void *buf, unsigned int size)
{
	int dataLength = strlen(authId);

	memset(buf, 0, size);

	XmControlHeader *xmHeader = (XmControlHeader *)buf;
//	memset(xmHeader, 0, sizeof(XmControlHeader));
	
	xmHeader->flags = 0xFF;
	xmHeader->version = 1;
#if 0
	xmHeader->msgId = bhtons(XM_CMD_LOGIN_REQ2);
	xmHeader->dataLength = bhtonl(dataLength);
#else
	xmHeader->msgId = XM_CMD_LOGIN_REQ2;
	xmHeader->dataLength = dataLength;
#endif
	memcpy( (unsigned char *)buf+sizeof(XmControlHeader), (unsigned char  *)authId, dataLength );
	
	return sizeof(XmControlHeader) + dataLength ;
}
/*
void user()
{
		msg = cJSON_PrintUnformatted(responseObj);
	//	EXT_DEBUGF(MUX_DEBUG_BROKER, "CONN %s output msg:'%s'", dataConn->name, msg);
		if(msg == NULL)
		{
			msg = cmn_malloc(128);
			snprintf(msg, 128, "%s", "Unknown");
		}

}
{
"UserName" :"admin",
"PassWord" : "6QNMIQGe",
"EncryptType":"NONE",
"DeviceType":"DVR",
"DeviceID":123456,
"ChannelNum":16
}
*/


int socktClientSearch(void)
{
	bsock_t clientSock = BASE_INVALID_SOCKET;
	bsock_t socket = BASE_INVALID_SOCKET;
	bsockaddr_in deviceAddr, srvAddr;
	bstr_t s;
	char senddata[BUF_SIZE], recvdata[BUF_SIZE];
	bssize_t sent, received, total_received;
	bstatus_t rc = 0, retval;
	bssize_t bytes;
	int count = 0;

	BASE_INFO("udp boardcast...");

	rc = bsock_socket(bAF_INET(), bSOCK_DGRAM(), 0, &socket);
	if (rc != 0)
	{
		sdkPerror("Unable to create service socket", rc);
		return -100;
	}

#if 1
	rc = bsock_socket(bAF_INET(), bSOCK_DGRAM(), 0, &clientSock);
	if (rc != 0)
	{
		sdkPerror("Unable to create client socket", rc);
		return -110;
	}
#endif

#if 0
	struct timeval timeout; 	 
	timeout.tv_sec = timeoutSecond;
	timeout.tv_usec = 0;

	if (bsock_setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		BASE_ERROR("Set socket REV timeout error:%s", strerror(errno));
	}
	
	if (bsock_setsockopt (socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
	{
		BASE_ERROR("Set socket SEND timeout error:%s", strerror(errno));
	}
#endif

	//char broadcast = '1';
	int broadcast = 1;
	if ((rc=bsock_setsockopt(socket, bSOL_SOCKET(), BASE_SO_BOARDCAST, &broadcast, sizeof(broadcast) )) != 0)
	{
		sdkPerror("Set socket BOARDCAST error:%s", rc);
	}

	if ((rc=bsock_setsockopt(clientSock, bSOL_SOCKET(), BASE_SO_BOARDCAST, &broadcast, sizeof(broadcast) )) != 0)
	{
		sdkPerror("Set socket BOARDCAST in client socket error:%s", rc);
	}

	bin_addr binAddr;
	binAddr.s_addr = BASE_INADDR_ANY;

	/* Bind server socket: service bind to local address, not broadcast address  */
	bbzero(&srvAddr, sizeof(srvAddr));
	srvAddr.sin_family = bAF_INET();
	srvAddr.sin_port = bhtons(static_cast<buint16_t>(XmServicePort::UDP_BOARDCAST_PORT) );
	srvAddr.sin_addr = binAddr;// binet_addr(bcstr(&s, ADDRESS));
//	srvAddr.sin_addr = binet_addr(bcstr(&s, "192.168.1.255"));
	if ((rc=bsock_bind(socket, &srvAddr, sizeof(srvAddr))) != 0)
	{
		sdkPerror("Bind UDP boardcast error:", rc);
		rc = -120;
		goto on_error;
	}

	BASE_DEBUG("Monitoring on port: %s:%d", binet_ntoa(srvAddr.sin_addr), bhtons(srvAddr.sin_port));
	/* Bind device socket: broadcast address  */
	binAddr.s_addr = BASE_INADDR_BROADCAST;
	bbzero(&deviceAddr, sizeof(deviceAddr));
	deviceAddr.sin_family = bAF_INET();
	deviceAddr.sin_port = bhtons(static_cast<buint16_t>(XmServicePort::UDP_BOARDCAST_PORT) );

	/* in multi-home host, broadcast address must be defined in one network */
#if  0
	deviceAddr.sin_addr = binAddr;// binet_addr(bcstr(&s, ADDRESS));
#else
	deviceAddr.sin_addr = binet_addr(bcstr(&s, "192.168.1.255"));
#endif

#if 0
	if ((rc=bsock_bind(cs, &srcaddr, sizeof(srcaddr))) != 0) {
		app_perror("...bind error", rc);
		rc = -121; goto on_error;
	}
#endif
	/* Test send/recv, with sendto */
	bssize_t length = constructClientRequest(senddata, sizeof(senddata));
	bytes = length;
	sent = length;

	/* send out in random port */
//	rc = bsock_sendto(socket, senddata, &sent, 0, &deviceAddr, sizeof(deviceAddr));
	rc = bsock_sendto(clientSock, senddata, &sent, 0, &deviceAddr, sizeof(deviceAddr));
	if (rc != BASE_SUCCESS || sent != length)
	{
		sdkPerror("sendto error", rc);
		rc = -140;
		goto on_error;
	}

	BASE_DEBUG("send %d byte to %s:%d", sent, binet_ntoa(deviceAddr.sin_addr), bhtons(deviceAddr.sin_port));
	CMN_HEX_DUMP(senddata, sent, "Sent out");
	count =0;
	while(1)
	{
		bsockaddr_in recvAddr;
		int srclen = sizeof(recvAddr);

		bbzero(&recvAddr, sizeof(recvAddr));

		rc = _waitSocket(socket, 500*4);
		if (rc == 0)
		{
			BASE_ERROR("timeout");
			bytes = 0;
			break;
//			batomic_inc(_timeoutCounter);
		}
		else if (rc < 0)
		{
			rc = bget_netos_error();
			sdkPerror("select() error", rc);
			break;
		}

		received = BUF_SIZE;
		/* recv packet from sending out port or service port both. Tested. JL */
		rc = bsock_recvfrom(socket, recvdata, &received, 0, &recvAddr, &srclen);
		if (rc != BASE_SUCCESS || received <= 0)
		{
			sdkPerror("recvfrom error", rc);
			rc = -150;
			goto on_error;
		}

#if 0		
		if (srclen != addrlen)
		{
			return -151;
		}
#endif		
//		if (bsockaddr_cmp(&recvAddr, srcaddr) != 0)
		{
			char srcaddr_str[32], addr_str[32];
			strcpy(addr_str, binet_ntoa(recvAddr.sin_addr));
			BASE_DEBUG( "#%d: Recv %d bytes from %s:%d", ++count, received, addr_str, bhtons(recvAddr.sin_port) );
			CMN_HEX_DUMP(recvdata, received, "Recved");
//			return -152;
		}

	}


on_error:
	retval = rc;
	
	if (socket != BASE_INVALID_SOCKET)
	{
		rc = bsock_close(socket);
		if (rc != BASE_SUCCESS)
		{
			sdkPerror("error in closing socket", rc);
			return -1010;
		}
	}

	if (clientSock != BASE_INVALID_SOCKET)
	{
		rc = bsock_close(clientSock);
		if (rc != BASE_SUCCESS)
		{
			sdkPerror("error in closing socket", rc);
			return -1010;
		}
	}


	return retval;
}


int socketClientRun(SocketClient *client)
{
	bsock_t sock;
	char send_buf[BUF_SIZE];
	char recv_buf[BUF_SIZE];
	bsockaddr_in addr;
	bstr_t s;
	bstatus_t rc;
	buint32_t buffer_id;
	buint32_t buffer_counter;

	bstatus_t last_recv_err = BASE_SUCCESS, last_send_err = BASE_SUCCESS;
	unsigned counter = 0;

	bpool_t *pool;
	int i;
	batomic_value_t last_received;
	btimestamp last_report;

	pool = bpool_create( sdkMemFactory, NULL, 4000, 4000, NULL );

	rc = batomic_create(pool, 0, &_totalBytes);
	if (rc != BASE_SUCCESS)
	{
		BASE_ERROR("...error: unable to create atomic variable", rc);
		return -30;
	}
	rc = batomic_create(pool, 0, &_invalidCounter);
	rc = batomic_create(pool, 0, &_timeoutCounter);
	TRACE();

#if 0
	rc = _sdkSocket(bAF_INET(), client->sockType, 0, -1, &sock);
#else
	int protocol = 0;
	rc = bsock_socket(bAF_INET(), client->sockType, protocol, &sock);
#endif
	if (rc != BASE_SUCCESS)
	{
		sdkPerror("...unable to create socket", rc);
		return -10;
	}



	TRACE();
	rc = bsockaddr_in_init(&addr, bcstr(&s, client->server), (buint16_t)client->port);
//	rc = bsockaddr_init(bAF_INET(),&addr, bcstr(&s, client->server), (buint16_t)client->port);
	if (rc != BASE_SUCCESS)
	{
		sdkPerror("...unable to resolve server", rc);
		return -15;
	}

	/* Connect client socket. */
//	addr.sin_addr = binet_addr(bcstr(&s, "127.0.0.1"));

	TRACE();
	rc = bsock_connect(sock, &addr,  sizeof(addr));//sizeof(struct sockaddr));//
	if (rc != BASE_SUCCESS)
	{
		sdkPerror("...connect() error", rc);
		bsock_close(sock);
		return -20;
	}
	TRACE();

//	BASE_INFO("...socket connected to %s:%d", binet_ntop2(bAF_INET(), &addr.sin_addr, (char *)addr, sizeof(addr)), bntohs(addr.sin_port));
	BASE_DEBUG("...socket connected to %s:%d", client->server, bntohs(addr.sin_port));

	/* Give other thread chance to initialize themselves! */
	bthreadSleepMs(200);


	for (;;)
	{
		int rc;
		bssize_t bytes;



		++counter;
		bssize_t length = constructClientRequest(send_buf, sizeof(send_buf));
		bytes = length;
		BASE_DEBUG("packet length: %d", length);
		CMN_HEX_DUMP(send_buf, length, "Send content:");
		rc = bsock_send(sock, send_buf, &bytes, 0);
		if (rc != BASE_SUCCESS || bytes != length )
		{
			if (rc != last_send_err)
			{
				sdkPerror("...send() error", rc);
				BASE_ERROR( "...ignoring subsequent error..");
				last_send_err = rc;
				bthreadSleepMs(100);
			}

			continue;
		}

		rc = _waitSocket(sock, 500*4);
		if (rc == 0)
		{
			BASE_ERROR("...timeout");
			bytes = 0;
			batomic_inc(_timeoutCounter);
		}
		else if (rc < 0)
		{
			rc = bget_netos_error();
			sdkPerror("...select() error", rc);
			break;
		}
		else
		{/* Receive back the original packet. */
			bytes = 0;
			do
			{
				bssize_t received = BUF_SIZE - bytes;
				rc = bsock_recv(sock, recv_buf+bytes, &received, 0);
				if (rc != BASE_SUCCESS || received == 0)
				{
					if (rc != last_recv_err)
					{
						sdkPerror("...recv() error", rc);
						BASE_WARN("...ignoring subsequent error..");
						last_recv_err = rc;
						bthreadSleepMs(100);
					}

					bytes = 0;
					received = 0;
					break;
				}

				BASE_DEBUG("recv: %d: %.*s", received, received - sizeof(XmControlHeader), recv_buf+sizeof(XmControlHeader) );
				CMN_HEX_DUMP(recv_buf, received, "Recved");
				

				bytes += received;
			} while (bytes != BUF_SIZE && bytes != 0);
		}

		if (bytes == 0)
			continue;

		if (bmemcmp(send_buf, recv_buf, BUF_SIZE) != 0)
		{
			recv_buf[BUF_SIZE-1] = '\0';
			BASE_ERROR("...error: buffer %u has changed!\nsend_buf=%s\nrecv_buf=%s\n", counter, send_buf, recv_buf);
			batomic_inc(_invalidCounter);
		}

		/* Accumulate total received. */
		batomic_add(_totalBytes, bytes);
	}

	bsock_close(sock);
	return 0;
}


