/* 
 *
 */
#include <baseGuid.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseOs.h>
#include <baseString.h>

#include <uuid/uuid.h>

const unsigned BASE_GUID_STRING_LENGTH=36;

unsigned bGUID_STRING_LENGTH()
{
    return BASE_GUID_STRING_LENGTH;
}

bstr_t* bgenerate_unique_string(bstr_t *str)
{
    enum {GUID_LEN = 36};
    char sguid[GUID_LEN + 1];
    uuid_t uuid = {0};
    
    BASE_ASSERT_RETURN(GUID_LEN <= BASE_GUID_STRING_LENGTH, NULL);
    BASE_ASSERT_RETURN(str->ptr != NULL, NULL);
    BASE_CHECK_STACK();
    
    uuid_generate(uuid);
    uuid_unparse(uuid, sguid);
    
    bmemcpy(str->ptr, sguid, GUID_LEN);
    str->slen = GUID_LEN;

    return str;
}

