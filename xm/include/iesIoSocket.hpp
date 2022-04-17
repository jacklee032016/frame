#ifndef		__IES_IO_SOCKET_HPP__
#define		__IES_IO_SOCKET_HPP__

#include <map>
#include <memory>
#include <string>
#include <functional>

namespace ienso
{
	namespace ctx
	{
		enum
		{
			SOCKET_BUF_SIZE = 1024,
		};

		enum class SocketAsyncMode
		{
			ONE_ASYNC_PER_SOCKET,	/* one async for one socket at anytime */
			ONE_ASYNC_PER_OP		/* one async for every ops of one socket */
		};
		
		class SocketImpl;
		class IoCtx;

		using	AsyncMap = std::map<int, std::shared_ptr<SocketImpl>>;

		/* return true, reload async op; false, end it */
		using SockCbAccepted = std::function<bool (bsock_t, const bsockaddr_t *, void *)>;
		using SockCbConnected = std::function<bool (bstatus_t, void *)>;

		using SockCbSent = std::function<bool (bssize_t, void *)>;

		using SockCbRead = std::function<bool (void *data, bsize_t size, bstatus_t status, void *)>;

		using SockCbRecvFrom = std::function<bool (void *data, bsize_t size, const	bsockaddr_t *srcAddr, int addrLen, bstatus_t status, void *)>;

		/* multiple async operations can works simultaneously on same IesSocket */
		class IesSocket
		{
			public:
				IesSocket(IoCtx *ctx, int sType =BASE_SOCK_STREAM, SocketAsyncMode      mode = SocketAsyncMode::ONE_ASYNC_PER_SOCKET);
				~IesSocket();

				const std::string& getName(void);
				
				/* called only when playing as service */
				bool	bind(unsigned short port, const std::string& localIp = "0.0.0.0"/* any address*/, bool isBc = false/* or Mc*/);

				/* error, return < 0; OK, BASE_SUCCESS; BASE_EPENDING */
				int				asyncAccept(SockCbAccepted cb);
				
				int 			asyncConnect(const std::string& svrIp, unsigned short port, SockCbConnected cb);
				int				asyncConnect(bsockaddr_in *remoteAddr, SockCbConnected cb);

				/* when return iD >0, status must be BASE_SUCCESS(sent out immediately), or BASE_EPENDING(wait on sent event)*/
				int				asyncSend(bsock_t newSock, void *data, bssize_t *size, SockCbSent cb);/* TCP server, send on new socket*/
				int				asyncSend(void *data, bssize_t *size, SockCbSent cb);/* TCP client */
				int				asyncRead(bsock_t newSock, SockCbRead cb);/* read     of TCP server */
				int				asyncRead(SockCbRead cb);/* read of TCP client */
				
				int				asyncSendTo(void *data, bssize_t *size, SockCbSent cb, const std::string& destIp, unsigned short port);
				int				asyncSendTo(void *data, bssize_t *size, SockCbSent cb, bsockaddr_t *destAddr);
				int				asyncRecvFrom(SockCbRecvFrom cb);

				
				bsock_t			getSocket(void);
				int				getSockType(void);
				const SocketAsyncMode	getMode(void);

				/* called in callback function */			
				bool 			removeAsync(int asyncId);
				/* called by client of socket: put it in delete list */
				bool 			cancelAsync(int asyncId);
				/* called by poll thread, return the number of cleaned async impl  */
				int 			removeAsyncs(void);

				void			debug(void);

			private:
				SocketAsyncMode					_mMode;
				
				bsock_t 						_mSock;
				int 							_mSockType;

				void							*_mUserData;
				bgrp_lock_t 					*_mGrpLock; /* for every timer entry */

				IoCtx							*_mCtx;
				int 							_mId;

				AsyncMap						_mTcpClients;	/* for new socket of TCP client when play as TCP server */
				int								_mImplId;

				/* 2 options for every async operation and its active socket 
				* First, one active for one socket, all operations of this socket are limited to this one active socket.
				* So, at any time, only one operation can be run for this socket. Simplify the maintainance and reasonable 
				* ops of active socket;
				* all operations for this socket can only associate with active socket once;
				*
				* Second, every operation of the socket has its active socket, and every operation can be run asynically with
				* other operations. More complicated and more efficient.
				* Every operation for this socket must be associated with different active socket when it is created;
				*/
				std::shared_ptr<SocketImpl>		_mImpl;	/* for the first option */

				AsyncMap						_mAsyncMap;	/* for the second option */
				AsyncMap						_mAsyncDeleteMap;

				std::string						_mName;

		};


	}
}

#endif

