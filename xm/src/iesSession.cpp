
#include "iesSession.hpp"
#include "iesDevice.hpp"
#include "iesDeviceMgr.hpp"
#include "iesCommand.hpp"
#include "iesResponse.hpp"

#include "libCmn.h"
#include "xm.hpp"

namespace ienso 
{
	namespace sdk
	{

	Packet::Packet(Session *session)
		:_mSize(0),
		_mHeaderSize(sizeof(XmControlHeader)),
		_mSession(session)
	{
	}
	
	Packet::~Packet()
	{
	}

	unsigned int Packet::getSize()
	{
		return _mSize;
	}

	/* called after recved packet into the buffer */
	void Packet::setSize(unsigned int size)
	{
		_mSize = size;
	}
	
	unsigned int Packet::getDataSize()
	{
		if(_mSize <= _mHeaderSize )
			return 0;

		return _mSize - _mHeaderSize;
	}
	
	void Packet::clear(void)
	{
		std::fill(std::begin(_mBuf), std::end(_mBuf), 0);
		_mSize = 0;
	}
	
	char* Packet::getHeader(void)
	{
		return _mBuf.data();
	}
	
	bool Packet::fillData(const std::string& jsonStr)
	{
		return fillData(jsonStr.c_str());
	}
	
	bool Packet::fillData(const char *jsonStr)
	{
		std::fill(std::begin(_mBuf)+ _mHeaderSize, std::end(_mBuf), 0);
		memcpy(_mBuf.data()+_mHeaderSize, jsonStr, strlen(jsonStr));
		_mSize = _mHeaderSize + strlen(jsonStr);

		return true;
	}
	
	const char* Packet::getData(unsigned int *size)
	{
		if(isEmpty())
		{
			*size = 0;
			return nullptr;
		}
		
		*size = _mSize - _mHeaderSize;
		return _mBuf.data()+_mHeaderSize;
	}

	char* Packet::getBuffer(void)
	{
		return _mBuf.data();
	}

	unsigned int Packet::getBufferSize(void)
	{
		return _mBuf.size();
	}

	bool Packet::isEmpty()
	{
		return (_mSize <= _mHeaderSize);
	}

	
	int Packet::generateRequest(MgmtObj *request)
	{
		XmControlHeader *xmHeader = (XmControlHeader *)_mBuf.data();

		const std::string data = request->serialize();
		fillData(data);
		
		xmHeader->flags = 0xFF;
		xmHeader->version = 1;
#if 0
		xmHeader->msgId = bhtons(XM_CMD_LOGIN_REQ2);
		xmHeader->dataLength = bhtonl(dataLength);
#else
		xmHeader->msgId = static_cast<unsigned short>(request->getRequestCode());
		xmHeader->dataLength = getDataSize();
#endif
		xmHeader->sessionId = _mSession->getSessionId();
		xmHeader->sequenceNumber = _mSession->getSequenceNo();

		xmHeader->totalPacket = 0;
		xmHeader->currentPacket = 0;

		return getSize();
	}
	
	std::shared_ptr<Response> Packet::parseResponse(char *buf, unsigned int size)
	{
		std::shared_ptr<Response> response = std::make_shared<Response>(_mSession);
		if(size < _mHeaderSize)
		{
			response->setErrorMessage("Received data is too small");
			return response;
		}
		
		if(response->parse(buf + _mHeaderSize, size- _mHeaderSize) == false)
		{
			return response;
		}
		
		XmControlHeader *xmHeader = (XmControlHeader *)buf;
		if(xmHeader->flags != 0xFF )
		{
			BASE_WARN("Flags in header is not right");
		}
		
		if(xmHeader->version != 1)
		{
			BASE_WARN("Version in header is not compatible");
		}

		if(xmHeader->sessionId != _mSession->getSessionId())
		{
			if(_mSession->getSessionId()!= 0)
			{
				BASE_WARN("Session is not correct: response:0X%x; local:0X%x", xmHeader->sessionId, _mSession->getSessionId() );
			}
			
			_mSession->setSessionId(xmHeader->sessionId);
		}

		if(xmHeader->dataLength != (size - _mHeaderSize) )
		{
			BASE_ERROR("Data Length is invalidate: it should be %d bytes, but %d bytes", (size- _mHeaderSize), xmHeader->dataLength);
			response->setErrorMessage("Data Length is wrong");
			return response;
		}


		return response;
	}


	Session::Session(Device *device, int encryptType )
		:_mIp(""),
		_mPort(0),
		_mUser(""),
		_mPasswd(""),
		_mEnryptType(encryptType),
		_mParent(device),
		_mSessionId(0),
		_mSequenceNo(0),
		_mPacketNo(0)
	{
		_mIoSocket = std::make_shared<ienso::ctx::IesSocket>(_mParent->getDeviceMgr()->getIoCtx());

		_mBuffer = std::make_shared<Packet>(this);		
		_mBuffer->clear();
	}
	
	Session::~Session()
	{
		_mBuffer->clear();
		_mBuffer.reset();
		
		BASE_DEBUG("Session destroied");
	}

	bool Session::start(const std::string& ip, unsigned short port, const std::string& user, const std::string& passwd)
	{
		bstatus_t rc;

		_mIp = ip;
		_mPort = port;
		_mUser = user;
		_mPasswd = passwd;

		if(_mIoSocket->asyncConnect(_mIp, _mPort, std::bind(&Session::_onConnect, this, std::placeholders::_1, std::placeholders::_2) ) < 0)
			return false;

		return true;
	}
	
	bool Session::stop(void)
	{
		return true;
	}

	bool Session::_onConnect(bstatus_t status, void *data)
	{
		bstatus_t rc;
		if(status != BASE_SUCCESS)
		{
			sdkPerror(status, "TCP Client connect to error:");
			return false;	/* stop asocket */
		}
		BASE_INFO("TCP Client connect to status: %d with socket, begin to send", status );
	
		std::shared_ptr<CmdLogin> cmd = std::make_shared<CmdLogin>(this);
		XmControlHeader *xmHeader = (XmControlHeader *)_mBuffer->getHeader();

		bssize_t size = _mBuffer->generateRequest(cmd.get() );
		
		BASE_DEBUG("packet length: %d", size);
		CMN_HEX_DUMP(_mBuffer->getBuffer(), size, "Send content:");

		rc = _mIoSocket->asyncSend(_mBuffer->getBuffer(), &size, 
			std::bind(&Session::_onSent, this, std::placeholders::_1, std::placeholders::_2));
		if(rc == BASE_FAILED || size != _mBuffer->getSize() )
		{
			BASE_ERROR("TCP Client send failed");
			return false;
		}
	
		if(1)//rc == BASE_SUCCESS)
		{/* sent out immediately */
			BASE_INFO("TCP Client begin to read...");
			_mIoSocket->asyncRead(std::bind(&Session::_onRead, this, std::placeholders::_1, std::placeholders::_2,
				std::placeholders::_3, std::placeholders::_4 ) );
		}
		return true;
	}


	bool Session::_onSent(bssize_t size, void *)
	{
		if(size > 0 )
		{
			if(0)//size != _mSize)
			{
//				BASE_WARN("TCP Client sent %d bytes, should be %d bytes", size, _mSize);
			}
			BASE_INFO("TCP Client sent %d bytes to server", size);
	
			_mIoSocket->asyncRead(std::bind(&Session::_onRead, this, std::placeholders::_1, std::placeholders::_2,
				std::placeholders::_3, std::placeholders::_4 ));
			return true;
		}
		else
		{
			sdkPerror(-size, "TCP Client send to server failed");
			return false;
		}
	}
	
	bool Session::_onRead(void *data, bsize_t size, bstatus_t status, void *userData)
	{
		if(status != BASE_SUCCESS)
		{
			sdkPerror(status, "TCP Client Read failed");
			return false;
		}
	
		BASE_INFO("TCP client read %d bytes", size);
		CMN_HEX_DUMP((char *)data, size, "TCP Read");
	
		std::shared_ptr<Response> response = _mBuffer->parseResponse((char *)data, size);
		
		response->debug();
		return false;
	}

	const std::string& Session::getIpAddress(void)
	{
		return _mIp;
	}
	
	const unsigned short	Session::getPort(void)
	{
		return _mPort;
	}
	
	const std::string& Session::getUser(void)
	{
		return _mUser;
	}
	
	const std::string& Session::getpassword(void)
	{
		return _mPasswd;
	}
	
	unsigned int		Session::getEncryptType(void)
	{
		return _mEnryptType;
	}


	unsigned int Session::getSessionId(void)
	{
		return _mSessionId;
	}
	
	void Session::setSessionId(unsigned int sId)
	{
		_mSessionId = sId;
	}

	unsigned int Session::getSequenceNo(void)
	{
		return _mSequenceNo;
	}

	CtrlSession::CtrlSession(Device *dev)
		:Session(dev )
	{
	}
	
	CtrlSession::~CtrlSession()
	{
	}

	}
}

