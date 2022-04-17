#include "iesIoCtx.hpp"
#include "iesIoSocket.hpp"

#include "iesProtocolExamples.hpp"
#include "xm.hpp"
#include <libBase.h>
#include "xmSdk.h"

#include "libCmn.h"


ienso::ctx::IoCtx *_ctx;

bool _udpBcRecvFrom(void *data, bsize_t size, const	bsockaddr_t *srcAddr, int addrLen, bstatus_t status, void *userData)
{
	ienso::ctx::IesSocket *socket = (ienso::ctx::IesSocket *)userData;
	bsockaddr_in *addr = (bsockaddr_in *)srcAddr;
	
	if(status != BASE_SUCCESS)
	{
		sdkPerror(status, "Send BC packet failed");
		return false;
	}

	BASE_INFO("UDP BC recv %d byte to %s:%d", size, binet_ntoa(addr->sin_addr), bhtons(addr->sin_port));
	CMN_HEX_DUMP((const char *)data, size, "RecvFrom");
#if 0	
	socket->asyncRecvFrom(
		std::bind(&_udpBcRecvFrom, 
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, 
		std::placeholders::_4, std::placeholders::_5, socket), socket);
#endif
	return true;
}



/* called when BC packet recved, and begin to recv */
bool _udpBcSent(bssize_t size, void *data)
{
	BTRACE();
	ienso::ctx::IesSocket *socket = (ienso::ctx::IesSocket *)data;
	if(size >= BASE_SUCCESS)
	{
		BASE_INFO("Sent BC packet ok, %d bytes!", size);
		socket->asyncRecvFrom(std::bind(&_udpBcRecvFrom, 
			std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, 
			std::placeholders::_4, std::placeholders::_5, socket));
	}
	else
	{
		sdkPerror(size, "Send BC packet failed");
	}

	return true;
}


XmControlHeader xmHeader;
bsockaddr_in srvAddr, bcAddr;

bool _udpBcPackets(const std::string& localIp, unsigned short bcPort, ienso::ctx::IoCtx *ctx)
{
	ienso::ctx::IesSocket *sock = new ienso::ctx::IesSocket(ctx, BASE_SOCK_DGRAM);
	bstatus_t rc;
	bstr_t s;
	char buf[1024];
	bssize_t sent = sizeof(XmControlHeader);

	if(sock->bind(bcPort, "192.168.1.1", true) == false)
	{

		return false;
	}
	
	memset(&xmHeader, 0, sizeof(XmControlHeader));
	xmHeader.flags = XM_HEAD_FLAGS;
	xmHeader.version = XM_HEAD_VERSION;
	xmHeader.msgId = static_cast<unsigned short>(XmCmdCode::IPSEARCH_REQ);
	
	CMN_HEX_DUMP((const char *)&xmHeader, sent, "Send BC:");
#if 1
	sock->asyncSendTo((void *)&xmHeader, &sent, 
		std::bind(&_udpBcSent, std::placeholders::_1, std::placeholders::_2) , 
		"255.255.255.255", bcPort);

	sock->asyncRecvFrom(std::bind(&_udpBcRecvFrom, 
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, 
		std::placeholders::_4, std::placeholders::_5, sock));
#endif
	return true;
}

bool _tcpOnConnect(bstatus_t status, void *data)
{
	ienso::ctx::IesSocket *socket = (ienso::ctx::IesSocket *)data;
	
	if(status != BASE_SUCCESS)
	{
		sdkPerror(status, "connect to error:");
		_ctx->stop();
//		delete socket;
		return false;	/* stop asocket */
	}
	BASE_INFO("connect to status: %d with socket: %p", status, socket );
	
	return true;
}

/* one TCP socket, only one connection */
static int _tcpAsyncConnect(const std::string& svrIp, unsigned short port, ienso::ctx::IoCtx *ctx)
{
	bstatus_t rc;
	ienso::ctx::IesSocket *socket= new ienso::ctx::IesSocket(ctx, BASE_SOCK_STREAM);

	int id = socket->asyncConnect(svrIp, port, std::bind(&_tcpOnConnect, std::placeholders::_1, std::placeholders::_2));
	if(id == BASE_FALSE)
	{
		BASE_ERROR("async connection failed");
	}

	BASE_DEBUG("Begin to async-connect to %s:%d", svrIp.c_str(), port);

	return 1;
}

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
	xmHeader->msgId = static_cast<unsigned short>(XmCmdCode::LOGIN_REQ2);
	xmHeader->dataLength = dataLength;
#endif
	memcpy( (unsigned char *)buf+sizeof(XmControlHeader), (unsigned char  *)authId, dataLength );
	
	return sizeof(XmControlHeader) + dataLength ;
}


int testSockets(void)
{
	ienso::ctx::IoCtx ctx(6);
	char	buf[2048];
	bssize_t size;
//	btime_val delay;
//	batomic_t *total;
	_ctx = &ctx;

	if(ctx.init() == false)
	{
		return -1;
	}

	size = constructClientRequest( buf, sizeof(buf));

#if 0
	UdpServer udpServer(&ctx);
	udpServer.start("192.168.1.1", 3600);

	UdpClient udpClient(&ctx);
	udpClient.start((void *)buf, size, "192.168.1.1", 3600);
#endif

#if 1
	TcpServer tcpServer(&ctx);
	tcpServer.start("192.168.1.1", 3600);

#if 0
	TcpClient tcpClient(&ctx);
	tcpClient.start((void *)buf, size, "192.168.1.1", 3600);
#else
	TcpClient tcpClient(&ctx);
	tcpClient.start((void *)buf, size, "192.168.1.12", 34567);
#endif	

#endif
//	_tcpAsyncConnect("192.168.1.13", 34567, &ctx);	/* error code */

//	_udpBcPackets("192.168.1.1", 34569, &ctx);
	BTRACE();

	ctx.run();

	BTRACE();
	return 0;
}

