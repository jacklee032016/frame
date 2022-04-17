/* 
 *
 */
#include <baseConfig.h>
#include <baseLog.h>
#include <baseIoQueue.h>

static const char *id = "config.c";

#define BASE_MAKE_VERSION3_1(a,b,d) 	#a "." #b d
#define BASE_MAKE_VERSION3_2(a,b,d)	BASE_MAKE_VERSION3_1(a,b,d)

#define BASE_MAKE_VERSION4_1(a,b,c,d) 	#a "." #b "." #c d
#define BASE_MAKE_VERSION4_2(a,b,c,d)	BASE_MAKE_VERSION4_1(a,b,c,d)

#if BASE_VERSION_NUM_REV
const char* BASE_VERSION = BASE_MAKE_VERSION4_2(BASE_VERSION_NUM_MAJOR,
						         BASE_VERSION_NUM_MINOR,
						         BASE_VERSION_NUM_REV,
						         BASE_VERSION_NUM_EXTRA);
#else
const char* BASE_VERSION = BASE_MAKE_VERSION3_2(BASE_VERSION_NUM_MAJOR,
						         BASE_VERSION_NUM_MINOR,
						         BASE_VERSION_NUM_EXTRA);
#endif

/*
 * Get  version string.
 */
const char* bget_version(void)
{
	return BASE_VERSION;
}

void bConfigDump(void)
{
	BASE_STR_INFO(id, " (c)2008-2016 Teluu Inc.");
	BASE_STR_INFO(id, "Dumping configurations:");
	BASE_STR_INFO(id, " BASE_VERSION                : %s", BASE_VERSION);
	BASE_STR_INFO(id, " BASE_M_NAME                 : %s", BASE_M_NAME);
	BASE_STR_INFO(id, " BASE_HAS_PENTIUM            : %d", BASE_HAS_PENTIUM);
	BASE_STR_INFO(id, " BASE_OS_NAME                : %s", BASE_OS_NAME);
	BASE_STR_INFO(id, " BASE_CC_NAME/VER_(1,2,3)    : %s-%d.%d.%d", BASE_CC_NAME, BASE_CC_VER_1, BASE_CC_VER_2, BASE_CC_VER_3);
	BASE_STR_INFO(id, " BASE_IS_(BIG/LITTLE)_ENDIAN : %s", (BASE_IS_BIG_ENDIAN?"big-endian":"little-endian"));
	BASE_STR_INFO(id, " BASE_HAS_INT64              : %d", BASE_HAS_INT64);
	BASE_STR_INFO(id, " BASE_HAS_FLOATING_POINT     : %d", BASE_HAS_FLOATING_POINT);
	BASE_STR_INFO(id, " BASE_LIB_DEBUG                  : %d", BASE_LIB_DEBUG);
	BASE_STR_INFO(id, " BASE_FUNCTIONS_ARE_INLINED  : %d", BASE_FUNCTIONS_ARE_INLINED);
	BASE_STR_INFO(id, " BASE_LOG_MAX_LEVEL          : %d", BASE_LOG_MAX_LEVEL);
	BASE_STR_INFO(id, " BASE_LOG_MAX_SIZE           : %d", BASE_LOG_MAX_SIZE);
	BASE_STR_INFO(id, " BASE_LOG_USE_STACK_BUFFER   : %d", BASE_LOG_USE_STACK_BUFFER);
	BASE_STR_INFO(id, " BASE_POOL_DEBUG             : %d", BASE_POOL_DEBUG);
	BASE_STR_INFO(id, " BASE_HAS_POOL_ALT_API       : %d", BASE_HAS_POOL_ALT_API);
	BASE_STR_INFO(id, " BASE_HAS_TCP                : %d", BASE_HAS_TCP);
	BASE_STR_INFO(id, " BASE_MAX_HOSTNAME           : %d", BASE_MAX_HOSTNAME);
	BASE_STR_INFO(id, " ioqueue type              : %s", bioqueue_name());
	BASE_STR_INFO(id, " BASE_IOQUEUE_MAX_HANDLES    : %d", BASE_IOQUEUE_MAX_HANDLES);
	BASE_STR_INFO(id, " BASE_IOQUEUE_HAS_SAFE_UNREG : %d", BASE_IOQUEUE_HAS_SAFE_UNREG);
	BASE_STR_INFO(id, " BASE_HAS_THREADS            : %d", BASE_HAS_THREADS);
	BASE_STR_INFO(id, " BASE_LOG_USE_STACK_BUFFER   : %d", BASE_LOG_USE_STACK_BUFFER);
	BASE_STR_INFO(id, " BASE_HAS_SEMAPHORE          : %d", BASE_HAS_SEMAPHORE);
	BASE_STR_INFO(id, " BASE_HAS_EVENT_OBJ          : %d", BASE_HAS_EVENT_OBJ);
	BASE_STR_INFO(id, " BASE_ENABLE_EXTRA_CHECK     : %d", BASE_ENABLE_EXTRA_CHECK);
	BASE_STR_INFO(id, " BASE_HAS_EXCEPTION_NAMES    : %d", BASE_HAS_EXCEPTION_NAMES);
	BASE_STR_INFO(id, " BASE_MAX_EXCEPTION_ID       : %d", BASE_MAX_EXCEPTION_ID);
	BASE_STR_INFO(id, " BASE_EXCEPTION_USE_WIN32_SEH: %d", BASE_EXCEPTION_USE_WIN32_SEH);
	BASE_STR_INFO(id, " BASE_TIMESTAMP_USE_RDTSC:   : %d", BASE_TIMESTAMP_USE_RDTSC);
	BASE_STR_INFO(id, " BASE_OS_HAS_CHECK_STACK     : %d", BASE_OS_HAS_CHECK_STACK);
	BASE_STR_INFO(id, " BASE_HAS_HIGH_RES_TIMER     : %d", BASE_HAS_HIGH_RES_TIMER);
	BASE_STR_INFO(id, " BASE_HAS_IPV6               : %d", BASE_HAS_IPV6);
}

