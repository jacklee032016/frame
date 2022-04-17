/* 
 *
 */
#include <basePool.h>
#include <basePoolBuf.h>
#include <baseRand.h>
#include <baseLog.h>
#include <baseExcept.h>
#include "testBaseTest.h"

/**
 * This file provides implementation of \b pool_test(). It tests the
 * functionality of the memory pool.
 *
 */


#if INCLUDE_POOL_TEST

#define SIZE	4096

/* Normally we should throw exception when memory alloc fails.
 * Here we do nothing so that the flow will go back to original caller,
 * which will test the result using NULL comparison. Normally caller will
 * catch the exception instead of checking for NULLs.
 */
static void null_callback(bpool_t *pool, bsize_t size)
{
    BASE_UNUSED_ARG(pool);
    BASE_UNUSED_ARG(size);
}

#define GET_FREE(p)	(bpool_get_capacity(p)-bpool_get_used_size(p))

/* Test that the capacity and used size reported by the pool is correct. 
 */
static int capacity_test(void)
{
    bpool_t *pool = bpool_create(mem, NULL, SIZE, 0, &null_callback);
    bsize_t freesize;

    BASE_INFO("...capacity_test()");

    if (!pool)
	return -200;

    freesize = GET_FREE(pool);

    if (bpool_alloc(pool, freesize) == NULL) {
	BASE_ERROR("...error: wrong freesize %u reported", freesize);
	bpool_release(pool);
	return -210;
    }

    bpool_release(pool);
    return 0;
}

/* Test that the alignment works. */
static int pool_alignment_test(void)
{
    bpool_t *pool;
    void *ptr;
    enum { MEMSIZE = 64, LOOP = 100 };
    unsigned i;

    BASE_INFO("...alignment test");

    pool = bpool_create(mem, NULL, BASE_POOL_SIZE+MEMSIZE, MEMSIZE, NULL);
    if (!pool)
	return -300;

#define IS_ALIGNED(p)	((((unsigned long)(bssize_t)p) & \
			   (BASE_POOL_ALIGNMENT-1)) == 0)

    for (i=0; i<LOOP; ++i) {
	/* Test first allocation */
	ptr = bpool_alloc(pool, 1);
	if (!IS_ALIGNED(ptr)) {
	    bpool_release(pool);
	    return -310;
	}

	/* Test subsequent allocation */
	ptr = bpool_alloc(pool, 1);
	if (!IS_ALIGNED(ptr)) {
	    bpool_release(pool);
	    return -320;
	}

	/* Test allocation after new block is created */
	ptr = bpool_alloc(pool, MEMSIZE*2+1);
	if (!IS_ALIGNED(ptr)) {
	    bpool_release(pool);
	    return -330;
	}

	/* Reset the pool */
	bpool_reset(pool);
    }

    /* Done */
    bpool_release(pool);

    return 0;
}

/* Test that the alignment works for pool on buf. */
static int pool_buf_alignment_test(void)
{
    bpool_t *pool;
    char buf[512];
    void *ptr;
    enum { LOOP = 100 };
    unsigned i;

    BASE_INFO("...pool_buf alignment test");

    pool = bpool_create_on_buf(NULL, buf, sizeof(buf));
    if (!pool)
	return -400;

    for (i=0; i<LOOP; ++i) {
	/* Test first allocation */
	ptr = bpool_alloc(pool, 1);
	if (!IS_ALIGNED(ptr)) {
	    bpool_release(pool);
	    return -410;
	}

	/* Test subsequent allocation */
	ptr = bpool_alloc(pool, 1);
	if (!IS_ALIGNED(ptr)) {
	    bpool_release(pool);
	    return -420;
	}

	/* Reset the pool */
	bpool_reset(pool);
    }

    /* Done */
    return 0;
}

/* Test function to drain the pool's space. 
 */
static int drain_test(bsize_t size, bsize_t increment)
{
    bpool_t *pool = bpool_create(mem, NULL, size, increment, 
				     &null_callback);
    bsize_t freesize;
    void *p;
    int status = 0;
    
    BASE_INFO("...drain_test(%d,%d)", size, increment);

    if (!pool)
	return -10;

    /* Get free size */
    freesize = GET_FREE(pool);
    if (freesize < 1) {
    	status=-15; 
	goto on_error;
    }

    /* Drain the pool until there's nothing left. */
    while (freesize > 0) {
	int size2;

	if (freesize > 255)
	    size2 = ((brand() & 0x000000FF) + BASE_POOL_ALIGNMENT) & 
		   ~(BASE_POOL_ALIGNMENT - 1);
	else
	    size2 = (int)freesize;

	p = bpool_alloc(pool, size2);
	if (!p) {
	    status=-20; goto on_error;
	}

	freesize -= size2;
    }

    /* Check that capacity is zero. */
    if (GET_FREE(pool) != 0) {
	BASE_ERROR("....error: returned free=%u (expecting 0)", GET_FREE(pool));
	status=-30; goto on_error;
    }

    /* Try to allocate once more */
    p = bpool_alloc(pool, 257);
    if (!p) {
	status=-40; goto on_error;
    }

    /* Check that capacity is NOT zero. */
    if (GET_FREE(pool) == 0) {
	status=-50; goto on_error;
    }


on_error:
    bpool_release(pool);
    return status;
}

/* Test the buffer based pool */
static int pool_buf_test(void)
{
    enum { STATIC_BUF_SIZE = 40 };
    /* 16 is the internal struct in pool_buf */
    static char buf[ STATIC_BUF_SIZE + sizeof(bpool_t) + 
		     sizeof(bpool_block) + 2 * BASE_POOL_ALIGNMENT];
    bpool_t *pool;
    void *p;
    BASE_USE_EXCEPTION;

    BASE_INFO("...pool_buf test");

    pool = bpool_create_on_buf("no name", buf, sizeof(buf));
    if (!pool)
	return -70;

    /* Drain the pool */
    BASE_TRY {
	if ((p=bpool_alloc(pool, STATIC_BUF_SIZE/2)) == NULL)
	    return -75;

	if ((p=bpool_alloc(pool, STATIC_BUF_SIZE/2)) == NULL)
	    return -76;
    }
    BASE_CATCH_ANY {
	return -77;
    }
    BASE_END;

    /* On the next alloc, exception should be thrown */
    BASE_TRY {
	p = bpool_alloc(pool, STATIC_BUF_SIZE);
	if (p != NULL) {
	    /* This is unexpected, the alloc should fail */
	    return -78;
	}
    }
    BASE_CATCH_ANY {
	/* This is the expected result */
    }
    BASE_END;

    /* Done */
    return 0;
}


int pool_test(void)
{
    enum { LOOP = 2 };
    int loop;
    int rc;

    rc = capacity_test();
    if (rc) return rc;

    rc = pool_alignment_test();
    if (rc) return rc;

    rc = pool_buf_alignment_test();
    if (rc) return rc;

    for (loop=0; loop<LOOP; ++loop) {
	/* Test that the pool should grow automaticly. */
	rc = drain_test(SIZE, SIZE);
	if (rc != 0) return rc;

	/* Test situation where pool is not allowed to grow. 
 	 * We expect the test to return correct error.
	 */
	rc = drain_test(SIZE, 0);
	if (rc != -40) return rc;
    }

    rc = pool_buf_test();
    if (rc != 0)
	return rc;


    return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_pool_test;
#endif	/* INCLUDE_POOL_TEST */

