/* 
 *
 */
#include <baseGuid.h>
#include <baseString.h>
#include <baseSock.h>
#include <windows.h>
#include <objbase.h>
#include <baseOs.h>


const unsigned BASE_GUID_STRING_LENGTH=32;

unsigned bGUID_STRING_LENGTH()
{
    return BASE_GUID_STRING_LENGTH;
}

BASE_INLINE_SPECIFIER void hex2digit(unsigned value, char *p)
{
    static char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
			 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    *p++ = hex[ (value & 0xF0) >> 4 ];
    *p++ = hex[ (value & 0x0F) ];
}

static void _guidToStr( GUID *guid, bstr_t *str )
{
	unsigned i;
	const unsigned char *src = (const unsigned char*)guid;
	char *dst = str->ptr;

	guid->Data1 = bntohl(guid->Data1);
	guid->Data2 = bntohs(guid->Data2);
	guid->Data3 = bntohs(guid->Data3);

	for (i=0; i<16; ++i)
	{
		hex2digit( *src, dst );
		dst += 2;
		++src;
	}
	str->slen = 32;
}


bstr_t* bgenerate_unique_string(bstr_t *str)
{
	GUID guid;

	BASE_CHECK_STACK();

	CoCreateGuid(&guid);
	_guidToStr( &guid, str );
	return str;
}

