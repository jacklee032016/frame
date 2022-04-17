/* 
 *
 */
#ifndef __UTIL_SHA1_H__
#define __UTIL_SHA1_H__

/**
 * @brief SHA1 encryption implementation
 */

#include <_baseTypes.h>

BASE_BEGIN_DECL

/**
 * @defgroup UTIL_SHA1 SHA1
 * @ingroup UTIL_ENCRYPTION
 * @{
 */

/** SHA1 context */
typedef struct bsha1_context
{
    buint32_t state[5];	/**< State  */
    buint32_t count[2];	/**< Count  */
    buint8_t	buffer[64];	/**< Buffer */
} bsha1_context;

/** SHA1 digest size is 20 bytes */
#define BASE_SHA1_DIGEST_SIZE	20


/** Initialize the algorithm. 
 *  @param ctx		SHA1 context.
 */
void bsha1_init(bsha1_context *ctx);

/** Append a stream to the message. 
 *  @param ctx		SHA1 context.
 *  @param data		Data.
 *  @param nbytes	Length of data.
 */
void bsha1_update(bsha1_context *ctx, 
			     const buint8_t *data, 
			     const bsize_t nbytes);

/** Finish the message and return the digest. 
 *  @param ctx		SHA1 context.
 *  @param digest	16 byte digest.
 */
void bsha1_final(bsha1_context *ctx, 
			    buint8_t digest[BASE_SHA1_DIGEST_SIZE]);

BASE_END_DECL

#endif

