#include "iesJsonFactory.hpp"
#include "libBase.h"

namespace ienso
{
	namespace json
	{

	static Json::StreamWriterBuilder		wBuilder;
	static Json::CharReaderBuilder			rBuilder;
	
	static std::unique_ptr<Json::CharReader>	reader(rBuilder.newCharReader());

#define	CHECK_FIELD(jsonObj, rtObj, field)	\
	{ rtObj = jsonObj[field]; if(rtObj == Json::Value::nullSingleton()) \
	{ BASE_ERROR("No field '%s' is found", field); return static_cast<int>(StatusCode::STATUS_CODE_FAIL); } }

#define	CHECK_BOOL_FIELD(jsonObj, rtObj, field)	\
	{ rtObj = jsonObj[field]; if(rtObj == Json::Value::nullSingleton()) \
	{ BASE_ERROR("No field '%s' is found", field); return false; } }

#define	CHECK_STR_FIELD(jsonObj, rtObj, field)	\
	{ rtObj = jsonObj[field]; if(rtObj == Json::Value::nullSingleton()) \
	{ BASE_ERROR("No field '%s' is found", field); return std::string(); } }

#define	CHECK_JSON_FIELD(jsonObj, rtObj, field)	\
	{ rtObj = jsonObj[field]; if(rtObj == Json::Value::nullSingleton()) \
	{ BASE_ERROR("No field '%s' is found", field); Json::Value::nullSingleton(); } }

		
//static Json::StreamWriterBuilder JsonFactory::wBuilder = Json::StreamWriterBuilder();
//static Json::CharReaderBuilder JsonFactory::rBuilder = Json::CharReaderBuilder();
//static std::unique_ptr<Json::CharReader> JsonFactory::reader = Json::CharReader(JsonFactory::rBuilder.newCharReader());


bool JsonFactory::parse(char *str, int size, Json::Value *obj)
{
	std::string errStr;
	
	if (!reader->parse((char *)str, str + size, obj, &errStr))
	{
		BASE_ERROR("Parse JSON error: %s", errStr.c_str() );
		return false;
	}

	return true;
}


const std::string JsonFactory::serialize(const Json::Value& obj)
{
	const std::string jsonStr = Json::writeString(wBuilder, obj);
	return jsonStr;
}

int JsonFactory::getStatus(const Json::Value &respObj, bool isRaw)
{
	Json::Value obj;
	
	CHECK_FIELD(respObj, obj, "Ret");
	
	int rc = obj.asInt();
	BASE_DEBUG("ret:%d", rc);
	ienso::json::JsonFactory::debug(respObj["Ret"]);
	ienso::json::JsonFactory::debug(obj);

	if(isRaw)
		return rc;
	
	if(rc == 100)
	{
		return static_cast<int>(StatusCode::STATUS_CODE_OK);
	}
	else
	{
		return static_cast<int>(StatusCode::STATUS_CODE_FAIL);
	}
}

const Json::Value JsonFactory::getDataObject(const Json::Value &respObj, bool *isOK)
{
	Json::Value obj;
	*isOK = false;
	
	CHECK_JSON_FIELD(respObj, obj, "Name");
	const char *name = obj.asCString();
	if(name == nullptr)
	{
		BASE_ERROR("No Name is found");
		return Json::Value::nullSingleton();
	}
	BASE_INFO("Name:%s", name );
	
	Json::Value dataObj = respObj[name];
	if(dataObj == Json::Value::nullSingleton())
	{
		BASE_ERROR("No IP is found");
		return Json::Value::nullSingleton();
	}

	*isOK = true;

	return dataObj;
}

std::string JsonFactory::getAddress(const Json::Value& dataObj, char *field, bool *isOK)
{
	int ip = getAddressInt(dataObj, field, isOK);
	if(isOK == false)
	{
		return std::string();
	}
	
	bin_addr addr;
	addr.s_addr = ip;
	
	BASE_INFO("Object IP:%d: %s", ip, binet_ntoa(addr) );
	
	return std::string(binet_ntoa(addr));
}

int JsonFactory::getAddressInt(const Json::Value& dataObj, char *field, bool *isOK)
{
	Json::Value obj;
	*isOK = false;
	
	CHECK_FIELD(dataObj, obj, field);
	
	int ip = obj.asInt();

	if(ip != -1)
	{
		*isOK = true; 
	}

	return ip;
}


int JsonFactory::getInteger(const Json::Value& dataObj, char *field, bool *isOK)
{
	Json::Value obj;
	*isOK = false;
	
	CHECK_FIELD(dataObj, obj, field);

	if(!obj.isNumeric())
	{
		return -1;
	}

	*isOK = true;
	return obj.asInt();
}

bool JsonFactory::getBool(const Json::Value& dataObj, char *field, bool *isOK)
{
	Json::Value obj;
	*isOK = false;
	
	CHECK_BOOL_FIELD(dataObj, obj, field);

	if(!obj.isBool())
	{
		return false;
	}

	*isOK = true;

	return obj.asBool();
}

std::string JsonFactory::getString(const Json::Value& dataObj, char *field, bool *isOK)
{
	Json::Value obj;
	*isOK = false;
	
	CHECK_STR_FIELD(dataObj, obj, field);

	if(!obj.isString())
	{
		return std::string();
	}

	*isOK = true;

	return obj.asString();
}

void JsonFactory::debug(const Json::Value& obj)
{
	std::string str = serialize(obj);

	BASE_DEBUG("%s", str.c_str());
}

#if 0
double Request::get_query_parameter( const string& name, const double default_value ) const
{
	double parameter = 0;
	
	try
	{
		parameter = stod( get_query_parameter( name ) );
	}
	catch ( const out_of_range )
	{
		parameter = default_value;
	}
	catch ( const invalid_argument )
	{
		parameter = default_value;
	}
	
	return parameter;
}
#endif

	}
}

