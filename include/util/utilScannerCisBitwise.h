/* 
 *
 */
#ifndef __UTIL_SCANNER_CIS_BIT_H__
#define __UTIL_SCANNER_CIS_BIT_H__

#include <_baseTypes.h>

BASE_BEGIN_DECL

/**
 * This describes the type of individual character specification in
 * #bcis_buf_t. Basicly the number of bits here
 */
#ifndef BASE_CIS_ELEM_TYPE
#   define BASE_CIS_ELEM_TYPE buint32_t
#endif

/**
 * This describes the type of individual character specification in
 * #bcis_buf_t.
 */
typedef BASE_CIS_ELEM_TYPE bcis_elem_t;

/**
 * Maximum number of input specification in a buffer.
 * Effectively this means the number of bits in bcis_elem_t.
 */
#define BASE_CIS_MAX_INDEX   (sizeof(bcis_elem_t) << 3)

/**
 * The scanner input specification buffer.
 */
typedef struct bcis_buf_t
{
    bcis_elem_t    cis_buf[256];  /**< Must be 256 (not 128)! */
    bcis_elem_t    use_mask;      /**< To keep used indexes.  */
} bcis_buf_t;

/**
 * Character input specification.
 */
typedef struct bcis_t
{
    bcis_elem_t   *cis_buf;       /**< Pointer to buffer.     */
    int              cis_id;        /**< Id.                    */
} bcis_t;


/**
 * Set the membership of the specified character.
 * Note that this is a macro, and arguments may be evaluated more than once.
 *
 * @param cis       Pointer to character input specification.
 * @param c         The character.
 */
#define BASE_CIS_SET(cis,c)   ((cis)->cis_buf[(int)(c)] |= (1 << (cis)->cis_id))

/**
 * Remove the membership of the specified character.
 * Note that this is a macro, and arguments may be evaluated more than once.
 *
 * @param cis       Pointer to character input specification.
 * @param c         The character to be removed from the membership.
 */
#define BASE_CIS_CLR(cis,c)   ((cis)->cis_buf[(int)c] &= ~(1 << (cis)->cis_id))

/**
 * Check the membership of the specified character.
 * Note that this is a macro, and arguments may be evaluated more than once.
 *
 * @param cis       Pointer to character input specification.
 * @param c         The character.
 */
#define BASE_CIS_ISSET(cis,c) ((cis)->cis_buf[(int)c] & (1 << (cis)->cis_id))



BASE_END_DECL

#endif

