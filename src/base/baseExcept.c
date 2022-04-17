/*
 *
 */
#include <baseExcept.h>
#include <baseOs.h>
#include <baseAssert.h>
#include <baseLog.h>
#include <baseErrno.h>
#include <baseString.h>

static long thread_local_id = -1;

#if defined(BASE_HAS_EXCEPTION_NAMES) && BASE_HAS_EXCEPTION_NAMES != 0
    static const char *exception_id_names[BASE_MAX_EXCEPTION_ID];
#else
    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    static int last_exception_id = 1;
#endif  /* BASE_HAS_EXCEPTION_NAMES */


#if !defined(BASE_EXCEPTION_USE_WIN32_SEH) || BASE_EXCEPTION_USE_WIN32_SEH==0
void bthrow_exception_(int exception_id)
{
    struct bexception_state_t *handler;

    handler = (struct bexception_state_t*) 
	      bthreadLocalGet(thread_local_id);
    if (handler == NULL) {
        BASE_CRIT("!!!FATAL: unhandled exception %s!\n", bexception_id_name(exception_id));
        bassert(handler != NULL);
        /* This will crash the system! */
    }
    bpop_exception_handler_(handler);
    blongjmp(handler->state, exception_id);
}

static void exception_cleanup(void *data)
{
	if (thread_local_id != -1)
	{
		bthreadLocalFree(thread_local_id);
		thread_local_id = -1;
	}

#if defined(BASE_HAS_EXCEPTION_NAMES) && BASE_HAS_EXCEPTION_NAMES != 0
	{
		unsigned i;
		for (i=0; i<BASE_MAX_EXCEPTION_ID; ++i)
		exception_id_names[i] = NULL;
	}
#else
	last_exception_id = 1;
#endif
}

void bpush_exception_handler_(struct bexception_state_t *rec)
{
	struct bexception_state_t *parent_handler = NULL;

	if (thread_local_id == -1) {
		bthreadLocalAlloc(&thread_local_id);
		bassert(thread_local_id != -1);
		libBaseAtExit(&exception_cleanup, "ExceptExit", NULL);
	}
	
	parent_handler = (struct bexception_state_t *)bthreadLocalGet(thread_local_id);
	rec->prev = parent_handler;
	bthreadLocalSet(thread_local_id, rec);
}

void bpop_exception_handler_(struct bexception_state_t *rec)
{
    struct bexception_state_t *handler;

    handler = (struct bexception_state_t *)
	      bthreadLocalGet(thread_local_id);
    if (handler && handler==rec) {
	bthreadLocalSet(thread_local_id, handler->prev);
    }
}
#endif

#if defined(BASE_HAS_EXCEPTION_NAMES) && BASE_HAS_EXCEPTION_NAMES != 0
bstatus_t bexception_id_alloc( const char *name,
                                           bexception_id_t *id)
{
    unsigned i;

    benter_critical_section();

    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    for (i=1; i<BASE_MAX_EXCEPTION_ID; ++i) {
        if (exception_id_names[i] == NULL) {
            exception_id_names[i] = name;
            *id = i;
            bleave_critical_section();
            return BASE_SUCCESS;
        }
    }

    bleave_critical_section();
    return BASE_ETOOMANY;
}

bstatus_t bexception_id_free( bexception_id_t id )
{
    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    BASE_ASSERT_RETURN(id>0 && id<BASE_MAX_EXCEPTION_ID, BASE_EINVAL);
    
    benter_critical_section();
    exception_id_names[id] = NULL;
    bleave_critical_section();

    return BASE_SUCCESS;

}

const char* bexception_id_name(bexception_id_t id)
{
    static char unknown_name[32];

    /*
     * Start from 1 (not 0)!!!
     * Exception 0 is reserved for normal path of setjmp()!!!
     */
    BASE_ASSERT_RETURN(id>0 && id<BASE_MAX_EXCEPTION_ID, "<Invalid ID>");

    if (exception_id_names[id] == NULL) {
        bansi_snprintf(unknown_name, sizeof(unknown_name), 
			 "exception %d", id);
	return unknown_name;
    }

    return exception_id_names[id];
}

#else   /* BASE_HAS_EXCEPTION_NAMES */
bstatus_t bexception_id_alloc( const char *name,
                                           bexception_id_t *id)
{
    BASE_ASSERT_RETURN(last_exception_id < BASE_MAX_EXCEPTION_ID-1, BASE_ETOOMANY);

    *id = last_exception_id++;
    return BASE_SUCCESS;
}

bstatus_t bexception_id_free( bexception_id_t id )
{
    return BASE_SUCCESS;
}

const char* bexception_id_name(bexception_id_t id)
{
    return "";
}

#endif  /* BASE_HAS_EXCEPTION_NAMES */



