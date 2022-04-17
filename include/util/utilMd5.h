/*
 *
 */
#ifndef __UTIL_MD5_H__
#define __UTIL_MD5_H__


#include <_baseTypes.h>

BASE_BEGIN_DECL

/**
 * @defgroup UTIL_MD5 MD5
 * @ingroup UTIL_ENCRYPTION
 * @{
 */


/** MD5 context. */
typedef struct bmd5_context
{
	buint32_t buf[4];	/**< buf    */
	buint32_t bits[2];	/**< bits   */
	buint8_t  in[64];	/**< in	    */
} bmd5_context;

/** Initialize the algorithm. 
 *  @param pms		MD5 context.
 */
void bmd5_init(bmd5_context *pms);

/** Append a string to the message. 
 *  @param pms		MD5 context.
 *  @param data		Data.
 *  @param nbytes	Length of data.
 */
void bmd5_update( bmd5_context *pms, 
			     const buint8_t *data, unsigned nbytes);

/** Finish the message and return the digest. 
 *  @param pms		MD5 context.
 *  @param digest	16 byte digest.
 */
void bmd5_final(bmd5_context *pms, buint8_t digest[16]);


BASE_END_DECL


#endif

