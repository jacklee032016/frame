/* 
 *
 */
#include <basePoolBuf.h>
#include <baseAssert.h>
#include <baseOs.h>
#include <baseLog.h>

static struct _bpool_factory stack_based_factory;

struct creation_param
{
	void	*stack_buf;
	bsize_t	 size;
};

static int is_initialized;
static long tls = -1;
static void* stack_alloc(bpool_factory *factory, bsize_t size);

static void pool_buf_cleanup(void *data)
{
	if (tls != -1)
	{
		bthreadLocalFree(tls);
		tls = -1;
	}
	
	if (is_initialized)
		is_initialized = 0;
}

static bstatus_t pool_buf_initialize(void)
{
	libBaseAtExit(&pool_buf_cleanup, "PoolExit", NULL);

	stack_based_factory.policy.block_alloc = &stack_alloc;
	return bthreadLocalAlloc(&tls);
}

static void* stack_alloc(bpool_factory *factory, bsize_t size)
{
	struct creation_param *param;
	void *buf;

	BASE_UNUSED_ARG(factory);

	param = (struct creation_param*) bthreadLocalGet(tls);
	if (param == NULL)
	{/* Don't assert(), this is normal no-memory situation */
		return NULL;
	}

	bthreadLocalSet(tls, NULL);

	BASE_ASSERT_RETURN(size <= param->size, NULL);

	buf = param->stack_buf;

	/* Prevent the buffer from being reused */
	param->stack_buf = NULL;

	return buf;
}


bpool_t* bpool_create_on_buf(const char *name, void *buf, bsize_t size)
{
#if BASE_HAS_POOL_ALT_API == 0
	struct creation_param param;
	bsize_t align_diff;

	BASE_ASSERT_RETURN(buf && size, NULL);

	if (!is_initialized)
	{
		if (pool_buf_initialize() != BASE_SUCCESS)
			return NULL;
		is_initialized = 1;
	}

	/* Check and align buffer */
	align_diff = (bsize_t)buf;
	if (align_diff & (BASE_POOL_ALIGNMENT-1))
	{
		align_diff &= (BASE_POOL_ALIGNMENT-1);
		buf = (void*) (((char*)buf) + align_diff);
		size -= align_diff;
	}

	param.stack_buf = buf;
	param.size = size;
	bthreadLocalSet(tls, &param);

	return bpool_create_int(&stack_based_factory, name, size, 0, bpool_factory_default_policy.callback);
#else
	BASE_UNUSED_ARG(buf);
	return bpool_create(NULL, name, size, size, NULL);
#endif
}

