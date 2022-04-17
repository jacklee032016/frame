#ifndef	__IES_DEVICE_HPP__
#define __IES_DEVICE_HPP__

#include <string>
#include <map>
#include <set>
#include <memory>


#include "json/json.hpp"

namespace ienso 
{
	namespace sdk
	{

		class Command;
		class Session;
		class DeviceMgr;

//		using CmdMap 	std::map<const std::string, std::shared_ptr<Command>>;

		using XmKeys = std::set<std::string>;
		
		class Device
		{
			public:
				Device(DeviceMgr *mgr);
				~Device();

				std::string validate(const Json::Value& dataObj);

				DeviceMgr* getDeviceMgr(void);

				
				bool startSession(const std::string& user, const std::string& passwd);
				

			private:
				DeviceMgr						*_mMgr;

				std::shared_ptr<Session>		_mCtrlSession;
				std::string 					_mUser;
				std::string						_mPasswd;
				int								_mEncryptType;


				Json::Value						_mJsonData;


				int								deviceType;
				std::string						mac;

				/**/
				int								hostIP;
				int								gateWay;
				int								submask;

				std::string						hostName;
				
				unsigned short					sslPort;
				unsigned short					httpPort;
				unsigned int					maxBps;
				
				std::string						monMode;
				unsigned int					tcpMaxConn;
				unsigned int					tcpPort;
				std::string						transferPlan;
				
				unsigned int					udpPort;
				bool							useHSDownLoad;
				
				std::string						SN;
				std::string						version;
				std::string						buildDate;
				std::string						otherFunction;;
				
				XmKeys							_mXmKeys;
				
				
				void							_init(void);
		};
	
	}
}

#endif

