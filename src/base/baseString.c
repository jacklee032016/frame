/* 
 *
 */
 
#include <baseString.h>
#include <baseAssert.h>
#include <basePool.h>
#include <baseCtype.h>
#include <baseRand.h>
#include <baseOs.h>
#include <baseErrno.h>
#include <_baseLimits.h>

#if BASE_FUNCTIONS_ARE_INLINED==0
#include <_baseString_i.h>
#endif


bssize_t bstrspn(const bstr_t *str, const bstr_t *set_char)
{
    bssize_t i, j, count = 0;
    for (i = 0; i < str->slen; i++) {
	if (count != i) 
	    break;

	for (j = 0; j < set_char->slen; j++) {
	    if (str->ptr[i] == set_char->ptr[j])
		count++;
	}
    }
    return count;
}


bssize_t bstrspn2(const bstr_t *str, const char *set_char)
{
    bssize_t i, j, count = 0;
    for (i = 0; i < str->slen; i++) {
	if (count != i)
	    break;

	for (j = 0; set_char[j] != 0; j++) {
	    if (str->ptr[i] == set_char[j])
		count++;
	}
    }
    return count;
}


bssize_t bstrcspn(const bstr_t *str, const bstr_t *set_char)
{
    bssize_t i, j;
    for (i = 0; i < str->slen; i++) {
	for (j = 0; j < set_char->slen; j++) {
	    if (str->ptr[i] == set_char->ptr[j])
		return i;
	}
    }
    return i;
}


bssize_t bstrcspn2(const bstr_t *str, const char *set_char)
{
    bssize_t i, j;
    for (i = 0; i < str->slen; i++) {
	for (j = 0; set_char[j] != 0; j++) {
	    if (str->ptr[i] == set_char[j])
		return i;
	}
    }
    return i;
}


bssize_t bstrtok(const bstr_t *str, const bstr_t *delim,
			     bstr_t *tok, bsize_t start_idx)
{    
    bssize_t str_idx;

    tok->slen = 0;
    if ((str->slen == 0) || ((bsize_t)str->slen < start_idx)) {
	return str->slen;
    }
    
    tok->ptr = str->ptr + start_idx;
    tok->slen = str->slen - start_idx;

    str_idx = bstrspn(tok, delim);
    if (start_idx+str_idx == (bsize_t)str->slen) {
	return str->slen;
    }    
    tok->ptr += str_idx;
    tok->slen -= str_idx;

    tok->slen = bstrcspn(tok, delim);
    return start_idx + str_idx;
}


bssize_t bstrtok2(const bstr_t *str, const char *delim,
			       bstr_t *tok, bsize_t start_idx)
{
    bssize_t str_idx;

    tok->slen = 0;
    if ((str->slen == 0) || ((bsize_t)str->slen < start_idx)) {
	return str->slen;
    }

    tok->ptr = str->ptr + start_idx;
    tok->slen = str->slen - start_idx;

    str_idx = bstrspn2(tok, delim);
    if (start_idx + str_idx == (bsize_t)str->slen) {
	return str->slen;
    }
    tok->ptr += str_idx;
    tok->slen -= str_idx;

    tok->slen = bstrcspn2(tok, delim);
    return start_idx + str_idx;
}


char* bstrstr(const bstr_t *str, const bstr_t *substr)
{
    const char *s, *ends;

    /* Special case when substr is zero */
    if (substr->slen == 0) {
	return (char*)str->ptr;
    }

    s = str->ptr;
    ends = str->ptr + str->slen - substr->slen;
    for (; s<=ends; ++s) {
	if (bansi_strncmp(s, substr->ptr, substr->slen)==0)
	    return (char*)s;
    }
    return NULL;
}


char* bstristr(const bstr_t *str, const bstr_t *substr)
{
    const char *s, *ends;

    /* Special case when substr is zero */
    if (substr->slen == 0) {
	return (char*)str->ptr;
    }

    s = str->ptr;
    ends = str->ptr + str->slen - substr->slen;
    for (; s<=ends; ++s) {
	if (bansi_strnicmp(s, substr->ptr, substr->slen)==0)
	    return (char*)s;
    }
    return NULL;
}


bstr_t* bstrltrim( bstr_t *str )
{
    char *end = str->ptr + str->slen;
    register char *p = str->ptr;
    while (p < end && bisspace(*p))
	++p;
    str->slen -= (p - str->ptr);
    str->ptr = p;
    return str;
}

bstr_t* bstrrtrim( bstr_t *str )
{
    char *end = str->ptr + str->slen;
    register char *p = end - 1;
    while (p >= str->ptr && bisspace(*p))
        --p;
    str->slen -= ((end - p) - 1);
    return str;
}

char* bcreate_random_string(char *str, bsize_t len)
{
    unsigned i;
    char *p = str;

    BASE_CHECK_STACK();

    for (i=0; i<len/8; ++i) {
	buint32_t val = brand();
	bval_to_hex_digit( (val & 0xFF000000) >> 24, p+0 );
	bval_to_hex_digit( (val & 0x00FF0000) >> 16, p+2 );
	bval_to_hex_digit( (val & 0x0000FF00) >>  8, p+4 );
	bval_to_hex_digit( (val & 0x000000FF) >>  0, p+6 );
	p += 8;
    }
    for (i=i * 8; i<len; ++i) {
	*p++ = bhex_digits[ brand() & 0x0F ];
    }
    return str;
}

long bstrtol(const bstr_t *str)
{
    BASE_CHECK_STACK();

    if (str->slen > 0 && (str->ptr[0] == '+' || str->ptr[0] == '-')) {
        bstr_t s;
        
        s.ptr = str->ptr + 1;
        s.slen = str->slen - 1;
        return (str->ptr[0] == '-'? -(long)bstrtoul(&s) : bstrtoul(&s));
    } else
        return bstrtoul(str);
}


bstatus_t bstrtol2(const bstr_t *str, long *value)
{
    bstr_t s;
    unsigned long retval = 0;
    bbool_t is_negative = BASE_FALSE;
    int rc = 0;

    BASE_CHECK_STACK();

    if (!str || !value) {
        return BASE_EINVAL;
    }

    s = *str;
    bstrltrim(&s);

    if (s.slen == 0)
        return BASE_EINVAL;

    if (s.ptr[0] == '+' || s.ptr[0] == '-') {
        is_negative = (s.ptr[0] == '-');
        s.ptr += 1;
        s.slen -= 1;
    }

    rc = bstrtoul3(&s, &retval, 10);
    if (rc == BASE_EINVAL) {
        return rc;
    } else if (rc != BASE_SUCCESS) {
        *value = is_negative ? BASE_MINLONG : BASE_MAXLONG;
        return is_negative ? BASE_ETOOSMALL : BASE_ETOOBIG;
    }

    if (retval > BASE_MAXLONG && !is_negative) {
        *value = BASE_MAXLONG;
        return BASE_ETOOBIG;
    }

    if (retval > (BASE_MAXLONG + 1UL) && is_negative) {
        *value = BASE_MINLONG;
        return BASE_ETOOSMALL;
    }

    *value = is_negative ? -(long)retval : retval;

    return BASE_SUCCESS;
}

unsigned long bstrtoul(const bstr_t *str)
{
    unsigned long value;
    unsigned i;

    BASE_CHECK_STACK();

    value = 0;
    for (i=0; i<(unsigned)str->slen; ++i) {
	if (!bisdigit(str->ptr[i]))
	    break;
	value = value * 10 + (str->ptr[i] - '0');
    }
    return value;
}

unsigned long bstrtoul2(const bstr_t *str, bstr_t *endptr,
				  unsigned base)
{
    unsigned long value;
    unsigned i;

    BASE_CHECK_STACK();

    value = 0;
    if (base <= 10) {
	for (i=0; i<(unsigned)str->slen; ++i) {
	    unsigned c = (str->ptr[i] - '0');
	    if (c >= base)
		break;
	    value = value * base + c;
	}
    } else if (base == 16) {
	for (i=0; i<(unsigned)str->slen; ++i) {
	    if (!bisxdigit(str->ptr[i]))
		break;
	    value = value * 16 + bhex_digit_to_val(str->ptr[i]);
	}
    } else {
	bassert(!"Unsupported base");
	i = 0;
	value = 0xFFFFFFFFUL;
    }

    if (endptr) {
	endptr->ptr = str->ptr + i;
	endptr->slen = str->slen - i;
    }

    return value;
}

bstatus_t bstrtoul3(const bstr_t *str, unsigned long *value,
				unsigned base)
{
    bstr_t s;
    unsigned i;

    BASE_CHECK_STACK();

    if (!str || !value) {
        return BASE_EINVAL;
    }

    s = *str;
    bstrltrim(&s);

    if (s.slen == 0 || s.ptr[0] < '0' ||
	(base <= 10 && (unsigned)s.ptr[0] > ('0' - 1) + base) ||
	(base == 16 && !bisxdigit(s.ptr[0])))
    {
        return BASE_EINVAL;
    }

    *value = 0;
    if (base <= 10) {
	for (i=0; i<(unsigned)s.slen; ++i) {
	    unsigned c = s.ptr[i] - '0';
	    if (s.ptr[i] < '0' || (unsigned)s.ptr[i] > ('0' - 1) + base) {
		break;
	    }
	    if (*value > BASE_MAXULONG / base) {
		*value = BASE_MAXULONG;
		return BASE_ETOOBIG;
	    }

	    *value *= base;
	    if ((BASE_MAXULONG - *value) < c) {
		*value = BASE_MAXULONG;
		return BASE_ETOOBIG;
	    }
	    *value += c;
	}
    } else if (base == 16) {
	for (i=0; i<(unsigned)s.slen; ++i) {
	    unsigned c = bhex_digit_to_val(s.ptr[i]);
	    if (!bisxdigit(s.ptr[i]))
		break;

	    if (*value > BASE_MAXULONG / base) {
		*value = BASE_MAXULONG;
		return BASE_ETOOBIG;
	    }
	    *value *= base;
	    if ((BASE_MAXULONG - *value) < c) {
		*value = BASE_MAXULONG;
		return BASE_ETOOBIG;
	    }
	    *value += c;
	}
    } else {
	bassert(!"Unsupported base");
	return BASE_EINVAL;
    }
    return BASE_SUCCESS;
}

float bstrtof(const bstr_t *str)
{
    bstr_t part;
    char *pdot;
    float val;

    if (str->slen == 0)
	return 0;

    pdot = (char*)bmemchr(str->ptr, '.', str->slen);
    part.ptr = str->ptr;
    part.slen = pdot ? pdot - str->ptr : str->slen;

    if (part.slen)
	val = (float)bstrtol(&part);
    else
	val = 0;

    if (pdot) {
	part.ptr = pdot + 1;
	part.slen = (str->ptr + str->slen - pdot - 1);
	if (part.slen) {
	    bstr_t endptr;
	    float fpart, fdiv;
	    int i;
	    fpart = (float)bstrtoul2(&part, &endptr, 10);
	    fdiv = 1.0;
	    for (i=0; i<(part.slen - endptr.slen); ++i)
		    fdiv = fdiv * 10;
	    if (val >= 0)
		val += (fpart / fdiv);
	    else
		val -= (fpart / fdiv);
	}
    }
    return val;
}

int butoa(unsigned long val, char *buf)
{
    return butoa_pad(val, buf, 0, 0);
}

int butoa_pad( unsigned long val, char *buf, int min_dig, int pad)
{
    char *p;
    int len;

    BASE_CHECK_STACK();

    p = buf;
    do {
        unsigned long digval = (unsigned long) (val % 10);
        val /= 10;
        *p++ = (char) (digval + '0');
    } while (val > 0);

    len = (int)(p-buf);
    while (len < min_dig) {
	*p++ = (char)pad;
	++len;
    }
    *p-- = '\0';

    do {
        char temp = *p;
        *p = *buf;
        *buf = temp;
        --p;
        ++buf;
    } while (buf < p);

    return len;
}
