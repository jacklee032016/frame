/* 
 *
 */
#include <baseCtype.h>
#include <baseGuid.h>
#include <basePool.h>

bstr_t* bgenerate_unique_string_lower(bstr_t *str)
{
	int i;

	bgenerate_unique_string(str);
	for (i = 0; i < str->slen; i++)
		str->ptr[i] = (char)btolower(str->ptr[i]);

	return str;
}

void bcreate_unique_string(bpool_t *pool, bstr_t *str)
{
	str->ptr = (char*)bpool_alloc(pool, BASE_GUID_STRING_LENGTH);
	bgenerate_unique_string(str);
}

void bcreate_unique_string_lower(bpool_t *pool, bstr_t *str)
{
	int i;

	bcreate_unique_string(pool, str);
	for (i = 0; i < str->slen; i++)
		str->ptr[i] = (char)btolower(str->ptr[i]);
}

