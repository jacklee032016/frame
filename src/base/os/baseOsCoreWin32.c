/* 
 *
 */
#include <baseOs.h>
#include <basePool.h>
#include <baseLog.h>
#include <baseString.h>
#include <baseGuid.h>
#include <baseRand.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseExcept.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(BASE_HAS_WINSOCK2_H) && BASE_HAS_WINSOCK2_H != 0
#  include <winsock2.h>
#endif

#if defined(BASE_HAS_WINSOCK_H) && BASE_HAS_WINSOCK_H != 0
#  include <winsock.h>
#endif

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
#   include "../../../third_party/threademulation/include/ThreadEmulation.h"
#endif

/* Activate mutex related logging if BASE_LIB_DEBUG_MUTEX is set, otherwise
 * use default level 6 logging.
 */
#if defined(BASE_DEBUG_MUTEX) && BASE_DEBUG_MUTEX
#   undef BASE_LIB_DEBUG
#   define BASE_LIB_DEBUG	    1
#   define LOG_MUTEX(expr)  BASE_LOG(5,expr)
#else
#   define LOG_MUTEX(expr)  BASE_LOG(6,expr)
#endif


/*
 * Implementation of bthread_t.
 */
struct bthread_t
{
	char					objName[BASE_MAX_OBJ_NAME];
	HANDLE				hthread;
	DWORD				idthread;
	bthread_proc		*proc;
	void					*arg;

#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK!=0
	buint32_t	    stk_size;
	buint32_t	    stk_max_usage;
	char	   *stk_start;
	const char	   *caller_file;
	int		    caller_line;
#endif
};


/*
 * Implementation of bmutex_t.
 */
struct _bmutex_t
{
#if BASE_WIN32_WINNT >= 0x0400
    CRITICAL_SECTION	crit;
#else
    HANDLE		hMutex;
#endif
    char		objName[BASE_MAX_OBJ_NAME];
#if BASE_LIB_DEBUG
    int		        nesting_level;
    bthread_t	       *owner;
#endif
};

/*
 * Implementation of bsem_t.
 */
typedef struct bsem_t
{
    HANDLE		hSemaphore;
    char		objName[BASE_MAX_OBJ_NAME];
} bmem_t;


/*
 * Implementation of bevent_t.
 */
struct bevent_t
{
    HANDLE		hEvent;
    char		objName[BASE_MAX_OBJ_NAME];
};

/*
 * Implementation of batomic_t.
 */
struct _batomic_t
{
    long value;
};

/*
 * Flag and reference counter for  instance.
 */
static int initialized;

/*
 * Static global variables.
 */
static bthread_desc main_thread;
static long thread_tls_id = -1;
static bmutex_t _criticalSectionMutex;
static unsigned atexit_count;
static void (*atexit_func[32])(void);

buint32_t bgetpid(void)
{
	BASE_CHECK_STACK();
	return GetCurrentProcessId();
}


/*
 * Get thread priority value for the thread.
 */
int bthreadGetPrio(bthread_t *thread)
{
#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	BASE_UNUSED_ARG(thread);
	return -1;
#else
	return GetThreadPriority(thread->hthread);
#endif
}


/*
 * Set the thread priority.
 */
bstatus_t bthreadSetPrio(bthread_t *thread,  int prio)
{
#if BASE_HAS_THREADS
	BASE_ASSERT_RETURN(thread, BASE_EINVAL);
	BASE_ASSERT_RETURN(prio>=THREAD_PRIORITY_IDLE && 
		prio<=THREAD_PRIORITY_TIME_CRITICAL,
	     BASE_EINVAL);

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	if (SetThreadPriorityRT(thread->hthread, prio) == FALSE)
#else
	if (SetThreadPriority(thread->hthread, prio) == FALSE)
#endif
		return BASE_RETURN_OS_ERROR(GetLastError());

	return BASE_SUCCESS;

#else
	BASE_UNUSED_ARG(thread);
	BASE_UNUSED_ARG(prio);
	bassert("bthreadSetPrio() called in non-threading mode!");
	return BASE_EINVALIDOP;
#endif
}


/*
 * Get the lowest priority value available on this system.
 */
int bthreadGetPrioMin(bthread_t *thread)
{
	BASE_UNUSED_ARG(thread);
	return THREAD_PRIORITY_IDLE;
}


/*
 * Get the highest priority value available on this system.
 */
int bthreadGetPrioMax(bthread_t *thread)
{
	BASE_UNUSED_ARG(thread);
	return THREAD_PRIORITY_TIME_CRITICAL;
}



/*
 * bthreadRegister(..)
 */
bstatus_t bthreadRegister ( const char *cstr_thread_name,
					 bthread_desc desc,
                                         bthread_t **thread_ptr)
{
    char stack_ptr;
    bstatus_t rc;
    bthread_t *thread = (bthread_t *)desc;
    bstr_t thread_name = bstr((char*)cstr_thread_name);

    /* Size sanity check. */
    if (sizeof(bthread_desc) < sizeof(bthread_t)) {
	bassert(!"Not enough bthread_desc size!");
	return BASE_EBUG;
    }

    /* If a thread descriptor has been registered before, just return it. */
    if (bthreadLocalGet (thread_tls_id) != 0) {
	// 2006-02-26 bennylp:
	//  This wouldn't work in all cases!.
	//  If thread is created by external module (e.g. sound thread),
	//  thread may be reused while the pool used for the thread descriptor
	//  has been deleted by application.
	//*thread_ptr = (bthread_t*)bthreadLocalGet (thread_tls_id);
        //return BASE_SUCCESS;
    }

    /* Initialize and set the thread entry. */
    bbzero(desc, sizeof(struct bthread_t));
    thread->hthread = GetCurrentThread();
    thread->idthread = GetCurrentThreadId();

#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK!=0
    thread->stk_start = &stack_ptr;
    thread->stk_size = 0xFFFFFFFFUL;
    thread->stk_max_usage = 0;
#else
    stack_ptr = '\0';
#endif

    if (cstr_thread_name && bstrlen(&thread_name) < sizeof(thread->objName)-1)
	bansi_snprintf(thread->objName, sizeof(thread->objName), 
			 cstr_thread_name, thread->idthread);
    else
	bansi_snprintf(thread->objName, sizeof(thread->objName), 
		         "thr%p", (void*)(bssize_t)thread->idthread);
    
    rc = bthreadLocalSet(thread_tls_id, thread);
    if (rc != BASE_SUCCESS)
	return rc;

    *thread_ptr = thread;
    return BASE_SUCCESS;
}


static DWORD WINAPI _threadMain(void *param)
{
	bthread_t *rec = param;
	DWORD result;

#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK!=0
	rec->stk_start = (char*)&rec;
#endif

	if (bthreadLocalSet(thread_tls_id, rec) != BASE_SUCCESS)
	{
		bassert(!"TLS is not set (libBaseInit() error?)");
	}

	BASE_STR_INFO(rec->objName, "Thread started");

	result = (*rec->proc)(rec->arg);

	BASE_STR_INFO(rec->objName, "Thread quitting");
#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK!=0
	BASE_STR_INFO(rec->objName, "Thread stack max usage=%u by %s:%d", rec->stk_max_usage, rec->caller_file, rec->caller_line);
#endif

	BTRACE();
	return (DWORD)result;
}

/*
 * bthreadCreate(...)
 */
bstatus_t bthreadCreate( bpool_t *pool, const char *thread_name, bthread_proc *proc, void *arg, bsize_t stack_size, unsigned flags, bthread_t **thread_ptr)
{
	DWORD dwflags = 0;
	bthread_t *rec;

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	BASE_UNUSED_ARG(stack_size);
#endif

	BASE_CHECK_STACK();
	BASE_ASSERT_RETURN(pool && proc && thread_ptr, BASE_EINVAL);

	/* Set flags */
	if (flags & BASE_THREAD_SUSPENDED)
		dwflags |= CREATE_SUSPENDED;

	/* Create thread record and assign name for the thread */
	rec = (struct bthread_t*) bpool_calloc(pool, 1, sizeof(bthread_t));
	if (!rec)
		return BASE_ENOMEM;

	/* Set name. */
	if (!thread_name)
		thread_name = "thr%p";

	if (strchr(thread_name, '%'))
	{
		bansi_snprintf(rec->objName, BASE_MAX_OBJ_NAME, thread_name, rec);
	}
	else
	{
		bansi_strncpy(rec->objName, thread_name, BASE_MAX_OBJ_NAME);
		rec->objName[BASE_MAX_OBJ_NAME-1] = '\0';
	}

	BASE_STR_INFO(rec->objName, "Thread created");

#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK!=0
	rec->stk_size = stack_size ? (buint32_t)stack_size : 0xFFFFFFFFUL;
	rec->stk_max_usage = 0;
#endif

	/* Create the thread. */
	rec->proc = proc;
	rec->arg = arg;

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	rec->hthread = CreateThreadRT(NULL, 0,  _threadMain, rec, dwflags, NULL);
#else
	rec->hthread = CreateThread(NULL, stack_size, _threadMain, rec, dwflags, &rec->idthread);
#endif

	if (rec->hthread == NULL)
		return BASE_RETURN_OS_ERROR(GetLastError());

	/* Success! */
	*thread_ptr = rec;
	return BASE_SUCCESS;
}

/*
 */
bstatus_t bthreadResume(bthread_t *p)
{
	bthread_t *rec = (bthread_t*)p;

	BASE_CHECK_STACK();
	BASE_ASSERT_RETURN(p, BASE_EINVAL);

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	if (ResumeThreadRT(rec->hthread) == (DWORD)-1)
#else
	if (ResumeThread(rec->hthread) == (DWORD)-1)
#endif    
		return BASE_RETURN_OS_ERROR(GetLastError());
	else
		return BASE_SUCCESS;
}


/*
 */
bstatus_t bthreadJoin(bthread_t *p)
{
	bthread_t *rec = (bthread_t *)p;
	DWORD rc;

	BASE_CHECK_STACK();
	BASE_ASSERT_RETURN(p, BASE_EINVAL);

	if (p == bthreadThis())
		return BASE_ECANCELLED;

	BASE_STR_INFO(bthreadThis()->objName, "Joining thread %s", p->objName);

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	rc = WaitForSingleObjectEx(rec->hthread, INFINITE, FALSE);
#else
	rc = WaitForSingleObject(rec->hthread, INFINITE);
#endif    

BTRACE();
	if (rc==WAIT_OBJECT_0)
		return BASE_SUCCESS;
	else if (rc==WAIT_TIMEOUT)
		return BASE_ETIMEDOUT;
	else
		return BASE_RETURN_OS_ERROR(GetLastError());
}

/*
 */
bstatus_t bthreadDestroy(bthread_t *p)
{
    bthread_t *rec = (bthread_t *)p;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(p, BASE_EINVAL);

    if (CloseHandle(rec->hthread) == TRUE)
        return BASE_SUCCESS;
    else
        return BASE_RETURN_OS_ERROR(GetLastError());
}

/*
 * bthreadSleepMs()
 */
bstatus_t bthreadSleepMs(unsigned msec)
{
    BASE_CHECK_STACK();

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    SleepRT(msec);
#else
    Sleep(msec);
#endif

    return BASE_SUCCESS;
}

/*
 * batomic_create()
 */
bstatus_t batomic_create( bpool_t *pool, 
                                      batomic_value_t initial,
                                      batomic_t **atomic_ptr)
{
    batomic_t *atomic_var = bpool_alloc(pool, sizeof(batomic_t));
    if (!atomic_var)
	return BASE_ENOMEM;

    atomic_var->value = initial;
    *atomic_ptr = atomic_var;

    return BASE_SUCCESS;
}

/*
 * batomic_destroy()
 */
bstatus_t batomic_destroy( batomic_t *var )
{
    BASE_UNUSED_ARG(var);
    BASE_ASSERT_RETURN(var, BASE_EINVAL);

    return 0;
}

/*
 * batomic_set()
 */
void batomic_set( batomic_t *atomic_var, batomic_value_t value)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_ON_FAIL(atomic_var, return);

    InterlockedExchange(&atomic_var->value, value);
}

/*
 * batomic_get()
 */
batomic_value_t batomic_get(batomic_t *atomic_var)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(atomic_var, 0);

    return atomic_var->value;
}

/*
 * batomic_inc_and_get()
 */
batomic_value_t batomic_inc_and_get(batomic_t *atomic_var)
{
    BASE_CHECK_STACK();

#if defined(BASE_WIN32_WINNT) && BASE_WIN32_WINNT >= 0x0400
    return InterlockedIncrement(&atomic_var->value);
#else
    return InterlockedIncrement(&atomic_var->value);
#endif
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
    BASE_CHECK_STACK();

#if defined(BASE_WIN32_WINNT) && BASE_WIN32_WINNT >= 0x0400
    return InterlockedDecrement(&atomic_var->value);
#else
    return InterlockedDecrement(&atomic_var->value);
#endif
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
 * batomic_add()
 */
void batomic_add( batomic_t *atomic_var,
			    batomic_value_t value )
{
    BASE_ASSERT_ON_FAIL(atomic_var, return);
#if defined(BASE_WIN32_WINNT) && BASE_WIN32_WINNT >= 0x0400
    InterlockedExchangeAdd( &atomic_var->value, value );
#else
    InterlockedExchangeAdd( &atomic_var->value, value );
#endif
}

/*
 * batomic_add_and_get()
 */
batomic_value_t batomic_add_and_get( batomic_t *atomic_var,
			                         batomic_value_t value)
{
#if defined(BASE_WIN32_WINNT) && BASE_WIN32_WINNT >= 0x0400
    long oldValue = InterlockedExchangeAdd( &atomic_var->value, value);
    return oldValue + value;
#else
    long oldValue = InterlockedExchangeAdd( &atomic_var->value, value);
    return oldValue + value;
#endif
}

/*
 * bthreadLocalAlloc()
 */
bstatus_t bthreadLocalAlloc(long *index)
{
	BASE_ASSERT_RETURN(index != NULL, BASE_EINVAL);

	//Can't check stack because this function is called in the
	//beginning before main thread is initialized.
	//BASE_CHECK_STACK();

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	*index = TlsAllocRT();
#else
	*index = TlsAlloc();
#endif

	if (*index == TLS_OUT_OF_INDEXES)
		return BASE_RETURN_OS_ERROR(GetLastError());
	else
		return BASE_SUCCESS;
}

/*
 * bthreadLocalFree()
 */
void bthreadLocalFree(long index)
{
	BASE_CHECK_STACK();
#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	TlsFreeRT(index);
#else
	TlsFree(index);
#endif    
}

/*
 * bthreadLocalSet()
 */
bstatus_t bthreadLocalSet(long index, void *value)
{
    BOOL rc;

    //Can't check stack because this function is called in the
    //beginning before main thread is initialized.
    //BASE_CHECK_STACK();

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    rc = TlsSetValueRT(index, value);
#else
    rc = TlsSetValue(index, value);
#endif
    
    return rc!=0 ? BASE_SUCCESS : BASE_RETURN_OS_ERROR(GetLastError());
}

/*
 * bthreadLocalGet()
 */
void* bthreadLocalGet(long index)
{
    //Can't check stack because this function is called
    //by BASE_CHECK_STACK() itself!!!
    //BASE_CHECK_STACK();
#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	return TlsGetValueRT(index);
#else
	return TlsGetValue(index);
#endif
}

static bstatus_t _initMutex(bmutex_t *mutex, const char *name)
{
	BASE_CHECK_STACK();

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
	InitializeCriticalSectionEx(&mutex->crit, 0, 0);
#elif BASE_WIN32_WINNT >= 0x0400
	InitializeCriticalSection(&mutex->crit);
#else
	mutex->hMutex = CreateMutex(NULL, FALSE, NULL);
	if (!mutex->hMutex)
	{
		return BASE_RETURN_OS_ERROR(GetLastError());
	}
#endif

#if BASE_LIB_DEBUG
	/* Set owner. */
	mutex->nesting_level = 0;
	mutex->owner = NULL;
#endif

	/* Set name. */
	if (!name)
	{
		name = "mtx%p";
	}
	
	if (strchr(name, '%'))
	{
		bansi_snprintf(mutex->objName, BASE_MAX_OBJ_NAME, name, mutex);
	}
	else
	{
		bansi_strncpy(mutex->objName, name, BASE_MAX_OBJ_NAME);
		mutex->objName[BASE_MAX_OBJ_NAME-1] = '\0';
	}

	BASE_STR_INFO(mutex->objName, "Mutex created");
	return BASE_SUCCESS;
}


/*
 * bmutex_lock()
 */
bstatus_t bmutex_lock(bmutex_t *mutex)
{
    bstatus_t status;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(mutex, BASE_EINVAL);

//    LOG_MUTEX((mutex->objName, "Mutex: thread %s is waiting", bthreadThis()->objName));

#if BASE_WIN32_WINNT >= 0x0400
    EnterCriticalSection(&mutex->crit);
    status=BASE_SUCCESS;
#else
    if (WaitForSingleObject(mutex->hMutex, INFINITE)==WAIT_OBJECT_0)
        status = BASE_SUCCESS;
    else
        status = BASE_STATUS_FROM_OS(GetLastError());

#endif
//    LOG_MUTEX((mutex->objName, (status==BASE_SUCCESS ? "Mutex acquired by thread %s" : "FAILED by %s"), bthreadThis()->objName));

#if BASE_LIB_DEBUG
    if (status == BASE_SUCCESS) {
	mutex->owner = bthreadThis();
	++mutex->nesting_level;
    }
#endif

    return status;
}

/*
 * bmutex_unlock()
 */
bstatus_t bmutex_unlock(bmutex_t *mutex)
{
	bstatus_t status;

	BASE_CHECK_STACK();
	BASE_ASSERT_RETURN(mutex, BASE_EINVAL);

#if BASE_LIB_DEBUG
	bassert(mutex->owner == bthreadThis());
	if (--mutex->nesting_level == 0)
	{
		mutex->owner = NULL;
	}
#endif

	//    LOG_MUTEX((mutex->objName, "Mutex released by thread %s", bthreadThis()->objName));

#if BASE_WIN32_WINNT >= 0x0400
	LeaveCriticalSection(&mutex->crit);
	status=BASE_SUCCESS;
#else
	status = ReleaseMutex(mutex->hMutex) ? BASE_SUCCESS : BASE_STATUS_FROM_OS(GetLastError());
#endif
    return status;
}

/*
 * bmutex_trylock()
 */
bstatus_t bmutex_trylock(bmutex_t *mutex)
{
    bstatus_t status;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(mutex, BASE_EINVAL);

    LOG_MUTEX((mutex->objName, "Mutex: thread %s is trying", 
				bthreadThis()->objName));

#if BASE_WIN32_WINNT >= 0x0400
    status=TryEnterCriticalSection(&mutex->crit) ? BASE_SUCCESS : BASE_EUNKNOWN;
#else
    status = WaitForSingleObject(mutex->hMutex, 0)==WAIT_OBJECT_0 ? 
                BASE_SUCCESS : BASE_ETIMEDOUT;
#endif
    if (status==BASE_SUCCESS) {
	LOG_MUTEX((mutex->objName, "Mutex acquired by thread %s", 
				  bthreadThis()->objName));

#if BASE_LIB_DEBUG
	mutex->owner = bthreadThis();
	++mutex->nesting_level;
#endif
    } else {
	LOG_MUTEX((mutex->objName, "Mutex: thread %s's trylock() failed", 
				    bthreadThis()->objName));
    }

    return status;
}

/*
 * bmutex_destroy()
 */
bstatus_t bmutex_destroy(bmutex_t *mutex)
{
	BASE_CHECK_STACK();
	BASE_ASSERT_RETURN(mutex, BASE_EINVAL);

	LOG_MUTEX((mutex->objName, "Mutex destroyed"));

#if BASE_WIN32_WINNT >= 0x0400
	DeleteCriticalSection(&mutex->crit);
	return BASE_SUCCESS;
#else
	return CloseHandle(mutex->hMutex) ? BASE_SUCCESS : BASE_RETURN_OS_ERROR(GetLastError());
#endif
}

/*
 * bmutex_is_locked()
 */
bbool_t bmutex_is_locked(bmutex_t *mutex)
{
#if BASE_LIB_DEBUG
    return mutex->owner == bthreadThis();
#else
    BASE_UNUSED_ARG(mutex);
    bassert(!"BASE_LIB_DEBUG is not set!");
    return 1;
#endif
}

/*
 * Win32 lacks Read/Write mutex, so include the emulation.
 */
#include "_baseOsRwmutex.c"

/*
 * benter_critical_section()
 */
void benter_critical_section(void)
{
    bmutex_lock(&_criticalSectionMutex);
}


/*
 * bleave_critical_section()
 */
void bleave_critical_section(void)
{
    bmutex_unlock(&_criticalSectionMutex);
}

///////////////////////////////////////////////////////////////////////////////
#if defined(BASE_HAS_SEMAPHORE) && BASE_HAS_SEMAPHORE != 0

/*
 * bsem_create()
 */
bstatus_t bsem_create( bpool_t *pool, 
                                   const char *name,
				   unsigned initial, 
                                   unsigned max,
                                   bsem_t **sem_ptr)
{
    bsem_t *sem;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(pool && sem_ptr, BASE_EINVAL);

    sem = bpool_alloc(pool, sizeof(*sem));    

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    /** SEMAPHORE_ALL_ACCESS **/
    sem->hSemaphore = CreateSemaphoreEx(NULL, initial, max, NULL, 0,
					SEMAPHORE_ALL_ACCESS);
#else
    sem->hSemaphore = CreateSemaphore(NULL, initial, max, NULL);
#endif
    
    if (!sem->hSemaphore)
	return BASE_RETURN_OS_ERROR(GetLastError());

    /* Set name. */
    if (!name) {
	name = "sem%p";
    }
    if (strchr(name, '%')) {
	bansi_snprintf(sem->objName, BASE_MAX_OBJ_NAME, name, sem);
    } else {
	bansi_strncpy(sem->objName, name, BASE_MAX_OBJ_NAME);
	sem->objName[BASE_MAX_OBJ_NAME-1] = '\0';
    }

    LOG_MUTEX((sem->objName, "Semaphore created"));

    *sem_ptr = sem;
    return BASE_SUCCESS;
}

static bstatus_t bsem_wait_for(bsem_t *sem, unsigned timeout)
{
    DWORD result;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sem, BASE_EINVAL);

    LOG_MUTEX((sem->objName, "Semaphore: thread %s is waiting", 
			      bthreadThis()->objName));
    
#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    result = WaitForSingleObjectEx(sem->hSemaphore, timeout, FALSE);
#else
    result = WaitForSingleObject(sem->hSemaphore, timeout);
#endif

    if (result == WAIT_OBJECT_0) {
	LOG_MUTEX((sem->objName, "Semaphore acquired by thread %s", 
				  bthreadThis()->objName));
    } else {
	LOG_MUTEX((sem->objName, "Semaphore: thread %s FAILED to acquire", 
				  bthreadThis()->objName));
    }

    if (result==WAIT_OBJECT_0)
        return BASE_SUCCESS;
    else if (result==WAIT_TIMEOUT)
        return BASE_ETIMEDOUT;
    else
        return BASE_RETURN_OS_ERROR(GetLastError());
}

/*
 * bsem_wait()
 */
bstatus_t bsem_wait(bsem_t *sem)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sem, BASE_EINVAL);

    return bsem_wait_for(sem, INFINITE);
}

/*
 * bsem_trywait()
 */
bstatus_t bsem_trywait(bsem_t *sem)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sem, BASE_EINVAL);

    return bsem_wait_for(sem, 0);
}

/*
 * bsem_post()
 */
bstatus_t bsem_post(bsem_t *sem)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sem, BASE_EINVAL);

    LOG_MUTEX((sem->objName, "Semaphore released by thread %s",
			      bthreadThis()->objName));

    if (ReleaseSemaphore(sem->hSemaphore, 1, NULL))
        return BASE_SUCCESS;
    else
        return BASE_RETURN_OS_ERROR(GetLastError());
}

/*
 * bsem_destroy()
 */
bstatus_t bsem_destroy(bsem_t *sem)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(sem, BASE_EINVAL);

    LOG_MUTEX((sem->objName, "Semaphore destroyed by thread %s",
			      bthreadThis()->objName));

    if (CloseHandle(sem->hSemaphore))
        return BASE_SUCCESS;
    else
        return BASE_RETURN_OS_ERROR(GetLastError());
}

#endif	/* BASE_HAS_SEMAPHORE */
///////////////////////////////////////////////////////////////////////////////


#if defined(BASE_HAS_EVENT_OBJ) && BASE_HAS_EVENT_OBJ != 0

/*
 * bevent_create()
 */
bstatus_t bevent_create( bpool_t *pool, 
                                     const char *name,
				     bbool_t manual_reset, 
                                     bbool_t initial,
                                     bevent_t **event_ptr)
{
    bevent_t *event;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(pool && event_ptr, BASE_EINVAL);

    event = bpool_alloc(pool, sizeof(*event));
    if (!event)
        return BASE_ENOMEM;

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    event->hEvent = CreateEventEx(NULL, NULL,
				 (manual_reset? 0x1:0x0) | (initial? 0x2:0x0),
				 EVENT_ALL_ACCESS);
#else
    event->hEvent = CreateEvent(NULL, manual_reset ? TRUE : FALSE,
			        initial ? TRUE : FALSE, NULL);
#endif

    if (!event->hEvent)
	return BASE_RETURN_OS_ERROR(GetLastError());

    /* Set name. */
    if (!name) {
	name = "evt%p";
    }
    if (strchr(name, '%')) {
	bansi_snprintf(event->objName, BASE_MAX_OBJ_NAME, name, event);
    } else {
	bansi_strncpy(event->objName, name, BASE_MAX_OBJ_NAME);
	event->objName[BASE_MAX_OBJ_NAME-1] = '\0';
    }

    BASE_STR_INFO(event->objName, "Event created");

    *event_ptr = event;
    return BASE_SUCCESS;
}

static bstatus_t bevent_wait_for(bevent_t *event, unsigned timeout)
{
    DWORD result;

    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(event, BASE_EINVAL);

    BASE_STR_INFO(event->objName, "Event: thread %s is waiting",  bthreadThis()->objName);

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    result = WaitForSingleObjectEx(event->hEvent, timeout, FALSE);
#else
    result = WaitForSingleObject(event->hEvent, timeout);
#endif

    if (result == WAIT_OBJECT_0) {
	BASE_STR_INFO(event->objName, "Event: thread %s is released", bthreadThis()->objName);
    } else {
	BASE_STR_INFO(event->objName, "Event: thread %s FAILED to acquire",  bthreadThis()->objName);
    }

    if (result==WAIT_OBJECT_0)
        return BASE_SUCCESS;
    else if (result==WAIT_TIMEOUT)
        return BASE_ETIMEDOUT;
    else
        return BASE_RETURN_OS_ERROR(GetLastError());
}

/*
 * bevent_wait()
 */
bstatus_t bevent_wait(bevent_t *event)
{
    BASE_ASSERT_RETURN(event, BASE_EINVAL);

    return bevent_wait_for(event, INFINITE);
}

/*
 * bevent_trywait()
 */
bstatus_t bevent_trywait(bevent_t *event)
{
    BASE_ASSERT_RETURN(event, BASE_EINVAL);

    return bevent_wait_for(event, 0);
}

/*
 * bevent_set()
 */
bstatus_t bevent_set(bevent_t *event)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(event, BASE_EINVAL);

    BASE_STR_INFO(event->objName, "Setting event");

    if (SetEvent(event->hEvent))
        return BASE_SUCCESS;
    else
        return BASE_RETURN_OS_ERROR(GetLastError());
}

/*
 * bevent_pulse()
 */
bstatus_t bevent_pulse(bevent_t *event)
{
#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    BASE_UNUSED_ARG(event);
    bassert(!"bevent_pulse() not supported!");
    return BASE_ENOTSUP;
#else
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(event, BASE_EINVAL);

   BASE_STR_INFO(event->objName, "Pulsing event");

    if (PulseEvent(event->hEvent))
	return BASE_SUCCESS;
    else
	return BASE_RETURN_OS_ERROR(GetLastError());
#endif
}

/*
 * bevent_reset()
 */
bstatus_t bevent_reset(bevent_t *event)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(event, BASE_EINVAL);

    BASE_STR_INFO(event->objName, "Event is reset");

    if (ResetEvent(event->hEvent))
        return BASE_SUCCESS;
    else
        return BASE_RETURN_OS_ERROR(GetLastError());
}

/*
 * bevent_destroy()
 */
bstatus_t bevent_destroy(bevent_t *event)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(event, BASE_EINVAL);

    BASE_STR_INFO(event->objName, "Event is destroying");

    if (CloseHandle(event->hEvent))
        return BASE_SUCCESS;
    else
        return BASE_RETURN_OS_ERROR(GetLastError());
}

#endif	/* BASE_HAS_EVENT_OBJ */

#if defined(BASE_TERM_HAS_COLOR) && BASE_TERM_HAS_COLOR != 0
static WORD _bColor2OsAttr(bcolor_t color)
{
	WORD attr = 0;

	if (color & BASE_TERM_COLOR_R)
		attr |= FOREGROUND_RED;
	if (color & BASE_TERM_COLOR_G)
		attr |= FOREGROUND_GREEN;
	if (color & BASE_TERM_COLOR_B)
		attr |= FOREGROUND_BLUE;
	if (color & BASE_TERM_COLOR_BRIGHT)
		attr |= FOREGROUND_INTENSITY;

	return attr;
}

static bcolor_t _osAttr2bColor(WORD attr)
{
	int color = 0;

	if (attr & FOREGROUND_RED)
		color |= BASE_TERM_COLOR_R;
	if (attr & FOREGROUND_GREEN)
		color |= BASE_TERM_COLOR_G;
	if (attr & FOREGROUND_BLUE)
		color |= BASE_TERM_COLOR_B;
	if (attr & FOREGROUND_INTENSITY)
		color |= BASE_TERM_COLOR_BRIGHT;

	return color;
}


bstatus_t bterm_set_color(bcolor_t color)
{
	BOOL rc;
	WORD attr = 0;

	BASE_CHECK_STACK();

	attr = _bColor2OsAttr(color);
	rc = SetConsoleTextAttribute( GetStdHandle(STD_OUTPUT_HANDLE), attr);
	return rc ? BASE_SUCCESS : BASE_RETURN_OS_ERROR(GetLastError());
}

bcolor_t bterm_get_color(void)
{
	CONSOLE_SCREEN_BUFFER_INFO info;

	BASE_CHECK_STACK();

	GetConsoleScreenBufferInfo( GetStdHandle(STD_OUTPUT_HANDLE), &info);
	return _osAttr2bColor(info.wAttributes);
}

#endif	/* BASE_TERM_HAS_COLOR */

#include "_baseOsCommon.c"

bstatus_t libBaseInit(void)
{
	WSADATA wsa;
	char dummy_guid[32]; /* use maximum GUID length */
	bstr_t guid;
	bstatus_t rc;

	/* Check if  have been initialized */
	if (initialized)
	{
		++initialized;
		return BASE_SUCCESS;
	}

	/* Init Winsock.. */
	if (WSAStartup(MAKEWORD(2,0), &wsa) != 0)
	{
		return BASE_RETURN_OS_ERROR(WSAGetLastError());
	}

	/* Init this thread's TLS. */
	if ((rc=_bthreadInit()) != BASE_SUCCESS)
	{
		return rc;
	}

	/* Init logging */
	extLogInit();
	BASE_DEBUG("libBase %s is initializing...", BASE_VERSION);

	/* Init random seed. */
	/* Or probably not. Let application in charge of this */
	/* bsrand( GetCurrentProcessId() ); */

	/* Initialize critical section. */
	if ((rc=_initMutex(&_criticalSectionMutex, "mutex%p")) != BASE_SUCCESS)
		return rc;

	/* Startup GUID. */
	guid.ptr = dummy_guid;
	bgenerate_unique_string( &guid );

	/* Initialize exception ID for the pool. 
	* Must do so after critical section is configured.
	*/
	rc = bexception_id_alloc("/No memory", &BASE_NO_MEMORY_EXCEPTION);
	if (rc != BASE_SUCCESS)
		return rc;

	/* Startup timestamp */
#if defined(BASE_HAS_HIGH_RES_TIMER) && BASE_HAS_HIGH_RES_TIMER != 0
	{
		btimestamp dummy_ts;
		if ((rc=bTimeStampGetFreq(&dummy_ts)) != BASE_SUCCESS)
		{
			return rc;
		}
		
		if ((rc=bTimeStampGet(&dummy_ts)) != BASE_SUCCESS)
		{
			return rc;
		}
	}
#endif   

	/* Flag  as initialized */
	++initialized;
	bassert(initialized == 1);

	BASE_INFO("libBase %s for Windows initialized", BASE_VERSION);

	return BASE_SUCCESS;
}


void libBaseShutdown()
{
	int i;

	/* Only perform shutdown operation when 'initialized' reaches zero */
	bassert(initialized > 0);
	if (--initialized != 0)
		return;

	BASE_DEBUG("libBase %s is destorying...", BASE_VERSION);
	/* Display stack usage */
#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK!=0
	{
		bthread_t *rec = (bthread_t*)main_thread;
		BASE_STR_INFO(rec->objName, "Main thread stack max usage=%u by %s:%d", rec->stk_max_usage, rec->caller_file, rec->caller_line);
	}
#endif

	/* Call atexit() functions */
	_exitHandleQuit();

	/* Free exception ID */
	if (BASE_NO_MEMORY_EXCEPTION != -1)
	{
		bexception_id_free(BASE_NO_MEMORY_EXCEPTION);
		BASE_NO_MEMORY_EXCEPTION = -1;
	}

	/* Destroy  critical section */
	bmutex_destroy(&_criticalSectionMutex);

	/* Free  TLS */
	if (thread_tls_id != -1)
	{
		bthreadLocalFree(thread_tls_id);
		thread_tls_id = -1;
	}

	/* Clear static variables */
	berrno_clear_handlers();

	/* Ticket #1132: Assertion when (re)starting  on different thread */
	bbzero(main_thread, sizeof(main_thread));

	/* Shutdown Winsock */
	WSACleanup();
	BASE_DEBUG("libBase %s is destoried", BASE_VERSION);
}


