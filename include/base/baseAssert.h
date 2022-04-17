/* 
 *
 */
#ifndef __BASE_ASSERT_H__
#define __BASE_ASSERT_H__

/**
 * @brief Assertion macro bassert().
 */

#include <baseConfig.h>
#include <compat/assert.h>

/**
 * @defgroup bassert Assertion Macro
 * @ingroup BASE_MISC
 * @{
 *
 * Assertion and other helper macros for sanity checking.
 */

/**
 * @hideinitializer
 * Check during debug build that an expression is true. If the expression
 * computes to false during run-time, then the program will stop at the
 * offending statements.
 * For release build, this macro will not do anything.
 *
 * @param expr	    The expression to be evaluated.
 */
#ifndef bassert
#define bassert(expr)   assert(expr)
#endif


/**
 * @hideinitializer
 * If #BASE_ENABLE_EXTRA_CHECK is declared and the value is non-zero, then 
 * #BASE_ASSERT_RETURN macro will evaluate the expression in @a expr during
 * run-time. If the expression yields false, assertion will be triggered
 * and the current function will return with the specified return value.
 *
 * If #BASE_ENABLE_EXTRA_CHECK is not declared or is zero, then no run-time
 * checking will be performed. The macro simply evaluates to bassert(expr).
 */
#if defined(BASE_ENABLE_EXTRA_CHECK) && BASE_ENABLE_EXTRA_CHECK != 0
#   define BASE_ASSERT_RETURN(expr,retval)    \
	    do { \
		if (!(expr)) { bassert(expr); return retval; } \
	    } while (0)
#else
#   define BASE_ASSERT_RETURN(expr,retval)    bassert(expr)
#endif

/**
 * @hideinitializer
 * If #BASE_ENABLE_EXTRA_CHECK is declared and non-zero, then 
 * #BASE_ASSERT_ON_FAIL macro will evaluate the expression in @a expr during
 * run-time. If the expression yields false, assertion will be triggered
 * and @a exec_on_fail will be executed.
 *
 * If #BASE_ENABLE_EXTRA_CHECK is not declared or is zero, then no run-time
 * checking will be performed. The macro simply evaluates to bassert(expr).
 */
#if defined(BASE_ENABLE_EXTRA_CHECK) && BASE_ENABLE_EXTRA_CHECK != 0
#   define BASE_ASSERT_ON_FAIL(expr,exec_on_fail)    \
	    do { \
		bassert(expr); \
		if (!(expr)) exec_on_fail; \
	    } while (0)
#else
#   define BASE_ASSERT_ON_FAIL(expr,exec_on_fail)    bassert(expr)
#endif

#endif	/* __BASE_ASSERT_H__ */

