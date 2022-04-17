#ifndef		__IES_PROTOCOL_EXAMPLES_HPP__
#define 	__IES_PROTOCOL_EXAMPLES_HPP__

#include "iesIoCtx.hpp"
#include "iesIoSocket.hpp"

#include "xm.hpp"
#include <libBase.h>
#include "xmSdk.h"

class TcpClient
{
	public:
		TcpClient(ienso::ctx::IoCtx *ctx );			
		~TcpClient();

		int start(void *data, bssize_t size, const std::string& svrIp, unsigned short port);

	private:
		ienso::ctx::IoCtx 		*_mCtx;
		ienso::ctx::IesSocket	_mSock;

		void					*_mData;
		bssize_t				_mSize;

		
		bool _onConnect(bstatus_t status, void *data);

		bool _onSent(bssize_t size, void *);

		bool _onRead(void *data, bsize_t size, bstatus_t status, void *userData);
		
};


class TcpServer
{
	public:
		TcpServer(ienso::ctx::IoCtx *ctx );
		~TcpServer();

		int start( const std::string& localIp, unsigned short port);

	private:
		ienso::ctx::IoCtx 		*_mCtx;
		ienso::ctx::IesSocket	_mSock;

		char					_mData[1024];
		bssize_t				_mSize;

		bsock_t					sock4Client;

		bool _onAccept(bsock_t newSock, const bsockaddr_t *baddr, void *userData);

		bool _onSent(bssize_t size, void *);

		bool _onRead(void *data, bsize_t size, bstatus_t status, void *userData);
		
};


class UdpClient
{
	public:
		UdpClient(ienso::ctx::IoCtx *ctx );
		~UdpClient();

		int start(void *data, bssize_t size, const std::string& svrIp, unsigned short port);

	private:
		ienso::ctx::IoCtx 		*_mCtx;
		ienso::ctx::IesSocket	_mSock;
		std::string			_mSvrIp;
		unsigned short 		_mPort;

		void					*_mData;
		bssize_t				_mSize;
		int						_mRecvCount;

		bool _onSent(bssize_t size, void *);

		bool _onRecvFrom(void *data, bssize_t size, const	bsockaddr_t *srcAddr, int addrLen, bstatus_t status, void *);
		
};


class UdpServer
{
	public:
		UdpServer(ienso::ctx::IoCtx *ctx );
		~UdpServer();

		int start( const std::string& localIp, unsigned short port);

	private:
		ienso::ctx::IoCtx 		*_mCtx;
		ienso::ctx::IesSocket	_mSock;

		char					_mData[1024];
		bssize_t				_mSize;

		bsockaddr_t 			*_clientAddr;
		int						_addrLength;

		bool _onSent(bssize_t size, void *);
		bool _onRecvFrom(void *data, bsize_t size, const bsockaddr_t *srcAddr, int addrLen, bstatus_t status, void *userData);
		
};



#endif

