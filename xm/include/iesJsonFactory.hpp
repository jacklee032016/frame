
#ifndef	__IES_JSON_FACTORY_HPP__
#define	__IES_JSON_FACTORY_HPP__

#include <memory>
#include "json/json.hpp"

namespace ienso
{

	namespace json	
	{

		enum class StatusCode
		{
			STATUS_CODE_OK	= 200,
			STATUS_CODE_FAIL	= 600,
		};

		
			
		class JsonFactory
		{
			public:

				static bool parse(char *str, int size, Json::Value *obj);
				static const std::string serialize(const Json::Value& obj);

				/* get from response object */
				static int getStatus(const Json::Value &res, bool isRaw = false);
				static const Json::Value getDataObject(const Json::Value &respObj, bool *isOK);

				/** get from data object */
				static std::string getAddress(const Json::Value& dataObj, char *field, bool *isOK);
				static int getAddressInt(const Json::Value& dataObj, char *field, bool *isOK);

				static int getInteger(const Json::Value& dataObj, char *field, bool *isOK);
				static bool getBool(const Json::Value& dataObj, char *field, bool *isOK);
				static std::string getString(const Json::Value& dataObj, char *field, bool *isOK);

				static void debug(const Json::Value& obj);
#if 0

				static Json::StreamWriterBuilder		wBuilder;
				static Json::CharReaderBuilder			rBuilder;
	
				static std::unique_ptr<Json::CharReader> 	reader;
#endif

			private:

		};

	
	};

};

#endif

