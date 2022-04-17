/* 
 *
 */
#ifndef __BASE_POOL_STACK_H__
#define __BASE_POOL_STACK_H__

#include <basePool.h>

/**
 * @defgroup BASE_POOL_BUFFER Stack/Buffer Based Memory Pool Allocator
 * @ingroup BASE_POOL_GROUP
 * @brief Stack/buffer based pool.
 *
 * This section describes an implementation of memory pool which uses
 * memory allocated from the stack. Application creates this pool
 * by specifying a buffer (which can be allocated from static memory or
 * stack variable), and then use normal pool API to access/use the pool.
 *
 * If the buffer specified during pool creation is a buffer located in the
 * stack, the pool will be invalidated (or implicitly destroyed) when the
 * execution leaves the enclosing block containing the buffer. Note
 * that application must make sure that any objects allocated from this
 * pool (such as mutexes) have been destroyed before the pool gets
 * invalidated.
 *
 * Sample usage:
 *
 * \code
  #include <libBase.h>

  static void test()
  {
    char buffer[500];
    bpool_t *pool;
    void *p;

    pool = bpool_create_on_buf("thepool", buffer, sizeof(buffer));

    // Use the pool as usual
    p = bpool_alloc(pool, ...);
    ...

    // No need to release the pool
  }

  int main()
  {
    libBaseInit();
    test();
    return 0;
  }

   \endcode
 *
 * @{
 */

BASE_BEGIN_DECL

/**
 * Create the pool using the specified buffer as the pool's memory.
 * Subsequent allocations made from the pool will use the memory from
 * this buffer.
 *
 * If the buffer specified in the parameter is a buffer located in the
 * stack, the pool will be invalid (or implicitly destroyed) when the
 * execution leaves the enclosing block containing the buffer. Note
 * that application must make sure that any objects allocated from this
 * pool (such as mutexes) have been destroyed before the pool gets
 * invalidated.
 *
 * @param name	    Optional pool name.
 * @param buf	    Buffer to be used by the pool.
 * @param size	    The size of the buffer.
 *
 * @return	    The memory pool instance.
 */
bpool_t* bpool_create_on_buf(const char *name,
					  void *buf,
					  bsize_t size);

BASE_END_DECL

#endif

