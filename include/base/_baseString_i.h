/* 
 *
 */

#include <baseAssert.h>
#include <basePool.h>

BASE_IDEF(bstr_t) bstr(char *str)
{
    bstr_t dst;
    dst.ptr = str;
    dst.slen = str ? bansi_strlen(str) : 0;
    return dst;
}

BASE_IDEF(bstr_t*) bstrdup(bpool_t *pool,
			      bstr_t *dst,
			      const bstr_t *src)
{
    /* Without this, destination will be corrupted */
    if (dst == src)
	return dst;

    if (src->slen) {
	dst->ptr = (char*)bpool_alloc(pool, src->slen);
	bmemcpy(dst->ptr, src->ptr, src->slen);
    }
    dst->slen = src->slen;
    return dst;
}

BASE_IDEF(bstr_t*) bstrdup_with_null( bpool_t *pool,
					bstr_t *dst,
					const bstr_t *src)
{
    dst->ptr = (char*)bpool_alloc(pool, src->slen+1);
    if (src->slen) {
	bmemcpy(dst->ptr, src->ptr, src->slen);
    }
    dst->slen = src->slen;
    dst->ptr[dst->slen] = '\0';
    return dst;
}

BASE_IDEF(bstr_t*) bstrdup2(bpool_t *pool,
			      bstr_t *dst,
			      const char *src)
{
    dst->slen = src ? bansi_strlen(src) : 0;
    if (dst->slen) {
	dst->ptr = (char*)bpool_alloc(pool, dst->slen);
	bmemcpy(dst->ptr, src, dst->slen);
    } else {
	dst->ptr = NULL;
    }
    return dst;
}

BASE_IDEF(bstr_t*) bstrdup2_with_null( bpool_t *pool,
					 bstr_t *dst,
					 const char *src)
{
    dst->slen = src ? bansi_strlen(src) : 0;
    dst->ptr = (char*)bpool_alloc(pool, dst->slen+1);
    if (dst->slen) {
	bmemcpy(dst->ptr, src, dst->slen);
    }
    dst->ptr[dst->slen] = '\0';
    return dst;
}

BASE_IDEF(bstr_t) bstrdup3(bpool_t *pool, const char *src)
{
    bstr_t temp;
    bstrdup2(pool, &temp, src);
    return temp;
}

BASE_IDEF(bstr_t*) bstrassign( bstr_t *dst, bstr_t *src )
{
    dst->ptr = src->ptr;
    dst->slen = src->slen;
    return dst;
}

BASE_IDEF(bstr_t*) bstrcpy(bstr_t *dst, const bstr_t *src)
{
    dst->slen = src->slen;
    if (src->slen > 0)
	bmemcpy(dst->ptr, src->ptr, src->slen);
    return dst;
}

BASE_IDEF(bstr_t*) bstrcpy2(bstr_t *dst, const char *src)
{
    dst->slen = src ? bansi_strlen(src) : 0;
    if (dst->slen > 0)
	bmemcpy(dst->ptr, src, dst->slen);
    return dst;
}

BASE_IDEF(bstr_t*) bstrncpy( bstr_t *dst, const bstr_t *src, 
			       bssize_t max)
{
    bassert(max >= 0);
    if (max > src->slen) max = src->slen;
    if (max > 0)
	bmemcpy(dst->ptr, src->ptr, max);
    dst->slen = max;
    return dst;
}

BASE_IDEF(bstr_t*) bstrncpy_with_null( bstr_t *dst, const bstr_t *src,
					 bssize_t max)
{
    bassert(max > 0);

    if (max <= src->slen)
	max = max-1;
    else
	max = src->slen;

    bmemcpy(dst->ptr, src->ptr, max);
    dst->ptr[max] = '\0';
    dst->slen = max;
    return dst;
}


BASE_IDEF(int) bstrcmp( const bstr_t *str1, const bstr_t *str2)
{
    if (str1->slen == 0) {
	return str2->slen==0 ? 0 : -1;
    } else if (str2->slen == 0) {
	return 1;
    } else {
	bsize_t min = (str1->slen < str2->slen)? str1->slen : str2->slen;
	int res = bmemcmp(str1->ptr, str2->ptr, min);
	if (res == 0) {
	    return (str1->slen < str2->slen) ? -1 :
		    (str1->slen == str2->slen ? 0 : 1);
	} else {
	    return res;
	}
    }
}

BASE_IDEF(int) bstrncmp( const bstr_t *str1, const bstr_t *str2, 
			 bsize_t len)
{
    bstr_t copy1, copy2;

    if (len < (unsigned)str1->slen) {
	copy1.ptr = str1->ptr;
	copy1.slen = len;
	str1 = &copy1;
    }

    if (len < (unsigned)str2->slen) {
	copy2.ptr = str2->ptr;
	copy2.slen = len;
	str2 = &copy2;
    }

    return bstrcmp(str1, str2);
}

BASE_IDEF(int) bstrncmp2( const bstr_t *str1, const char *str2, 
			  bsize_t len)
{
    bstr_t copy2;

    if (str2) {
	copy2.ptr = (char*)str2;
	copy2.slen = bansi_strlen(str2);
    } else {
	copy2.slen = 0;
    }

    return bstrncmp(str1, &copy2, len);
}

BASE_IDEF(int) bstrcmp2( const bstr_t *str1, const char *str2 )
{
    bstr_t copy2;

    if (str2) {
	copy2.ptr = (char*)str2;
	copy2.slen = bansi_strlen(str2);
    } else {
	copy2.ptr = NULL;
	copy2.slen = 0;
    }

    return bstrcmp(str1, &copy2);
}

BASE_IDEF(int) bstricmp( const bstr_t *str1, const bstr_t *str2)
{
    if (str1->slen == 0) {
	return str2->slen==0 ? 0 : -1;
    } else if (str2->slen == 0) {
	return 1;
    } else {
	bsize_t min = (str1->slen < str2->slen)? str1->slen : str2->slen;
	int res = bansi_strnicmp(str1->ptr, str2->ptr, min);
	if (res == 0) {
	    return (str1->slen < str2->slen) ? -1 :
		    (str1->slen == str2->slen ? 0 : 1);
	} else {
	    return res;
	}
    }
}

#if defined(BASE_HAS_STRICMP_ALNUM) && BASE_HAS_STRICMP_ALNUM!=0
BASE_IDEF(int) strnicmp_alnum( const char *str1, const char *str2,
			     int len)
{
    if (len==0)
	return 0;
    else {
	register const buint32_t *p1 = (buint32_t*)str1, 
		                   *p2 = (buint32_t*)str2;
	while (len > 3 && (*p1 & 0x5F5F5F5F)==(*p2 & 0x5F5F5F5F))
	    ++p1, ++p2, len-=4;

	if (len > 3)
	    return -1;
#if defined(BASE_IS_LITTLE_ENDIAN) && BASE_IS_LITTLE_ENDIAN!=0
	else if (len==3)
	    return ((*p1 & 0x005F5F5F)==(*p2 & 0x005F5F5F)) ? 0 : -1;
	else if (len==2)
	    return ((*p1 & 0x00005F5F)==(*p2 & 0x00005F5F)) ? 0 : -1;
	else if (len==1)
	    return ((*p1 & 0x0000005F)==(*p2 & 0x0000005F)) ? 0 : -1;
#else
	else if (len==3)
	    return ((*p1 & 0x5F5F5F00)==(*p2 & 0x5F5F5F00)) ? 0 : -1;
	else if (len==2)
	    return ((*p1 & 0x5F5F0000)==(*p2 & 0x5F5F0000)) ? 0 : -1;
	else if (len==1)
	    return ((*p1 & 0x5F000000)==(*p2 & 0x5F000000)) ? 0 : -1;
#endif
	else 
	    return 0;
    }
}

BASE_IDEF(int) bstricmp_alnum(const bstr_t *str1, const bstr_t *str2)
{
    register int len = str1->slen;

    if (len != str2->slen) {
	return (len < str2->slen) ? -1 : 1;
    } else if (len == 0) {
	return 0;
    } else {
	register const buint32_t *p1 = (buint32_t*)str1->ptr, 
		                   *p2 = (buint32_t*)str2->ptr;
	while (len > 3 && (*p1 & 0x5F5F5F5F)==(*p2 & 0x5F5F5F5F))
	    ++p1, ++p2, len-=4;

	if (len > 3)
	    return -1;
#if defined(BASE_IS_LITTLE_ENDIAN) && BASE_IS_LITTLE_ENDIAN!=0
	else if (len==3)
	    return ((*p1 & 0x005F5F5F)==(*p2 & 0x005F5F5F)) ? 0 : -1;
	else if (len==2)
	    return ((*p1 & 0x00005F5F)==(*p2 & 0x00005F5F)) ? 0 : -1;
	else if (len==1)
	    return ((*p1 & 0x0000005F)==(*p2 & 0x0000005F)) ? 0 : -1;
#else
	else if (len==3)
	    return ((*p1 & 0x5F5F5F00)==(*p2 & 0x5F5F5F00)) ? 0 : -1;
	else if (len==2)
	    return ((*p1 & 0x5F5F0000)==(*p2 & 0x5F5F0000)) ? 0 : -1;
	else if (len==1)
	    return ((*p1 & 0x5F000000)==(*p2 & 0x5F000000)) ? 0 : -1;
#endif
	else 
	    return 0;
    }
}
#endif	/* BASE_HAS_STRICMP_ALNUM */

BASE_IDEF(int) bstricmp2( const bstr_t *str1, const char *str2)
{
    bstr_t copy2;

    if (str2) {
	copy2.ptr = (char*)str2;
	copy2.slen = bansi_strlen(str2);
    } else {
	copy2.ptr = NULL;
	copy2.slen = 0;
    }

    return bstricmp(str1, &copy2);
}

BASE_IDEF(int) bstrnicmp( const bstr_t *str1, const bstr_t *str2, 
			  bsize_t len)
{
    bstr_t copy1, copy2;

    if (len < (unsigned)str1->slen) {
	copy1.ptr = str1->ptr;
	copy1.slen = len;
	str1 = &copy1;
    }

    if (len < (unsigned)str2->slen) {
	copy2.ptr = str2->ptr;
	copy2.slen = len;
	str2 = &copy2;
    }

    return bstricmp(str1, str2);
}

BASE_IDEF(int) bstrnicmp2( const bstr_t *str1, const char *str2, 
			   bsize_t len)
{
    bstr_t copy2;

    if (str2) {
	copy2.ptr = (char*)str2;
	copy2.slen = bansi_strlen(str2);
    } else {
	copy2.slen = 0;
    }

    return bstrnicmp(str1, &copy2, len);
}

BASE_IDEF(void) bstrcat(bstr_t *dst, const bstr_t *src)
{
    if (src->slen) {
	bmemcpy(dst->ptr + dst->slen, src->ptr, src->slen);
	dst->slen += src->slen;
    }
}

BASE_IDEF(void) bstrcat2(bstr_t *dst, const char *str)
{
    bsize_t len = str? bansi_strlen(str) : 0;
    if (len) {
	bmemcpy(dst->ptr + dst->slen, str, len);
	dst->slen += len;
    }
}

BASE_IDEF(bstr_t*) bstrtrim( bstr_t *str )
{
    bstrltrim(str);
    bstrrtrim(str);
    return str;
}

