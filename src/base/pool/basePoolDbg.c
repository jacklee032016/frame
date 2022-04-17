/* 
 *
 */
#include <basePool.h>
#include <baseString.h>

#if BASE_HAS_POOL_ALT_API

#if BASE_HAS_MALLOC_H
#   include <malloc.h>
#endif


#if BASE_HAS_STDLIB_H
#   include <stdlib.h>
#endif


#if ((defined(BASE_WIN32) && BASE_WIN32!=0) || \
     (defined(BASE_WIN64) && BASE_WIN64 != 0)) && \
     defined(BASE_LIB_DEBUG) && BASE_LIB_DEBUG!=0 && !BASE_NATIVE_STRING_IS_UNICODE
#   include <windows.h>
#   define MTRACE(msg)	OutputDebugString(msg)
#endif

/* Uncomment this to enable MTRACE */
//#undef MTRACE



int BASE_NO_MEMORY_EXCEPTION;


int bNO_MEMORY_EXCEPTION()
{
    return BASE_NO_MEMORY_EXCEPTION;
}

/* Create pool */
bpool_t* bpool_create_imp( const char *file, int line,
				       void *factory,
				       const char *name,
				       bsize_t initial_size,
				       bsize_t increment_size,
				       bpool_callback *callback)
{
    bpool_t *pool;

    BASE_UNUSED_ARG(file);
    BASE_UNUSED_ARG(line);
    BASE_UNUSED_ARG(factory);
    BASE_UNUSED_ARG(initial_size);
    BASE_UNUSED_ARG(increment_size);

    pool = malloc(sizeof(struct bpool_t));
    if (!pool)
		return NULL;

    if (name)
	{
		bansi_strncpy(pool->objName, name, sizeof(pool->objName));
		pool->objName[sizeof(pool->objName)-1] = '\0';
    }
	else
	{
		strcpy(pool->objName, "altpool");
    }

    pool->factory = NULL;
    pool->first_mem = NULL;
    pool->used_size = 0;
    pool->cb = callback;

    return pool;
}


/* Release pool */
void bpool_release_imp(bpool_t *pool)
{
    bpool_reset(pool);
    free(pool);
}

/* Get pool name */
const char* bpool_getobjname_imp(bpool_t *pool)
{
    BASE_UNUSED_ARG(pool);
    return "pooldbg";
}

/* Reset pool */
void bpool_reset_imp(bpool_t *pool)
{
    struct bpool_mem *mem;

    mem = pool->first_mem;
    while (mem) {
	struct bpool_mem *next = mem->next;
	free(mem);
	mem = next;
    }

    pool->first_mem = NULL;
}

/* Get capacity */
bsize_t bpool_get_capacity_imp(bpool_t *pool)
{
    BASE_UNUSED_ARG(pool);

    /* Unlimited capacity */
    return 0x7FFFFFFFUL;
}

/* Get total used size */
bsize_t bpool_get_used_size_imp(bpool_t *pool)
{
    return pool->used_size;
}

/* Allocate memory from the pool */
void* bpool_alloc_imp( const char *file, int line, 
				 bpool_t *pool, bsize_t sz)
{
    struct bpool_mem *mem;

    BASE_UNUSED_ARG(file);
    BASE_UNUSED_ARG(line);

    mem = malloc(sz + sizeof(struct bpool_mem));
    if (!mem) {
	if (pool->cb)
	    (*pool->cb)(pool, sz);
	return NULL;
    }

    mem->next = pool->first_mem;
    pool->first_mem = mem;

#ifdef MTRACE
    {
	char msg[120];
	bansi_sprintf(msg, "Mem %X (%d+%d bytes) allocated by %s:%d\r\n",
			mem, sz, sizeof(struct bpool_mem), 
			file, line);
	MTRACE(msg);
    }
#endif

    return ((char*)mem) + sizeof(struct bpool_mem);
}

/* Allocate memory from the pool and zero the memory */
void* bpool_calloc_imp( const char *file, int line, 
				  bpool_t *pool, unsigned cnt, 
				  unsigned elemsz)
{
    void *mem;

    mem = bpool_alloc_imp(file, line, pool, cnt*elemsz);
    if (!mem)
	return NULL;

    bbzero(mem, cnt*elemsz);
    return mem;
}

/* Allocate memory from the pool and zero the memory */
void* bpool_zalloc_imp( const char *file, int line, 
				  bpool_t *pool, bsize_t sz)
{
    return bpool_calloc_imp(file, line, pool, 1, sz); 
}



#endif	/* BASE_HAS_POOL_ALT_API */
