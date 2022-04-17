/* 
 *
 */

#ifndef __BASE_POOL_ALT_H__
#define __BASE_POOL_ALT_H__

#define __BASE_POOL_H__

BASE_BEGIN_DECL

/**
 * The type for function to receive callback from the pool when it is unable
 * to allocate memory. The elegant way to handle this condition is to throw
 * exception, and this is what is expected by most of this library 
 * components.
 */
typedef void bpool_callback(bpool_t *pool, bsize_t size);

struct bpool_mem
{
	struct bpool_mem *next;

	/* data follows immediately */
};


struct _bpool_t
{
	struct bpool_mem	*first_mem;
	
	bpool_factory    	*factory;
	char					objName[32];
	bsize_t			used_size;
	bpool_callback		*cb;
};


#define BASE_POOL_SIZE	        (sizeof(struct _bpool_t))

/**
 * This constant denotes the exception number that will be thrown by default
 * memory factory policy when memory allocation fails.
 */
extern int BASE_NO_MEMORY_EXCEPTION;



/*
 * Declare all pool API as macro that calls the implementation function.
 */
#define bpool_create(factory, name, initSize, incSize, callback)   \
	bpool_create_imp(__FILE__, __LINE__, factory, name, initSize, incSize, callback)

#define bpool_release(pool)		    bpool_release_imp(pool)
#define bpool_getobjname(pool)	    bpool_getobjname_imp(pool)
#define bpool_reset(pool)		    bpool_reset_imp(pool)
#define bpool_get_capacity(pool)	    bpool_get_capacity_imp(pool)
#define bpool_get_used_size(pool)	    bpool_get_used_size_imp(pool)
#define bpool_alloc(pool,sz)		    \
	bpool_alloc_imp(__FILE__, __LINE__, pool, sz)

#define bpool_calloc(pool,cnt,elem)	    \
	bpool_calloc_imp(__FILE__, __LINE__, pool, cnt, elem)

#define bpool_zalloc(pool,sz)		    \
	bpool_zalloc_imp(__FILE__, __LINE__, pool, sz)



/*
 * Declare prototypes for pool implementation API.
 */

/* Create pool */
bpool_t* bpool_create_imp(const char *file, int line,
				       void *factory,
				       const char *name,
				       bsize_t initial_size,
				       bsize_t increment_size,
				       bpool_callback *callback);

/* Release pool */
void bpool_release_imp(bpool_t *pool);

/* Get pool name */
const char* bpool_getobjname_imp(bpool_t *pool);

/* Reset pool */
void bpool_reset_imp(bpool_t *pool);

/* Get capacity */
bsize_t bpool_get_capacity_imp(bpool_t *pool);

/* Get total used size */
bsize_t bpool_get_used_size_imp(bpool_t *pool);

/* Allocate memory from the pool */
void* bpool_alloc_imp(const char *file, int line, 
				 bpool_t *pool, bsize_t sz);

/* Allocate memory from the pool and zero the memory */
void* bpool_calloc_imp(const char *file, int line, 
				  bpool_t *pool, unsigned cnt, 
				  unsigned elemsz);

/* Allocate memory from the pool and zero the memory */
void* bpool_zalloc_imp(const char *file, int line, 
				  bpool_t *pool, bsize_t sz);


#define BASE_POOL_ZALLOC_T(pool,type) \
	    ((type*)bpool_zalloc(pool, sizeof(type)))
#define BASE_POOL_ALLOC_T(pool,type) \
	    ((type*)bpool_alloc(pool, sizeof(type)))
	    
#ifndef BASE_POOL_ALIGNMENT
#define BASE_POOL_ALIGNMENT    4
#endif

/* This structure declares pool factory interface. */
typedef struct _bpool_factory_policy
{
	/**
	* Allocate memory block (for use by pool). This function is called
	* by memory pool to allocate memory block.
	* 
	* @param factory	Pool factory.
	* @param size	The size of memory block to allocate.
	*
	* @return		Memory block.
	*/
	void* (*block_alloc)(bpool_factory *factory, bsize_t size);

	/**
	* Free memory block.
	*
	* @param factory	Pool factory.
	* @param mem	Memory block previously allocated by block_alloc().
	* @param size	The size of memory block.
	*/
	void (*block_free)(bpool_factory *factory, void *mem, bsize_t size);

	/**
	* Default callback to be called when memory allocation fails.
	*/
	bpool_callback *callback;

	/**
	* Option flags.
	*/
	unsigned flags;

} bpool_factory_policy;

struct _bpool_factory
{
	bpool_factory_policy policy;
	int dummy;
};

struct _bcaching_pool 
{
    bpool_factory factory;

    /* just to make it compilable */
    unsigned used_count;
    unsigned used_size;
    unsigned peak_used_size;
};

/* just to make it compilable */
typedef struct bpool_block
{
    int dummy;
} bpool_block;

#define bcaching_pool_init( cp, pol, mac)
#define bcaching_pool_destroy(cp)
#define bpool_factory_dump(pf, detail)

BASE_END_DECL

#endif

