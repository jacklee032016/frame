/* 
 *
 */
#include <basePool.h>
#include <baseExcept.h>
#include <baseOs.h>
#include <compat/malloc.h>

#if !BASE_HAS_POOL_ALT_API

/*
 * This file contains pool default policy definition and implementation.
 */
#include "_basePoolSignature.h"

static void *_mallocDefaultBlockAlloc(bpool_factory *factory, bsize_t size)
{
	void *p;

	BASE_CHECK_STACK();

	if (factory->on_block_alloc)
	{
		int rc;
		rc = factory->on_block_alloc(factory, size);
		if (!rc)
			return NULL;
	}

	p = malloc(size+(SIG_SIZE << 1));

	if (p == NULL)
	{
		if (factory->on_block_free)
			factory->on_block_free(factory, size);
	}
	else
	{
		/* Apply signature when BASE_SAFE_POOL is set. It will move
		* "p" pointer forward.	*/
		APPLY_SIG(p, size);
	}

	return p;
}

static void _mallocDefaultBlockFree(bpool_factory *factory, void *mem,  bsize_t size)
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

	free(mem);
}

static void _mallocDefaultPoolCallback(bpool_t *pool, bsize_t size)
{
	BASE_CHECK_STACK();
	BASE_UNUSED_ARG(pool);
	BASE_UNUSED_ARG(size);

	BASE_THROW(BASE_NO_MEMORY_EXCEPTION);
}

bpool_factory_policy bpool_factory_default_policy =
{
	&_mallocDefaultBlockAlloc,
	&_mallocDefaultBlockFree,
	&_mallocDefaultPoolCallback,
	0
};

const bpool_factory_policy* bpool_factory_get_default_policy(void)
{
    return &bpool_factory_default_policy;
}

#endif

