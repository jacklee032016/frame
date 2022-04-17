/* 
 */
#include <basePool.h>
#include <baseLog.h>
#include <baseString.h>
#include <baseAssert.h>
#include <baseLock.h>
#include <baseOs.h>
#include <basePoolBuf.h>

#if !BASE_HAS_POOL_ALT_API

static bsize_t pool_sizes[BASE_CACHING_POOL_ARRAY_SIZE] = 
{
    256, 512, 1024, 2048, 4096, 8192, 12288, 16384, 
    20480, 24576, 28672, 32768, 40960, 49152, 57344, 65536
};

/* Index where the search for size should begin.
 * Start with pool_sizes[5], which is 8192.
 */
#define START_SIZE  5

static bpool_t* _cpoolCreatePool(bpool_factory *pf, const char *name, bsize_t initial_size, bsize_t increment_sz, bpool_callback *callback)
{
	bcaching_pool *cp = (bcaching_pool*)pf;
	bpool_t *pool;
	int idx;

	BASE_CHECK_STACK();

	block_acquire(cp->lock);

	/* Use pool factory's policy when callback is NULL */
	if (callback == NULL)
	{
		callback = pf->policy.callback;
	}

	/* Search the suitable size for the pool. 
	* We'll just do linear search to the size array, as the array size itself
	* is only a few elements. Binary search I suspect will be less efficient
	* for this purpose.
	*/
	if (initial_size <= pool_sizes[START_SIZE])
	{
		for (idx=START_SIZE-1; idx >= 0 && pool_sizes[idx] >= initial_size;	--idx)
			;
		++idx;
	}
	else
	{
		for (idx=START_SIZE+1; idx < BASE_CACHING_POOL_ARRAY_SIZE && pool_sizes[idx] < initial_size; ++idx)
			;
	}

	/* Check whether there's a pool in the list. */
	if (idx==BASE_CACHING_POOL_ARRAY_SIZE || blist_empty(&cp->free_list[idx]))
	{
		/* No pool is available. */
		/* Set minimum size. */
		if (idx < BASE_CACHING_POOL_ARRAY_SIZE)
			initial_size =  pool_sizes[idx];

		/* Create new pool */
		pool = bpool_create_int(&cp->factory, name, initial_size, increment_sz, callback);
		if (!pool)
		{
			block_release(cp->lock);
			return NULL;
		}
	}
	else
	{
		/* Get one pool from the list. */
		pool = (bpool_t*) cp->free_list[idx].next;
		blist_erase(pool);

		/* Initialize the pool. */
		bpool_init_int(pool, name, increment_sz, callback);

		/* Update pool manager's free capacity. */
		if (cp->capacity > bpool_get_capacity(pool))
		{
			cp->capacity -= bpool_get_capacity(pool);
		}
		else
		{
			cp->capacity = 0;
		}

		BASE_STR_INFO(pool->objName, "pool reused, size=%u", pool->capacity);
	}

	/* Put in used list. */
	blist_insert_before( &cp->used_list, pool );

	/* Mark factory data */
	pool->factory_data = (void*) (bssize_t) idx;

	/* Increment used count. */
	++cp->used_count;

	block_release(cp->lock);
	return pool;
}


static void _cpoolReleasePool( bpool_factory *pf, bpool_t *pool)
{
    bcaching_pool *cp = (bcaching_pool*)pf;
    bsize_t pool_capacity;
    unsigned i;

    BASE_CHECK_STACK();

    BASE_ASSERT_ON_FAIL(pf && pool, return);

    block_acquire(cp->lock);

#if BASE_SAFE_POOL
    /* Make sure pool is still in our used list */
    if (blist_find_node(&cp->used_list, pool) != pool) {
	bassert(!"Attempt to destroy pool that has been destroyed before");
	return;
    }
#endif

    /* Erase from the used list. */
    blist_erase(pool);

    /* Decrement used count. */
    --cp->used_count;

    pool_capacity = bpool_get_capacity(pool);

    /* Destroy the pool if the size is greater than our size or if the total
     * capacity in our recycle list (plus the size of the pool) exceeds 
     * maximum capacity.
   . */
    if (pool_capacity > pool_sizes[BASE_CACHING_POOL_ARRAY_SIZE-1] ||
	cp->capacity + pool_capacity > cp->max_capacity)
    {
	bpool_destroy_int(pool);
	block_release(cp->lock);
	return;
    }

    /* Reset pool. */
    BASE_STR_INFO(pool->objName, "recycle(): cap=%d, used=%d(%d%%)", 
	       pool_capacity, bpool_get_used_size(pool), 
	       bpool_get_used_size(pool)*100/pool_capacity);
    bpool_reset(pool);

    pool_capacity = bpool_get_capacity(pool);

    /*
     * Otherwise put the pool in our recycle list.
     */
    i = (unsigned) (unsigned long) (bssize_t) pool->factory_data;

    bassert(i<BASE_CACHING_POOL_ARRAY_SIZE);
    if (i >= BASE_CACHING_POOL_ARRAY_SIZE ) {
	/* Something has gone wrong with the pool. */
	bpool_destroy_int(pool);
	block_release(cp->lock);
	return;
    }

    blist_insert_after(&cp->free_list[i], pool);
    cp->capacity += pool_capacity;

    block_release(cp->lock);
}

static void _cpoolDumpStatus(bpool_factory *factory, bbool_t detail )
{
#if BASE_LOG_MAX_LEVEL >= 3
    bcaching_pool *cp = (bcaching_pool*)factory;

    block_acquire(cp->lock);

    BASE_INFO(" Dumping caching pool:");
    BASE_INFO("   Capacity=%u, max_capacity=%u, used_cnt=%u", cp->capacity, cp->max_capacity, cp->used_count);
    if (detail) {
	bpool_t *pool = (bpool_t*) cp->used_list.next;
	bsize_t total_used = 0, total_capacity = 0;
        BASE_INFO("  Dumping all active pools:");
	while (pool != (void*)&cp->used_list) {
	    bsize_t pool_capacity = bpool_get_capacity(pool);
	    BASE_INFO("   %16s: %8d of %8d (%d%%) used", 
				  bpool_getobjname(pool), 
				  bpool_get_used_size(pool), 
				  pool_capacity,
				  bpool_get_used_size(pool)*100/pool_capacity);
	    total_used += bpool_get_used_size(pool);
	    total_capacity += pool_capacity;
	    pool = pool->next;
	}
	if (total_capacity) {
	    BASE_INFO("  Total %9d of %9d (%d %%) used!",
				  total_used, total_capacity,
				  total_used * 100 / total_capacity);
	}
    }

    block_release(cp->lock);
#else
    BASE_UNUSED_ARG(factory);
    BASE_UNUSED_ARG(detail);
#endif
}


static bbool_t _cpoolOnBlockAlloc(bpool_factory *f, bsize_t sz)
{
    bcaching_pool *cp = (bcaching_pool*)f;

    //Can't lock because mutex is not recursive
    //if (cp->mutex) bmutex_lock(cp->mutex);

    cp->used_size += sz;
    if (cp->used_size > cp->peak_used_size)
	cp->peak_used_size = cp->used_size;

    //if (cp->mutex) bmutex_unlock(cp->mutex);

    return BASE_TRUE;
}


static void _cpoolOnBlockFree(bpool_factory *f, bsize_t sz)
{
	bcaching_pool *cp = (bcaching_pool*)f;

	//bmutex_lock(cp->mutex);
	cp->used_size -= sz;
	//bmutex_unlock(cp->mutex);
}


void bcaching_pool_init( bcaching_pool *cp, const bpool_factory_policy *policy, bsize_t max_capacity)
{
	int i;
	bpool_t *pool;

	BASE_CHECK_STACK();
	bbzero(cp, sizeof(*cp));

	cp->max_capacity = max_capacity;
	blist_init(&cp->used_list);
	for (i=0; i<BASE_CACHING_POOL_ARRAY_SIZE; ++i)
		blist_init(&cp->free_list[i]);

	if (policy == NULL)
	{
		policy = &bpool_factory_default_policy;
	}

	bmemcpy(&cp->factory.policy, policy, sizeof(bpool_factory_policy));
	cp->factory.create_pool = &_cpoolCreatePool;
	cp->factory.release_pool = &_cpoolReleasePool;
	cp->factory.dump_status = &_cpoolDumpStatus;
	cp->factory.on_block_alloc = &_cpoolOnBlockAlloc;
	cp->factory.on_block_free = &_cpoolOnBlockFree;

	pool = bpool_create_on_buf("cachingpool", cp->pool_buf, sizeof(cp->pool_buf));
	if(block_create_simple_mutex(pool, "cachingpool", &cp->lock) != BASE_SUCCESS )
	{
		BASE_ERROR("Mutex of pool failed");
	}
}

void bcaching_pool_destroy( bcaching_pool *cp )
{
	int i;
	bpool_t *pool;

	BASE_CHECK_STACK();

	/* Delete all pool in free list */
	for (i=0; i < BASE_CACHING_POOL_ARRAY_SIZE; ++i)
	{
		bpool_t *next;
		pool = (bpool_t*) cp->free_list[i].next;
		for (; pool != (void*)&cp->free_list[i]; pool = next)
		{
			next = pool->next;
			blist_erase(pool);
			bpool_destroy_int(pool);
		}
	}

	/* Delete all pools in used list */
	pool = (bpool_t*) cp->used_list.next;
	while (pool != (bpool_t*) &cp->used_list)
	{
		bpool_t *next = pool->next;
		blist_erase(pool);

		BASE_STR_INFO(pool->objName, "Pool is not released by application, releasing now");
		bpool_destroy_int(pool);
		pool = next;
	}

	if (cp->lock)
	{
		block_destroy(cp->lock);
		block_create_null_mutex(NULL, "cachingpool", &cp->lock);
	}
}

#endif

