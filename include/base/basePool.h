/* 
 *
 */

#include <baseList.h>

/* See if we use pool's alternate API.
 * The alternate API is used e.g. to implement pool debugging.
 */
#if BASE_HAS_POOL_ALT_API
#include <_basePoolAlt.h>
#endif


#ifndef __BASE_POOL_H__
#define __BASE_POOL_H__

/**
 * @brief Memory Pool.
 */

BASE_BEGIN_DECL

/**
 * @defgroup BASE_POOL_GROUP Fast Memory Pool
 * @brief
 * Memory pools allow dynamic memory allocation comparable to malloc or the 
 * new in operator C++. Those implementations are not desirable for very
 * high performance applications or real-time systems, because of the 
 * performance bottlenecks and it suffers from fragmentation issue.
 *
 * \section BASE_POOL_INTRO_SEC 's Memory Pool
 * \subsection BASE_POOL_ADVANTAGE_SUBSEC Advantages
 * 
 * 's pool has many advantages over traditional malloc/new operator and
 * over other memory pool implementations, because:
 *  - unlike other memory pool implementation, it allows allocation of
 *    memory chunks of different sizes,
 *  - it's very very fast. 
 *    \n
 *    Memory chunk allocation is not only an O(1) 
 *    operation, but it's also very simple (just 
 *    few pointer arithmetic operations) and it doesn't require locking 
 *    any mutex,
 *  - it's memory efficient.
 *    \n
 *    Pool doesn't keep track individual memory chunks allocated by
 *    applications, so there is no additional overhead needed for each
 *    memory allocation (other than possible additional of few bytes, up to
 *    BASE_POOL_ALIGNMENT-1, for aligning the memory). 
 *    But see the @ref BASE_POOL_CAVEATS_SUBSEC below.
 *  - it prevents memory leaks. 
 *    \n
 *    Memory pool inherently has garbage collection functionality. In fact, 
 *    there is no need to free the chunks allocated from the memory pool.
 *    All chunks previously allocated from the pool will be freed once the
 *    pool itself is destroyed. This would prevent memory leaks that haunt
 *    programmers for decades, and it provides additional performance 
 *    advantage over traditional malloc/new operator.
 *
 * Even more, 's memory pool provides some additional usability and
 * flexibility for applications:
 *  - memory leaks are easily traceable, since memory pool is assigned name,
 *    and application can inspect what pools currently active in the system.
 *  - by design, memory allocation from a pool is not thread safe. We assumed
 *    that a pool will be owned by a higher level object, and thread safety 
 *    should be handled by that object. This enables very fast pool operations
 *    and prevents unnecessary locking operations,
 *  - by default, the memory pool API behaves more like C++ new operator, 
 *    in that it will throw BASE_NO_MEMORY_EXCEPTION exception (see 
 *    @ref BASE_EXCEPT) when memory chunk allocation fails. This enables failure
 *    handling to be done on more high level function (instead of checking
 *    the result of bpool_alloc() everytime). If application doesn't like
 *    this, the default behavior can be changed on global basis by supplying 
 *    different policy to the pool factory.
 *  - any memory allocation backend allocator/deallocator may be used. By
 *    default, the policy uses malloc() and free() to manage the pool's block,
 *    but application may use different strategy, for example to allocate
 *    memory blocks from a globally static memory location.
 *
 *
 * \subsection BASE_POOL_PERFORMANCE_SUBSEC Performance
 * 
 * The result of 's memory design and careful implementation is a
 * memory allocation strategy that can speed-up the memory allocations
 * and deallocations by up to <b>30 times</b> compared to standard
 * malloc()/free() (more than 150 million allocations per second on a
 * P4/3.0GHz Linux machine).
 *
 * (Note: your mileage may vary, of course. You can see how much 's
 *  pool improves the performance over malloc()/free() in your target
 *  system by running test application).
 *
 *
 * \subsection BASE_POOL_CAVEATS_SUBSEC Caveats
 *
 * There are some caveats though!
 *
 * When creating pool,  requires applications to specify the initial
 * pool size, and as soon as the pool is created,  allocates memory
 * from the system by that size. Application designers MUST choose the 
 * initial pool size carefully, since choosing too big value will result in
 * wasting system's memory.
 *
 * But the pool can grow. Application designer can specify how the
 * pool will grow in size, by specifying the size increment when creating
 * the pool.
 *
 * The pool, however, <b>cannot</b> shrink! Since there is <b>no</b> 
 * function to deallocate memory chunks, there is no way for the pool to 
 * release back unused memory to the system. 
 * Application designers must be aware that constant memory allocations 
 * from pool that has infinite life-time may cause the memory usage of 
 * the application to grow over time.
 *
 *
 * \section BASE_POOL_USING_SEC Using Memory Pool
 *
 * This section describes how to use 's memory pool framework.
 * As we hope the readers will witness, 's memory pool API is quite
 * straightforward. 
 *
 * \subsection BASE_POOL_USING_F Create Pool Factory
 * First, application needs to initialize a pool factory (this normally
 * only needs to be done once in one application).  provides
 * a pool factory implementation called caching pool (see @ref 
 * BASE_CACHING_POOL), and it is initialized by calling #bcaching_pool_init().
 *
 * \subsection BASE_POOL_USING_P Create The Pool
 * Then application creates the pool object itself with #bpool_create(),
 * specifying among other thing the pool factory where the pool should
 * be created from, the pool name, initial size, and increment/expansion
 * size.
 *
 * \subsection BASE_POOL_USING_M Allocate Memory as Required
 * Then whenever application needs to allocate dynamic memory, it would
 * call #bpool_alloc(), #bpool_calloc(), or #bpool_zalloc() to
 * allocate memory chunks from the pool.
 *
 * \subsection BASE_POOL_USING_DP Destroy the Pool
 * When application has finished with the pool, it should call 
 * #bpool_release() to release the pool object back to the factory. 
 * Depending on the types of the factory, this may release the memory back 
 * to the operating system.
 *
 * \subsection BASE_POOL_USING_Dc Destroy the Pool Factory
 * And finally, before application quites, it should deinitialize the
 * pool factory, to make sure that all memory blocks allocated by the
 * factory are released back to the operating system. After this, of 
 * course no more memory pool allocation can be requested.
 *
 * \subsection BASE_POOL_USING_EX Example
 * Below is a sample complete program that utilizes 's memory pool.
 *
 * \code

   #include <libBase.h>

   static void my_perror(const char *title, bstatus_t status)
   {
	BASE_PERROR(1,(THIS_FILE, status, title));
   }

   static void pool_demo_1(bpool_factory *pfactory)
   {
	unsigned i;
	bpool_t *pool;

	// Must create pool before we can allocate anything
	pool = bpool_create(pfactory,	 // the factory
			      "pool1",	 // pool's name
			      4000,	 // initial size
			      4000,	 // increment size
			      NULL);	 // use default callback.
	if (pool == NULL) {
	    my_perror("Error creating pool", BASE_ENOMEM);
	    return;
	}

	// Demo: allocate some memory chunks
	for (i=0; i<1000; ++i) {
	    void *p;

	    p = bpool_alloc(pool, (brand()+1) % 512);

	    // Do something with p
	    ...

	    // Look! No need to free p!!
	}

	// Done with silly demo, must free pool to release all memory.
	bpool_release(pool);
   }

   int main()
   {
	bcaching_pool cp;
	bstatus_t status;

        // Must init  before anything else
	status = libBaseInit();
	if (status != BASE_SUCCESS) {
	    my_perror("Error initializing ", status);
	    return 1;
	}

	// Create the pool factory, in this case, a caching pool,
	// using default pool policy.
	bcaching_pool_init(&cp, NULL, 1024*1024 );

	// Do a demo
	pool_demo_1(&cp.factory);

	// Done with demos, destroy caching pool before exiting app.
	bcaching_pool_destroy(&cp);

	return 0;
   }

   \endcode
 *
 * More information about pool factory, the pool object, and caching pool
 * can be found on the Module Links below.
 */


/**
 * @defgroup BASE_POOL Memory Pool Object
 * @ingroup BASE_POOL_GROUP
 * @brief
 * The memory pool is an opaque object created by pool factory.
 * Application uses this object to request a memory chunk, by calling
 * #bpool_alloc(), #bpool_calloc(), or #bpool_zalloc(). 
 * When the application has finished using
 * the pool, it must call #bpool_release() to free all the chunks previously
 * allocated and release the pool back to the factory.
 *
 * A memory pool is initialized with an initial amount of memory, which is
 * called a block. Pool can be configured to dynamically allocate more memory 
 * blocks when it runs out of memory. 
 *
 * The pool doesn't keep track of individual memory allocations
 * by user, and the user doesn't have to free these indidual allocations. This
 * makes memory allocation simple and very fast. All the memory allocated from
 * the pool will be destroyed when the pool itself is destroyed.
 *
 * \section BASE_POOL_THREADING_SEC More on Threading Policies
 * - By design, memory allocation from a pool is not thread safe. We assumed 
 *   that a pool will be owned by an object, and thread safety should be 
 *   handled by that object. Thus these functions are not thread safe: 
 *	- #bpool_alloc, 
 *	- #bpool_calloc, 
 *	- and other pool statistic functions.
 * - Threading in the pool factory is decided by the policy set for the
 *   factory when it was created.
 *
 * \section BASE_POOL_EXAMPLES_SEC Examples
 *
 * For some sample codes on how to use the pool, please see:
 *  - @ref page_baselib_pool_test
 *
 * @{
 */

/**
 * The type for function to receive callback from the pool when it is unable
 * to allocate memory. The elegant way to handle this condition is to throw
 * exception, and this is what is expected by most of this library 
 * components.
 */
typedef void bpool_callback(bpool_t *pool, bsize_t size);

/**
 * This class, which is used internally by the pool, describes a single 
 * block of memory from which user memory allocations will be allocated from.
 */
typedef struct bpool_block
{
    BASE_DECL_LIST_MEMBER(struct bpool_block);  /**< List's prev and next.  */
    unsigned char    *buf;                      /**< Start of buffer.       */
    unsigned char    *cur;                      /**< Current alloc ptr.     */
    unsigned char    *end;                      /**< End of buffer.         */
} bpool_block;


/**
 * This structure describes the memory pool. Only implementors of pool factory
 * need to care about the contents of this structure.
 */
struct _bpool_t
{
    BASE_DECL_LIST_MEMBER(struct _bpool_t);  /**< Standard list elements.    */

    /** Pool name */
    char	    objName[BASE_MAX_OBJ_NAME];

    /** Pool factory. */
    bpool_factory *factory;

    /** Data put by factory */
    void	    *factory_data;

    /** Current capacity allocated by the pool. */
    bsize_t	    capacity;

    /** Size of memory block to be allocated when the pool runs out of memory */
    bsize_t	    increment_size;

    /** List of memory blocks allcoated by the pool. */
    bpool_block   block_list;

    /** The callback to be called when the pool is unable to allocate memory. */
    bpool_callback *callback;

};


/**
 * Guidance on how much memory required for initial pool administrative data.
 */
#define BASE_POOL_SIZE	        (sizeof(struct _bpool_t))

/** 
 * Pool memory alignment (must be power of 2). 
 */
#ifndef BASE_POOL_ALIGNMENT
#   define BASE_POOL_ALIGNMENT    4
#endif

/**
 * Create a new pool from the pool factory. This wrapper will call create_pool
 * member of the pool factory.
 *
 * @param factory	    The pool factory.
 * @param name		    The name to be assigned to the pool. The name should 
 *			    not be longer than BASE_MAX_OBJ_NAME (32 chars), or 
 *			    otherwise it will be truncated.
 * @param initial_size	    The size of initial memory blocks taken by the pool.
 *			    Note that the pool will take 68+20 bytes for 
 *			    administrative area from this block.
 * @param increment_size    the size of each additional blocks to be allocated
 *			    when the pool is running out of memory. If user 
 *			    requests memory which is larger than this size, then 
 *			    an error occurs.
 *			    Note that each time a pool allocates additional block, 
 *			    it needs BASE_POOL_SIZE more to store some 
 *			    administrative info.
 * @param callback	    Callback to be called when error occurs in the pool.
 *			    If this value is NULL, then the callback from pool
 *			    factory policy will be used.
 *			    Note that when an error occurs during pool creation, 
 *			    the callback itself is not called. Instead, NULL 
 *			    will be returned.
 *
 * @return                  The memory pool, or NULL.
 */
bpool_t* bpool_create(bpool_factory *factory, 
				    const char *name,
				    bsize_t initial_size, 
				    bsize_t increment_size,
				    bpool_callback *callback);

/**
 * Release the pool back to pool factory.
 *
 * @param pool	    Memory pool.
 */
void bpool_release( bpool_t *pool );


/**
 * Release the pool back to pool factory and set the pool pointer to zero.
 *
 * @param ppool	    Pointer to memory pool.
 */
void bpool_safe_release( bpool_t **ppool );


/**
 * Release the pool back to pool factory and set the pool pointer to zero.
 * The memory pool content will be wiped out first before released.
 *
 * @param ppool	    Pointer to memory pool.
 */
void bpool_secure_release( bpool_t **ppool );


/**
 * Get pool object name.
 *
 * @param pool the pool.
 *
 * @return pool name as NULL terminated string.
 */
const char * bpool_getobjname( const bpool_t *pool );

/**
 * Reset the pool to its state when it was initialized.
 * This means that if additional blocks have been allocated during runtime, 
 * then they will be freed. Only the original block allocated during 
 * initialization is retained. This function will also reset the internal 
 * counters, such as pool capacity and used size.
 *
 * @param pool the pool.
 */
void bpool_reset( bpool_t *pool );


/**
 * Get the pool capacity, that is, the system storage that have been allocated
 * by the pool, and have been used/will be used to allocate user requests.
 * There's no guarantee that the returned value represent a single
 * contiguous block, because the capacity may be spread in several blocks.
 *
 * @param pool	the pool.
 *
 * @return the capacity.
 */
bsize_t bpool_get_capacity( bpool_t *pool );

/**
 * Get the total size of user allocation request.
 *
 * @param pool	the pool.
 *
 * @return the total size.
 */
bsize_t bpool_get_used_size( bpool_t *pool );

/**
 * Allocate storage with the specified size from the pool.
 * If there's no storage available in the pool, then the pool can allocate more
 * blocks if the increment size is larger than the requested size.
 *
 * @param pool	    the pool.
 * @param size	    the requested size.
 *
 * @return pointer to the allocated memory.
 *
 * @see BASE_POOL_ALLOC_T
 */
void* bpool_alloc( bpool_t *pool, bsize_t size);

/**
 * Allocate storage  from the pool, and initialize it to zero.
 * This function behaves like bpool_alloc(), except that the storage will
 * be initialized to zero.
 *
 * @param pool	    the pool.
 * @param count	    the number of elements in the array.
 * @param elem	    the size of individual element.
 *
 * @return pointer to the allocated memory.
 */
void* bpool_calloc( bpool_t *pool, bsize_t count, bsize_t elem);


/**
 * Allocate storage from the pool and initialize it to zero.
 *
 * @param pool	    The pool.
 * @param size	    The size to be allocated.
 *
 * @return	    Pointer to the allocated memory.
 *
 * @see BASE_POOL_ZALLOC_T
 */
BASE_INLINE_SPECIFIER void* bpool_zalloc(bpool_t *pool, bsize_t size)
{
    return bpool_calloc(pool, 1, size);
}


/**
 * This macro allocates memory from the pool and returns the instance of
 * the specified type. It provides a stricker type safety than bpool_alloc()
 * since the return value of this macro will be type-casted to the specified
 * type.
 *
 * @param pool	    The pool
 * @param type	    The type of object to be allocated
 *
 * @return	    Memory buffer of the specified type.
 */
#define BASE_POOL_ALLOC_T(pool,type) \
	    ((type*)bpool_alloc(pool, sizeof(type)))

/**
 * This macro allocates memory from the pool, zeroes the buffer, and 
 * returns the instance of the specified type. It provides a stricker type 
 * safety than bpool_zalloc() since the return value of this macro will be 
 * type-casted to the specified type.
 *
 * @param pool	    The pool
 * @param type	    The type of object to be allocated
 *
 * @return	    Memory buffer of the specified type.
 */
#define BASE_POOL_ZALLOC_T(pool,type) \
	    ((type*)bpool_zalloc(pool, sizeof(type)))

/*
 * Internal functions
 */
void* bpool_alloc_from_block(bpool_block *block, bsize_t size);
void* bpool_allocate_find(bpool_t *pool, bsize_t size);


	
/**
 * @}	// BASE_POOL
 */

/* **************************************************************************/
/**
 * @defgroup BASE_POOL_FACTORY Pool Factory and Policy
 * @ingroup BASE_POOL_GROUP
 * @brief
 * A pool object must be created through a factory. A factory not only provides
 * generic interface functions to create and release pool, but also provides 
 * strategy to manage the life time of pools. One sample implementation, 
 * \a bcaching_pool, can be set to keep the pools released by application for
 * future use as long as the total memory is below the limit.
 * 
 * The pool factory interface declared in  is designed to be extensible.
 * Application can define its own strategy by creating it's own pool factory
 * implementation, and this strategy can be used even by existing library
 * without recompilation.
 *
 * \section BASE_POOL_FACTORY_ITF Pool Factory Interface
 * The pool factory defines the following interface:
 *  - \a policy: the memory pool factory policy.
 *  - \a create_pool(): create a new memory pool.
 *  - \a release_pool(): release memory pool back to factory.
 *
 * \section BASE_POOL_FACTORY_POL Pool Factory Policy.
 *
 * A pool factory only defines functions to create and release pool and how
 * to manage pools, but the rest of the functionalities are controlled by
 * policy. A pool policy defines:
 *  - how memory block is allocated and deallocated (the default implementation
 *    allocates and deallocate memory by calling malloc() and free()).
 *  - callback to be called when memory allocation inside a pool fails (the
 *    default implementation will throw BASE_NO_MEMORY_EXCEPTION exception).
 *  - concurrency when creating and releasing pool from/to the factory.
 *
 * A pool factory can be given different policy during creation to make
 * it behave differently. For example, caching pool factory can be configured
 * to allocate and deallocate from a static/contiguous/preallocated memory 
 * instead of using malloc()/free().
 * 
 * What strategy/factory and what policy to use is not defined by , but
 * instead is left to application to make use whichever is most efficient for
 * itself.
 *
 * The pool factory policy controls the behaviour of memory factories, and
 * defines the following interface:
 *  - \a block_alloc(): allocate memory block from backend memory mgmt/system.
 *  - \a block_free(): free memory block back to backend memory mgmt/system.
 * @{
 */

/* We unfortunately don't have support for factory policy options as now,
   so we keep this commented at the moment.
enum BASE_POOL_FACTORY_OPTION
{
    BASE_POOL_FACTORY_SERIALIZE = 1
};
*/

/**
 * This structure declares pool factory interface.
 */
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

/**
 * \def BASE_NO_MEMORY_EXCEPTION
 * This constant denotes the exception number that will be thrown by default
 * memory factory policy when memory allocation fails.
 *
 * @see bNO_MEMORY_EXCEPTION()
 */
extern	int BASE_NO_MEMORY_EXCEPTION;

/**
 * Get #BASE_NO_MEMORY_EXCEPTION constant.
 */ 
int bNO_MEMORY_EXCEPTION(void);

/**
 * This global variable points to default memory pool factory policy.
 * The behaviour of the default policy is:
 *  - block allocation and deallocation use malloc() and free().
 *  - callback will raise BASE_NO_MEMORY_EXCEPTION exception.
 *  - access to pool factory is not serialized (i.e. not thread safe).
 *
 * @see bpool_factory_get_default_policy
 */
extern	bpool_factory_policy 	bpool_factory_default_policy;


/**
 * Get the default pool factory policy.
 *
 * @return the pool policy.
 */
const bpool_factory_policy* bpool_factory_get_default_policy(void);


/**
 * This structure contains the declaration for pool factory interface.
 */
struct _bpool_factory
{
    /**
     * Memory pool policy.
     */
    bpool_factory_policy policy;

    /**
    * Create a new pool from the pool factory.
    *
    * @param factory	The pool factory.
    * @param name	the name to be assigned to the pool. The name should 
    *			not be longer than BASE_MAX_OBJ_NAME (32 chars), or 
    *			otherwise it will be truncated.
    * @param initial_size the size of initial memory blocks taken by the pool.
    *			Note that the pool will take 68+20 bytes for 
    *			administrative area from this block.
    * @param increment_size the size of each additional blocks to be allocated
    *			when the pool is running out of memory. If user 
    *			requests memory which is larger than this size, then 
    *			an error occurs.
    *			Note that each time a pool allocates additional block, 
    *			it needs 20 bytes (equal to sizeof(bpool_block)) to 
    *			store some administrative info.
    * @param callback	Cllback to be called when error occurs in the pool.
    *			Note that when an error occurs during pool creation, 
    *			the callback itself is not called. Instead, NULL 
    *			will be returned.
    *
    * @return the memory pool, or NULL.
    */
    bpool_t*	(*create_pool)( bpool_factory *factory,
				const char *name,
				bsize_t initial_size, 
				bsize_t increment_size,
				bpool_callback *callback);

    /**
     * Release the pool to the pool factory.
     *
     * @param factory	The pool factory.
     * @param pool	The pool to be released.
    */
    void (*release_pool)( bpool_factory *factory, bpool_t *pool );

    /**
     * Dump pool status to log.
     *
     * @param factory	The pool factory.
     */
    void (*dump_status)( bpool_factory *factory, bbool_t detail );

    /**
     * This is optional callback to be called by allocation policy when
     * it allocates a new memory block. The factory may use this callback
     * for example to keep track of the total number of memory blocks
     * currently allocated by applications.
     *
     * @param factory	    The pool factory.
     * @param size	    Size requested by application.
     *
     * @return		    MUST return BASE_TRUE, otherwise the block
     *                      allocation is cancelled.
     */
    bbool_t (*on_block_alloc)(bpool_factory *factory, bsize_t size);

    /**
     * This is optional callback to be called by allocation policy when
     * it frees memory block. The factory may use this callback
     * for example to keep track of the total number of memory blocks
     * currently allocated by applications.
     *
     * @param factory	    The pool factory.
     * @param size	    Size freed.
     */
    void (*on_block_free)(bpool_factory *factory, bsize_t size);

};

/**
 * This function is intended to be used by pool factory implementors.
 * @param factory           Pool factory.
 * @param name              Pool name.
 * @param initial_size      Initial size.
 * @param increment_size    Increment size.
 * @param callback          Callback.
 * @return                  The pool object, or NULL.
 */
bpool_t* bpool_create_int(	bpool_factory *factory, 
					const char *name,
					bsize_t initial_size, 
					bsize_t increment_size,
					bpool_callback *callback);

/**
 * This function is intended to be used by pool factory implementors.
 * @param pool              The pool.
 * @param name              Pool name.
 * @param increment_size    Increment size.
 * @param callback          Callback function.
 */
void bpool_init_int( bpool_t *pool, 
				const char *name,
				bsize_t increment_size,
				bpool_callback *callback);

/**
 * This function is intended to be used by pool factory implementors.
 * @param pool      The memory pool.
 */
void bpool_destroy_int( bpool_t *pool );


/**
 * Dump pool factory state.
 * @param pf	    The pool factory.
 * @param detail    Detail state required.
 */
BASE_INLINE_SPECIFIER void bpool_factory_dump( bpool_factory *pf,
				      bbool_t detail )
{
    (*pf->dump_status)(pf, detail);
}

/**
 *  @}	// BASE_POOL_FACTORY
 */

/* **************************************************************************/

/**
 * @defgroup BASE_CACHING_POOL Caching Pool Factory
 * @ingroup BASE_POOL_GROUP
 * @brief
 * Caching pool is one sample implementation of pool factory where the
 * factory can reuse memory to create a pool. Application defines what the 
 * maximum memory the factory can hold, and when a pool is released the
 * factory decides whether to destroy the pool or to keep it for future use.
 * If the total amount of memory in the internal cache is still within the
 * limit, the factory will keep the pool in the internal cache, otherwise the
 * pool will be destroyed, thus releasing the memory back to the system.
 *
 * @{
 */

/**
 * Number of unique sizes, to be used as index to the free list.
 * Each pool in the free list is organized by it's size.
 */
#define BASE_CACHING_POOL_ARRAY_SIZE	16

/**
 * Declaration for caching pool. Application doesn't normally need to
 * care about the contents of this struct, it is only provided here because
 * application need to define an instance of this struct (we can not allocate
 * the struct from a pool since there is no pool factory yet!).
 */
struct _bcaching_pool 
{
    /** Pool factory interface, must be declared first. */
    bpool_factory factory;

    /** Current factory's capacity, i.e. number of bytes that are allocated
     *  and available for application in this factory. The factory's
     *  capacity represents the size of all pools kept by this factory
     *  in it's free list, which will be returned to application when it
     *  requests to create a new pool.
     */
    bsize_t	    capacity;

    /** Maximum size that can be held by this factory. Once the capacity
     *  has exceeded @a max_capacity, further #bpool_release() will
     *  flush the pool. If the capacity is still below the @a max_capacity,
     *  #bpool_release() will save the pool to the factory's free list.
     */
    bsize_t       max_capacity;

    /**
     * Number of pools currently held by applications. This number gets
     * incremented everytime #bpool_create() is called, and gets
     * decremented when #bpool_release() is called.
     */
    bsize_t       used_count;

    /**
     * Total size of memory currently used by application.
     */
    bsize_t	    used_size;

    /**
     * The maximum size of memory used by application throughout the life
     * of the caching pool.
     */
    bsize_t	    peak_used_size;

    /**
     * Lists of pools in the cache, indexed by pool size.
     */
    blist	    free_list[BASE_CACHING_POOL_ARRAY_SIZE];

    /**
     * List of pools currently allocated by applications.
     */
    blist	    used_list;

    /**
     * Internal pool.
     */
    char	    pool_buf[256 * (sizeof(size_t) / 4)];

    /**
     * Mutex.
     */
    block_t	   *lock;
};



/**
 * Initialize caching pool.
 *
 * @param ch_pool	The caching pool factory to be initialized.
 * @param policy	Pool factory policy.
 * @param max_capacity	The total capacity to be retained in the cache. When
 *			the pool is returned to the cache, it will be kept in
 *			recycling list if the total capacity of pools in this
 *			list plus the capacity of the pool is still below this
 *			value.
 */
void bcaching_pool_init( bcaching_pool *ch_pool, 
				    const bpool_factory_policy *policy,
				    bsize_t max_capacity);


/**
 * Destroy caching pool, and release all the pools in the recycling list.
 *
 * @param ch_pool	The caching pool.
 */
void bcaching_pool_destroy( bcaching_pool *ch_pool );

/**
 * @}	// BASE_CACHING_POOL
 */

#  if BASE_FUNCTIONS_ARE_INLINED
#    include "pool_i.h"
#  endif

BASE_END_DECL
    
#endif


