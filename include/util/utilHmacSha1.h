/* 
 *
 */
#ifndef __UTIL_HMAC_SHA1_H__
#define __UTIL_HMAC_SHA1_H__

/**
 * @brief HMAC SHA1 Message Authentication
 */

#include <_baseTypes.h>
#include <utilSha1.h>

BASE_BEGIN_DECL

/**
 * @defgroup UTIL_HMAC_SHA1 HMAC SHA1 Message Authentication
 * @ingroup UTIL_ENCRYPTION
 * @{
 *
 * This module contains the implementation of HMAC: Keyed-Hashing 
 * for Message Authentication, as described in RFC 2104.
 */

/**
 * The HMAC-SHA1 context used in the incremental HMAC calculation.
 */
typedef struct bhmac_sha1_context
{
    bsha1_context context;	/**< SHA1 context	    */
    buint8_t	    k_opad[64];	/**< opad xor-ed with key   */
} bhmac_sha1_context;


/**
 * Calculate HMAC-SHA1 digest for the specified input and key with this
 * single function call.
 *
 * @param input		Pointer to the input stream.
 * @param input_len	Length of input stream in bytes.
 * @param key		Pointer to the authentication key.
 * @param key_len	Length of the authentication key.
 * @param digest	Buffer to be filled with HMAC SHA1 digest.
 */
void bhmac_sha1(const buint8_t *input, unsigned input_len, 
			   const buint8_t *key, unsigned key_len, 
			   buint8_t digest[20]);


/**
 * Initiate HMAC-SHA1 context for incremental hashing.
 *
 * @param hctx		HMAC-SHA1 context.
 * @param key		Pointer to the authentication key.
 * @param key_len	Length of the authentication key.
 */
void bhmac_sha1_init(bhmac_sha1_context *hctx, 
			        const buint8_t *key, unsigned key_len);

/**
 * Append string to the message.
 *
 * @param hctx		HMAC-SHA1 context.
 * @param input		Pointer to the input stream.
 * @param input_len	Length of input stream in bytes.
 */
void bhmac_sha1_update(bhmac_sha1_context *hctx,
				  const buint8_t *input, 
				  unsigned input_len);

/**
 * Finish the message and return the digest. 
 *
 * @param hctx		HMAC-SHA1 context.
 * @param digest	Buffer to be filled with HMAC SHA1 digest.
 */
void bhmac_sha1_final(bhmac_sha1_context *hctx,
				 buint8_t digest[20]);


BASE_END_DECL


#endif

