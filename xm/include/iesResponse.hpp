
#ifndef	__IES_RESPONSE_HPP__
#define __IES_RESPONSE_HPP__

#include "iesCommand.hpp"

namespace ienso 
{
	namespace sdk
	{

	
		class Response: public MgmtObj
		{
			public:
				Response(Session *session);
				~Response();

				unsigned int	getStatusCode(void);
				void			setStatusCode(unsigned int code);
				void			setErrorMessage(const std::string& msg);
				const std::string& getErrorMessage(void);
				
				bool 			parse(char *jsonStr, unsigned int size);

				void 			debug(void);

				static const char* getErrorMsg(int code);

			private:
				unsigned int				_mStatusCode;

				std::string					_mErrorMsg;
				
		};

	}
}

#endif

