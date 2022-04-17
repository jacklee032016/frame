
#include "iesResponse.hpp"

namespace ienso 
{
	namespace sdk
	{

		typedef	struct __MsgType
		{
			int		code;
			char	*msg;
		}_MsgType;

		_MsgType _msgs[] =
		{
			{
				XMS_OK,
				"OK",
			},
			{
				XMS_UNKNOWN_ERROR,
				"Unknown error",
			},
			{
				XMS_NOT_SUPPORT_VERSION,
				"Version not support",
			},
			{
				XMS_INVALIDATE_REQUEST,
				"Invalidate request",
			},
			{
				XMS_USER_LOGGED,
				"User has logged",
			},
			{
				XMS_USER_UNLOGGED,
				"User has not logged",
			},
			{
				XMS_PASSWD_INCORRECT,
				"Password incorrect",
			},
			{
				XMS_NON_PRIVILEDGE,
				"None priviledge",
			},
			{
				XMS_TIMEOUT,
				"Timeout",
			},
			{
				XMS_LOOKUP_FAILED,
				"Lookup failed: no file",
			},
			{
				XMS_LOOKUP_PARTLY,
				"Lookup OK, part file",
			},
					
			{
				XMS_LOOKUP_OK,
				"Lookup OK, all file",
			},
			{
				XMS_USER_EXISTED,
				"User existed",
			},
			{
				XMS_USER_NOT_EXIST,
				"User not exist",
			},
			{
				XMS_USER_GROUP_EXISTED,
				"User group existed",
			},
			{
				XMS_USER_GROUP_NOT_EXIST,
				"User group not exist",
			},
			{
				XMS_UNKNOWN,
				"Undefined",
			},
			{
				XMS_MSG_WRONG_FORMAT,
				"Message format wrong",
			},

			{
				XMS_PTZ_PROTOCOL_UNSET,
				"PTZ protocol undefined",
			},
			{
				XMS_NO_FILE_FOUND,
				"File not found",
			},
			{
				XMS_CFG_NOT_USED,
				"Configuration is not used",
			},
			{
				XMS_MEDIA_CHAN_NOT_CONNECT,
				"Media channel not conncted",
			},
			{
				XMS_OK_NEED_REBOOT,
				"OK. Device need to reboot",
			},


			/* 2xx */
			{
				XMS_USER_NOT_LOG,
				"User not logged",
			},
			{
				XMS_PASSWRD_WRONG,
				"Password wrong",
			},
			{
				XMS_USER_INVALIDATE,
				"User invalidate",
			},
			{
				XMS_USER_LOCKED,
				"User locked",
			},
			{
				XMS_USER_IN_BLACKLIST,
				"User in blacklist",
			},
			{
				XMS_USER_NAME_REGISTERED,
				"User name has been registered",
			},
			{
				XMS_INPUT_INVALIDATE,
				"Input invalidate",
			},

			{
				XMS_INDEX_REPLICATED,
				"Index replicated",
			},
			{
				XMS_OBJECT_NOT_EXIST,
				"Object not exist",
			},
			{
				XMS_ACCOUNT_IS_USING,
				"Account is using",
			},
			{
				XMS_SUBSET_OUT_OF_RANGE,
				"Subset is out of range",
			},
			{
				XMS_PASSWORD_INVALIDATE,
				"Password invalidate",
			},
			{
				XMS_PASSWORD_NOT_MATCH,
				"Password not match",
			},
			{
				XMS_USER_RESERVED,
				"User account is reserved",
			},

				
			{/* 500 */
				XMS_COMMAND_INVALIDATE,
				"Command invalidate",
			},
			{
				XMS_SPEAK_IS_ON,
				"Speaker has been ON",
			},
			{
				XMS_SPEAK_IS_OFF,
				"Speaker is OFF",
			},
			{
				XMS_UPDATE_BEGAN,
				"Update has begun",
			},
			{
				XMS_UPDATE_NOT_BEGIN,
				"Update not begin yet",
			},
			{
				XMS_UPDATE_DATA_INVALIDATE,
				"Update data invalidate",
			},
			{
				XMS_UPDATE_FAILED,
				"Update failed",
			},
			{
				XMS_UPDATE_OK,
				"Update OK",
			},
			{
				XMS_RESET_FAILED,
				"Reset failed",
			},
			{
				XMS_REBOOT_DEVICE_NEEDED,
				"Device need to reboot",
			},
			{
				XMS_RESET_INVALIDATE,
				"Reset configuration invalidate",
			},

			/* 6xx */
			{
				XMS_REBOOT_APP_NEEDED,
				"Application need to reboot",
			},
			{
				XMS_REBOOT_SYS_NEEDED,
				"System need to reboot",
			},
			{
				XMS_ERROR_WRITE_FILE,
				"write file failed",
			},
			{
				XMS_FEATURE_NOT_SUPPORT,
				"Feature not support",
			},
			{
				XMS_VALIDATE_FAILED,
				"Validation failed",
			},
			{
				XMS_CFG_NOT_EXIST,
				"Configuration not exist",
			},
			{
				XMS_CFG_PARSE_WRONG,
				"Configuration parsing failed",
			},
			{
				-1,
				"Unknown",
			},
		};
			
		
	
		Response::Response(Session *session)
			: MgmtObj(session, XmCmdCode::LOGIN_REQ2, XmCmdCode::LOGIN_RSP),
			_mStatusCode(XMS_OK)
		{
		}
		
		Response::~Response()
		{
		}
		
		unsigned int	Response::getStatusCode(void)
		{
			return	_mStatusCode;				
		}
		
		void Response::setStatusCode(unsigned int code)
		{
			_mStatusCode = code;
		}
		
		const std::string& Response::getErrorMessage(void)
		{
			return _mErrorMsg;
		}
		
		void Response::setErrorMessage(const std::string& msg)
		{
			_mErrorMsg = msg;
		}
		
		bool Response:: parse(char *jsonStr, unsigned int size)
		{
			if(MgmtObj::parse(jsonStr, size) == false)
				return false;

			_mStatusCode = ienso::json::JsonFactory::getStatus(_mJsonObj, true);
			if(_mStatusCode != XMS_OK )
			{
				BASE_ERROR("Parse error: code:%d; msg: %s", _mStatusCode, Response::getErrorMsg(_mStatusCode) );
				setErrorMessage(Response::getErrorMsg(_mStatusCode) );
				return false;
			}
			
			return true;
		}
		
		
		void Response::debug(void)
		{
			BASE_DEBUG("Response: code:%d(%s); \n'%s'", _mStatusCode, getErrorMessage().c_str(), ienso::json::JsonFactory::serialize(&_mJsonObj).c_str() );
		}
		

		const char* Response::getErrorMsg(int code)
		{
			_MsgType *type = _msgs;

			while(type->code != -1 )
			{
				if(type->code == code )
				{
					return type->msg;
				}
				
				type++;
			}

			return type->msg;
		}
		
	}
}

