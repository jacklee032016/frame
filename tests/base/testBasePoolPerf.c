/* $Id: pool_perf.c 5170 2015-08-25 08:45:46Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include "testBaseTest.h"

#if INCLUDE_POOL_PERF_TEST

#include <libBase.h>
#include <compat/malloc.h>

#if !BASE_HAS_HIGH_RES_TIMER
# error Need high resolution timer for this test.
#endif

#define LOOP	    10
#define COUNT	    1024
static unsigned	    sizes[COUNT];
static char	   *p[COUNT];
#define MIN_SIZE    4
#define MAX_SIZE    512
static unsigned total_size;


static int pool_test_pool()
{
    int i;
    bpool_t *pool = bpool_create(mem, NULL, total_size + 4*COUNT, 0, NULL);
    if (!pool)
	return -1;

    for (i=0; i<COUNT; ++i) {
	char *ptr;
	if ( (ptr=(char*)bpool_alloc(pool, sizes[i])) == NULL) {
	    BASE_ERROR("   error: pool failed to allocate %d bytes",  sizes[i]);
	    bpool_release(pool);
	    return -1;
	}
	*ptr = '\0';
    }

    bpool_release(pool);
    return 0;
}

/* Symbian doesn't have malloc()/free(), so we use new/delete instead */
//#if defined(BASE_SYMBIAN) && BASE_SYMBIAN != 0
#if 0
static int pool_test_malloc_free()
{
    int i; /* must be signed */

    for (i=0; i<COUNT; ++i) {
		p[i] = new char[sizes[i]];
		if (!p[i]) {
			BASE_ERROR("   error: malloc failed to allocate %d bytes",
					  sizes[i]);
			--i;
			while (i >= 0) {
				delete [] p[i];
				--i;
			}
			return -1;
		}
		*p[i] = '\0';
    }

    for (i=0; i<COUNT; ++i) {
    	delete [] p[i];
    }

    return 0;
}

#else	/* BASE_SYMBIAN */

static int pool_test_malloc_free()
{
    int i; /* must be signed */

    for (i=0; i<COUNT; ++i) {
	p[i] = (char*)malloc(sizes[i]);
	if (!p[i]) {
	    BASE_ERROR("   error: malloc failed to allocate %d bytes", sizes[i]);
	    --i;
	    while (i >= 0)
		free(p[i]), --i;
	    return -1;
	}
	*p[i] = '\0';
    }

    for (i=0; i<COUNT; ++i) {
	free(p[i]);
    }

    return 0;
}

#endif /* BASE_SYMBIAN */

int pool_perf_test()
{
    unsigned i;
    buint32_t pool_time=0, malloc_time=0, pool_time2=0;
    btimestamp start, end;
    buint32_t best, worst;

    /* Initialize size of chunks to allocate in for the test. */
    for (i=0; i<COUNT; ++i) {
	unsigned aligned_size;
	sizes[i] = MIN_SIZE + (brand() % MAX_SIZE);
	aligned_size = sizes[i];
	if (aligned_size & (BASE_POOL_ALIGNMENT-1))
	    aligned_size = ((aligned_size + BASE_POOL_ALIGNMENT - 1)) & ~(BASE_POOL_ALIGNMENT - 1);
	total_size += aligned_size;
    }

    /* Add some more for pool admin area */
    total_size += 512;

    BASE_INFO("Benchmarking pool..");

    /* Warmup */
    pool_test_pool();
    pool_test_malloc_free();

    for (i=0; i<LOOP; ++i) {
	bTimeStampGet(&start);
	if (pool_test_pool()) {
	    return 1;
	}
	bTimeStampGet(&end);
	pool_time += (end.u32.lo - start.u32.lo);

	bTimeStampGet(&start);
	if (pool_test_malloc_free()) {
	    return 2;
	}
	bTimeStampGet(&end);
	malloc_time += (end.u32.lo - start.u32.lo);

	bTimeStampGet(&start);
	if (pool_test_pool()) {
	    return 4;
	}
	bTimeStampGet(&end);
	pool_time2 += (end.u32.lo - start.u32.lo);
    }

    BASE_INFO("..LOOP count:                        %u",LOOP);
    BASE_INFO("..number of alloc/dealloc per loop:  %u",COUNT);
    BASE_INFO("..pool allocation/deallocation time: %u",pool_time);
    BASE_INFO("..malloc/free time:                  %u",malloc_time);
    BASE_INFO("..pool again, second invocation:     %u",pool_time2);

    if (pool_time2==0) pool_time2=1;
    if (pool_time < pool_time2)
	best = pool_time, worst = pool_time2;
    else
	best = pool_time2, worst = pool_time;
    
    /* avoid division by zero */
    if (best==0) best=1;
    if (worst==0) worst=1;

    BASE_INFO("..pool speedup over malloc best=%dx, worst=%dx", 
			  (int)(malloc_time/best),
			  (int)(malloc_time/worst));
    return 0;
}


#endif	/* INCLUDE_POOL_PERF_TEST */

