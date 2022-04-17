/* 
 *
 */
#ifndef __UTIL_STRING_H__
#define __UTIL_STRING_H__

/**
 * @brief More string functions.
 */

#include <_baseTypes.h>
#include <utilScanner.h>

/**
 * @defgroup UTIL_STRING String Escaping Utilities
 * @{
 */

BASE_BEGIN_DECL

/**
 * Unescape string. If source string does not contain any escaped
 * characters, the function would simply return the original string.
 * Otherwise a new string will be allocated.
 *
 * @param pool	    Pool to allocate the string.
 * @param src	    Source string to unescape.
 *
 * @return	    String with no escaped characters.
 */
bstr_t bstr_unescape( bpool_t *pool, const bstr_t *src);

/**
 * Unescape string to destination.
 *
 * @param dst	    Target string.
 * @param src	    Source string.
 *
 * @return	    Target string.
 */
bstr_t* bstrcpy_unescape(bstr_t *dst, const bstr_t *src);

/**
 * Copy string to destination while escaping reserved characters, up to
 * the specified maximum length.
 *
 * @param dst	    Target string.
 * @param src	    Source string.
 * @param max	    Maximum length to copy to target string.
 * @param unres	    Unreserved characters, which are allowed to appear 
 *		    unescaped.
 *
 * @return	    The target string if all characters have been copied 
 *		    successfully, or NULL if there's not enough buffer to
 *		    escape the strings.
 */
bstr_t* bstrncpy_escape(bstr_t *dst, const bstr_t *src,
				     bssize_t max, const bcis_t *unres);


/**
 * Copy string to destination while escaping reserved characters, up to
 * the specified maximum length.
 *
 * @param dst	    Target string.
 * @param src	    Source string.
 * @param max	    Maximum length to copy to target string.
 * @param unres	    Unreserved characters, which are allowed to appear 
 *		    unescaped.
 *
 * @return	    The length of the destination, or -1 if there's not
 *		    enough buffer.
 */
bssize_t bstrncpy2_escape(char *dst, const bstr_t *src,
				       bssize_t max, const bcis_t *unres);

BASE_END_DECL


#endif

