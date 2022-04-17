/*
 *
 */
#include <basePool.h>
#include <baseExcept.h>
#include <baseOs.h>


static void *default_block_alloc(bpool_factory *factory, bsize_t size)
{
    BASE_CHECK_STACK();
    BASE_UNUSED_ARG(factory);

    return kmalloc(size, GFP_ATOMIC);
}

static void default_block_free(bpool_factory *factory, 
			       void *mem, bsize_t size)
{
    BASE_CHECK_STACK();
    BASE_UNUSED_ARG(factory);
    BASE_UNUSED_ARG(size);

    kfree(mem);
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
    &default_block_alloc,
    &default_block_free,
    &default_pool_callback,
    0
};

const bpool_factory_policy* bpool_factory_get_default_policy(void)
{
    return &bpool_factory_default_policy;
}

