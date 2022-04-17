

static bstatus_t _bthreadInit(void)
{
#if BASE_HAS_THREADS
	bstatus_t rc;
	bthread_t *dummy;

	rc = bthreadLocalAlloc(&thread_tls_id );
	if (rc != BASE_SUCCESS)
	{
		return rc;
	}
	return bthreadRegister("thr%p", (long*)&main_thread, &dummy);
#else
	BASE_CRIT("Thread init error. Threading is not enabled!");
	return BASE_EINVALIDOP;
#endif
}

/*
 * bthread-get_name()
 */
const char* bthreadGetName(bthread_t *p)
{
#if BASE_HAS_THREADS
	bthread_t *rec = (bthread_t*)p;

	BASE_CHECK_STACK();
	BASE_ASSERT_RETURN(p, "");

	return rec->objName;
#else
	return "";
#endif
}


bthread_t* bthreadThis(void)
{
#if BASE_HAS_THREADS
	bthread_t *rec = (bthread_t*)bthreadLocalGet(thread_tls_id);
	if (rec == NULL)
	{
		bassert(!"Calling libBase from unknown/external thread. You must register external threads with bthreadRegister() before calling any libBase functions.");
	}

	/*
	* MUST NOT check stack because this function is called
	* by BASE_CHECK_STACK() itself!!!
	*
	*/

	return rec;
#else
	bassert(!"Threading is not enabled!");
	return NULL;
#endif
}


/*
 * Get native thread handle
 */
void* bthreadGetOsHandle(bthread_t *thread) 
{
	BASE_ASSERT_RETURN(thread, NULL);

#if BASE_HAS_THREADS
#ifdef	_MSC_VER
	return thread->hthread;
#else
	return thread->thread;
#endif
#else
	bassert("bthreadIsRegistered() called in non-threading mode!");
	return NULL;
#endif
}

/*
 * Check if this thread has been registered to .
 */
bbool_t bthreadIsRegistered(void)
{
#if BASE_HAS_THREADS
	return bthreadLocalGet(thread_tls_id) != 0;
#else
	bassert("bthreadIsRegistered() called in non-threading mode!");
	return BASE_TRUE;
#endif
}

#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK != 0
/*
 * bthread_check_stack()
 * Implementation for BASE_CHECK_STACK()
 */
void bthread_check_stack(const char *file, int line)
{
	char stk_ptr;
	buint32_t usage;

	if(initialized == 0)
	{/* before init or afte destoried, never check stack */
		return;
	}
	
//	printf("%s.%d\n", file, line );
	bthread_t *thread = bthreadThis();

	bassert(thread);

	/* Calculate current usage. */
	usage = (&stk_ptr > thread->stk_start) ? (buint32_t)(&stk_ptr - thread->stk_start): (buint32_t)(thread->stk_start - &stk_ptr);

	/* Assert if stack usage is dangerously high. */
	bassert("STACK OVERFLOW!! " && (usage <= thread->stk_size - 128));

	/* Keep statistic. */
	if (usage > thread->stk_max_usage)
	{
		thread->stk_max_usage = usage;
		thread->caller_file = file;
		thread->caller_line = line;
	}

}

/*
 * bthread_get_stack_max_usage()
 */
buint32_t bthread_get_stack_max_usage(bthread_t *thread)
{
    return thread->stk_max_usage;
}

/*
 * bthread_get_stack_info()
 */
bstatus_t bthread_get_stack_info( bthread_t *thread, const char **file, int *line )
{
	bassert(thread);

	*file = thread->caller_file;
	*line = thread->caller_line;
	return 0;
}

#endif	/* BASE_OS_HAS_CHECK_STACK */


/*
 * bmutex_create()
 */
bstatus_t bmutex_create(bpool_t *pool,      const char *name, int type, bmutex_t **ptr_mutex)
{
#if BASE_HAS_THREADS
	bstatus_t rc;
	bmutex_t *mutex;

#ifdef	_MSC_VER
	BASE_UNUSED_ARG(type);
#endif

	BASE_ASSERT_RETURN(pool && ptr_mutex, BASE_EINVAL);

	mutex = BASE_POOL_ALLOC_T(pool, bmutex_t);
	BASE_ASSERT_RETURN(mutex, BASE_ENOMEM);

#ifdef	_MSC_VER
	rc = _initMutex(mutex, name);
#else
	rc=_initMutex(mutex, name, type);
#endif
	if (rc != BASE_SUCCESS)
		return rc;

	*ptr_mutex = mutex;
	return BASE_SUCCESS;
#else /* BASE_HAS_THREADS */
	*ptr_mutex = (bmutex_t*)1;
	return BASE_SUCCESS;
#endif
}

/*
 * bmutex_create_simple()
 */
bstatus_t bmutex_create_simple( bpool_t *pool,
                                            const char *name,
					    bmutex_t **mutex )
{
    return bmutex_create(pool, name, BASE_MUTEX_SIMPLE, mutex);
}

/*
 * bmutex_create_recursive()
 */
bstatus_t bmutex_create_recursive( bpool_t *pool,
					       const char *name,
					       bmutex_t **mutex )
{
    return bmutex_create(pool, name, BASE_MUTEX_RECURSE, mutex);
}

/*
 * brun_app()
 */
int brun_app(bmain_func_ptr main_func, int argc, char *argv[], unsigned flags)
{
	BASE_UNUSED_ARG(flags);
	return (*main_func)(argc, argv);
}

typedef	struct
{
	char				name[32];
	AtExitFunc		handle;
	void				*data;	
}ExitHandle;

//static void (*atexit_func[32])(void);

static ExitHandle	_exitHandles[32];
static unsigned	_atExitCount = 0;

bstatus_t libBaseAtExit(AtExitFunc func, char *_name, void *_data)
{
	if (_atExitCount >= BASE_ARRAY_SIZE(_exitHandles))
		return BASE_ETOOMANY;

	_exitHandles[_atExitCount++].handle = func;
	snprintf(_exitHandles[_atExitCount].name, sizeof(_exitHandles[_atExitCount].name), "%s", _name);
	_exitHandles[_atExitCount].data = _data;
	
	return BASE_SUCCESS;
}

static int	_exitHandleQuit(void)
{
	int i;
	
	/* Call atexit() functions */
	for (i=_atExitCount-1; i>=0; --i)
	{
		BASE_DEBUG("AtExit '%s' is quit now...", _exitHandles[i].name);
		_exitHandles[i].handle( _exitHandles[i].data);
	}
	
	i = _atExitCount;
	_atExitCount = 0;

	return i;
}

