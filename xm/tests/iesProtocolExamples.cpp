#include "iesIoCtx.hpp"
#include "iesIoSocket.hpp"
#include "iesProtocolExamples.hpp"

#include "xm.hpp"
#include <libBase.h>
#include "xmSdk.h"

#include "libCmn.h"

TcpClient::TcpClient(ienso::ctx::IoCtx *ctx )
	:_mCtx(ctx),
	_mSock(ctx)
{
};
	
TcpClient::~TcpClient()
{
};

int TcpClient::start(void *data, bssize_t size, const std::string& svrIp, unsigned short port)
{
	_mData = data;
	_mSize = size;
	
	return _mSock.asyncConnect(svrIp, port, std::bind(&TcpClient::_onConnect, this, std::placeholders::_1, std::placeholders::_2) );
}


bool TcpClient::_onConnect(bstatus_t status, void *data)
{
	bstatus_t rc;
	if(status != BASE_SUCCESS)
	{
		sdkPerror(status, "TCP Client connect to error:");
		return false;	/* stop asocket */
	}
	BASE_INFO("TCP Client connect to status: %d with socket, begin to send", status );

	rc = _mSock.asyncSend(_mData, &_mSize, 
		std::bind(&TcpClient::_onSent, this, std::placeholders::_1, std::placeholders::_2));
	if(rc == BASE_FAILED)
	{
		BASE_ERROR("TCP Client send failed");
		return false;
	}

	if(1)//rc == BASE_SUCCESS)
	{/* sent out immediately */
		BASE_INFO("TCP Client begin to read...");
		_mSock.asyncRead(std::bind(&TcpClient::_onRead, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4 ) );
	}
	return true;
}

bool TcpClient::_onSent(bssize_t size, void *)
{
	if(size > 0 )
	{
		if(size != _mSize)
		{
			BASE_WARN("TCP Client sent %d bytes, should be %d bytes", size, _mSize);
		}
		BASE_INFO("TCP Client sent %d bytes to server", size);

		_mSock.asyncRead(std::bind(&TcpClient::_onRead, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4 ));
		return true;
	}
	else
	{
		sdkPerror(-size, "TCP Client send to server failed");
		return false;
	}
}

bool TcpClient::_onRead(void *data, bsize_t size, bstatus_t status, void *userData)
{
	if(status != BASE_SUCCESS)
	{
		sdkPerror(status, "TCP Client Read failed");
		return false;
	}

	BASE_INFO("TCP client read %d bytes", size);
	CMN_HEX_DUMP((char *)data, size, "TCP Read");

	return false;
}



TcpServer::TcpServer(ienso::ctx::IoCtx *ctx )
	:_mCtx(ctx),
	_mSock(ctx),
	sock4Client(BASE_INVALID_SOCKET),
	_mSize(0)
{
	_mSize += snprintf(_mData+_mSize, sizeof(_mData) - _mSize, "%s", "this is replay from TCP server" );
	
};
	
TcpServer::~TcpServer()
{
};

int TcpServer::start( const std::string& localIp, unsigned short port)
{
	_mSock.bind(port, localIp);
	
	return _mSock.asyncAccept(std::bind(&TcpServer::_onAccept, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}


bool TcpServer::_onAccept(bsock_t newSock, const bsockaddr_t *baddr, void *userData)
{
	bsockaddr_in *addr = (bsockaddr_in *)baddr;
	sock4Client = newSock;
	BASE_INFO("TCP Server accept new connect from %s:%d, begin to read on new socket %d...", binet_ntoa(addr->sin_addr), bntohs(addr->sin_port), sock4Client );
	_mSock.asyncRead(sock4Client, std::bind(&TcpServer::_onRead, this, 
		std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4) );
	return true;
}

bool TcpServer::_onSent(bssize_t size, void *)
{
	if(size > 0 )
	{
		if(size != _mSize)
		{
			BASE_WARN("Sent %d bytes, should be %d bytes", size, _mSize);
		}
		BASE_INFO("TCP sent %d bytes to server", size);

		_mSock.asyncRead(sock4Client, std::bind(&TcpServer::_onRead, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4 ));
		return true;
	}
	else
	{
		sdkPerror(-size, "Send to server failed");
		return false;
	}
}

bool TcpServer::_onRead(void *data, bsize_t size, bstatus_t status, void *userData)
{
	bstatus_t rc;

	if(status == BASE_EEOF)
	{
		BASE_INFO("TCP Server: client has close connection");
		return false;
	}

	if(status != BASE_SUCCESS)
	{
		sdkPerror(status, "TCP Server Read failed");
		return false;
	}

	BASE_INFO("TCP Server read %d bytes", size);
	CMN_HEX_DUMP((char *)data, size, "TCP Read");

	rc = _mSock.asyncSend(sock4Client, (void *)_mData, &_mSize, 
		std::bind(&TcpServer::_onSent, this, std::placeholders::_1, std::placeholders::_2));
	if(rc == BASE_FAILED)
	{
		BASE_ERROR("TCP Server send failed");
		return false;
	}

	/* every operation can only be started once */
#if 0
	if(rc == BASE_SUCCESS)
	{/* sent out immediately */
		_mSock.asyncRead(sock4Client,std::bind(&TcpServer::_onRead, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4 ));
	}
#endif	
	return true;
}



UdpClient::UdpClient(ienso::ctx::IoCtx *ctx )
	:_mCtx(ctx),
	_mSock(ctx, BASE_SOCK_DGRAM)
{
};
	
UdpClient::~UdpClient()
{
};

int UdpClient::start(void *data, bssize_t size, const std::string& svrIp, unsigned short port)
{
	bstatus_t rc;
	_mData = data;
	_mSize = size;
	_mSvrIp = svrIp;
	_mPort = port;
	_mRecvCount = 0;
	
	BASE_INFO("UDP Client sent %d bytes to %s:%d", size, svrIp.c_str(), port);
	rc = _mSock.asyncSendTo(data, &size, std::bind(&UdpClient::_onSent, this, std::placeholders::_1, std::placeholders::_2), svrIp, port);
	if(rc == BASE_FAILED)
	{
		BASE_INFO("UDP Client sent faild");
		return -1;
	}
	
	if( 1)//rc == BASE_SUCCESS)
	{
		_mSock.asyncRecvFrom(std::bind(&UdpClient::_onRecvFrom, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4,
			std::placeholders::_5, std::placeholders::_6 ));
		return rc;
	}

	return rc;
}


bool UdpClient::_onSent(bssize_t size, void *)
{
	if(size > 0 )
	{
		if(size != _mSize)
		{
			BASE_WARN("UDP Client sent %d bytes, should be %d bytes", size, _mSize);
		}
		BASE_INFO("UDP Client sent %d bytes to server", size);

		_mSock.asyncRecvFrom(std::bind(&UdpClient::_onRecvFrom, this, std::placeholders::_1, std::placeholders::_2,
			std::placeholders::_3, std::placeholders::_4,
			std::placeholders::_5, std::placeholders::_6 ));
		return true;
	}
	else
	{
		sdkPerror(-size, "UDP Client send to server failed");
		return false;
	}
}

bool UdpClient::_onRecvFrom(void *data, bssize_t size, const	bsockaddr_t *srcAddr, int addrLen, bstatus_t status, void *)
{
	if(status != BASE_SUCCESS)
	{
		sdkPerror(status, "UDP Client Read failed");
		return false;
	}

	BASE_INFO("UDP client read %d bytes", size);
	CMN_HEX_DUMP((char *)data, size, "UDP Read");
	_mRecvCount++;

	if(_mRecvCount == 10)
	{
		return false;
	}

	bstatus_t rc = _mSock.asyncSendTo(data, &size, std::bind(&UdpClient::_onSent, this, std::placeholders::_1, std::placeholders::_2), _mSvrIp, _mPort);
	if(rc == BASE_FAILED)
	{
		BASE_INFO("UDP Client sent faild");
		return false;
	}
	return true;
}


UdpServer::UdpServer(ienso::ctx::IoCtx *ctx )
	:_mCtx(ctx),
	_mSock(ctx, BASE_SOCK_DGRAM),
	_mSize(0)
{
	_mSize += snprintf(_mData+_mSize, sizeof(_mData) - _mSize, "%s", "This is replay from UDP server" );
	
};
	
UdpServer::~UdpServer()
{
};

int UdpServer::start( const std::string& localIp, unsigned short port)
{
	_mSock.bind(port, localIp, true);
	
	return _mSock.asyncRecvFrom(std::bind(&UdpServer::_onRecvFrom, this, 
		std::placeholders::_1, std::placeholders::_2, 
		std::placeholders::_3, std::placeholders::_4,
		std::placeholders::_5, std::placeholders::_6));
}

bool UdpServer::_onSent(bssize_t size, void *)
{
	if(size > 0 )
	{
		if(size != _mSize)
		{
			BASE_WARN("UDP Server Sent %d bytes, should be %d bytes", size, _mSize);
		}
		BASE_INFO("UDP Server sent %d bytes to server", size);

		return _mSock.asyncRecvFrom(std::bind(&UdpServer::_onRecvFrom, this, 
			std::placeholders::_1, std::placeholders::_2, 
			std::placeholders::_3, std::placeholders::_4,
			std::placeholders::_5, std::placeholders::_6));
		return true;
	}
	else
	{
		sdkPerror(-size, "UDP Server send failed");
		return false;
	}
}


bool UdpServer::_onRecvFrom(void *data, bsize_t size, const bsockaddr_t *srcAddr, int addrLen, bstatus_t status, void *userData)
{
	bstatus_t rc;

	if(status == BASE_EEOF)
	{
		BASE_INFO("UDP Server: client has close connection");
		return false;
	}

	if(status != BASE_SUCCESS)
	{
		sdkPerror(status, "UDP Server Read failed");
		return false;
	}

	BASE_INFO("UDP Server read %d bytes", size);
	CMN_HEX_DUMP((char *)data, size, "UDP Read");
//			_clientAddr = srcAddr;
//			_addrLength = addrLen;

	rc = _mSock.asyncSendTo((void *)_mData, &_mSize, 
		std::bind(&UdpServer::_onSent, this, std::placeholders::_1, std::placeholders::_2), (bsockaddr_t *)srcAddr);
	if(rc == BASE_FAILED)
	{
		BASE_ERROR("UDP Server send failed");
		return false;
	}

#if 0
	if(rc == BASE_SUCCESS)
	{/* sent out immediately */
		return _mSock.asyncRecvFrom(std::bind(&UdpServer::_onRecvFrom, this, 
			std::placeholders::_1, std::placeholders::_2, 
			std::placeholders::_3, std::placeholders::_4,
			std::placeholders::_5, std::placeholders::_6));
	}
#endif	
	return true;
}

