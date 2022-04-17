/* 
 *
 */
#include <utilString.h>
#include <baseCtype.h>
#include <baseString.h>
#include <basePool.h>

bstr_t bstr_unescape( bpool_t *pool, const bstr_t *src_str)
{
    char *src = src_str->ptr;
    char *end = src + src_str->slen;
    bstr_t dst_str;
    char *dst;
    
    if (bstrchr(src_str, '%')==NULL)
	return *src_str;

    dst = dst_str.ptr = (char*) bpool_alloc(pool, src_str->slen);

    while (src != end) {
	if (*src == '%' && src < end-2 && bisxdigit(*(src+1)) && 
	    bisxdigit(*(src+2))) 
	{
	    *dst = (buint8_t) ((bhex_digit_to_val(*(src+1)) << 4) + 
				 bhex_digit_to_val(*(src+2)));
	    ++dst;
	    src += 3;
	} else {
	    *dst++ = *src++;
	}
    }
    dst_str.slen = dst - dst_str.ptr;
    return dst_str;
}

bstr_t* bstrcpy_unescape(bstr_t *dst_str,
				     const bstr_t *src_str)
{
    const char *src = src_str->ptr;
    const char *end = src + src_str->slen;
    char *dst = dst_str->ptr;
    
    while (src != end) {
	if (*src == '%' && src < end-2) {
	    *dst = (buint8_t) ((bhex_digit_to_val(*(src+1)) << 4) + 
				 bhex_digit_to_val(*(src+2)));
	    ++dst;
	    src += 3;
	} else {
	    *dst++ = *src++;
	}
    }
    dst_str->slen = dst - dst_str->ptr;
    return dst_str;
}

bssize_t bstrncpy2_escape( char *dst_str, const bstr_t *src_str,
				       bssize_t max, const bcis_t *unres)
{
    const char *src = src_str->ptr;
    const char *src_end = src + src_str->slen;
    char *dst = dst_str;
    char *dst_end = dst + max;

    if (max < src_str->slen)
	return -1;

    while (src != src_end && dst != dst_end) {
	if (bcis_match(unres, *src)) {
	    *dst++ = *src++;
	} else {
	    if (dst < dst_end-2) {
		*dst++ = '%';
		bval_to_hex_digit(*src, dst);
		dst+=2;
		++src;
	    } else {
		break;
	    }
	}
    }

    return src==src_end ? dst-dst_str : -1;
}

bstr_t* bstrncpy_escape(bstr_t *dst_str, 
				    const bstr_t *src_str,
				    bssize_t max, const bcis_t *unres)
{
    dst_str->slen = bstrncpy2_escape(dst_str->ptr, src_str, max, unres);
    return dst_str->slen < 0 ? NULL : dst_str;
}

