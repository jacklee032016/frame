/* 
 *
 */
#include <basePool.h>
#include <baseExcept.h>
#include <baseOs.h>

#if !BASE_HAS_POOL_ALT_API

/*
 * This file contains pool default policy definition and implementation.
 */
#include "_basePoolSignature.h"
 

static void *operator_new(bpool_factory *factory, bsize_t size)
{
    void *mem;

    BASE_CHECK_STACK();

    if (factory->on_block_alloc) {
		int rc;
		rc = factory->on_block_alloc(factory, size);
		if (!rc)
		    return NULL;
    }
    
    mem = (void*) new char[size+(SIG_SIZE << 1)];
    
    /* Exception for new operator may be disabled, so.. */
    if (mem) {
	/* Apply signature when BASE_SAFE_POOL is set. It will move
	 * "mem" pointer forward.
	 */
	APPLY_SIG(mem, size);
    }

    return mem;
}

static void operator_delete(bpool_factory *factory, void *mem, bsize_t size)
{
    BASE_CHECK_STACK();

    if (factory->on_block_free) 
        factory->on_block_free(factory, size);
    
    /* Check and remove signature when BASE_SAFE_POOL is set. It will
     * move "mem" pointer backward.
     */
    REMOVE_SIG(mem, size);

    /* Note that when BASE_SAFE_POOL is set, the actual size of the block
     * is size + SIG_SIZE*2.
     */

    char *p = (char*)mem;
    delete [] p;
}

static void default_pool_callback(bpool_t *pool, bsize_t size)
{
    BASE_CHECK_STACK();
    BASE_UNUSED_ARG(pool);
    BASE_UNUSED_ARG(size);

    BASE_THROW(BASE_NO_MEMORY_EXCEPTION);
}

bpool_factory_policy bpool_factory_default_policy = 
{
    &operator_new,
    &operator_delete,
    &default_pool_callback,
    0
};

const bpool_factory_policy* bpool_factory_get_default_policy(void)
{
    return &bpool_factory_default_policy;
}

 
#endif	/* BASE_HAS_POOL_ALT_API */

