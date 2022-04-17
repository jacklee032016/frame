/* 
 *
 */
#include <basePool.h>
#include <baseLog.h>
#include <baseExcept.h>
#include <baseAssert.h>
#include <baseOs.h>

#if !BASE_HAS_POOL_ALT_API


/* Include inline definitions when inlining is disabled. */
#if !BASE_FUNCTIONS_ARE_INLINED
#include <_basePool_i.h>
#endif

#define LOG(expr)   		    BASE_LOG(6,expr)
#define ALIGN_PTR(PTR,ALIGNMENT)    (PTR + (-(bssize_t)(PTR) & (ALIGNMENT-1)))

int BASE_NO_MEMORY_EXCEPTION;

int bNO_MEMORY_EXCEPTION()
{
    return BASE_NO_MEMORY_EXCEPTION;
}

/*
 * Create new block.
 * Create a new big chunk of memory block, from which user allocation will be
 * taken from.
 */
static bpool_block *bpool_create_block( bpool_t *pool, bsize_t size)
{
    bpool_block *block;

    BASE_CHECK_STACK();
    bassert(size >= sizeof(bpool_block));

    LOG((pool->objName, "create_block(sz=%u), cur.cap=%u, cur.used=%u", 
	 size, pool->capacity, bpool_get_used_size(pool)));

    /* Request memory from allocator. */
    block = (bpool_block*) 
	(*pool->factory->policy.block_alloc)(pool->factory, size);
    if (block == NULL) {
	(*pool->callback)(pool, size);
	return NULL;
    }

    /* Add capacity. */
    pool->capacity += size;

    /* Set start and end of buffer. */
    block->buf = ((unsigned char*)block) + sizeof(bpool_block);
    block->end = ((unsigned char*)block) + size;

    /* Set the start pointer, aligning it as needed */
    block->cur = ALIGN_PTR(block->buf, BASE_POOL_ALIGNMENT);

    /* Insert in the front of the list. */
    blist_insert_after(&pool->block_list, block);

    LOG((pool->objName," block created, buffer=%p-%p",block->buf, block->end));

    return block;
}

/*
 * Allocate memory chunk for user from available blocks.
 * This will iterate through block list to find space to allocate the chunk.
 * If no space is available in all the blocks, a new block might be created
 * (depending on whether the pool is allowed to resize).
 */
void* bpool_allocate_find(bpool_t *pool, bsize_t size)
{
    bpool_block *block = pool->block_list.next;
    void *p;
    bsize_t block_size;

    BASE_CHECK_STACK();

    while (block != &pool->block_list) {
	p = bpool_alloc_from_block(block, size);
	if (p != NULL)
	    return p;
	block = block->next;
    }
    /* No available space in all blocks. */

    /* If pool is configured NOT to expand, return error. */
    if (pool->increment_size == 0) {
	LOG((pool->objName, "Can't expand pool to allocate %u bytes "
	     "(used=%u, cap=%u)",
	     size, bpool_get_used_size(pool), pool->capacity));
	(*pool->callback)(pool, size);
	return NULL;
    }

    /* If pool is configured to expand, but the increment size
     * is less than the required size, expand the pool by multiple
     * increment size. Also count the size wasted due to aligning
     * the block.
     */
    if (pool->increment_size < 
	    size + sizeof(bpool_block) + BASE_POOL_ALIGNMENT) 
    {
        bsize_t count;
        count = (size + pool->increment_size + sizeof(bpool_block) +
                 BASE_POOL_ALIGNMENT) / 
                pool->increment_size;
        block_size = count * pool->increment_size;

    } else {
        block_size = pool->increment_size;
    }

    LOG((pool->objName, 
	 "%u bytes requested, resizing pool by %u bytes (used=%u, cap=%u)",
	 size, block_size, bpool_get_used_size(pool), pool->capacity));

    block = bpool_create_block(pool, block_size);
    if (!block)
	return NULL;

    p = bpool_alloc_from_block(block, size);
    bassert(p != NULL);
#if BASE_LIB_DEBUG
    if (p == NULL) {
	BASE_UNUSED_ARG(p);
    }
#endif
    return p;
}

/*
 * Internal function to initialize pool.
 */
void bpool_init_int(  bpool_t *pool, 
				const char *name,
				bsize_t increment_size,
				bpool_callback *callback)
{
    BASE_CHECK_STACK();

    pool->increment_size = increment_size;
    pool->callback = callback;

    if (name) {
	if (strchr(name, '%') != NULL) {
	    bansi_snprintf(pool->objName, sizeof(pool->objName), 
			     name, pool);
	} else {
	    bansi_strncpy(pool->objName, name, BASE_MAX_OBJ_NAME);
	    pool->objName[BASE_MAX_OBJ_NAME-1] = '\0';
	}
    } else {
	pool->objName[0] = '\0';
    }
}

/*
 * Create new memory pool.
 */
bpool_t* bpool_create_int( bpool_factory *f, const char *name, bsize_t initial_size, bsize_t increment_size, bpool_callback *callback)
{
	bpool_t *pool;
	bpool_block *block;
	buint8_t *buffer;

	BASE_CHECK_STACK();

	/* Size must be at least sizeof(bpool)+sizeof(bpool_block) */
	BASE_ASSERT_RETURN(initial_size >= sizeof(bpool_t)+sizeof(bpool_block),NULL);

	/* If callback is NULL, set calback from the policy */
	if (callback == NULL)
		callback = f->policy.callback;

	/* Allocate initial block */
	buffer = (buint8_t*) (*f->policy.block_alloc)(f, initial_size);
	if (!buffer)
		return NULL;

	/* Set pool administrative data. */
	pool = (bpool_t*)buffer;
	bbzero(pool, sizeof(*pool));

	blist_init(&pool->block_list);
	pool->factory = f;

	/* Create the first block from the memory. */
	block = (bpool_block*) (buffer + sizeof(*pool));
	block->buf = ((unsigned char*)block) + sizeof(bpool_block);
	block->end = buffer + initial_size;

	/* Set the start pointer, aligning it as needed */
	block->cur = ALIGN_PTR(block->buf, BASE_POOL_ALIGNMENT);

	blist_insert_after(&pool->block_list, block);

	bpool_init_int(pool, name, increment_size, callback);

	/* Pool initial capacity and used size */
	pool->capacity = initial_size;

	LOG((pool->objName, "pool created, size=%u", pool->capacity));
	return pool;
}

/*
 * Reset the pool to the state when it was created.
 * All blocks will be deallocated except the first block. All memory areas
 * are marked as free.
 */
static void reset_pool(bpool_t *pool)
{
	bpool_block *block;

	BASE_CHECK_STACK();

	block = pool->block_list.prev;
	if (block == &pool->block_list)
		return;

	/* Skip the first block because it is occupying the same memory
	as the pool itself.
	*/
	block = block->prev;

	while (block != &pool->block_list)
	{
		bpool_block *prev = block->prev;
		blist_erase(block);
		(*pool->factory->policy.block_free)(pool->factory, block, 
		block->end - (unsigned char*)block);
		block = prev;
	}

	block = pool->block_list.next;

	/* Set the start pointer, aligning it as needed */
	block->cur = ALIGN_PTR(block->buf, BASE_POOL_ALIGNMENT);

	pool->capacity = block->end - (unsigned char*)pool;
}

/*
 * The public function to reset pool.
 */
void bpool_reset(bpool_t *pool)
{
    LOG((pool->objName, "reset(): cap=%d, used=%d(%d%%)", 
	pool->capacity, bpool_get_used_size(pool), 
	bpool_get_used_size(pool)*100/pool->capacity));

    reset_pool(pool);
}

/*
 * Destroy the pool.
 */
void bpool_destroy_int(bpool_t *pool)
{
    bsize_t initial_size;

    LOG((pool->objName, "destroy(): cap=%d, used=%d(%d%%), block0=%p-%p", 
	pool->capacity, bpool_get_used_size(pool), 
	bpool_get_used_size(pool)*100/pool->capacity,
	((bpool_block*)pool->block_list.next)->buf, 
	((bpool_block*)pool->block_list.next)->end));

    reset_pool(pool);
    initial_size = ((bpool_block*)pool->block_list.next)->end - 
		   (unsigned char*)pool;
    if (pool->factory->policy.block_free)
	(*pool->factory->policy.block_free)(pool->factory, pool, initial_size);
}


#endif	/* BASE_HAS_POOL_ALT_API */

