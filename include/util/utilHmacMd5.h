/* 
 *
 */
#ifndef __UTIL_HMAC_MD5_H__
#define __UTIL_HMAC_MD5_H__

/**
 * @brief HMAC MD5 Message Authentication
 */

/**
 * @defgroup UTIL_ENCRYPTION Encryption Algorithms
 */

#include <_baseTypes.h>
#include <utilMd5.h>

BASE_BEGIN_DECL

/**
 * @defgroup UTIL_HMAC_MD5 HMAC MD5 Message Authentication
 * @ingroup UTIL_ENCRYPTION
 * @{
 *
 * This module contains the implementation of HMAC: Keyed-Hashing 
 * for Message Authentication, as described in RFC 2104
 */

/**
 * The HMAC-MD5 context used in the incremental HMAC calculation.
 */
typedef struct bhmac_md5_context
{
    bmd5_context  context;	/**< MD5 context	    */
    buint8_t	    k_opad[64];	/**< opad xor-ed with key   */
} bhmac_md5_context;


/**
 * Calculate HMAC MD5 digest for the specified input and key.
 *
 * @param input		Pointer to the input stream.
 * @param input_len	Length of input stream in bytes.
 * @param key		Pointer to the authentication key.
 * @param key_len	Length of the authentication key.
 * @param digest	Buffer to be filled with HMAC MD5 digest.
 */
void bhmac_md5(const buint8_t *input, unsigned input_len, 
			  const buint8_t *key, unsigned key_len, 
			  buint8_t digest[16]);


/**
 * Initiate HMAC-MD5 context for incremental hashing.
 *
 * @param hctx		HMAC-MD5 context.
 * @param key		Pointer to the authentication key.
 * @param key_len	Length of the authentication key.
 */
void bhmac_md5_init(bhmac_md5_context *hctx, 
			       const buint8_t *key, unsigned key_len);

/**
 * Append string to the message.
 *
 * @param hctx		HMAC-MD5 context.
 * @param input		Pointer to the input stream.
 * @param input_len	Length of input stream in bytes.
 */
void bhmac_md5_update(bhmac_md5_context *hctx,
				 const buint8_t *input, 
				 unsigned input_len);

/**
 * Finish the message and return the digest. 
 *
 * @param hctx		HMAC-MD5 context.
 * @param digest	Buffer to be filled with HMAC MD5 digest.
 */
void bhmac_md5_final(bhmac_md5_context *hctx,
				buint8_t digest[16]);


BASE_END_DECL

#endif

