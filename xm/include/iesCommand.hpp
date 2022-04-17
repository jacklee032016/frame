#ifndef	__IES_COMMAND_HPP__
#define __IES_COMMAND_HPP__

#include <map>
#include <memory>

#include "iesSession.hpp"
#if 0
#include "cJSON.h"
#else
#include "json/json.hpp"
#include "iesJsonFactory.hpp"
#endif
#include "xm.hpp"

namespace ienso 
{
	namespace sdk
	{

		class Device;
		class Session;

		class MgmtObj
		{
			public:
				MgmtObj(Session *session, XmCmdCode reqCode, XmCmdCode respCode);
				virtual ~MgmtObj();

//				virtual char *serialize(void);
				virtual const std::string serialize(void);
				virtual bool parse(char *buf, unsigned int size);

				XmCmdCode	getRequestCode(void);


			protected:
					Session 						*_mSession;
#if 0
					cJSON							*_mJsonObj;
#else
					Json::Value 					_mJsonObj;
#endif

			private:

					XmCmdCode						_mReqCode;
					XmCmdCode						_mRespCode;
		};


		class CmdLogin: public MgmtObj
		{
			public:
				CmdLogin(Session *session);
				~CmdLogin();

				
				
			private:
				
				
		};


		
	}
}

#endif




