/* $Id: pool_i.h 5990 2019-05-15 02:43:01Z nanang $ */
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


#include <baseString.h>


BASE_IDEF(bsize_t) bpool_get_capacity( bpool_t *pool )
{
    return pool->capacity;
}

BASE_IDEF(bsize_t) bpool_get_used_size( bpool_t *pool )
{
    bpool_block *b = pool->block_list.next;
    bsize_t used_size = sizeof(bpool_t);
    while (b != &pool->block_list) {
	used_size += (b->cur - b->buf) + sizeof(bpool_block);
	b = b->next;
    }
    return used_size;
}

BASE_IDEF(void*) bpool_alloc_from_block( bpool_block *block, bsize_t size )
{
    /* The operation below is valid for size==0. 
     * When size==0, the function will return the pointer to the pool
     * memory address, but no memory will be allocated.
     */
    if (size & (BASE_POOL_ALIGNMENT-1)) {
	size = (size + BASE_POOL_ALIGNMENT) & ~(BASE_POOL_ALIGNMENT-1);
    }
    if ((bsize_t)(block->end - block->cur) >= size) {
	void *ptr = block->cur;
	block->cur += size;
	return ptr;
    }
    return NULL;
}

BASE_IDEF(void*) bpool_alloc( bpool_t *pool, bsize_t size)
{
    void *ptr = bpool_alloc_from_block(pool->block_list.next, size);
    if (!ptr)
	ptr = bpool_allocate_find(pool, size);
    return ptr;
}


BASE_IDEF(void*) bpool_calloc( bpool_t *pool, bsize_t count, bsize_t size)
{
    void *buf = bpool_alloc( pool, size*count);
    if (buf)
	bbzero(buf, size * count);
    return buf;
}

BASE_IDEF(const char *) bpool_getobjname( const bpool_t *pool )
{
    return pool->objName;
}

BASE_IDEF(bpool_t*) bpool_create( bpool_factory *f, const char *name, bsize_t initial_size, bsize_t increment_size, bpool_callback *callback)
{
    return (*f->create_pool)(f, name, initial_size, increment_size, callback);
}

BASE_IDEF(void) bpool_release( bpool_t *pool )
{
	BTRACE();
    if (pool->factory->release_pool)
	(*pool->factory->release_pool)(pool->factory, pool);
}


BASE_IDEF(void) bpool_safe_release( bpool_t **ppool )
{
    bpool_t *pool = *ppool;
    *ppool = NULL;
    if (pool)
	bpool_release(pool);
}

BASE_IDEF(void) bpool_secure_release( bpool_t **ppool )
{
    bpool_block *b;
    bpool_t *pool = *ppool;
    *ppool = NULL;

    if (!pool)
	return;

    b = pool->block_list.next;
    while (b != &pool->block_list) {
	volatile unsigned char *p = b->buf;
	while (p < b->end) *p++ = 0;
	b = b->next;
    }

    bpool_release(pool);
}
