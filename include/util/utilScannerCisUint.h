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
#   define BASE_CIS_ELEM_TYPE int
#endif

/**
 * This describes the type of individual character specification in
 * #bcis_buf_t.
 */
typedef BASE_CIS_ELEM_TYPE bcis_elem_t;

/** bcis_buf_t is not used when uint back-end is used. */
typedef int bcis_buf_t;

/**
 * Character input specification.
 */
typedef struct bcis_t
{
    BASE_CIS_ELEM_TYPE	cis_buf[256];	/**< Internal buffer.	*/
} bcis_t;


/**
 * Set the membership of the specified character.
 * Note that this is a macro, and arguments may be evaluated more than once.
 *
 * @param cis       Pointer to character input specification.
 * @param c         The character.
 */
#define BASE_CIS_SET(cis,c)   ((cis)->cis_buf[(int)(c)] = 1)

/**
 * Remove the membership of the specified character.
 * Note that this is a macro, and arguments may be evaluated more than once.
 *
 * @param cis       Pointer to character input specification.
 * @param c         The character to be removed from the membership.
 */
#define BASE_CIS_CLR(cis,c)   ((cis)->cis_buf[(int)c] = 0)

/**
 * Check the membership of the specified character.
 * Note that this is a macro, and arguments may be evaluated more than once.
 *
 * @param cis       Pointer to character input specification.
 * @param c         The character.
 */
#define BASE_CIS_ISSET(cis,c) ((cis)->cis_buf[(int)c])



BASE_END_DECL

#endif

