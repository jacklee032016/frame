#ifndef	__IES_SESSION_HPP__
#define __IES_SESSION_HPP__

#include <map>
#include <memory>
#include <string>
#include <array>

#include "iesIoCtx.hpp"
#include "libBase.h"

namespace ienso 
{
	namespace sdk
	{

		class Device;
		class MgmtObj;
		class Session;
		class Response;
		class ienso::ctx::IesSocket;
		

		using CmdMap	=	std::map<std::string, std::shared_ptr<MgmtObj>>;

//		#define	BUF_SIZE			4096
		enum {BUF_SIZE = 4096};
		using	IesBuffer =	std::array<char, BUF_SIZE>;
		
		class Packet
		{
			public:
				Packet(Session *session);
				~Packet();

				/* return length of binary request */
				int	generateRequest(MgmtObj *request);
				std::shared_ptr<Response> parseResponse(char *buf, unsigned int size);
				
				void clear(void);

				unsigned int getSize();
				void setSize(unsigned int size);
				
				unsigned int getDataSize();
				
				bool fillData(const std::string& jsonStr);
				bool fillData(const char *jsonStr);
				
				char* getHeader(void);
				const char* getData(unsigned int *size);
				
				char* getBuffer(void);
				unsigned int getBufferSize(void);

				bool isEmpty();
//				const std::string 

			private:
				
				IesBuffer				_mBuf;
				unsigned int			_mSize;
				unsigned int			_mHeaderSize;

				Session					*_mSession;
		};
		
		class Session
		{
			public:
				Session(Device *device, int encryptType = 0 );
				virtual ~Session();

				virtual bool start(const std::string& ip = "192.168.1.12", unsigned    short port = 34567, const std::string& user="admin", const std::string& passwd="");
				virtual bool stop(void);


				const std::string& getIpAddress(void);
				const unsigned short 	getPort(void);
				const std::string& getUser(void);
				const std::string& getpassword(void);
				unsigned int		getEncryptType(void);

				unsigned int		getSessionId(void);
				void				setSessionId(unsigned int sId);

				unsigned int		getSequenceNo(void);

			private:

				Device								*_mParent;
				std::shared_ptr<ienso::ctx::IesSocket>	_mIoSocket;
				
				std::string							_mIp;
				unsigned short						_mPort;
				
				std::string							_mUser;
				std::string							_mPasswd;

				int									_mEnryptType;

				unsigned int						_mSessionId;
				unsigned int						_mSequenceNo;
				unsigned int						_mPacketNo;

				int									_mSockType; /* TCP/UDP etc.*/
				bsock_t								_mSocket;

				std::shared_ptr<Packet>				_mBuffer;

				/* shared_ptr make parent/child not covarint again */
//				std::shared_ptr<Command>			_mCurrentCmd;
				MgmtObj								*_mCurrentCmd;

				bool _onConnect(bstatus_t status, void *data);
				
				bool _onSent(bssize_t size, void *);
				
				bool _onRead(void *data, bsize_t size, bstatus_t status, void *userData);
		};


		class CtrlSession:public Session
		{
			public:
				CtrlSession(Device *dev);
				~CtrlSession();

			private:
				
			
		};
	}
}

#endif



