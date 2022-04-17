
#include "iesIoCtx.hpp"
#include "iesIoSocket.hpp"

#include "xm.hpp"

#include "libUtil.h"

namespace ienso
{
	namespace ctx
	{

	class IoCtx;
	
	class SocketImpl
	{
		public:
			SocketImpl(IoCtx *ctx,      IesSocket *parent, int id);
			SocketImpl(IoCtx *ctx,      IesSocket *parent, int id, bsock_t newSock);
			~SocketImpl();

			bool 			init(void);

			/* return status from active call directly */
			bstatus_t		asyncAccept(SockCbAccepted cb);
			bstatus_t		asyncConnect(bsockaddr_in *remoteAddr, SockCbConnected cb);

			/* when BASE_PENDING, means wait onSent event; when BASE_SUCCESS, means sent out immediately, without onSent event */
			bstatus_t		asyncSend(void *data, bssize_t *size, SockCbSent cb);
			bstatus_t		asyncRead(SockCbRead cb);
			
			/* when BASE_PENDING, means wait onSent event; when BASE_SUCCESS, means sent out immediately, without onSent event */
			bstatus_t		asyncSendTo(void *data, bssize_t *size, SockCbSent cb, bsockaddr_t *destAddr);
			bstatus_t		asyncRecvFrom(SockCbRecvFrom cb);
	

			bool			runAccept(	   bsock_t newsock, const bsockaddr_t *srcAddr, int srcAddrLen);
			bool			runConnect(bstatus_t status);
			bool			runSent(bssize_t size);
			bool			runRead(void *data, bsize_t size,	 bstatus_t status);
			bool			runRecvfrom(void *data, bsize_t size,	const bsockaddr_t *srcAddr, int addrLen, bstatus_t status);
			
	
		private:
	
			SockCbAccepted					_mCbAccept;
			SockCbConnected 				_mCbConn;
			SockCbSent						_mCbSent;
			SockCbRead						_mCbRead;
			SockCbRecvFrom					_mCbRecvfrom;
	
			IesSocket 						*_mParent;
			bsock_t 						_mSock;
			int 							_mSockType;
			bactivesock_t					*_mActiveSock;
			
			bioqueue_op_key_t				_mOpKey;
			
			bactivesock_cb					_mActiveCallback;
			
			void							*_mUserData;
			bgrp_lock_t 					*_mGrpLock; /* for every timer entry */
	
			IoCtx							*_mCtx;
			int 							_mId;

			bool						_initActiveSocket(void);
			
	};

	/* callback schema for active socket */
	static bbool_t _onDataRecvfrom(bactivesock_t *asock, void *data, bsize_t size,
		const bsockaddr_t *srcAddr, int addrLen, bstatus_t status)
	{
		SocketImpl *impl =(SocketImpl *)bactivesock_get_user_data(asock);
BTRACE();
		impl->runRecvfrom(data, size, srcAddr, addrLen, status);
	
		return BASE_TRUE;
	}

	static bbool_t _onDataRead(bactivesock_t *asock, void *data, bsize_t size, bstatus_t status, bsize_t *remainder)
	{
		SocketImpl *impl =(SocketImpl *)bactivesock_get_user_data(asock);

#if 0	
		if (status != BASE_SUCCESS && status != BASE_EPENDING)
		{
			BASE_CRIT("err: status=%d", status);
			return BASE_FALSE;
		}
#endif
	
		BTRACE();
		impl->runRead(data, size, status);
		*remainder = 0; /* notify ioqueue all data has been consumed now */
	
		return BASE_TRUE;
	}

	static bbool_t _onDataSent(bactivesock_t *asock, bioqueue_op_key_t *op_key, bssize_t sent)
	{
		SocketImpl *impl =(SocketImpl *)bactivesock_get_user_data(asock);
	
		BTRACE();
		BASE_UNUSED_ARG(op_key);

		impl->runSent(sent);	
	
		return BASE_TRUE;
	}
	
	bbool_t _onAcceptComplete(bactivesock_t *asock, 		bsock_t newSock, const bsockaddr_t *srcAddr, int srcAddrLen)
	{
		SocketImpl *impl =(SocketImpl *)bactivesock_get_user_data(asock);
	
		BTRACE();
		if(impl->runAccept(newSock, srcAddr, srcAddrLen) == false)
		{/* no more accept, so close this asock */
			return BASE_FALSE;
		}
		/* continue to accept again */
		return BASE_TRUE;
	}

	bbool_t _onConnectComplete(bactivesock_t *asock, bstatus_t status)
	{
		BTRACE();
		SocketImpl *impl =(SocketImpl *)bactivesock_get_user_data(asock);
	
		impl->runConnect(status);	
		return BASE_TRUE;
	}


	SocketImpl::SocketImpl(IoCtx *ctx,      IesSocket *parent, int id)
		:_mCtx(ctx),
		_mId(id),
		_mParent(parent),
		_mSock(parent->getSocket()),
		_mSockType(parent->getSockType() ),
		_mCbAccept(nullptr),
		_mCbConn(nullptr),
		_mCbSent(nullptr),
		_mCbRead(nullptr),
		_mCbRecvfrom(nullptr),
		_mUserData(nullptr),
		_mGrpLock(nullptr),
		_mActiveSock(nullptr)
	{
		init();
	}

	SocketImpl::SocketImpl(IoCtx *ctx,		IesSocket *parent, int id, bsock_t newSock)
		:_mCtx(ctx),
		_mId(id),
		_mParent(parent),
		_mSock(newSock),
		_mSockType(BASE_SOCK_STREAM ),
		_mCbAccept(nullptr),
		_mCbConn(nullptr),
		_mCbSent(nullptr),
		_mCbRead(nullptr),
		_mCbRecvfrom(nullptr),
		_mUserData(nullptr),
		_mGrpLock(nullptr),
		_mActiveSock(nullptr)
	{
		init();
		BASE_INFO("New socket:%d", _mSock);
	}

	SocketImpl::~SocketImpl()
	{
		if(_mActiveSock != nullptr )
		{
			BASE_DEBUG("destory active socket in SocketImpl#%d", _mId);
			bactivesock_close(_mActiveSock);
		}

		BASE_DEBUG("SocketImpl#%d destroied", _mId);
	}
	
	bool SocketImpl::init(void)
	{
	
		_mActiveCallback.on_accept_complete = _onAcceptComplete;
		_mActiveCallback.on_connect_complete = _onConnectComplete;
		_mActiveCallback.on_accept_complete2 = nullptr;
		_mActiveCallback.on_data_sent = _onDataSent;
		_mActiveCallback.on_data_read = _onDataRead;
		_mActiveCallback.on_data_recvfrom = _onDataRecvfrom;

		if(_mParent->getMode()==SocketAsyncMode::ONE_ASYNC_PER_SOCKET)
		{
			return _initActiveSocket();
		}
		else
		{
		}
		
		return true;
	}	

	bool SocketImpl::_initActiveSocket(void)
	{
		bstatus_t rc = bactivesock_create(_mCtx->getMemPool(), _mSock, _mSockType, NULL, _mCtx->getIoQueue(), &_mActiveCallback, this, &_mActiveSock);
		if (rc != BASE_SUCCESS)
		{
			sdkPerror(rc, "%s Unable create active socket", _mParent->getName().c_str() );
			bsock_close(_mSock);
			_mSock = BASE_INVALID_SOCKET;
			return false;
		}

		return true;
	}
		
	bstatus_t SocketImpl::asyncAccept(   SockCbAccepted cb)
	{
		if(_mSockType != BASE_SOCK_STREAM)
		{
			BASE_ERROR("UDP socket can't accept connection");
			return BASE_FAILED;
		}
		
		_mCbAccept = cb;
		_mActiveCallback.on_accept_complete = _onAcceptComplete;
		if(_mParent->getMode()==SocketAsyncMode::ONE_ASYNC_PER_SOCKET)
		{
		}
		else
		{
			_initActiveSocket();
		}

		return bactivesock_start_accept(_mActiveSock, _mCtx->getMemPool());
	}


	bstatus_t SocketImpl::asyncConnect(	 bsockaddr_in *remoteAddr, SockCbConnected cb)
	{
		if(_mSockType != BASE_SOCK_STREAM)
		{
			BASE_ERROR("UDP socket can't connect to service");
			return BASE_FAILED;
		}
		_mCbConn = cb;

		_mActiveCallback.on_connect_complete = _onConnectComplete;
		if(_mParent->getMode()==SocketAsyncMode::ONE_ASYNC_PER_SOCKET)
		{
		}
		else
		{
			_initActiveSocket();
		}
		
		return bactivesock_start_connect(_mActiveSock, _mCtx->getMemPool(), (bsockaddr_t *) remoteAddr, sizeof(bsockaddr_in) );
	}

	bstatus_t SocketImpl::asyncSend(void *data, bssize_t *size, SockCbSent cb)
	{
		if(_mSockType != BASE_SOCK_STREAM)
		{
			BASE_ERROR("UDP socket must send to dest address");
			return BASE_FAILED;
		}
		_mCbSent = cb;

		_mActiveCallback.on_data_sent = _onDataSent;
		if(_mParent->getMode()==SocketAsyncMode::ONE_ASYNC_PER_SOCKET)
		{
		}
		else
		{
			_initActiveSocket();
		}
		BASE_INFO("%s send on socket %d", _mParent->getName().c_str(), _mSock);
		return bactivesock_send(_mActiveSock, &_mOpKey/* op key */, data, size, 0/* flags to ioqueue_send()*/ );			
	}

	bstatus_t SocketImpl::asyncRead(SockCbRead cb)
	{
		if(_mSockType != BASE_SOCK_STREAM)
		{
			BASE_ERROR("UDP socket must recv from remote address");
			return BASE_FAILED;
		}
		
		_mCbRead = cb;

		_mActiveCallback.on_data_read = _onDataRead;
		if(_mParent->getMode()==SocketAsyncMode::ONE_ASYNC_PER_SOCKET)
		{
		}
		else
		{
			_initActiveSocket();
		}
		bstatus_t rc = bactivesock_start_read(_mActiveSock, _mCtx->getMemPool(), SOCKET_BUF_SIZE, 0 );
		if(rc != BASE_SUCCESS)
		{
			sdkPerror(rc, "%s start read failed", _mParent->getName().c_str() );
		}
		else
		{		
			BASE_DEBUG("%s start read OK", _mParent->getName().c_str() );
		}
		return rc;
	}


	bstatus_t SocketImpl::asyncSendTo(void *data, bssize_t *size, SockCbSent cb, bsockaddr_t *destAddr)
	{
		if(_mSockType != BASE_SOCK_DGRAM)
		{
			BASE_ERROR("TCP socket never send to dest address");
			return BASE_FAILED;
		}
		
		_mCbSent = cb;

		bsockaddr_in *addr = (bsockaddr_in *)destAddr;

		_mActiveCallback.on_data_sent = _onDataSent;
		if(_mParent->getMode()==SocketAsyncMode::ONE_ASYNC_PER_SOCKET)
		{
		}
		else
		{
			_initActiveSocket();
		}
		bstatus_t rc = bactivesock_sendto(_mActiveSock, &_mOpKey, data, size, 0/* flags*/, destAddr, sizeof(bsockaddr_in) );			
		if(rc != BASE_SUCCESS && rc != BASE_EPENDING)
		{
			sdkPerror(rc, "%s sendto %s:%d failed", _mParent->getName().c_str(), binet_ntoa(addr->sin_addr), bntohs(addr->sin_port));
			return BASE_FAILED;
		}
		
		BASE_DEBUG("%s send %d bytes to %s:%d OK", _mParent->getName().c_str(), *size, binet_ntoa(addr->sin_addr), bntohs(addr->sin_port) );
		return rc;
	}

	bstatus_t SocketImpl::asyncRecvFrom(SockCbRecvFrom cb)
	{
		if(_mSockType != BASE_SOCK_DGRAM)
		{
			BASE_ERROR("TCP socket never recv from dest address");
			return BASE_FAILED;
		}
		_mCbRecvfrom = cb;

		if(_mParent->getMode()==SocketAsyncMode::ONE_ASYNC_PER_SOCKET)
		{
			_mActiveCallback.on_data_recvfrom = _onDataRecvfrom;
		}
		else
		{
			_initActiveSocket();
		}
		
		BASE_DEBUG("%s start recv ...", _mParent->getName().c_str() );
		bstatus_t rc = bactivesock_start_recvfrom(_mActiveSock, _mCtx->getMemPool(), SOCKET_BUF_SIZE, 0 );
		if(rc != BASE_SUCCESS)
		{
			sdkPerror(rc, "%s start recvfrom failed", _mParent->getName().c_str());
		}
		else
		{
			BASE_DEBUG("%s start recv OK", _mParent->getName().c_str() );
		}
		return rc;
	}

	
	bool SocketImpl::runAccept(	 bsock_t newSock, const bsockaddr_t *srcAddr, int srcAddrLen)
	{
		if(_mCbAccept(newSock, srcAddr, _mUserData) == false)
		{
			bstatus_t status = bactivesock_close(_mActiveSock);
			if(status != BASE_SUCCESS)
			{
				sdkPerror(status, "Close asock failed");
			}
			/* put async link into list in socket, then close and destory by poll thread */
//			_mParent->removeAsync(_mId);

			return false;
		}

		/* re-monitor to recv new accept */
		return true;
	}
	
	bool SocketImpl::runConnect(bstatus_t status)
	{
		if(_mCbConn(status, _mUserData) == false)		
		{
			bstatus_t status = bactivesock_close(_mActiveSock);
			if(status != BASE_SUCCESS)
			{
				sdkPerror(status, "%s Close asock failed", _mParent->getName().c_str());
			}
//			_mParent->removeAsync(_mId);
			return false;
		}

		return true;
	}

	bool SocketImpl::runSent(bssize_t size)
	{
		if(_mCbSent(size, _mUserData) == false)
		{
			bstatus_t status = bactivesock_close(_mActiveSock);
			if(status != BASE_SUCCESS)
			{
				sdkPerror(status, "Close asock failed");
			}

//			_mParent->removeAsync(_mId);

			return false;
		}

		return true;
	}

	bool SocketImpl::runRead(void *data, bsize_t size,	 bstatus_t status)
	{
		if(_mCbRead(data, size, status, _mUserData) == false)
		{
			bstatus_t status = bactivesock_close(_mActiveSock);
			if(status != BASE_SUCCESS)
			{
				sdkPerror(status, "Close asock failed");
			}

//			_mParent->removeAsync(_mId);

			return false;
		}

		return true;
	}

	bool SocketImpl::runRecvfrom(void *data, bsize_t size,	const bsockaddr_t *srcAddr, int addrLen, bstatus_t status)
	{
		if(_mCbRecvfrom(data, size, srcAddr, addrLen, status, _mUserData) == false)
		{
			bstatus_t status = bactivesock_close(_mActiveSock);
			if(status != BASE_SUCCESS)
			{
				sdkPerror(status, "Close asock failed");
			}

//			_mParent->removeAsync(_mId);

			return false;
		}
		return true;
	}

	
	IesSocket::IesSocket(IoCtx *ctx,     int sType, SocketAsyncMode mode)
		:_mCtx(ctx),
		_mMode(mode),
		_mId(1),
		_mImplId(1),
		_mSock(BASE_INVALID_SOCKET),
		_mSockType(sType),
		_mUserData(nullptr),
		_mGrpLock(nullptr)
	{
		/* for both client and service; enough for client */
		bstatus_t rc = bsock_socket(bAF_INET(), sType, 0, &_mSock);
		if (rc != BASE_SUCCESS)
		{
			sdkPerror(rc, "Unable to create socket");
			_mSock = BASE_INVALID_SOCKET;
			return;
		}

#ifdef	_MSC_VER	
		long/* DWORD*/ timeout = 800;	/*ms */
#else		
		btime_val timeout;
//		struct timeval timeout;      
		timeout.sec = 0;
		timeout.msec = 800;
#endif

		rc = bsock_setsockopt( _mSock, BASE_SOL_SOCKET, BASE_SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
		if(rc != BASE_SUCCESS)
		{
			sdkPerror(rc, "Set socket REV timeout error");
		}

		rc = bsock_setsockopt( _mSock, BASE_SOL_SOCKET, BASE_SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
		if(rc != BASE_SUCCESS)
		{
			sdkPerror(rc, "Set socket SEND timeout error" );
		}

		_mImpl = std::make_shared<SocketImpl>(_mCtx, this, _mImplId++);
	
		_mId = _mCtx->registerSocket(this);

		if(_mSockType == BASE_SOCK_STREAM)
		{
			_mName = "TCP";
		}
		else
		{
			_mName = "UDP";
		}
	}

	IesSocket::~IesSocket()
	{
		_mCtx->unregisterSocket(_mId);

		_mTcpClients.clear();
		
		_mAsyncMap.clear();
		_mAsyncDeleteMap.clear();
		
		if(_mSock != BASE_INVALID_SOCKET)
		{
			bsock_close(_mSock);
		}
		BASE_DEBUG("IesSocket#%d destroied", _mId);
	}

	const std::string& IesSocket::getName(void)
	{
		return _mName;
	}

	const SocketAsyncMode IesSocket::getMode(void)
	{
		return _mMode;
	}

	void IesSocket::debug(void)
	{
		BASE_DEBUG("IesSocket %s, %d Async Callback", _mName.c_str(), _mAsyncMap.size() );
	}

	bool IesSocket::bind(unsigned short port, const std::string& localIp, bool isBc/* or Mc*/)
	{
		bstatus_t rc;
		bsockaddr_in srvAddr;
		bstr_t	str;

		if(isBc)
		{
			//char broadcast = '1';
			int broadcast = 1;
			if ((rc=bsock_setsockopt(_mSock, bSOL_SOCKET(), BASE_SO_BOARDCAST, &broadcast, sizeof(broadcast) )) != 0)
			{
				sdkPerror(rc, "Set socket BOARDCAST error:%s");
			}
		}
		
//			bin_addr binAddr;
//			binAddr.s_addr = BASE_INADDR_ANY;
		
		/* Bind server socket: service bind to local address, not broadcast address  */
		bbzero(&srvAddr, sizeof(srvAddr));
		srvAddr.sin_family = bAF_INET();
		srvAddr.sin_port = bhtons(port);
//			srvAddr.sin_addr = binAddr;
		srvAddr.sin_addr = binet_addr(bcstr(&str, localIp.c_str()) );
		
		if ((rc=bsock_bind(_mSock, &srvAddr, sizeof(srvAddr))) != 0)
		{
			sdkPerror(rc, "Bind local address error:");
			return false;
		}

		if (_mSockType == BASE_SOCK_STREAM )
		{
			rc = bsock_listen(_mSock, BASE_SOMAXCONN);
			if (rc != BASE_SUCCESS)
			{
				sdkPerror(rc, "TCP listen failed:");
				return false;
			}
		}

		_mName += " Server";
		

		return true;
	}

	int IesSocket::asyncAccept(   SockCbAccepted cb)
	{
		bstatus_t rc;
		if(_mSockType != BASE_SOCK_STREAM || _mSock == BASE_INVALID_SOCKET)
		{
			BASE_ERROR("UDP socket can't accept connection");
			return BASE_FAILED;
		}

		rc = _mImpl->asyncAccept(cb);
		if( rc != BASE_SUCCESS)
		{
			sdkPerror(rc, "ActiveSocket Accept failed");
		}
		return rc;
	}

	int IesSocket::asyncConnect(	 const std::string& svrIp, unsigned short port, SockCbConnected cb)
	{
		bsockaddr_in addr;
		bstr_t str;
		bstatus_t rc;
		
		rc = bsockaddr_in_init(&addr, bcstr(&str, svrIp.c_str()), (buint16_t)port);
		if (rc != BASE_SUCCESS)
		{
			sdkPerror(rc, "Unable to resolve device address");
			return BASE_FAILED;
		}

		return asyncConnect(&addr, cb);
	}

	int IesSocket::asyncConnect(	 bsockaddr_in *remoteAddr, SockCbConnected cb)
	{
		bstatus_t rc;
		if(_mSockType != BASE_SOCK_STREAM || _mSock == BASE_INVALID_SOCKET)
		{
			BASE_ERROR("UDP socket can't connect to service");
			return BASE_FAILED;
		}

		rc = _mImpl->asyncConnect(remoteAddr, cb);
		if(rc != BASE_SUCCESS && rc != BASE_EPENDING)
		{
			sdkPerror(rc, "ActiveSocket connect to %s:%d failed", binet_ntoa(remoteAddr->sin_addr), bntohs(remoteAddr->sin_port));
			return BASE_FAILED;
		}
		
		BASE_INFO("Async Connect to %s:%d %s", binet_ntoa(remoteAddr->sin_addr), bntohs(remoteAddr->sin_port), (rc==BASE_SUCCESS)?"OK":"Pending" );
		return rc;
	}

	/* for newly client in TCP service */
	int IesSocket::asyncSend(bsock_t newSock, void *data, bssize_t *size, SockCbSent cb)
	{
		bstatus_t rc;
		if(_mSockType != BASE_SOCK_STREAM || _mSock == BASE_INVALID_SOCKET)
		{
			BASE_ERROR("UDP socket must send to dest address");
			return BASE_FAILED;
		}

		if(newSock == BASE_INVALID_SOCKET)
		{
			BASE_ERROR("New socket is invalidate");
			return BASE_FAILED;
		}

		auto f = _mTcpClients.find(newSock);
		if(f == _mTcpClients.end() )
		{
			std::shared_ptr<SocketImpl> impl = std::make_shared<SocketImpl>(_mCtx, this, _mId, newSock);
			_mTcpClients[newSock] = impl;
			rc  = impl->asyncSend(data, size, cb);
		}
		else
		{
			rc = f->second->asyncSend(data, size, cb);
		}

		if(rc != BASE_SUCCESS && rc != BASE_EPENDING )
		{
			BASE_ERROR("ActiveSocket send failed on socket %d", _mSock);
			sdkPerror(rc, "ActiveSocket send failed on socket %d", _mSock);
			return BASE_FAILED;
		}
		
		return rc;
	}

	int IesSocket::asyncSend(void *data, bssize_t *size, SockCbSent cb)
	{
		bstatus_t rc;
		if(_mSockType != BASE_SOCK_STREAM || _mSock == BASE_INVALID_SOCKET)
		{
			BASE_ERROR("UDP socket must send to dest address");
			return BASE_FAILED;
		}

		rc = _mImpl->asyncSend(data, size, cb);
		if(rc != BASE_SUCCESS && rc != BASE_EPENDING )
		{
			BASE_ERROR("ActiveSocket send failed on socket %d", _mSock);
			sdkPerror(rc, "ActiveSocket send failed on socket %d", _mSock);
			return BASE_FAILED;
		}
		
		return rc;
	}

	int IesSocket::asyncRead(bsock_t newSock, SockCbRead cb)
	{
		if(_mSockType != BASE_SOCK_STREAM || _mSock == BASE_INVALID_SOCKET)
		{
			BASE_ERROR("UDP socket must recv from remote address");
			return BASE_FAILED;
		}
		
		if(newSock == BASE_INVALID_SOCKET)
		{
			BASE_ERROR("New socket is invalidate");
			return BASE_FAILED;
		}

		auto f = _mTcpClients.find(newSock);
		if(f == _mTcpClients.end() )
		{
			std::shared_ptr<SocketImpl> impl = std::make_shared<SocketImpl>(_mCtx, this, _mId, newSock);
			_mTcpClients[newSock] = impl;
			
			return impl->asyncRead(cb);
		}
		else
		{
			return f->second->asyncRead(cb);
		}

	}

	int IesSocket::asyncRead(SockCbRead cb)
	{
		if(_mSockType != BASE_SOCK_STREAM || _mSock == BASE_INVALID_SOCKET)
		{
			BASE_ERROR("UDP socket must recv from remote address");
			return BASE_FAILED;
		}

		return _mImpl->asyncRead(cb);
	}

	int IesSocket::asyncSendTo(void *data, bssize_t *size, SockCbSent cb, const std::string& destIp, unsigned short port)
	{
		bsockaddr_in addr;
		bstr_t str;
		bstatus_t rc;
		
		rc = bsockaddr_in_init(&addr, bcstr(&str, destIp.c_str()), (buint16_t)port);
		if (rc != BASE_SUCCESS)
		{
			sdkPerror(rc, "Unable to resolve device address");
			return BASE_FAILED;
		}

		BASE_DEBUG("ActiveSocket send/sendto %s:%d...", binet_ntoa(addr.sin_addr), bntohs(addr.sin_port));
		return asyncSendTo(data, size, cb, (bsockaddr_t *)&addr);
	}


	int IesSocket::asyncSendTo(void *data, bssize_t *size, SockCbSent cb, bsockaddr_t *destAddr)
	{
		if(_mSockType != BASE_SOCK_DGRAM || _mSock == BASE_INVALID_SOCKET)
		{
			BASE_ERROR("TCP socket never send to dest address");
			return BASE_FAILED;
		}

		return _mImpl->asyncSendTo(data, size, cb, destAddr);
	}

	int IesSocket::asyncRecvFrom(SockCbRecvFrom cb)
	{
		if(_mSockType != BASE_SOCK_DGRAM || _mSock == BASE_INVALID_SOCKET)
		{
			BASE_ERROR("TCP socket never recv from dest address");
			return BASE_FAILED;
		}

		return _mImpl->asyncRecvFrom(cb);
	}

	bsock_t IesSocket::getSocket(void)
	{
		return _mSock;
	}
	
	int IesSocket::getSockType(void)
	{
		return _mSockType;
	}
	

	bool IesSocket::removeAsync(int asyncId)
	{
		auto find = _mAsyncMap.find(asyncId);
	
		if(find != _mAsyncMap.end() )
		{
//				_mAsyncDeleteMap[find->first] = find->second;
			_mAsyncMap.erase(find); /* remove() op on iterator */
			return true;
		}
	
		return false;
	}
	
	
	bool IesSocket::cancelAsync(int asyncId)
	{
		auto find = _mAsyncMap.find(asyncId);
	
		if(find != _mAsyncMap.end() )
		{
			_mAsyncDeleteMap[find->first] = find->second;
			_mAsyncMap.erase(find); /* remove() op on iterator */
			return true;
		}
	
		return false;
	}
	
	int IesSocket::removeAsyncs(void)
	{
		_mAsyncDeleteMap.clear();

		return BASE_SUCCESS;
	}

	}
}

