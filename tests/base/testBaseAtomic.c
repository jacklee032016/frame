/* 
 *
 */
#include "testBaseTest.h"
#include <libBase.h>

/**
 * \page page_baselib_atomic_test Test: Atomic Variable
 *
 * This file provides implementation of \b atomic_test(). It tests the
 * functionality of the atomic variable API.
 *
 * \section atomic_test_sec Scope of the Test
 *
 * API tested:
 *  - batomic_create()
 *  - batomic_get()
 *  - batomic_inc()
 *  - batomic_dec()
 *  - batomic_set()
 *  - batomic_destroy()
 */


#if INCLUDE_ATOMIC_TEST

int atomic_test(void)
{
    bpool_t *pool;
    batomic_t *atomic_var;
    bstatus_t rc;

    pool = bpool_create(mem, NULL, 4096, 0, NULL);
    if (!pool)
        return -10;

    /* create() */
    rc = batomic_create(pool, 111, &atomic_var);
    if (rc != 0) {
        return -20;
    }

    /* get: check the value. */
    if (batomic_get(atomic_var) != 111)
        return -30;

    /* increment. */
    batomic_inc(atomic_var);
    if (batomic_get(atomic_var) != 112)
        return -40;

    /* decrement. */
    batomic_dec(atomic_var);
    if (batomic_get(atomic_var) != 111)
        return -50;

    /* set */
    batomic_set(atomic_var, 211);
    if (batomic_get(atomic_var) != 211)
        return -60;

    /* add */
    batomic_add(atomic_var, 10);
    if (batomic_get(atomic_var) != 221)
        return -60;

    /* check the value again. */
    if (batomic_get(atomic_var) != 221)
        return -70;

    /* destroy */
    rc = batomic_destroy(atomic_var);
    if (rc != 0)
        return -80;

    bpool_release(pool);

    return 0;
}


#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_atomic_test;
#endif  /* INCLUDE_ATOMIC_TEST */

