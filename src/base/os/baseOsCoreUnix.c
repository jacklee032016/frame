/* 
 *
 */
/*
 * Contributors:
 * - Thanks for Zetron, Inc. (Phil Torre, ptorre@zetron.com) for donating
 *   the RTEMS port.
 */
#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif
#include <baseOs.h>
#include <baseAssert.h>
#include <basePool.h>
#include <baseLog.h>
#include <baseRand.h>
#include <baseString.h>
#include <baseGuid.h>
#include <baseExcept.h>
#include <baseErrno.h>

#if defined(BASE_HAS_SEMAPHORE_H) && BASE_HAS_SEMAPHORE_H != 0
#  include <semaphore.h>
#endif

#include <unistd.h>	    // getpid()
#include <errno.h>	    // errno

#include <pthread.h>


#define SIGNATURE1  0xDEAFBEEF
#define SIGNATURE2  0xDEADC0DE

#ifndef BASE_JNI_HAS_JNI_ONLOAD
#  define BASE_JNI_HAS_JNI_ONLOAD    BASE_ANDROID
#endif

#if defined(BASE_JNI_HAS_JNI_ONLOAD) && BASE_JNI_HAS_JNI_ONLOAD != 0

#include <jni.h>

JavaVM *bjni_jvm = NULL;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    bjni_jvm = vm;
    
    return JNI_VERSION_1_4;
}
#endif

struct bthread_t
{
    char	    objName[BASE_MAX_OBJ_NAME];
    pthread_t	    thread;
    bthread_proc *proc;
    void	   *arg;
    buint32_t	    signature1;
    buint32_t	    signature2;

    bmutex_t	   *suspended_mutex;

#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK!=0
    buint32_t	    stk_size;
    buint32_t	    stk_max_usage;
    char	   *stk_start;
    const char	   *caller_file;
    int		    caller_line;
#endif
};

struct _batomic_t
{
	bmutex_t	       *mutex;
	batomic_value_t	value;
};

struct _bmutex_t
{
    pthread_mutex_t     mutex;
    char		objName[BASE_MAX_OBJ_NAME];
#if BASE_LIB_DEBUG
    int		        nesting_level;
    bthread_t	       *owner;
    char		owner_name[BASE_MAX_OBJ_NAME];
#endif
};

#if defined(BASE_HAS_SEMAPHORE) && BASE_HAS_SEMAPHORE != 0
struct bsem_t
{
    sem_t	       *sem;
    char		objName[BASE_MAX_OBJ_NAME];
};
#endif /* BASE_HAS_SEMAPHORE */

#if defined(BASE_HAS_EVENT_OBJ) && BASE_HAS_EVENT_OBJ != 0
struct bevent_t
{
    enum event_state {
	EV_STATE_OFF,
	EV_STATE_SET,
	EV_STATE_PULSED
    } state;

    bmutex_t		mutex;
    pthread_cond_t	cond;

    bbool_t		auto_reset;
    unsigned		threads_waiting;
    unsigned		threads_to_release;
};
#endif	/* BASE_HAS_EVENT_OBJ */


/*
 * Flag and reference counter for  instance.
 */
static int initialized;

#if BASE_HAS_THREADS
    static bthread_t main_thread;
    static long thread_tls_id;
    static bmutex_t critical_section;
#else
#   define MAX_THREADS 32
    static int tls_flag[MAX_THREADS];
    static void *tls[MAX_THREADS];
#endif


buint32_t bgetpid(void)
{
	BASE_CHECK_STACK();
	return getpid();
}

/*
 * Get thread priority value for the thread.
 */
int bthreadGetPrio(bthread_t *thread)
{
#if BASE_HAS_THREADS
    struct sched_param param;
    int policy;
    int rc;

    rc = pthread_getschedparam (thread->thread, &policy, &param);
    if (rc != 0)
	return -1;

    return param.sched_priority;
#else
    BASE_UNUSED_ARG(thread);
    return 1;
#endif
}


/*
 * Set the thread priority.
 */
bstatus_t bthreadSetPrio(bthread_t *thread,  int prio)
{
#if BASE_HAS_THREADS
    struct sched_param param;
    int policy;
    int rc;

    rc = pthread_getschedparam (thread->thread, &policy, &param);
    if (rc != 0)
	return BASE_RETURN_OS_ERROR(rc);

    param.sched_priority = prio;

    rc = pthread_setschedparam(thread->thread, policy, &param);
    if (rc != 0)
	return BASE_RETURN_OS_ERROR(rc);

    return BASE_SUCCESS;
#else
    BASE_UNUSED_ARG(thread);
    BASE_UNUSED_ARG(prio);
    bassert("bthreadSetPrio() called in non-threading mode!");
    return 1;
#endif
}


/*
 * Get the lowest priority value available on this system.
 */
int bthreadGetPrioMin(bthread_t *thread)
{
    struct sched_param param;
    int policy;
    int rc;

    rc = pthread_getschedparam(thread->thread, &policy, &param);
    if (rc != 0)
	return -1;

#if defined(_POSIX_PRIORITY_SCHEDULING)
    return sched_get_priority_min(policy);
#elif defined __OpenBSD__
    /* Thread prio min/max are declared in OpenBSD private hdr */
    return 0;
#else
    bassert("bthreadGetPrioMin() not supported!");
    return 0;
#endif
}


/*
 * Get the highest priority value available on this system.
 */
int bthreadGetPrioMax(bthread_t *thread)
{
    struct sched_param param;
    int policy;
    int rc;

    rc = pthread_getschedparam(thread->thread, &policy, &param);
    if (rc != 0)
	return -1;

#if defined(_POSIX_PRIORITY_SCHEDULING)
    return sched_get_priority_max(policy);
#elif defined __OpenBSD__
    /* Thread prio min/max are declared in OpenBSD private hdr */
    return 31;
#else
    bassert("bthreadGetPrioMax() not supported!");
    return 0;
#endif
}



/*
 * bthreadRegister(..)
 */
bstatus_t bthreadRegister ( const char *cstr_thread_name, bthread_desc desc, bthread_t **ptr_thread)
{
#if BASE_HAS_THREADS
	char stack_ptr;
	bstatus_t rc;
	
	bthread_t *thread = (bthread_t *)desc;
	bstr_t thread_name = bstr((char*)cstr_thread_name);

	/* Size sanity check. */
	if (sizeof(bthread_desc) < sizeof(bthread_t))
	{
		bassert(!"Not enough bthread_desc size!");
		return BASE_EBUG;
	}

	/* Warn if this thread has been registered before */
	if (bthreadLocalGet (thread_tls_id) != 0)
	{
		// 2006-02-26 bennylp:
		//  This wouldn't work in all cases!.
		//  If thread is created by external module (e.g. sound thread),
		//  thread may be reused while the pool used for the thread descriptor
		//  has been deleted by application.
		//*thread_ptr = (bthread_t*)bthreadLocalGet (thread_tls_id);
		//return BASE_SUCCESS;
		BASE_INFO("Info: possibly re-registering existing thread");
	}

	/* On the other hand, also warn if the thread descriptor buffer seem to
	* have been used to register other threads.
	*/
	bassert(thread->signature1 != SIGNATURE1 ||thread->signature2 != SIGNATURE2 ||(thread->thread == pthread_self()));

	/* Initialize and set the thread entry. */
	bbzero(desc, sizeof(struct bthread_t));
	thread->thread = pthread_self();
	thread->signature1 = SIGNATURE1;
	thread->signature2 = SIGNATURE2;

	if(cstr_thread_name && bstrlen(&thread_name) < sizeof(thread->objName)-1)
		bansi_snprintf(thread->objName, sizeof(thread->objName), cstr_thread_name, thread->thread);
	else
		bansi_snprintf(thread->objName, sizeof(thread->objName), "thr%p", (void*)thread->thread);

	rc = bthreadLocalSet(thread_tls_id, thread);
	if (rc != BASE_SUCCESS)
	{
		bbzero(desc, sizeof(struct bthread_t));
		return rc;
	}

#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK!=0
	thread->stk_start = &stack_ptr;
	thread->stk_size = 0xFFFFFFFFUL;
	thread->stk_max_usage = 0;
#else
	BASE_UNUSED_ARG(stack_ptr);
#endif

	*ptr_thread = thread;
	return BASE_SUCCESS;
#else
	bthread_t *thread = (bthread_t*)desc;
	*ptr_thread = thread;
	return BASE_SUCCESS;
#endif
}


#if BASE_HAS_THREADS
/*
 * This is the main entry for all threads.
 */
static void *_threadMain(void *param)
{
	bthread_t *rec = (bthread_t*)param;
	void *result;
	bstatus_t rc;

#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK!=0
	rec->stk_start = (char*)&rec;
#endif

	/* Set current thread id. */
	rc = bthreadLocalSet(thread_tls_id, rec);
	if (rc != BASE_SUCCESS)
	{
		bassert(!"Thread TLS ID is not set (libBaseInit() error?)");
	}

	/* Check if suspension is required. */
	if (rec->suspended_mutex) {
		bmutex_lock(rec->suspended_mutex);
		bmutex_unlock(rec->suspended_mutex);
	}

	BASE_STR_INFO(rec->objName, "Thread started");

	/* Call user's entry! */
	result = (void*)(long)(*rec->proc)(rec->arg);

	/* Done. */
	BASE_STR_INFO(rec->objName, "Thread quitting");

	return result;
}
#endif

/*
 */
bstatus_t bthreadCreate( bpool_t *pool,
				      const char *thread_name,
				      bthread_proc *proc,
				      void *arg,
				      bsize_t stack_size,
				      unsigned flags,
				      bthread_t **ptr_thread)
{
#if BASE_HAS_THREADS
    bthread_t *rec;
    pthread_attr_t thread_attr;
    void *stack_addr;
    int rc;

    BASE_UNUSED_ARG(stack_addr);

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(pool && proc && ptr_thread, BASE_EINVAL);

    /* Create thread record and assign name for the thread */
    rec = (struct bthread_t*) bpool_zalloc(pool, sizeof(bthread_t));
    BASE_ASSERT_RETURN(rec, BASE_ENOMEM);

    /* Set name. */
    if (!thread_name)
	thread_name = "thr%p";

    if (strchr(thread_name, '%')) {
	bansi_snprintf(rec->objName, BASE_MAX_OBJ_NAME, thread_name, rec);
    } else {
	strncpy(rec->objName, thread_name, BASE_MAX_OBJ_NAME);
	rec->objName[BASE_MAX_OBJ_NAME-1] = '\0';
    }

    /* Set default stack size */
    if (stack_size == 0)
	stack_size = BASE_THREAD_DEFAULT_STACK_SIZE;

#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK!=0
    rec->stk_size = stack_size;
    rec->stk_max_usage = 0;
#endif

    /* Emulate suspended thread with mutex. */
    if (flags & BASE_THREAD_SUSPENDED) {
	rc = bmutex_create_simple(pool, NULL, &rec->suspended_mutex);
	if (rc != BASE_SUCCESS) {
	    return rc;
	}

	bmutex_lock(rec->suspended_mutex);
    } else {
	bassert(rec->suspended_mutex == NULL);
    }


    /* Init thread attributes */
    pthread_attr_init(&thread_attr);

#if defined(BASE_THREAD_SET_STACK_SIZE) && BASE_THREAD_SET_STACK_SIZE!=0
    /* Set thread's stack size */
    rc = pthread_attr_setstacksize(&thread_attr, stack_size);
    if (rc != 0)
	return BASE_RETURN_OS_ERROR(rc);
#endif	/* BASE_THREAD_SET_STACK_SIZE */


#if defined(BASE_THREAD_ALLOCATE_STACK) && BASE_THREAD_ALLOCATE_STACK!=0
    /* Allocate memory for the stack */
    stack_addr = bpool_alloc(pool, stack_size);
    BASE_ASSERT_RETURN(stack_addr, BASE_ENOMEM);

    rc = pthread_attr_setstackaddr(&thread_attr, stack_addr);
    if (rc != 0)
	return BASE_RETURN_OS_ERROR(rc);
#endif	/* BASE_THREAD_ALLOCATE_STACK */


    /* Create the thread. */
    rec->proc = proc;
    rec->arg = arg;
    rc = pthread_create( &rec->thread, &thread_attr, &_threadMain, rec);
    if (rc != 0) {
	return BASE_RETURN_OS_ERROR(rc);
    }

    *ptr_thread = rec;

    BASE_STR_INFO(rec->objName, "Thread created");
    return BASE_SUCCESS;
#else
    bassert(!"Threading is disabled!");
    return BASE_EINVALIDOP;
#endif
}


/*
 * bthreadResume()
 */
bstatus_t bthreadResume(bthread_t *p)
{
    bstatus_t rc;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(p, BASE_EINVAL);

    rc = bmutex_unlock(p->suspended_mutex);

    return rc;
}

/*
 * bthreadJoin()
 */
bstatus_t bthreadJoin(bthread_t *p)
{
#if BASE_HAS_THREADS
    bthread_t *rec = (bthread_t *)p;
    void *ret;
    int result;

    BASE_CHECK_STACK();

    if (p == bthreadThis())
	return BASE_ECANCELLED;

    BASE_STR_INFO(bthreadThis()->objName, "Joining thread %s", p->objName);
    result = pthread_join( rec->thread, &ret);

    if (result == 0)
	return BASE_SUCCESS;
    else {
	/* Calling pthread_join() on a thread that no longer exists and
	 * getting back ESRCH isn't an error (in this context).
	 * Thanks Phil Torre <ptorre@zetron.com>.
	 */
	return result==ESRCH ? BASE_SUCCESS : BASE_RETURN_OS_ERROR(result);
    }
#else
    BASE_CHECK_STACK();
    bassert(!"No multithreading support!");
    return BASE_EINVALIDOP;
#endif
}

/*
 */
bstatus_t bthreadDestroy(bthread_t *p)
{
	BASE_CHECK_STACK();

	/* Destroy mutex used to suspend thread */
	if (p->suspended_mutex)
	{
		bmutex_destroy(p->suspended_mutex);
		p->suspended_mutex = NULL;
	}

	return BASE_SUCCESS;
}

/*
 */
bstatus_t bthreadSleepMs(unsigned msec)
{
/* TODO: should change this to something like BASE_OS_HAS_NANOSLEEP */
#if defined(BASE_RTEMS) && BASE_RTEMS!=0
	enum { NANOSEC_PER_MSEC = 1000000 };
	struct timespec req;

	BASE_CHECK_STACK();
	req.tv_sec = msec / 1000;
	req.tv_nsec = (msec % 1000) * NANOSEC_PER_MSEC;

	if (nanosleep(&req, NULL) == 0)
		return BASE_SUCCESS;

	return BASE_RETURN_OS_ERROR(bget_native_os_error());
#else
	BASE_CHECK_STACK();

	bset_os_error(0);

	usleep(msec * 1000);

	/* MacOS X (reported on 10.5) seems to always set errno to ETIMEDOUT.
	* It does so because usleep() is declared to return int, and we're
	* supposed to check for errno only when usleep() returns non-zero.
	* Unfortunately, usleep() is declared to return void in other platforms
	* so it's not possible to always check for the return value (unless
	* we add a detection routine in autoconf).
	*
	* As a workaround, here we check if ETIMEDOUT is returned and
	* return successfully if it is.
	*/
	if (bget_native_os_error() == ETIMEDOUT)
		return BASE_SUCCESS;

	return bget_os_error();

#endif	/* BASE_RTEMS */
}


///////////////////////////////////////////////////////////////////////////////
/*
 * batomic_create()
 */
bstatus_t batomic_create( bpool_t *pool,
				      batomic_value_t initial,
				      batomic_t **ptr_atomic)
{
    bstatus_t rc;
    batomic_t *atomic_var;

    atomic_var = BASE_POOL_ZALLOC_T(pool, batomic_t);

    BASE_ASSERT_RETURN(atomic_var, BASE_ENOMEM);

#if BASE_HAS_THREADS
    rc = bmutex_create(pool, "atm%p", BASE_MUTEX_SIMPLE, &atomic_var->mutex);
    if (rc != BASE_SUCCESS)
	return rc;
#endif
    atomic_var->value = initial;

    *ptr_atomic = atomic_var;
    return BASE_SUCCESS;
}

/*
 * batomic_destroy()
 */
bstatus_t batomic_destroy( batomic_t *atomic_var )
{
    bstatus_t status;

    BASE_ASSERT_RETURN(atomic_var, BASE_EINVAL);
    
#if BASE_HAS_THREADS
    status = bmutex_destroy( atomic_var->mutex );
    if (status == BASE_SUCCESS) {
        atomic_var->mutex = NULL;
    }
    return status;
#else
    return 0;
#endif
}

/*
 * batomic_set()
 */
void batomic_set(batomic_t *atomic_var, batomic_value_t value)
{
    bstatus_t status;

    BASE_CHECK_STACK();
    BASE_ASSERT_ON_FAIL(atomic_var, return);

#if BASE_HAS_THREADS
    status = bmutex_lock( atomic_var->mutex );
    if (status != BASE_SUCCESS) {
        return;
    }
#endif
    atomic_var->value = value;
#if BASE_HAS_THREADS
    bmutex_unlock( atomic_var->mutex);
#endif
}

/*
 * batomic_get()
 */
batomic_value_t batomic_get(batomic_t *atomic_var)
{
    batomic_value_t oldval;

    BASE_CHECK_STACK();

#if BASE_HAS_THREADS
    bmutex_lock( atomic_var->mutex );
#endif
    oldval = atomic_var->value;
#if BASE_HAS_THREADS
    bmutex_unlock( atomic_var->mutex);
#endif
    return oldval;
}

/*
 * batomic_inc_and_get()
 */
batomic_value_t batomic_inc_and_get(batomic_t *atomic_var)
{
    batomic_value_t new_value;

    BASE_CHECK_STACK();

#if BASE_HAS_THREADS
    bmutex_lock( atomic_var->mutex );
#endif
    new_value = ++atomic_var->value;
#if BASE_HAS_THREADS
    bmutex_unlock( atomic_var->mutex);
#endif

    return new_value;
}
/*
 * batomic_inc()
 */
void batomic_inc(batomic_t *atomic_var)
{
    BASE_ASSERT_ON_FAIL(atomic_var, return);
    batomic_inc_and_get(atomic_var);
}

/*
 * batomic_dec_and_get()
 */
batomic_value_t batomic_dec_and_get(batomic_t *atomic_var)
{
    batomic_value_t new_value;

    BASE_CHECK_STACK();

#if BASE_HAS_THREADS
    bmutex_lock( atomic_var->mutex );
#endif
    new_value = --atomic_var->value;
#if BASE_HAS_THREADS
    bmutex_unlock( atomic_var->mutex);
#endif

    return new_value;
}

/*
 * batomic_dec()
 */
void batomic_dec(batomic_t *atomic_var)
{
    BASE_ASSERT_ON_FAIL(atomic_var, return);
    batomic_dec_and_get(atomic_var);
}

/*
 * batomic_add_and_get()
 */
batomic_value_t batomic_add_and_get( batomic_t *atomic_var,
                                                 batomic_value_t value )
{
    batomic_value_t new_value;

#if BASE_HAS_THREADS
    bmutex_lock(atomic_var->mutex);
#endif

    atomic_var->value += value;
    new_value = atomic_var->value;

#if BASE_HAS_THREADS
    bmutex_unlock(atomic_var->mutex);
#endif

    return new_value;
}

/*
 * batomic_add()
 */
void batomic_add( batomic_t *atomic_var,
                            batomic_value_t value )
{
    BASE_ASSERT_ON_FAIL(atomic_var, return);
    batomic_add_and_get(atomic_var, value);
}

/*
 * bthreadLocalAlloc()
 */
bstatus_t bthreadLocalAlloc(long *p_index)
{
#if BASE_HAS_THREADS
	pthread_key_t key;
	int rc;

	BASE_ASSERT_RETURN(p_index != NULL, BASE_EINVAL);

	bassert( sizeof(pthread_key_t) <= sizeof(long));
	if ((rc=pthread_key_create(&key, NULL)) != 0)
		return BASE_RETURN_OS_ERROR(rc);

	*p_index = key;
	return BASE_SUCCESS;
#else
	int i;

	for (i=0; i<MAX_THREADS; ++i)
	{
		if (tls_flag[i] == 0)
			break;
	}
	
	if (i == MAX_THREADS)
		return BASE_ETOOMANY;

	tls_flag[i] = 1;
	tls[i] = NULL;

	*p_index = i;
	return BASE_SUCCESS;
#endif
}

/*
 * bthreadLocalFree()
 */
void bthreadLocalFree(long index)
{
	BASE_CHECK_STACK();
#if BASE_HAS_THREADS
	pthread_key_delete(index);
#else
	tls_flag[index] = 0;
#endif
}

/*
 * Associate a value with the key
 */
bstatus_t bthreadLocalSet(long index, void *value)
{
	//Can't check stack because this function is called in the
	//beginning before main thread is initialized.
	//BASE_CHECK_STACK();
#if BASE_HAS_THREADS
	int rc=pthread_setspecific(index, value);
		return rc==0 ? BASE_SUCCESS : BASE_RETURN_OS_ERROR(rc);
#else
	bassert(index >= 0 && index < MAX_THREADS);
	tls[index] = value;
	return BASE_SUCCESS;
#endif
}

void* bthreadLocalGet(long index)
{
    //Can't check stack because this function is called
    //by BASE_CHECK_STACK() itself!!!
    //BASE_CHECK_STACK();
#if BASE_HAS_THREADS
    return pthread_getspecific(index);
#else
    bassert(index >= 0 && index < MAX_THREADS);
    return tls[index];
#endif
}

void benter_critical_section(void)
{
#if BASE_HAS_THREADS
    bmutex_lock(&critical_section);
#endif
}

void bleave_critical_section(void)
{
#if BASE_HAS_THREADS
    bmutex_unlock(&critical_section);
#endif
}


///////////////////////////////////////////////////////////////////////////////
#if defined(BASE_LINUX) && BASE_LINUX!=0
BASE_BEGIN_DECL
int pthread_mutexattr_settype(pthread_mutexattr_t*,int);
BASE_END_DECL
#endif

static bstatus_t _initMutex(bmutex_t *mutex, const char *name, int type)
{
#if BASE_HAS_THREADS
    pthread_mutexattr_t attr;
    int rc;

    BASE_CHECK_STACK();

    rc = pthread_mutexattr_init(&attr);
    if (rc != 0)
	return BASE_RETURN_OS_ERROR(rc);

    if (type == BASE_MUTEX_SIMPLE) {
#if (defined(BASE_LINUX) && BASE_LINUX!=0) || \
    defined(BASE_HAS_PTHREAD_MUTEXATTR_SETTYPE)
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
#elif (defined(BASE_RTEMS) && BASE_RTEMS!=0) || \
       defined(BASE_PTHREAD_MUTEXATTR_T_HAS_RECURSIVE)
	/* Nothing to do, default is simple */
#else
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
#endif
    } else {
#if (defined(BASE_LINUX) && BASE_LINUX!=0) || \
     defined(BASE_HAS_PTHREAD_MUTEXATTR_SETTYPE)
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#elif (defined(BASE_RTEMS) && BASE_RTEMS!=0) || \
       defined(BASE_PTHREAD_MUTEXATTR_T_HAS_RECURSIVE)
	// Phil Torre <ptorre@zetron.com>:
	// The RTEMS implementation of POSIX mutexes doesn't include
	// pthread_mutexattr_settype(), so what follows is a hack
	// until I get RTEMS patched to support the set/get functions.
	//
	// More info:
	//   newlib's pthread also lacks pthread_mutexattr_settype(),
	//   but it seems to have mutexattr.recursive.
	BASE_TODO(FIX_RTEMS_RECURSIVE_MUTEX_TYPE)
	attr.recursive = 1;
#else
	rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#endif
    }

    if (rc != 0) {
	return BASE_RETURN_OS_ERROR(rc);
    }

    rc = pthread_mutex_init(&mutex->mutex, &attr);
    if (rc != 0) {
	return BASE_RETURN_OS_ERROR(rc);
    }

    rc = pthread_mutexattr_destroy(&attr);
    if (rc != 0) {
	bstatus_t status = BASE_RETURN_OS_ERROR(rc);
	pthread_mutex_destroy(&mutex->mutex);
	return status;
    }

#if BASE_LIB_DEBUG
    /* Set owner. */
    mutex->nesting_level = 0;
    mutex->owner = NULL;
    mutex->owner_name[0] = '\0';
#endif

    /* Set name. */
    if (!name) {
	name = "mtx%p";
    }
    if (strchr(name, '%')) {
	bansi_snprintf(mutex->objName, BASE_MAX_OBJ_NAME, name, mutex);
    } else {
	strncpy(mutex->objName, name, BASE_MAX_OBJ_NAME);
	mutex->objName[BASE_MAX_OBJ_NAME-1] = '\0';
    }

    BASE_STR_INFO(mutex->objName, "Mutex created");
    return BASE_SUCCESS;
#else /* BASE_HAS_THREADS */
    return BASE_SUCCESS;
#endif
}


/*
 * bmutex_lock()
 */
bstatus_t bmutex_lock(bmutex_t *mutex)
{
#if BASE_HAS_THREADS
    bstatus_t status;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(mutex, BASE_EINVAL);

#if BASE_LIB_DEBUG
    BASE_STR_INFO(mutex->objName, "Mutex: thread %s is waiting (mutex owner=%s)", bthreadThis()->objName, mutex->owner_name);
#else
    BASE_STR_INFO(mutex->objName, "Mutex: thread %s is waiting", bthreadThis()->objName );
#endif

    status = pthread_mutex_lock( &mutex->mutex );


#if BASE_LIB_DEBUG
    if (status == BASE_SUCCESS) {
	mutex->owner = bthreadThis();
	bansi_strcpy(mutex->owner_name, mutex->owner->objName);
	++mutex->nesting_level;
    }

    BASE_STR_INFO(mutex->objName, (status==0) ?"Mutex acquired by thread %s (level=%d)" :"Mutex acquisition FAILED by %s (level=%d)"), 
		bthreadThis()->objName, mutex->nesting_level);
#else
    BASE_STR_INFO(mutex->objName, (status==0) ? "Mutex acquired by thread %s" : "FAILED by %s", bthreadThis()->objName);
#endif

    if (status == 0)
	return BASE_SUCCESS;
    else
	return BASE_RETURN_OS_ERROR(status);
#else	/* BASE_HAS_THREADS */
    bassert( mutex == (bmutex_t*)1 );
    return BASE_SUCCESS;
#endif
}

/*
 * bmutex_unlock()
 */
bstatus_t bmutex_unlock(bmutex_t *mutex)
{
#if BASE_HAS_THREADS
    bstatus_t status;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(mutex, BASE_EINVAL);

#if BASE_LIB_DEBUG
    bassert(mutex->owner == bthreadThis());
    if (--mutex->nesting_level == 0) {
	mutex->owner = NULL;
	mutex->owner_name[0] = '\0';
    }

    BASE_STR_INFO(mutex->objName, "Mutex released by thread %s (level=%d)",
				bthreadThis()->objName,
				mutex->nesting_level);
#else
    BASE_STR_INFO(mutex->objName, "Mutex released by thread %s", bthreadThis()->objName);
#endif

    status = pthread_mutex_unlock( &mutex->mutex );
    if (status == 0)
	return BASE_SUCCESS;
    else
	return BASE_RETURN_OS_ERROR(status);

#else /* BASE_HAS_THREADS */
    bassert( mutex == (bmutex_t*)1 );
    return BASE_SUCCESS;
#endif
}

/*
 * bmutex_trylock()
 */
bstatus_t bmutex_trylock(bmutex_t *mutex)
{
#if BASE_HAS_THREADS
    int status;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(mutex, BASE_EINVAL);

    BASE_STR_INFO(mutex->objName, "Mutex: thread %s is trying", 	bthreadThis()->objName);
    status = pthread_mutex_trylock( &mutex->mutex );

    if (status==0) {
#if BASE_LIB_DEBUG
	mutex->owner = bthreadThis();
	bansi_strcpy(mutex->owner_name, mutex->owner->objName);
	++mutex->nesting_level;

	BASE_STR_INFO(mutex->objName, "Mutex acquired by thread %s (level=%d)",
				   bthreadThis()->objName,
				   mutex->nesting_level);
#else
	BASE_STR_INFO(mutex->objName, "Mutex acquired by thread %s", bthreadThis()->objName);
#endif
    }
	else {
	BASE_STR_INFO(mutex->objName, "Mutex: thread %s's trylock() failed", bthreadThis()->objName);
    }

    if (status==0)
	return BASE_SUCCESS;
    else
	return BASE_RETURN_OS_ERROR(status);
#else	/* BASE_HAS_THREADS */
    bassert( mutex == (bmutex_t*)1);
    return BASE_SUCCESS;
#endif
}

/*
 * bmutex_destroy()
 */
bstatus_t bmutex_destroy(bmutex_t *mutex)
{
    enum { RETRY = 4 };
    int status = 0;
    unsigned retry;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(mutex, BASE_EINVAL);

#if BASE_HAS_THREADS
    BASE_STR_INFO(mutex->objName, "Mutex destroyed by thread %s", bthreadThis()->objName);

    for (retry=0; retry<RETRY; ++retry) {
	status = pthread_mutex_destroy( &mutex->mutex );
	if (status == BASE_SUCCESS)
	    break;
	else if (retry<RETRY-1 && status == EBUSY)
	    pthread_mutex_unlock(&mutex->mutex);
    }

    if (status == 0)
	return BASE_SUCCESS;
    else {
	return BASE_RETURN_OS_ERROR(status);
    }
#else
    bassert( mutex == (bmutex_t*)1 );
    status = BASE_SUCCESS;
    return status;
#endif
}

#if BASE_LIB_DEBUG
bbool_t bmutex_is_locked(bmutex_t *mutex)
{
#if BASE_HAS_THREADS
    return mutex->owner == bthreadThis();
#else
    return 1;
#endif
}
#endif

///////////////////////////////////////////////////////////////////////////////
/*
 * Include Read/Write mutex emulation for POSIX platforms that lack it (e.g.
 * RTEMS). Otherwise use POSIX rwlock.
 */
#if defined(BASE_EMULATE_RWMUTEX) && BASE_EMULATE_RWMUTEX!=0
    /* We need semaphore functionality to emulate rwmutex */
#   if !defined(BASE_HAS_SEMAPHORE) || BASE_HAS_SEMAPHORE==0
#	error "Semaphore support needs to be enabled to emulate rwmutex"
#   endif
#   include "_baseOsRwmutex.c"
#else
struct brwmutex_t
{
    pthread_rwlock_t rwlock;
};

bstatus_t brwmutex_create(bpool_t *pool, const char *name,
				      brwmutex_t **p_mutex)
{
    brwmutex_t *rwm;
    bstatus_t status;

    BASE_UNUSED_ARG(name);

    rwm = BASE_POOL_ALLOC_T(pool, brwmutex_t);
    BASE_ASSERT_RETURN(rwm, BASE_ENOMEM);

    status = pthread_rwlock_init(&rwm->rwlock, NULL);
    if (status != 0)
	return BASE_RETURN_OS_ERROR(status);

    *p_mutex = rwm;
    return BASE_SUCCESS;
}

/*
 * Lock the mutex for reading.
 *
 */
bstatus_t brwmutex_lock_read(brwmutex_t *mutex)
{
    bstatus_t status;

    status = pthread_rwlock_rdlock(&mutex->rwlock);
    if (status != 0)
	return BASE_RETURN_OS_ERROR(status);

    return BASE_SUCCESS;
}

/*
 * Lock the mutex for writing.
 *
 */
bstatus_t brwmutex_lock_write(brwmutex_t *mutex)
{
    bstatus_t status;

    status = pthread_rwlock_wrlock(&mutex->rwlock);
    if (status != 0)
	return BASE_RETURN_OS_ERROR(status);

    return BASE_SUCCESS;
}

/*
 * Release read lock.
 *
 */
bstatus_t brwmutex_unlock_read(brwmutex_t *mutex)
{
    return brwmutex_unlock_write(mutex);
}

/*
 * Release write lock.
 *
 */
bstatus_t brwmutex_unlock_write(brwmutex_t *mutex)
{
    bstatus_t status;

    status = pthread_rwlock_unlock(&mutex->rwlock);
    if (status != 0)
	return BASE_RETURN_OS_ERROR(status);

    return BASE_SUCCESS;
}

/*
 * Destroy reader/writer mutex.
 *
 */
bstatus_t brwmutex_destroy(brwmutex_t *mutex)
{
    bstatus_t status;

    status = pthread_rwlock_destroy(&mutex->rwlock);
    if (status != 0)
	return BASE_RETURN_OS_ERROR(status);

    return BASE_SUCCESS;
}

#endif	/* BASE_EMULATE_RWMUTEX */


///////////////////////////////////////////////////////////////////////////////
#if defined(BASE_HAS_SEMAPHORE) && BASE_HAS_SEMAPHORE != 0

/*
 * bsem_create()
 */
bstatus_t bsem_create( bpool_t *pool,
				   const char *name,
				   unsigned initial,
				   unsigned max,
				   bsem_t **ptr_sem)
{
#if BASE_HAS_THREADS
    bsem_t *sem;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(pool != NULL && ptr_sem != NULL, BASE_EINVAL);

    sem = BASE_POOL_ALLOC_T(pool, bsem_t);
    BASE_ASSERT_RETURN(sem, BASE_ENOMEM);

#if defined(BASE_DARWINOS) && BASE_DARWINOS!=0
    /* MacOS X doesn't support anonymous semaphore */
    {
	char sem_name[BASE_GUID_MAX_LENGTH+1];
	bstr_t nam;

	/* We should use SEM_NAME_LEN, but this doesn't seem to be
	 * declared anywhere? The value here is just from trial and error
	 * to get the longest name supported.
	 */
#	define MAX_SEM_NAME_LEN	23

	/* Create a unique name for the semaphore. */
	if (BASE_GUID_STRING_LENGTH <= MAX_SEM_NAME_LEN) {
	    nam.ptr = sem_name;
	    bgenerate_unique_string(&nam);
	    sem_name[nam.slen] = '\0';
	} else {
	    bcreate_random_string(sem_name, MAX_SEM_NAME_LEN);
	    sem_name[MAX_SEM_NAME_LEN] = '\0';
	}

	/* Create semaphore */
	sem->sem = sem_open(sem_name, O_CREAT|O_EXCL, S_IRUSR|S_IWUSR,
			    initial);
	if (sem->sem == SEM_FAILED)
	    return BASE_RETURN_OS_ERROR(bget_native_os_error());

	/* And immediately release the name as we don't need it */
	sem_unlink(sem_name);
    }
#else
    sem->sem = BASE_POOL_ALLOC_T(pool, sem_t);
    if (sem_init( sem->sem, 0, initial) != 0)
	return BASE_RETURN_OS_ERROR(bget_native_os_error());
#endif

    /* Set name. */
    if (!name) {
	name = "sem%p";
    }
    if (strchr(name, '%')) {
	bansi_snprintf(sem->objName, BASE_MAX_OBJ_NAME, name, sem);
    } else {
	strncpy(sem->objName, name, BASE_MAX_OBJ_NAME);
	sem->objName[BASE_MAX_OBJ_NAME-1] = '\0';
    }

    BASE_STR_INFO(sem->objName, "Semaphore created" );

    *ptr_sem = sem;
    return BASE_SUCCESS;
#else
    *ptr_sem = (bsem_t*)1;
    return BASE_SUCCESS;
#endif
}

/*
 * bsem_wait()
 */
bstatus_t bsem_wait(bsem_t *sem)
{
#if BASE_HAS_THREADS
    int result;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sem, BASE_EINVAL);

    BASE_STR_INFO(sem->objName, "Semaphore: thread %s is waiting", bthreadThis()->objName);

    result = sem_wait( sem->sem );

    if (result == 0) {
	BASE_STR_INFO(sem->objName, "Semaphore acquired by thread %s",  bthreadThis()->objName);
    } else {
	BASE_STR_INFO(sem->objName, "Semaphore: thread %s FAILED to acquire", bthreadThis()->objName);
    }

    if (result == 0)
	return BASE_SUCCESS;
    else
	return BASE_RETURN_OS_ERROR(bget_native_os_error());
#else
    bassert( sem == (bsem_t*) 1 );
    return BASE_SUCCESS;
#endif
}

/*
 * bsem_trywait()
 */
bstatus_t bsem_trywait(bsem_t *sem)
{
#if BASE_HAS_THREADS
    int result;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sem, BASE_EINVAL);

    result = sem_trywait( sem->sem );

    if (result == 0) {
	BASE_STR_INFO(sem->objName, "Semaphore acquired by thread %s", bthreadThis()->objName);
    }
    if (result == 0)
	return BASE_SUCCESS;
    else
	return BASE_RETURN_OS_ERROR(bget_native_os_error());
#else
    bassert( sem == (bsem_t*)1 );
    return BASE_SUCCESS;
#endif
}

/*
 * bsem_post()
 */
bstatus_t bsem_post(bsem_t *sem)
{
#if BASE_HAS_THREADS
    int result;
    BASE_STR_INFO(sem->objName, "Semaphore released by thread %s", bthreadThis()->objName);
    result = sem_post( sem->sem );

    if (result == 0)
	return BASE_SUCCESS;
    else
	return BASE_RETURN_OS_ERROR(bget_native_os_error());
#else
    bassert( sem == (bsem_t*) 1);
    return BASE_SUCCESS;
#endif
}

/*
 * bsem_destroy()
 */
bstatus_t bsem_destroy(bsem_t *sem)
{
#if BASE_HAS_THREADS
    int result;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sem, BASE_EINVAL);

    BASE_STR_INFO(sem->objName, "Semaphore destroyed by thread %s", bthreadThis()->objName);
#if defined(BASE_DARWINOS) && BASE_DARWINOS!=0
    result = sem_close( sem->sem );
#else
    result = sem_destroy( sem->sem );
#endif

    if (result == 0)
	return BASE_SUCCESS;
    else
	return BASE_RETURN_OS_ERROR(bget_native_os_error());
#else
    bassert( sem == (bsem_t*) 1 );
    return BASE_SUCCESS;
#endif
}

#endif	/* BASE_HAS_SEMAPHORE */

#if defined(BASE_HAS_EVENT_OBJ) && BASE_HAS_EVENT_OBJ != 0

/*
 * bevent_create()
 */
bstatus_t bevent_create(bpool_t *pool, const char *name,
				    bbool_t manual_reset, bbool_t initial,
				    bevent_t **ptr_event)
{
    bevent_t *event;

    event = BASE_POOL_ALLOC_T(pool, bevent_t);

    _initMutex(&event->mutex, name, BASE_MUTEX_SIMPLE);
    pthread_cond_init(&event->cond, 0);
    event->auto_reset = !manual_reset;
    event->threads_waiting = 0;

    if (initial) {
	event->state = EV_STATE_SET;
	event->threads_to_release = 1;
    } else {
	event->state = EV_STATE_OFF;
	event->threads_to_release = 0;
    }

    *ptr_event = event;
    return BASE_SUCCESS;
}

static void event_on_one_release(bevent_t *event)
{
    if (event->state == EV_STATE_SET) {
	if (event->auto_reset) {
	    event->threads_to_release = 0;
	    event->state = EV_STATE_OFF;
	} else {
	    /* Manual reset remains on */
	}
    } else {
	if (event->auto_reset) {
	    /* Only release one */
	    event->threads_to_release = 0;
	    event->state = EV_STATE_OFF;
	} else {
	    event->threads_to_release--;
	    bassert(event->threads_to_release >= 0);
	    if (event->threads_to_release==0)
		event->state = EV_STATE_OFF;
	}
    }
}

/*
 * bevent_wait()
 */
bstatus_t bevent_wait(bevent_t *event)
{
    pthread_mutex_lock(&event->mutex.mutex);
    event->threads_waiting++;
    while (event->state == EV_STATE_OFF)
	pthread_cond_wait(&event->cond, &event->mutex.mutex);
    event->threads_waiting--;
    event_on_one_release(event);
    pthread_mutex_unlock(&event->mutex.mutex);
    return BASE_SUCCESS;
}

/*
 * bevent_trywait()
 */
bstatus_t bevent_trywait(bevent_t *event)
{
    bstatus_t status;

    pthread_mutex_lock(&event->mutex.mutex);
    status = event->state != EV_STATE_OFF ? BASE_SUCCESS : -1;
    if (status==BASE_SUCCESS) {
	event_on_one_release(event);
    }
    pthread_mutex_unlock(&event->mutex.mutex);

    return status;
}

/*
 * bevent_set()
 */
bstatus_t bevent_set(bevent_t *event)
{
    pthread_mutex_lock(&event->mutex.mutex);
    event->threads_to_release = 1;
    event->state = EV_STATE_SET;
    if (event->auto_reset)
	pthread_cond_signal(&event->cond);
    else
	pthread_cond_broadcast(&event->cond);
    pthread_mutex_unlock(&event->mutex.mutex);
    return BASE_SUCCESS;
}

/*
 * bevent_pulse()
 */
bstatus_t bevent_pulse(bevent_t *event)
{
    pthread_mutex_lock(&event->mutex.mutex);
    if (event->threads_waiting) {
	event->threads_to_release = event->auto_reset ? 1 :
					event->threads_waiting;
	event->state = EV_STATE_PULSED;
	if (event->threads_to_release==1)
	    pthread_cond_signal(&event->cond);
	else
	    pthread_cond_broadcast(&event->cond);
    }
    pthread_mutex_unlock(&event->mutex.mutex);
    return BASE_SUCCESS;
}

/*
 * bevent_reset()
 */
bstatus_t bevent_reset(bevent_t *event)
{
    pthread_mutex_lock(&event->mutex.mutex);
    event->state = EV_STATE_OFF;
    event->threads_to_release = 0;
    pthread_mutex_unlock(&event->mutex.mutex);
    return BASE_SUCCESS;
}

/*
 * bevent_destroy()
 */
bstatus_t bevent_destroy(bevent_t *event)
{
    bmutex_destroy(&event->mutex);
    pthread_cond_destroy(&event->cond);
    return BASE_SUCCESS;
}

#endif	/* BASE_HAS_EVENT_OBJ */

#if defined(BASE_TERM_HAS_COLOR) && BASE_TERM_HAS_COLOR != 0
/*
 * Terminal
 */

/**
 * Set terminal color.
 */
bstatus_t bterm_set_color(bcolor_t color)
{
    /* put bright prefix to ansi_color */
    char ansi_color[12] = "\033[01;3";

    if (color & BASE_TERM_COLOR_BRIGHT) {
	color ^= BASE_TERM_COLOR_BRIGHT;
    } else {
	strcpy(ansi_color, "\033[00;3");
    }

    switch (color) {
    case 0:
	/* black color */
	strcat(ansi_color, "0m");
	break;
    case BASE_TERM_COLOR_R:
	/* red color */
	strcat(ansi_color, "1m");
	break;
    case BASE_TERM_COLOR_G:
	/* green color */
	strcat(ansi_color, "2m");
	break;
    case BASE_TERM_COLOR_B:
	/* blue color */
	strcat(ansi_color, "4m");
	break;
    case BASE_TERM_COLOR_R | BASE_TERM_COLOR_G:
	/* yellow color */
	strcat(ansi_color, "3m");
	break;
    case BASE_TERM_COLOR_R | BASE_TERM_COLOR_B:
	/* magenta color */
	strcat(ansi_color, "5m");
	break;
    case BASE_TERM_COLOR_G | BASE_TERM_COLOR_B:
	/* cyan color */
	strcat(ansi_color, "6m");
	break;
    case BASE_TERM_COLOR_R | BASE_TERM_COLOR_G | BASE_TERM_COLOR_B:
	/* white color */
	strcat(ansi_color, "7m");
	break;
    default:
	/* default console color */
	strcpy(ansi_color, "\033[00m");
	break;
    }

    fputs(ansi_color, stdout);

    return BASE_SUCCESS;
}

/**
 * Get current terminal foreground color.
 */
bcolor_t bterm_get_color(void)
{
    return 0;
}
#endif	/* BASE_TERM_HAS_COLOR */


#include "_baseOsCommon.c"

bstatus_t libBaseInit(void)
{
	char dummy_guid[BASE_GUID_MAX_LENGTH];
	bstr_t guid;
	bstatus_t rc;

	/* Check if  have been initialized */
	if (initialized)
	{
		++initialized;
		return BASE_SUCCESS;
	}

#if BASE_HAS_THREADS
	/* Init this thread's TLS. */
	if ((rc=_bthreadInit()) != 0)
	{
		return rc;
	}

	/* Critical section. */
	if ((rc=_initMutex(&critical_section, "Critsec", BASE_MUTEX_RECURSE)) != 0)
		return rc;

#endif

	/* Init logging */
	extLogInit();
	BASE_DEBUG("libBase %s is initializing...", BASE_VERSION);

	/* Initialize exception ID for the pool.
	* Must do so after critical section is configured.
	*/
	rc = bexception_id_alloc("/No memory", &BASE_NO_MEMORY_EXCEPTION);
	if (rc != BASE_SUCCESS)
		return rc;

	/* Init random seed. */
	/* Or probably not. Let application in charge of this */
	/* bsrand( clock() ); */

	/* Startup GUID. */
	guid.ptr = dummy_guid;
	bgenerate_unique_string( &guid );

	/* Startup timestamp */
#if defined(BASE_HAS_HIGH_RES_TIMER) && BASE_HAS_HIGH_RES_TIMER != 0
	{
		btimestamp dummy_ts;
		if ((rc=bTimeStampGet(&dummy_ts)) != 0)
		{
			return rc;
		}
	}
#endif

	/* Flag  as initialized */
	++initialized;
	bassert(initialized == 1);

	BASE_INFO("libBase %s for POSIX initialized", BASE_VERSION);

	return BASE_SUCCESS;
}


/*
 */
void libBaseShutdown()
{
	int i;

	/* Only perform shutdown operation when 'initialized' reaches zero */
	bassert(initialized > 0);
	if (--initialized != 0)
		return;

	BASE_DEBUG("libBase %s is destorying", BASE_VERSION);

	/* Call atexit() functions */
	_exitHandleQuit();

	/* Free exception ID */
	if (BASE_NO_MEMORY_EXCEPTION != -1)
	{
		bexception_id_free(BASE_NO_MEMORY_EXCEPTION);
		BASE_NO_MEMORY_EXCEPTION = -1;
	}

#if BASE_HAS_THREADS
	/* Destroy  critical section */
	bmutex_destroy(&critical_section);

	/* Free  TLS */
	if (thread_tls_id != -1)
	{
		bthreadLocalFree(thread_tls_id);
		thread_tls_id = -1;
	}

	/* Ticket #1132: Assertion when (re)starting  on different thread */
	bbzero(&main_thread, sizeof(main_thread));
#endif

	/* Clear static variables */
	berrno_clear_handlers();
	BASE_DEBUG("libBase %s is destoried", BASE_VERSION);
}


