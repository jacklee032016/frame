
#include "libBase.h"

#include "iesDevice.hpp"
#include "iesSession.hpp"
#include "iesJsonFactory.hpp"

namespace ienso 
{
	namespace sdk
	{

#define	CHECK_FIELD(dataObj, rtValue, field, isOK)	\
		{ Json::Value rtObj = jsonObj[field]; if(rtObj == Json::Value::nullSingleton()) \
		{ BASE_ERROR("No field '%s' is found", field); return STATUS_FAILED; }

	Device::Device(DeviceMgr *mgr)
		:_mMgr(mgr)
	{
		_init();
		BTRACE();
	}
	
	Device::~Device()
	{
		if(_mCtrlSession != nullptr)
		{
			_mCtrlSession.reset();
		}
		
		BASE_DEBUG("Device is destroied");
	}

	void Device::_init(void)
	{
		_mXmKeys.insert("DeviceType");
		_mXmKeys.insert("MAC");
		
		_mXmKeys.insert("HostIP");
		_mXmKeys.insert("GateWay");
		_mXmKeys.insert("Submask");
		_mXmKeys.insert("HostName");

		_mXmKeys.insert("SSLPort");
		_mXmKeys.insert("HttpPort");
		_mXmKeys.insert("MaxBps");
		
		_mXmKeys.insert("MonMode");
		_mXmKeys.insert("TCPMaxConn");
		_mXmKeys.insert("TCPPort");
		_mXmKeys.insert("TransferPlan");

		_mXmKeys.insert("UDPPort");
		_mXmKeys.insert("UseHSDownLoad");

		_mXmKeys.insert("SN");
		_mXmKeys.insert("Version");
		_mXmKeys.insert("BuildDate");
		_mXmKeys.insert("OtherFunction");
	}

	std::string Device::validate(const Json::Value& dataObj)
	{
		bool isOK;
		BTRACE();
		Json::Value::Members members = dataObj.getMemberNames();
BTRACE();
		for(auto f: members)
		{
			auto f2 = _mXmKeys.find(f);
			if(f2 == _mXmKeys.end() )
			{
				BTRACE();
				return std::string();
			}
		}

		deviceType = ienso::json::JsonFactory::getInteger(dataObj, "DeviceType", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "DeviceType");
			return std::string();
		}

		mac = ienso::json::JsonFactory::getString(dataObj, "MAC", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "MAC");
			return std::string();
		}
		
		hostName = ienso::json::JsonFactory::getString(dataObj, "HostName", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "HostName");
			return std::string();
		}

		hostIP = ienso::json::JsonFactory::getAddressInt(dataObj, "HostIP", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "HostIP");
			return std::string();
		}

		gateWay = ienso::json::JsonFactory::getAddressInt(dataObj, "GateWay", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "GateWay");
			return std::string();
		}

		submask = ienso::json::JsonFactory::getAddressInt(dataObj, "Submask", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "Submask");
			return std::string();
		}

		sslPort = ienso::json::JsonFactory::getInteger(dataObj, "SSLPort", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "SSLPort");
			return std::string();
		}
		httpPort = ienso::json::JsonFactory::getInteger(dataObj, "HttpPort", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "HttpPort");
			return std::string();
		}
		maxBps = ienso::json::JsonFactory::getInteger(dataObj, "MaxBps", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "MaxBps");
			return std::string();
		}
		tcpMaxConn = ienso::json::JsonFactory::getInteger(dataObj, "TCPMaxConn", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "TCPMaxConn");
			return std::string();
		}

		tcpPort = ienso::json::JsonFactory::getInteger(dataObj, "TCPPort", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "TCPPort");
			return std::string();
		}
		udpPort = ienso::json::JsonFactory::getInteger(dataObj, "UDPPort", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "UDPPort");
			return std::string();
		}
		
		useHSDownLoad = ienso::json::JsonFactory::getBool(dataObj, "UseHSDownLoad", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "UseHSDownLoad");
			return std::string();
		}

		monMode = ienso::json::JsonFactory::getString(dataObj, "MonMode", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "MonMode");
			return std::string();
		}
		SN = ienso::json::JsonFactory::getString(dataObj, "SN", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "SN");
			return std::string();
		}
		transferPlan = ienso::json::JsonFactory::getString(dataObj, "TransferPlan", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "TransferPlan");
			return std::string();
		}
		version = ienso::json::JsonFactory::getString(dataObj, "Version", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "Version");
			return std::string();
		}
		buildDate = ienso::json::JsonFactory::getString(dataObj, "BuildDate", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "BuildDate");
			return std::string();
		}
		otherFunction = ienso::json::JsonFactory::getString(dataObj, "OtherFunction", &isOK);
		if(isOK == false)
		{
			BASE_ERROR("Field %s is invalidate", "OtherFunction");
			return std::string();
		}
		
		bin_addr addr;
		addr.s_addr = hostIP;
		return std::string(binet_ntoa(addr));
	}

	DeviceMgr* Device::getDeviceMgr(void)
	{
		return _mMgr;
	}

	bool Device::startSession(const std::string& user, const std::string& passwd)
	{
		_mCtrlSession = std::make_shared<Session>(this);
		bin_addr add;
		add.s_addr = hostIP;
		std::string ip(binet_ntoa(add));
		
		_mCtrlSession->start(ip, tcpPort, user, passwd);

		return true;
	}
	
	}
}


