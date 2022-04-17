/*
 *
 */
#ifndef __UTIL_CRC32_H__
#define __UTIL_CRC32_H__

/**
 * @brief CRC32 implementation
 */

#include <utilTypes.h>

BASE_BEGIN_DECL

/**
 * @defgroup UTIL_CRC32 CRC32 (Cyclic Redundancy Check)
 * @ingroup UTIL_ENCRYPTION
 * @{
 * This implements CRC32 algorithm. See ITU-T V.42 for the formal 
 * specification.
 */

/** CRC32 context. */
typedef struct bcrc32_context
{
    buint32_t	crc_state;	/**< Current state. */
} bcrc32_context;


/**
 * Initialize CRC32 context.
 *
 * @param ctx	    CRC32 context.
 */
void bcrc32_init(bcrc32_context *ctx);

/**
 * Feed data incrementally to the CRC32 algorithm.
 *
 * @param ctx	    CRC32 context.
 * @param data	    Input data.
 * @param nbytes    Length of the input data.
 *
 * @return	    The current CRC32 value.
 */
buint32_t bcrc32_update(bcrc32_context *ctx, 
				     const buint8_t *data,
				     bsize_t nbytes);

/**
 * Finalize CRC32 calculation and retrieve the CRC32 value.
 *
 * @param ctx	    CRC32 context.
 *
 * @return	    The current CRC value.
 */
buint32_t bcrc32_final(bcrc32_context *ctx);

/**
 * Perform one-off CRC32 calculation to the specified data.
 *
 * @param data	    Input data.
 * @param nbytes    Length of input data.
 *
 * @return	    CRC value of the data.
 */
buint32_t bcrc32_calc(const buint8_t *data,
				   bsize_t nbytes);


BASE_END_DECL


#endif

