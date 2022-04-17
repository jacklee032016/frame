/* 
 *
 */
#ifndef __BASE_GUID_H__
#define __BASE_GUID_H__

/**
 * @brief GUID Globally Unique Identifier.
 */
#include <_baseTypes.h>


BASE_BEGIN_DECL

/**
 * @defgroup BASE_DS Data Structure.
 */
/**
 * @defgroup BASE_GUID Globally Unique Identifier
 * @ingroup BASE_DS
 * @{
 *
 * This module provides API to create string that is globally unique.
 * If application doesn't require that strong requirement, it can just
 * use #bcreate_random_string() instead.
 */


/**
 * BASE_GUID_STRING_LENGTH specifies length of GUID string. The value is
 * dependent on the algorithm used internally to generate the GUID string.
 * If real GUID generator is used, then the length will be between 32 and
 * 36 bytes. Application should not assume which algorithm will
 * be used by GUID generator.
 *
 * Regardless of the actual length of the GUID, it will not exceed
 * BASE_GUID_MAX_LENGTH characters.
 *
 * @see bGUID_STRING_LENGTH()
 * @see BASE_GUID_MAX_LENGTH
 */
extern	const unsigned	BASE_GUID_STRING_LENGTH;

/**
 * Get #BASE_GUID_STRING_LENGTH constant.
 */
unsigned bGUID_STRING_LENGTH(void);

/**
 * BASE_GUID_MAX_LENGTH specifies the maximum length of GUID string,
 * regardless of which algorithm to use.
 */
#define BASE_GUID_MAX_LENGTH  36

/**
 * Create a globally unique string, which length is BASE_GUID_STRING_LENGTH
 * characters. Caller is responsible for preallocating the storage used
 * in the string.
 *
 * @param str       The string to store the result.
 *
 * @return          The string.
 */
bstr_t* bgenerate_unique_string(bstr_t *str);

/**
 * Create a globally unique string in lowercase, which length is
 * BASE_GUID_STRING_LENGTH characters. Caller is responsible for preallocating
 * the storage used in the string.
 *
 * @param str       The string to store the result.
 *
 * @return          The string.
 */
bstr_t* bgenerate_unique_string_lower(bstr_t *str);

/**
 * Generate a unique string.
 *
 * @param pool	    Pool to allocate memory from.
 * @param str	    The string.
 */
void bcreate_unique_string(bpool_t *pool, bstr_t *str);

/**
 * Generate a unique string in lowercase.
 *
 * @param pool	    Pool to allocate memory from.
 * @param str	    The string.
 */
void bcreate_unique_string_lower(bpool_t *pool, bstr_t *str);


BASE_END_DECL

#endif

