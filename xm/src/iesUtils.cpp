#include "xm.hpp"

#include "libBase.h"
#include "compact.h"
#include "libCmn.h"


int iseGetIntegerFromJsonObject(cJSON* json, const char * key)
{
	cJSON * obj = cJSON_GetObjectItem(json, key);
	
	if(cJSON_IsNumber(obj) )
		return obj->valueint;
	else
		return INVALIDATE_VALUE_U32;
}

char* iesGetStrFromJsonObject(cJSON* json, const char * key)
{
	cJSON * obj = cJSON_GetObjectItem(json, key);

//	MUX_DEBUG("obj of %s is %s", key, (obj==NULL)?"NULL":obj->string );
	if(cJSON_IsString(obj) )
	{
		return obj->valuestring;
	}
	else
	{
//		return "";
		return NULL;
	}
}


/* string with length of 0 is validate string for rest API/IP command */
int iesJsonGetStrIntoString(cJSON* json, const char * key, char *value, int size)
{
	char *_value = iesGetStrFromJsonObject(json, key);

//	MUX_DEBUG("_value of %s is %s", key, (_value==NULL)?"NULL":_value);
	if(IS_STRING_NULL(_value) )
	{
		return EXIT_FAILURE;
	}

	snprintf(value, size, "%s", _value);
	return EXIT_SUCCESS;
}

