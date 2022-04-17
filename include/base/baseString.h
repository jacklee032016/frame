/* 
 *
 */
 
#ifndef __BASE_STRING_H__
#define __BASE_STRING_H__

#include <_baseTypes.h>
#include <compat/string.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_PSTR String Operations
 * @ingroup BASE_DS
 * @{
 * This module provides string manipulation API.
 *
 * \section bpstr_not_null_sec  String is NOT Null Terminated!
 *
 * That is the first information that developers need to know. Instead
 * of using normal C string, strings in  are represented as
 * bstr_t structure below:
 *
 * <pre>
 *   typedef struct bstr_t
 *   {
 *       char      *ptr;
 *       bsize_t  slen;
 *   } bstr_t;
 * </pre>
 *
 * There are some advantages of using this approach:
 *  - the string can point to arbitrary location in memory even
 *    if the string in that location is not null terminated. This is
 *    most usefull for text parsing, where the parsed text can just
 *    point to the original text in the input. If we use C string,
 *    then we will have to copy the text portion from the input
 *    to a string variable.
 *  - because the length of the string is known, string copy operation
 *    can be made more efficient.
 *
 * Most of APIs in  that expect or return string will represent
 * the string as bstr_t instead of normal C string.
 *
 * \section bpstr_examples_sec Examples
 *
 * For some examples, please see:
 *  - @ref page_baselib_string_test
 */

/**
 * Create string initializer from a normal C string.
 *
 * @param str	Null terminated string to be stored.
 *
 * @return	bstr_t.
 */
bstr_t bstr(char *str);

/**
 * Create constant string from normal C string.
 *
 * @param str	The string to be initialized.
 * @param s	Null terminated string.
 *
 * @return	bstr_t.
 */
BASE_INLINE_SPECIFIER const bstr_t* bcstr(bstr_t *str, const char *s)
{
    str->ptr = (char*)s;
    str->slen = s ? (bssize_t)strlen(s) : 0;
    return str;
}

/**
 * Set the pointer and length to the specified value.
 *
 * @param str	    the string.
 * @param ptr	    pointer to set.
 * @param length    length to set.
 *
 * @return the string.
 */
BASE_INLINE_SPECIFIER bstr_t* bstrset( bstr_t *str, char *ptr, bsize_t length)
{
    str->ptr = ptr;
    str->slen = (bssize_t)length;
    return str;
}

/**
 * Set the pointer and length of the string to the source string, which
 * must be NULL terminated.
 *
 * @param str	    the string.
 * @param src	    pointer to set.
 *
 * @return the string.
 */
BASE_INLINE_SPECIFIER bstr_t* bstrset2( bstr_t *str, char *src)
{
    str->ptr = src;
    str->slen = src ? (bssize_t)strlen(src) : 0;
    return str;
}

/**
 * Set the pointer and the length of the string.
 *
 * @param str	    The target string.
 * @param begin	    The start of the string.
 * @param end	    The end of the string.
 *
 * @return the target string.
 */
BASE_INLINE_SPECIFIER bstr_t* bstrset3( bstr_t *str, char *begin, char *end )
{
    str->ptr = begin;
    str->slen = (bssize_t)(end-begin);
    return str;
}

/**
 * Assign string.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 *
 * @return the target string.
 */
bstr_t* bstrassign( bstr_t *dst, bstr_t *src );

/**
 * Copy string contents.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 *
 * @return the target string.
 */
bstr_t* bstrcpy(bstr_t *dst, const bstr_t *src);

/**
 * Copy string contents.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 *
 * @return the target string.
 */
bstr_t* bstrcpy2(bstr_t *dst, const char *src);

/**
 * Copy source string to destination up to the specified max length.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 * @param max	    Maximum characters to copy.
 *
 * @return the target string.
 */
bstr_t* bstrncpy(bstr_t *dst, const bstr_t *src, 
			       bssize_t max);

/**
 * Copy source string to destination up to the specified max length,
 * and NULL terminate the destination. If source string length is
 * greater than or equal to max, then max-1 will be copied.
 *
 * @param dst	    The target string.
 * @param src	    The source string.
 * @param max	    Maximum characters to copy.
 *
 * @return the target string.
 */
bstr_t* bstrncpy_with_null(bstr_t *dst, const bstr_t *src,
					 bssize_t max);

/**
 * Duplicate string.
 *
 * @param pool	    The pool.
 * @param dst	    The string result.
 * @param src	    The string to duplicate.
 *
 * @return the string result.
 */
bstr_t* bstrdup(bpool_t *pool,
			      bstr_t *dst,
			      const bstr_t *src);

/**
 * Duplicate string and NULL terminate the destination string.
 *
 * @param pool	    The pool.
 * @param dst	    The string result.
 * @param src	    The string to duplicate.
 *
 * @return	    The string result.
 */
bstr_t* bstrdup_with_null(bpool_t *pool,
					bstr_t *dst,
					const bstr_t *src);

/**
 * Duplicate string.
 *
 * @param pool	    The pool.
 * @param dst	    The string result.
 * @param src	    The string to duplicate.
 *
 * @return the string result.
 */
bstr_t* bstrdup2(bpool_t *pool,
			       bstr_t *dst,
			       const char *src);

/**
 * Duplicate string and NULL terminate the destination string.
 *
 * @param pool	    The pool.
 * @param dst	    The string result.
 * @param src	    The string to duplicate.
 *
 * @return	    The string result.
 */
bstr_t* bstrdup2_with_null(bpool_t *pool,
					 bstr_t *dst,
					 const char *src);


/**
 * Duplicate string.
 *
 * @param pool	    The pool.
 * @param src	    The string to duplicate.
 *
 * @return the string result.
 */
bstr_t bstrdup3(bpool_t *pool, const char *src);

/**
 * Return the length of the string.
 *
 * @param str	    The string.
 *
 * @return the length of the string.
 */
BASE_INLINE_SPECIFIER bsize_t bstrlen( const bstr_t *str )
{
    return str->slen;
}

/**
 * Return the pointer to the string data.
 *
 * @param str	    The string.
 *
 * @return the pointer to the string buffer.
 */
BASE_INLINE_SPECIFIER const char* bstrbuf( const bstr_t *str )
{
    return str->ptr;
}

/**
 * Compare strings. 
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
int bstrcmp( const bstr_t *str1, const bstr_t *str2);

/**
 * Compare strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
int bstrcmp2( const bstr_t *str1, const char *str2 );

/**
 * Compare strings. 
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
int bstrncmp( const bstr_t *str1, const bstr_t *str2, 
			  bsize_t len);

/**
 * Compare strings. 
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
int bstrncmp2( const bstr_t *str1, const char *str2, 
			   bsize_t len);

/**
 * Perform case-insensitive comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is equal to str2
 *      - > 0 if str1 is greater than str2
 */
int bstricmp(const bstr_t *str1, const bstr_t *str2);

/**
 * Perform lowercase comparison to the strings which consists of only
 * alnum characters. More over, it will only return non-zero if both
 * strings are not equal, not the usual negative or positive value.
 *
 * If non-alnum inputs are given, then the function may mistakenly 
 * treat two strings as equal.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The length to compare.
 *
 * @return 
 *      - 0	    if str1 is equal to str2
 *      - (-1)	    if not equal.
 */
#if defined(BASE_HAS_STRICMP_ALNUM) && BASE_HAS_STRICMP_ALNUM!=0
int strnicmp_alnum(const char *str1, const char *str2,
			     int len);
#else
#define strnicmp_alnum	bansi_strnicmp
#endif

/**
 * Perform lowercase comparison to the strings which consists of only
 * alnum characters. More over, it will only return non-zero if both
 * strings are not equal, not the usual negative or positive value.
 *
 * If non-alnum inputs are given, then the function may mistakenly 
 * treat two strings as equal.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *      - 0	    if str1 is equal to str2
 *      - (-1)	    if not equal.
 */
#if defined(BASE_HAS_STRICMP_ALNUM) && BASE_HAS_STRICMP_ALNUM!=0
int bstricmp_alnum(const bstr_t *str1, const bstr_t *str2);
#else
#define bstricmp_alnum    bstricmp
#endif

/**
 * Perform case-insensitive comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
int bstricmp2( const bstr_t *str1, const char *str2);

/**
 * Perform case-insensitive comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
int bstrnicmp( const bstr_t *str1, const bstr_t *str2, 
			   bsize_t len);

/**
 * Perform case-insensitive comparison to the strings.
 *
 * @param str1	    The string to compare.
 * @param str2	    The string to compare.
 * @param len	    The maximum number of characters to compare.
 *
 * @return 
 *	- < 0 if str1 is less than str2
 *      - 0   if str1 is identical to str2
 *      - > 0 if str1 is greater than str2
 */
int bstrnicmp2( const bstr_t *str1, const char *str2, 
			    bsize_t len);

/**
 * Concatenate strings.
 *
 * @param dst	    The destination string.
 * @param src	    The source string.
 */
void bstrcat(bstr_t *dst, const bstr_t *src);


/**
 * Concatenate strings.
 *
 * @param dst	    The destination string.
 * @param src	    The source string.
 */
void bstrcat2(bstr_t *dst, const char *src);


/**
 * Finds a character in a string.
 *
 * @param str	    The string.
 * @param chr	    The character to find.
 *
 * @return the pointer to first character found, or NULL.
 */
BASE_INLINE_SPECIFIER char* bstrchr( const bstr_t *str, int chr)
{
    if (str->slen == 0)
	return NULL;
    return (char*) memchr((char*)str->ptr, chr, str->slen);
}


/**
 * Find the first index of character, in a string, that does not belong to a 
 * set of characters.
 *
 * @param str	    The string.
 * @param set_char  The string containing the set of characters. 
 *
 * @return the index of the first character in the str that doesn't belong to 
 * set_char. If str starts with a character not in set_char, return 0.
 */
bssize_t bstrspn(const bstr_t *str, const bstr_t *set_char);


/**
 * Find the first index of character, in a string, that does not belong to a
 * set of characters.
 *
 * @param str	    The string.
 * @param set_char  The string containing the set of characters.
 *
 * @return the index of the first character in the str that doesn't belong to
 * set_char. If str starts with a character not in set_char, return 0.
 */
bssize_t bstrspn2(const bstr_t *str, const char *set_char);


/**
 * Find the first index of character, in a string, that belong to a set of 
 * characters.
 *
 * @param str	    The string.
 * @param set_char  The string containing the set of characters.
 *
 * @return the index of the first character in the str that belong to
 * set_char. If no match is found, return the length of str.
 */
bssize_t bstrcspn(const bstr_t *str, const bstr_t *set_char);


/**
 * Find the first index of character, in a string, that belong to a set of
 * characters.
 *
 * @param str	    The string.
 * @param set_char  The string containing the set of characters.
 *
 * @return the index of the first character in the str that belong to
 * set_char. If no match is found, return the length of str.
 */
bssize_t bstrcspn2(const bstr_t *str, const char *set_char);


/**
 * Find tokens from a string using the delimiter.
 *
 * @param str	    The string.
 * @param delim	    The string containing the delimiter. It might contain 
 *		    multiple character treated as unique set. If same character
 *		    was found on the set, it will be skipped.
 * @param tok	    The string containing the token.
 * @param start_idx The search will start from this index.
 *
 * @return the index of token from the str, or the length of the str
 * if the token is not found.
 */
bssize_t bstrtok(const bstr_t *str, const bstr_t *delim,
			      bstr_t *tok, bsize_t start_idx);


/**
 * Find tokens from a string using the delimiter.
 *
 * @param str	    The string.
 * @param delim	    The string containing the delimiter. It might contain
 *		    multiple character treated as unique set. If same character
 *		    was found on the set, it will be skipped.
 * @param tok	    The string containing the token.
 * @param start_idx The search will start from this index.
 *
 * @return the index of token from the str, or the length of the str
 * if the token is not found.
 */
bssize_t bstrtok2(const bstr_t *str, const char *delim,
			       bstr_t *tok, bsize_t start_idx);


/**
 * Find the occurence of a substring substr in string str.
 *
 * @param str	    The string to search.
 * @param substr    The string to search fo.
 *
 * @return the pointer to the position of substr in str, or NULL. Note
 *         that if str is not NULL terminated, the returned pointer
 *         is pointing to non-NULL terminated string.
 */
char* bstrstr(const bstr_t *str, const bstr_t *substr);

/**
 * Performs substring lookup like bstrstr() but ignores the case of
 * both strings.
 *
 * @param str	    The string to search.
 * @param substr    The string to search fo.
 *
 * @return the pointer to the position of substr in str, or NULL. Note
 *         that if str is not NULL terminated, the returned pointer
 *         is pointing to non-NULL terminated string.
 */
char* bstristr(const bstr_t *str, const bstr_t *substr);

/**
 * Remove (trim) leading whitespaces from the string.
 *
 * @param str	    The string.
 *
 * @return the string.
 */
bstr_t* bstrltrim( bstr_t *str );

/**
 * Remove (trim) the trailing whitespaces from the string.
 *
 * @param str	    The string.
 *
 * @return the string.
 */
bstr_t* bstrrtrim( bstr_t *str );

/**
 * Remove (trim) leading and trailing whitespaces from the string.
 *
 * @param str	    The string.
 *
 * @return the string.
 */
bstr_t* bstrtrim( bstr_t *str );

/**
 * Initialize the buffer with some random string. Note that the 
 * generated string is not NULL terminated.
 *
 * @param str	    the string to store the result.
 * @param length    the length of the random string to generate.
 *
 * @return the string.
 */
char* bcreate_random_string(char *str, bsize_t length);

/**
 * Convert string to signed integer. The conversion will stop as
 * soon as non-digit character is found or all the characters have
 * been processed.
 *
 * @param str	the string.
 *
 * @return the integer.
 */
long bstrtol(const bstr_t *str);

/**
 * Convert string to signed long integer. The conversion will stop as
 * soon as non-digit character is found or all the characters have
 * been processed.
 *
 * @param str   the string.
 * @param value Pointer to a long to receive the value.
 *
 * @return BASE_SUCCESS if successful.  Otherwise:
 *         BASE_ETOOSMALL if the value was an impossibly long negative number.
 *         In this case *value will be set to LONG_MIN.
 *         \n
 *         BASE_ETOOBIG if the value was an impossibly long positive number.
 *         In this case, *value will be set to LONG_MAX.
 *         \n
 *         BASE_EINVAL if the input string was NULL, the value pointer was NULL 
 *         or the input string could not be parsed at all such as starting with
 *         a character other than a '+', '-' or not in the '0' - '9' range.
 *         In this case, *value will be left untouched.
 */
bstatus_t bstrtol2(const bstr_t *str, long *value);


/**
 * Convert string to unsigned integer. The conversion will stop as
 * soon as non-digit character is found or all the characters have
 * been processed.
 *
 * @param str	the string.
 *
 * @return the unsigned integer.
 */
unsigned long bstrtoul(const bstr_t *str);

/**
 * Convert strings to an unsigned long-integer value.
 * This function stops reading the string input either when the number
 * of characters has exceeded the length of the input or it has read 
 * the first character it cannot recognize as part of a number, that is
 * a character greater than or equal to base. 
 *
 * @param str	    The input string.
 * @param endptr    Optional pointer to receive the remainder/unparsed
 *		    portion of the input.
 * @param base	    Number base to use.
 *
 * @return the unsigned integer number.
 */
unsigned long bstrtoul2(const bstr_t *str, bstr_t *endptr,
				   unsigned base);

/**
 * Convert string to unsigned long integer. The conversion will stop as
 * soon as non-digit character is found or all the characters have
 * been processed.
 *
 * @param str       The input string.
 * @param value     Pointer to an unsigned long to receive the value.
 * @param base	    Number base to use.
 *
 * @return BASE_SUCCESS if successful.  Otherwise:
 *         BASE_ETOOBIG if the value was an impossibly long positive number.
 *         In this case, *value will be set to ULONG_MAX.
 *         \n
 *         BASE_EINVAL if the input string was NULL, the value pointer was NULL 
 *         or the input string could not be parsed at all such as starting 
 *         with a character outside the base character range.  In this case,
 *         *value will be left untouched.
 */
bstatus_t bstrtoul3(const bstr_t *str, unsigned long *value,
				 unsigned base);

/**
 * Convert string to float.
 *
 * @param str	the string.
 *
 * @return the value.
 */
float bstrtof(const bstr_t *str);

/**
 * Utility to convert unsigned integer to string. Note that the
 * string will be NULL terminated.
 *
 * @param val	    the unsigned integer value.
 * @param buf	    the buffer
 *
 * @return the number of characters written
 */
int butoa(unsigned long val, char *buf);

/**
 * Convert unsigned integer to string with minimum digits. Note that the
 * string will be NULL terminated.
 *
 * @param val	    The unsigned integer value.
 * @param buf	    The buffer.
 * @param min_dig   Minimum digits to be printed, or zero to specify no
 *		    minimum digit.
 * @param pad	    The padding character to be put in front of the string
 *		    when the digits is less than minimum.
 *
 * @return the number of characters written.
 */
int butoa_pad( unsigned long val, char *buf, int min_dig, int pad);


/**
 * Fill the memory location with zero.
 *
 * @param dst	    The destination buffer.
 * @param size	    The number of bytes.
 */
BASE_INLINE_SPECIFIER void bbzero(void *dst, bsize_t size)
{
#if defined(BASE_HAS_BZERO) && BASE_HAS_BZERO!=0
    bzero(dst, size);
#else
    memset(dst, 0, size);
#endif
}


/**
 * Fill the memory location with value.
 *
 * @param dst	    The destination buffer.
 * @param c	    Character to set.
 * @param size	    The number of characters.
 *
 * @return the value of dst.
 */
BASE_INLINE_SPECIFIER void* bmemset(void *dst, int c, bsize_t size)
{
    return memset(dst, c, size);
}

/**
 * Copy buffer.
 *
 * @param dst	    The destination buffer.
 * @param src	    The source buffer.
 * @param size	    The size to copy.
 *
 * @return the destination buffer.
 */
BASE_INLINE_SPECIFIER void* bmemcpy(void *dst, const void *src, bsize_t size)
{
    return memcpy(dst, src, size);
}

/**
 * Move memory.
 *
 * @param dst	    The destination buffer.
 * @param src	    The source buffer.
 * @param size	    The size to copy.
 *
 * @return the destination buffer.
 */
BASE_INLINE_SPECIFIER void* bmemmove(void *dst, const void *src, bsize_t size)
{
    return memmove(dst, src, size);
}

/**
 * Compare buffers.
 *
 * @param buf1	    The first buffer.
 * @param buf2	    The second buffer.
 * @param size	    The size to compare.
 *
 * @return negative, zero, or positive value.
 */
BASE_INLINE_SPECIFIER int bmemcmp(const void *buf1, const void *buf2, bsize_t size)
{
    return memcmp(buf1, buf2, size);
}

/**
 * Find character in the buffer.
 *
 * @param buf	    The buffer.
 * @param c	    The character to find.
 * @param size	    The size to check.
 *
 * @return the pointer to location where the character is found, or NULL if
 *         not found.
 */
BASE_INLINE_SPECIFIER void* bmemchr(const void *buf, int c, bsize_t size)
{
    return (void*)memchr((void*)buf, c, size);
}


#if BASE_FUNCTIONS_ARE_INLINED
#include <_baseString_i>
#endif

BASE_END_DECL

#endif

