/*
 *
 */
#ifndef __UTIL_BASE64_H__
#define __UTIL_BASE64_H__

/**
 * @brief Base64 encoding and decoding
 */

#include <utilTypes.h>

BASE_BEGIN_DECL

/**
 * @defgroup UTIL_BASE64 Base64 Encoding/Decoding
 * @ingroup UTIL_ENCRYPTION
 * @{
 * This module implements base64 encoding and decoding.
 */

/**
 * Helper macro to calculate the approximate length required for base256 to
 * base64 conversion.
 */
#define BASE_BASE256_TO_BASE64_LEN(len)	(len * 4 / 3 + 3)

/**
 * Helper macro to calculate the approximage length required for base64 to
 * base256 conversion.
 */
#define BASE_BASE64_TO_BASE256_LEN(len)	(len * 3 / 4)


/**
 * Encode a buffer into base64 encoding.
 *
 * @param input	    The input buffer.
 * @param in_len    Size of the input buffer.
 * @param output    Output buffer. Caller must allocate this buffer with
 *		    the appropriate size.
 * @param out_len   On entry, it specifies the length of the output buffer. 
 *		    Upon return, this will be filled with the actual
 *		    length of the output buffer.
 *
 * @return	    BASE_SUCCESS on success.
 */
bstatus_t bbase64_encode(const buint8_t *input, int in_len,
				     char *output, int *out_len);


/**
 * Decode base64 string.
 *
 * @param input	    Input string.
 * @param out	    Buffer to store the output. Caller must allocate
 *		    this buffer with the appropriate size.
 * @param out_len   On entry, it specifies the length of the output buffer. 
 *		    Upon return, this will be filled with the actual
 *		    length of the output.
 */
bstatus_t bbase64_decode(const bstr_t *input, 
				      buint8_t *out, int *out_len);

BASE_END_DECL


#endif

