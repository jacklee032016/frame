#include "iesCommand.hpp"

namespace ienso 
{
	namespace sdk
	{


		MgmtObj::MgmtObj(Session *session, XmCmdCode reqCode, XmCmdCode respCode)
			:_mSession(session),
			_mReqCode(reqCode),
//			_mJsonObj(nullptr),
			_mRespCode(respCode)
		{
		}
			
		MgmtObj::~MgmtObj()
		{
		}

		const std::string MgmtObj::serialize(void)
		{
#if 0
			return cJSON_PrintUnformatted(_mJsonObj);
#else
			return ienso::json::JsonFactory::serialize(_mJsonObj);			
#endif
		}
		
		bool MgmtObj::parse(char *buf, unsigned int size)
		{
#if 0
			_mJsonObj = cJSON_Parse(buf);
			if(_mJsonObj == NULL)
				return false;
			return true;
#else
			return ienso::json::JsonFactory::parse(buf, size, &_mJsonObj);
#endif
		}

		XmCmdCode	MgmtObj::getRequestCode(void)
		{
			return _mRespCode;	
		}


		CmdLogin::CmdLogin(Session *session)
			:MgmtObj(session, XmCmdCode::LOGIN_REQ2, XmCmdCode::LOGIN_RSP)
		{
			_mJsonObj["EncryptType"] = "NONE";
			_mJsonObj["LoginType"] = "DVRIP-Web";
			_mJsonObj["UserName"] = _mSession->getUser();
			_mJsonObj["PassWord"] = _mSession->getpassword();
		}
		
		CmdLogin::~CmdLogin()
		{
		}


	}
}

