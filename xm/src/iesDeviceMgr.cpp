
#include <functional>

#include "iesDeviceMgr.hpp"
#include "iesDevice.hpp"
#include "iesSession.hpp"

#include "json/json.hpp"
#include "iesJsonFactory.hpp"

#include "xm.hpp"
#include "libUtil.h"
#include "libCmn.h"

namespace ienso 
{
	namespace sdk
	{

	
	DeviceMgr::DeviceMgr(ienso::ctx::IoCtx *ctx, const std::string& localIp, unsigned short port)
		:_mCtx(ctx),
		_mUdpBroadcast(ctx, BASE_SOCK_DGRAM),
		_mLocalIp(localIp),
		_mBcPort(port),
		_mBcSocket(BASE_INVALID_SOCKET)
	{

		_init();
	}
	
	DeviceMgr::~DeviceMgr()
	{
		_mDevMap.clear();
		
		BASE_DEBUG("DeviceMgr destroied");
	}

	bool 	DeviceMgr::addDevice(const std::string& ip, unsigned short port, const std::string& user, const std::string& passwd, int encryptType)
	{
#if 0
		std::shared_ptr<Session> session = std::make_shared<Session>(bSOCK_STREAM(), encryptType);

		if(session->start(ip, port, user, passwd) )
		{
			_mDevMap[ip] = std::make_shared<Device>(this);
			return true;
		}
#endif
		return false;
	}

	bool DeviceMgr::addDevice(const Json::Value& value)
	{
		BTRACE();
		std::shared_ptr<Device> dev = std::make_shared<Device>(this);

		std::string ip = dev->validate(value);
		if(ip.empty() )
		{
			return false;
		}
		
		BASE_INFO("find IP: %s", ip.c_str());
		auto f = _mDevMap.find(ip);
		if(f != _mDevMap.end() )
		{
			BASE_DEBUG("Device %s has existed", ip.c_str() );
			return false;
		}

		_mDevMap[ip] = dev;

		dev->startSession("admin", "abc123");

		return true;
		
	}

	bool	DeviceMgr::removeDevice(const std::string&       ip)
	{

		return true;
	}
	
	void	DeviceMgr::clearAll(void)
	{
		_mDevMap.clear();
	}

	Device		*DeviceMgr::findDevice(const std::string& ipAddress)
	{
		auto f = _mDevMap.find(ipAddress);
		if(f != _mDevMap.end() )
		{
			return f->second.get();
		}

		return nullptr;
	}
	
	bpool_factory* DeviceMgr::getPoolFactory(void)
	{
		return &_mCachingPool.factory;
	}

	ienso::ctx::IoCtx* DeviceMgr::getIoCtx(void)
	{
		return _mCtx;
	}

	bool DeviceMgr::_init(void)
	{
		bstatus_t rc;
		XmControlHeader *xmHeader;

		if(_mUdpBroadcast.bind(_mBcPort, _mLocalIp, true) == false)
		{
			BASE_ERROR("Initialization of device finding interface failed");
			return false;
		}

		if(_mCtx->addTimer(6000, true, std::bind(&DeviceMgr::_timerCallback, this, std::placeholders::_1), NULL) < 0)
		{
			BASE_ERROR("Timer is not created");
			return false;
		}

		xmHeader =(XmControlHeader *)_mFindPacket;
		
		memset(xmHeader, 0, sizeof(XmControlHeader));
		xmHeader->flags = XM_HEAD_FLAGS;
		xmHeader->version = XM_HEAD_VERSION;
		xmHeader->msgId = static_cast<unsigned short>(XmCmdCode::IPSEARCH_REQ);

		_mUdpBroadcast.asyncRecvFrom(std::bind(&DeviceMgr::_onRecvFrom, this, 
			std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
			std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
		
		return true;
	}

	void DeviceMgr::_timerCallback(void *data)
	{
		BASE_INFO("Timer callback for DeviceMgr");
		bssize_t sent = sizeof(XmControlHeader);

		_mUdpBroadcast.asyncSendTo(_mFindPacket, &sent, std::bind(&DeviceMgr::_onSent, this,
			std::placeholders::_1, std::placeholders::_2), 
			"255.255.255.255", _mBcPort);	
	
	}

	bool DeviceMgr::_onSent(bssize_t size, void *)
	{
		if(size > 0 )
		{
			if(size != sizeof(XmControlHeader))
			{
				BASE_WARN("UDP Server Sent %d bytes, should be %d bytes", size, sizeof(XmControlHeader) );
			}
			BASE_INFO("UDP Server sent %d bytes to server", size);
			return true;
		}
		else
		{
			sdkPerror(-size, "UDP Server send failed");
			return false;
		}
	}
	
	
	bool DeviceMgr::_onRecvFrom(void *data, bsize_t size, const bsockaddr_t *srcAddr, int addrLen, bstatus_t status, void *userData)
	{
		bstatus_t rc;
		bsockaddr_in *addr = (bsockaddr_in *)srcAddr;
	
		if(status == BASE_EEOF)
		{
			BASE_INFO("DeviceMgr: client has close connection");
			return false;
		}
	
		if(status != BASE_SUCCESS)
		{
			sdkPerror(status, "DeviceMgr Recv failed");
			return false;
		}
	
		BASE_INFO("DeviceMgr recv %d bytes from %s:%d", size, binet_ntoa(addr->sin_addr), bntohs(addr->sin_port));
		CMN_HEX_DUMP((char *)data, size, "DeviceMgr Recv");
		if(size <= sizeof(XmControlHeader))
		{
			BASE_INFO("JSON data invalidate, ignore it");
		}
		else
		{
			BASE_DEBUG("JSON:\n'%.*s'", size-sizeof(XmControlHeader), (char *)data+sizeof(XmControlHeader));
			
			Json::Value respObj;
			if(! ienso::json::JsonFactory::parse((char *)data+sizeof(XmControlHeader), size-sizeof(XmControlHeader), &respObj) )
			{
				BASE_ERROR("Parse JSON ");
				return false;
			}

			if(ienso::json::JsonFactory::getStatus(respObj) != static_cast<int>(ienso::json::StatusCode::STATUS_CODE_OK) )
			{
				BASE_ERROR("status code of response error");
				return false;
			}

			bool isOK;
			Json::Value dataObj = ienso::json::JsonFactory::getDataObject(respObj, &isOK);
			if(isOK)
			{
				addDevice(dataObj);
			}
				
		}
	
		return true;
	}

	
	}
}


