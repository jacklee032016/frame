#ifndef	__IES_DEVICE_MGR_HPP__
#define __IES_DEVICE_MGR_HPP__

#include <map>
#include <memory>
#include <string>

#include "iesIoCtx.hpp"
#include "iesIoSocket.hpp"
#include "json/json.hpp"

#include "libBase.h"

namespace ienso 
{
	namespace sdk
	{

		class Device;
		class ienso::ctx::IoCtx;
		class ienso::ctx::IesSocket;

		/* ip address, unique ID??? */
		using DeviceMap	= std::map<std::string, std::shared_ptr<Device>>;
		
		class DeviceMgr
		{
			public:
				/* localIp: IP used to lookup all devices; it must be defined explicitly when used in multi-homed host; 
				* 0.0.0.0 can only works in one NIC host
				*/
				DeviceMgr(ienso::ctx::IoCtx *ctx, const std::string& localIp="0.0.0.0", unsigned short port = 34569 );
				~DeviceMgr();

				bool 	lookupDevice(void);
				bool 	addDevice(const std::string& ip="192.168.1.12", unsigned short port= 34567, const std::string& user="admin", const std::string& passwd="", int encryptType = 0);

				bool 	addDevice(const Json::Value& value);

				bool	removeDevice(const std::string&       ip);
				void	clearAll(void);

				bpool_factory	*getPoolFactory(void);
				
				ienso::ctx::IoCtx	*getIoCtx(void);

				Device		*findDevice(const std::string& ipAddress);

			private:
				ienso::ctx::IoCtx			*_mCtx;
				ienso::ctx::IesSocket		_mUdpBroadcast;

				bcaching_pool				_mCachingPool;
				DeviceMap					_mDevMap;

				std::string					_mLocalIp;
				unsigned short				_mBcPort;
				
				bsockaddr_in				_mBcAddr;
				bsock_t						_mBcSocket = BASE_INVALID_SOCKET;/* broadcast socket */

				char						_mFindPacket[1024];


				bool	_init(void);
				void	_timerCallback(void *data);

				
				bool 	_onSent(bssize_t size, void *data);
				bool 	_onRecvFrom(void *data, bsize_t size, const bsockaddr_t *srcAddr, int addrLen, bstatus_t status, void *userData);
		};
	
	}
}

#endif


