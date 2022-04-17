/*
 *
 */
#include <baseGuid.h>
#include <baseAssert.h>
#include <baseRand.h>
#include <baseOs.h>
#include <baseString.h>

const unsigned BASE_GUID_STRING_LENGTH=32;

static char guid_chars[64];

unsigned bGUID_STRING_LENGTH()
{
    return BASE_GUID_STRING_LENGTH;
}

static void init_guid_chars(void)
{
    char *p = guid_chars;
    unsigned i;

    for (i=0; i<10; ++i)
	*p++ = '0'+i;

    for (i=0; i<26; ++i) {
	*p++ = 'a'+i;
	*p++ = 'A'+i;
    }

    *p++ = '-';
    *p++ = '.';
}

bstr_t* bgenerate_unique_string(bstr_t *str)
{
    char *p, *end;

    BASE_CHECK_STACK();

    if (guid_chars[0] == '\0') {
	benter_critical_section();
	if (guid_chars[0] == '\0') {
	    init_guid_chars();
	}
	bleave_critical_section();
    }

    /* This would only work if BASE_GUID_STRING_LENGTH is multiple of 2 bytes */
    bassert(BASE_GUID_STRING_LENGTH % 2 == 0);

    for (p=str->ptr, end=p+BASE_GUID_STRING_LENGTH; p<end; ) {
	buint32_t rand_val = brand();
	buint32_t rand_idx = RAND_MAX;

	for ( ; rand_idx>0 && p<end; rand_idx>>=8, rand_val>>=8, p++) {
	    *p = guid_chars[(rand_val & 0xFF) & 63];
	}
    }

    str->slen = BASE_GUID_STRING_LENGTH;
    return str;
}

