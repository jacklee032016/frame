/* 
 * 
 */
#ifndef __BASE_OS_H__
#define __BASE_OS_H__

/**
 * @file os.h
 * @brief OS dependent functions
 */
#include <_baseTypes.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_OS Operating System Dependent Functionality.
 */

/**
 * @defgroup BASE_SYS_INFO System Information
 * @ingroup BASE_OS
 * @{
 */

/**
 * These enumeration contains constants to indicate support of miscellaneous
 * system features. These will go in "flags" field of #bsys_info structure.
 */
typedef enum bsys_info_flag
{
	/* Support for Apple iOS background feature */
	BASE_SYS_HAS_IOS_BG = 1
} bsys_info_flag;


/**
 * This structure contains information about the system. Use #bget_sys_info()
 * to obtain the system information.
 */
typedef struct bsys_info
{
    /**
     * Null terminated string containing processor information (e.g. "i386",
     * "x86_64"). It may contain empty string if the value cannot be obtained.
     */
    bstr_t	machine;

    /**
     * Null terminated string identifying the system operation (e.g. "Linux",
     * "win32", "wince"). It may contain empty string if the value cannot be
     * obtained.
     */
    bstr_t	os_name;

    /**
     * A number containing the operating system version number. By convention,
     * this field is divided into four bytes, where the highest order byte
     * contains the most major version of the OS, the next less significant
     * byte contains the less major version, and so on. How the OS version
     * number is mapped into these four bytes would be specific for each OS.
     * For example, Linux-2.6.32-28 would yield "os_ver" value of 0x0206201c,
     * while for Windows 7 it will be 0x06010000 (because dwMajorVersion is
     * 6 and dwMinorVersion is 1 for Windows 7).
     *
     * This field may contain zero if the OS version cannot be obtained.
     */
    buint32_t	os_ver;

    /**
     * Null terminated string identifying the SDK name that is used to build
     * the library (e.g. "glibc", "uclibc", "msvc", "wince"). It may contain
     * empty string if the value cannot eb obtained.
     */
    bstr_t	sdk_name;

    /**
     * A number containing the SDK version, using the numbering convention as
     * the "os_ver" field. The value will be zero if the version cannot be
     * obtained.
     */
    buint32_t	sdk_ver;

    /**
     * A longer null terminated string identifying the underlying system with
     * as much information as possible.
     */
    bstr_t	info;

    /**
     * Other flags containing system specific information. The value is
     * bitmask of #bsys_info_flag constants.
     */
    buint32_t	flags;

} bsys_info;


/**
 * Obtain the system information.
 *
 * @return	System information structure.
 */
const bsys_info* bget_sys_info(void);

/**
 * @defgroup BASE_THREAD Threads
 * @ingroup BASE_OS
 * @{
 * This module provides multithreading API.

 * \section bthread_examples_sec Examples
 *
 * For examples, please see:
 *  - \ref page_baselib_thread_test
 *  - \ref page_baselib_sleep_test
 *
 */

/**
 * Thread creation flags:
 * - BASE_THREAD_SUSPENDED: specify that the thread should be created suspended.
 */
typedef enum _bthread_create_flags
{
	BASE_THREAD_SUSPENDED = 1
} bthread_create_flags;


/**
 * Type of thread entry function.
 */
//typedef int (BASE_THREAD_FUNC bthread_proc)(void*);
typedef int ( bthread_proc)(void*);

/**
 * Size of thread struct.
 */
#if !defined(BASE_THREAD_DESC_SIZE)
#   define BASE_THREAD_DESC_SIZE	    (64)
#endif

/**
 * Thread structure, to thread's state when the thread is created by external or native API. 
 */
typedef long bthread_desc[BASE_THREAD_DESC_SIZE];

/**
 * Get process ID.
 */
buint32_t bgetpid(void);

/**
 * Create a new thread.
 *
 * @param pool          The memory pool from which the thread record 
 *                      will be allocated from.
 * @param thread_name   The optional name to be assigned to the thread.
 * @param proc          Thread entry function.
 * @param arg           Argument to be passed to the thread entry function.
 * @param stack_size    The size of the stack for the new thread, or ZERO or
 *                      BASE_THREAD_DEFAULT_STACK_SIZE to let the 
 *		        library choose the reasonable size for the stack. 
 *                      For some systems, the stack will be allocated from 
 *                      the pool, so the pool must have suitable capacity.
 * @param flags         Flags for thread creation, which is bitmask combination 
 *                      from enum bthread_create_flags.
 * @param thread        Pointer to hold the newly created thread.
 *
 * @return	        BASE_SUCCESS on success, or the error code.
 */
bstatus_t bthreadCreate(  bpool_t *pool, 
                                        const char *thread_name,
				        bthread_proc *proc, 
                                        void *arg,
				        bsize_t stack_size, 
                                        unsigned flags,
					bthread_t **thread );

/**
 * Register a thread that was created by external or native API to .
 * This function must be called in the context of the thread being registered.
 * When the thread is created by external function or API call,
 * it must be 'registered' to  using bthreadRegister(), so that it can
 * cooperate with 's framework. During registration, some data needs to
 * be maintained, and this data must remain available during the thread's 
 * lifetime.
 *
 * @param thread_name   The optional name to be assigned to the thread.
 * @param desc          Thread descriptor, which must be available throughout 
 *                      the lifetime of the thread.
 * @param thread        Pointer to hold the created thread handle.
 *
 * @return              BASE_SUCCESS on success, or the error code.
 */
bstatus_t bthreadRegister ( const char *thread_name, bthread_desc desc, bthread_t **thread);

/**
 * Check if this thread has been registered to .
 *
 * @return		Non-zero if it is registered.
 */
bbool_t bthreadIsRegistered(void);


/**
 * Get thread priority value for the thread.
 *
 * @param thread	Thread handle.
 *
 * @return		Thread priority value, or -1 on error.
 */
int bthreadGetPrio(bthread_t *thread);


/**
 * Set the thread priority. The priority value must be in the priority
 * value range, which can be retrieved with #bthreadGetPrioMin() and
 * #bthreadGetPrioMax() functions.
 *
 * @param thread	Thread handle.
 * @param prio		New priority to be set to the thread.
 *
 * @return		BASE_SUCCESS on success or the error code.
 */
bstatus_t bthreadSetPrio(bthread_t *thread,  int prio);

/**
 * Get the lowest priority value available for this thread.
 *
 * @param thread	Thread handle.
 * @return		Minimum thread priority value, or -1 on error.
 */
int bthreadGetPrioMin(bthread_t *thread);


/**
 * Get the highest priority value available for this thread.
 *
 * @param thread	Thread handle.
 * @return		Minimum thread priority value, or -1 on error.
 */
int bthreadGetPrioMax(bthread_t *thread);


/**
 * Return native handle from bthread_t for manipulation using native
 * OS APIs.
 *
 * @param thread	 thread descriptor.
 *
 * @return		Native thread handle. For example, when the
 *			backend thread uses pthread, this function will
 *			return pointer to pthread_t, and on Windows,
 *			this function will return HANDLE.
 */
void* bthreadGetOsHandle(bthread_t *thread);

/**
 * Get thread name.
 *
 * @param thread    The thread handle.
 *
 * @return Thread name as null terminated string.
 */
const char* bthreadGetName(bthread_t *thread);

/**
 * Resume a suspended thread.
 *
 * @param thread    The thread handle.
 *
 * @return zero on success.
 */
bstatus_t bthreadResume(bthread_t *thread);

/**
 * Get the current thread.
 *
 * @return Thread handle of current thread.
 */
bthread_t* bthreadThis(void);

/**
 * Join thread, and block the caller thread until the specified thread exits.
 * If it is called from within the thread itself, it will return immediately
 * with failure status.
 * If the specified thread has already been dead, or it does not exist,
 * the function will return immediately with successful status.
 *
 * @param thread    The thread handle.
 *
 * @return BASE_SUCCESS on success.
 */
bstatus_t bthreadJoin(bthread_t *thread);


/**
 * Destroy thread and release resources allocated for the thread.
 * However, the memory allocated for the bthread_t itself will only be released
 * when the pool used to create the thread is destroyed.
 *
 * @param thread    The thread handle.
 *
 * @return zero on success.
 */
bstatus_t bthreadDestroy(bthread_t *thread);


/**
 * Put the current thread to sleep for the specified miliseconds.
 *
 * @param msec Miliseconds delay.
 *
 * @return zero if successfull.
 */
bstatus_t bthreadSleepMs(unsigned msec);

/**
 * @def BASE_CHECK_STACK()
 * BASE_CHECK_STACK() macro is used to check the sanity of the stack.
 * The OS implementation may check that no stack overflow occurs, and
 * it also may collect statistic about stack usage.
 */
#if defined(BASE_OS_HAS_CHECK_STACK) && BASE_OS_HAS_CHECK_STACK!=0

#define BASE_CHECK_STACK()		bthread_check_stack(__FILE__, __LINE__)

/** @internal
 * The implementation of stack checking. 
 */
void bthread_check_stack(const char *file, int line);

/** @internal
 * Get maximum stack usage statistic. 
 */
buint32_t bthread_get_stack_max_usage(bthread_t *thread);

/** @internal
 * Dump thread stack status. 
 */
bstatus_t bthread_get_stack_info(bthread_t *thread,
					      const char **file,
					      int *line);
#else

#define BASE_CHECK_STACK()
/** bthread_get_stack_max_usage() for the thread */
#define bthread_get_stack_max_usage(thread)	    0
/** bthread_get_stack_info() for the thread */
#define bthread_get_stack_info(thread,f,l)	    (*(f)="",*(l)=0)
#endif	/* BASE_OS_HAS_CHECK_STACK */

 
/**
 * @defgroup BASE_TLS Thread Local Storage.
 */

/** 
 * Allocate thread local storage index. The initial value of the variable at
 * the index is zero.
 *
 * @param index	    Pointer to hold the return value.
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t bthreadLocalAlloc(long *index);

/**
 * Deallocate thread local variable.
 *
 * @param index	    The variable index.
 */
void bthreadLocalFree(long index);

/**
 * Set the value of thread local variable.
 *
 * @param index	    The index of the variable.
 * @param value	    The value.
 */
bstatus_t bthreadLocalSet(long index, void *value);

/**
 * Get the value of thread local variable.
 *
 * @param index	    The index of the variable.
 * @return	    The value.
 */
void* bthreadLocalGet(long index);


/**
 * @defgroup BASE_ATOMIC Atomic Variables
 * @ingroup BASE_OS
 * @{
 *
 * This module provides API to manipulate atomic variables.
 *
 * \section batomic_examples_sec Examples
 *
 * For some example codes, please see:
 *  - @ref page_baselib_atomic_test
 */


/**
 * Create atomic variable.
 *
 * @param pool	    The pool.
 * @param initial   The initial value of the atomic variable.
 * @param atomic    Pointer to hold the atomic variable upon return.
 *
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t batomic_create( bpool_t *pool, 
				       batomic_value_t initial,
				       batomic_t **atomic );

/**
 * Destroy atomic variable.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return BASE_SUCCESS if success.
 */
bstatus_t batomic_destroy( batomic_t *atomic_var );

/**
 * Set the value of an atomic type, and return the previous value.
 *
 * @param atomic_var	the atomic variable.
 * @param value		value to be set to the variable.
 */
void batomic_set( batomic_t *atomic_var,  batomic_value_t value);

/**
 * Get the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return the value of the atomic variable.
 */
batomic_value_t batomic_get(batomic_t *atomic_var);

/**
 * Increment the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 */
void batomic_inc(batomic_t *atomic_var);

/**
 * Increment the value of an atomic type and get the result.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return              The incremented value.
 */
batomic_value_t batomic_inc_and_get(batomic_t *atomic_var);

/**
 * Decrement the value of an atomic type.
 *
 * @param atomic_var	the atomic variable.
 */
void batomic_dec(batomic_t *atomic_var);

/**
 * Decrement the value of an atomic type and get the result.
 *
 * @param atomic_var	the atomic variable.
 *
 * @return              The decremented value.
 */
batomic_value_t batomic_dec_and_get(batomic_t *atomic_var);

/**
 * Add a value to an atomic type.
 *
 * @param atomic_var	The atomic variable.
 * @param value		Value to be added.
 */
void batomic_add( batomic_t *atomic_var,
			     batomic_value_t value);

/**
 * Add a value to an atomic type and get the result.
 *
 * @param atomic_var	The atomic variable.
 * @param value		Value to be added.
 *
 * @return              The result after the addition.
 */
batomic_value_t batomic_add_and_get( batomic_t *atomic_var,
			                          batomic_value_t value);

/**
 * @defgroup BASE_MUTEX Mutexes.
 * @ingroup BASE_OS
 * @{
 *
 * Mutex manipulation. Alternatively, application can use higher abstraction
 * for lock objects, which provides uniform API for all kinds of lock 
 * mechanisms, including mutex. See @ref BASE_LOCK for more information.
 */

/**
 * Mutex types:
 *  - BASE_MUTEX_DEFAULT: default mutex type, which is system dependent.
 *  - BASE_MUTEX_SIMPLE: non-recursive mutex.
 *  - BASE_MUTEX_RECURSE: recursive mutex.
 */
typedef enum bmutex_type_e
{
	BASE_MUTEX_DEFAULT,
	BASE_MUTEX_SIMPLE,
	BASE_MUTEX_RECURSE
} bmutex_type_e;


/**
 * Create mutex of the specified type.
 *
 * @param pool	    The pool.
 * @param name	    Name to be associated with the mutex (for debugging).
 * @param type	    The type of the mutex, of type #bmutex_type_e.
 * @param mutex	    Pointer to hold the returned mutex instance.
 *
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t bmutex_create(bpool_t *pool, 
                                     const char *name,
				     int type, 
                                     bmutex_t **mutex);

/**
 * Create simple, non-recursive mutex.
 * This function is a simple wrapper for #bmutex_create to create 
 * non-recursive mutex.
 *
 * @param pool	    The pool.
 * @param name	    Mutex name.
 * @param mutex	    Pointer to hold the returned mutex instance.
 *
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t bmutex_create_simple( bpool_t *pool, const char *name,
					     bmutex_t **mutex );

/**
 * Create recursive mutex.
 * This function is a simple wrapper for #bmutex_create to create 
 * recursive mutex.
 *
 * @param pool	    The pool.
 * @param name	    Mutex name.
 * @param mutex	    Pointer to hold the returned mutex instance.
 *
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t bmutex_create_recursive( bpool_t *pool,
					        const char *name,
						bmutex_t **mutex );

/**
 * Acquire mutex lock.
 *
 * @param mutex	    The mutex.
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t bmutex_lock(bmutex_t *mutex);

/**
 * Release mutex lock.
 *
 * @param mutex	    The mutex.
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t bmutex_unlock(bmutex_t *mutex);

/**
 * Try to acquire mutex lock.
 *
 * @param mutex	    The mutex.
 * @return	    BASE_SUCCESS on success, or the error code if the
 *		    lock couldn't be acquired.
 */
bstatus_t bmutex_trylock(bmutex_t *mutex);

/**
 * Destroy mutex.
 *
 * @param mutex	    Te mutex.
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t bmutex_destroy(bmutex_t *mutex);

/**
 * Determine whether calling thread is owning the mutex (only available when
 * BASE_LIB_DEBUG is set).
 * @param mutex	    The mutex.
 * @return	    Non-zero if yes.
 */
bbool_t bmutex_is_locked(bmutex_t *mutex);

/**
 * @defgroup BASE_RW_MUTEX Reader/Writer Mutex
 * @ingroup BASE_OS
 * @{
 * Reader/writer mutex is a classic synchronization object where multiple
 * readers can acquire the mutex, but only a single writer can acquire the 
 * mutex.
 */

/**
 * Opaque declaration for reader/writer mutex.
 * Reader/writer mutex is a classic synchronization object where multiple
 * readers can acquire the mutex, but only a single writer can acquire the 
 * mutex.
 */
typedef struct brwmutex_t brwmutex_t;

/**
 * Create reader/writer mutex.
 *
 * @param pool	    Pool to allocate memory for the mutex.
 * @param name	    Name to be assigned to the mutex.
 * @param mutex	    Pointer to receive the newly created mutex.
 *
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t brwmutex_create(bpool_t *pool, const char *name, brwmutex_t **mutex);

/**
 * Lock the mutex for reading.
 *
 * @param mutex	    The mutex.
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t brwmutex_lock_read(brwmutex_t *mutex);

/**
 * Lock the mutex for writing.
 *
 * @param mutex	    The mutex.
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t brwmutex_lock_write(brwmutex_t *mutex);

/**
 * Release read lock.
 *
 * @param mutex	    The mutex.
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t brwmutex_unlock_read(brwmutex_t *mutex);

/**
 * Release write lock.
 *
 * @param mutex	    The mutex.
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t brwmutex_unlock_write(brwmutex_t *mutex);

/**
 * Destroy reader/writer mutex.
 *
 * @param mutex	    The mutex.
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t brwmutex_destroy(brwmutex_t *mutex);


/**
 * @defgroup BASE_CRIT_SEC Critical sections.
 * @ingroup BASE_OS
 * @{
 * Critical section protection can be used to protect regions where:
 *  - mutual exclusion protection is needed.
 *  - it's rather too expensive to create a mutex.
 *  - the time spent in the region is very very brief.
 *
 * Critical section is a global object, and it prevents any threads from
 * entering any regions that are protected by critical section once a thread
 * is already in the section.
 *
 * Critial section is \a not recursive!
 *
 * Application <b>MUST NOT</b> call any functions that may cause current
 * thread to block (such as allocating memory, performing I/O, locking mutex,
 * etc.) while holding the critical section.
 */
/**
 * Enter critical section.
 */
void benter_critical_section(void);

/**
 * Leave critical section.
 */
void bleave_critical_section(void);


/* **************************************************************************/
#if defined(BASE_HAS_SEMAPHORE) && BASE_HAS_SEMAPHORE != 0
/**
 * @defgroup BASE_SEM Semaphores.
 * @ingroup BASE_OS
 * @{
 *
 * This module provides abstraction for semaphores, where available.
 */

/**
 * Create semaphore.
 *
 * @param pool	    The pool.
 * @param name	    Name to be assigned to the semaphore (for logging purpose)
 * @param initial   The initial count of the semaphore.
 * @param max	    The maximum count of the semaphore.
 * @param sem	    Pointer to hold the semaphore created.
 *
 * @return	    BASE_SUCCESS on success, or the error code.
 */
bstatus_t bsem_create( bpool_t *pool, 
                                    const char *name,
				    unsigned initial, 
                                    unsigned max,
				    bsem_t **sem);

/**
 * Wait for semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	BASE_SUCCESS on success, or the error code.
 */
bstatus_t bsem_wait(bsem_t *sem);

/**
 * Try wait for semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	BASE_SUCCESS on success, or the error code.
 */
bstatus_t bsem_trywait(bsem_t *sem);

/**
 * Release semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	BASE_SUCCESS on success, or the error code.
 */
bstatus_t bsem_post(bsem_t *sem);

/**
 * Destroy semaphore.
 *
 * @param sem	The semaphore.
 *
 * @return	BASE_SUCCESS on success, or the error code.
 */
bstatus_t bsem_destroy(bsem_t *sem);

/**
 * @}
 */
#endif	/* BASE_HAS_SEMAPHORE */


#if defined(BASE_HAS_EVENT_OBJ) && BASE_HAS_EVENT_OBJ != 0
/**
 * @defgroup BASE_EVENT Event Object.
 * @ingroup BASE_OS
 * @{
 *
 * This module provides abstraction to event object (e.g. Win32 Event) where
 * available. Event objects can be used for synchronization among threads.
 */

/**
 * Create event object.
 *
 * @param pool		The pool.
 * @param name		The name of the event object (for logging purpose).
 * @param manual_reset	Specify whether the event is manual-reset
 * @param initial	Specify the initial state of the event object.
 * @param event		Pointer to hold the returned event object.
 *
 * @return event handle, or NULL if failed.
 */
bstatus_t bevent_create(bpool_t *pool, const char *name,
				     bbool_t manual_reset, bbool_t initial,
				     bevent_t **event);

/**
 * Wait for event to be signaled.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
bstatus_t bevent_wait(bevent_t *event);

/**
 * Try wait for event object to be signalled.
 *
 * @param event The event object.
 *
 * @return zero if successfull.
 */
bstatus_t bevent_trywait(bevent_t *event);

/**
 * Set the event object state to signaled. For auto-reset event, this 
 * will only release the first thread that are waiting on the event. For
 * manual reset event, the state remains signaled until the event is reset.
 * If there is no thread waiting on the event, the event object state 
 * remains signaled.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
bstatus_t bevent_set(bevent_t *event);

/**
 * Set the event object to signaled state to release appropriate number of
 * waiting threads and then reset the event object to non-signaled. For
 * manual-reset event, this function will release all waiting threads. For
 * auto-reset event, this function will only release one waiting thread.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
bstatus_t bevent_pulse(bevent_t *event);

/**
 * Set the event object state to non-signaled.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
bstatus_t bevent_reset(bevent_t *event);

/**
 * Destroy the event object.
 *
 * @param event	    The event object.
 *
 * @return zero if successfull.
 */
bstatus_t bevent_destroy(bevent_t *event);

/**
 * @}
 */
#endif	/* BASE_HAS_EVENT_OBJ */


/**
 * @addtogroup BASE_TIME Time Data Type and Manipulation.
 * @ingroup BASE_OS
 * @{
 * This module provides API for manipulating time.
 *
 * \section btime_examples_sec Examples
 *
 * For examples, please see:
 *  - \ref page_baselib_sleep_test
 */

/**
 * Get current time of day in local representation. Like unix version
 * @param tv	Variable to store the result.
 * @return zero if successfull.
 */
bstatus_t bgettimeofday(btime_val *tv);


/**
 * Parse time value into date/time representation.
 *
 * @param tv	The time.
 * @param pt	Variable to store the date time result.
 *
 * @return zero if successfull.
 */
bstatus_t btime_decode(const btime_val *tv, bparsed_time *pt);

/**
 * Encode date/time to time value.
 *
 * @param pt	The date/time.
 * @param tv	Variable to store time value result.
 *
 * @return zero if successfull.
 */
bstatus_t btime_encode(const bparsed_time *pt, btime_val *tv);

/**
 * Convert local time to GMT.
 *
 * @param tv	Time to convert.
 *
 * @return zero if successfull.
 */
bstatus_t btime_local_to_gmt(btime_val *tv);

/**
 * Convert GMT to local time.
 *
 * @param tv	Time to convert.
 *
 * @return zero if successfull.
 */
bstatus_t btime_gmt_to_local(btime_val *tv);


#if defined(BASE_TERM_HAS_COLOR) && BASE_TERM_HAS_COLOR != 0

/**
 * @defgroup BASE_TERM Terminal
 * @ingroup BASE_OS
 * @{
 */

/**
 * Set current terminal color.
 * @param color	    The RGB color.
 *
 * @return zero on success.
 */
bstatus_t bterm_set_color(bcolor_t color);

/**
 * Get current terminal foreground color.
 *
 * @return RGB color.
 */
bcolor_t bterm_get_color(void);

/**
 * @}
 */

#endif	/* BASE_TERM_HAS_COLOR */

/**
 * @defgroup BASE_TIMESTAMP High Resolution Timestamp
 * @ingroup BASE_OS
 * @{
 *
 *  provides <b>High Resolution Timestamp</b> API to access highest 
 * resolution timestamp value provided by the platform. The API is usefull
 * to measure precise elapsed time, and can be used in applications such
 * as profiling.
 *
 * The timestamp value is represented in cycles, and can be related to
 * normal time (in seconds or sub-seconds) using various functions provided.
 *
 * \section btimestamp_examples_sec Examples
 *
 * For examples, please see:
 *  - \ref page_baselib_sleep_test
 *  - \ref page_baselib_timestamp_test
 */

/*
 * High resolution timer.
 */
#if defined(BASE_HAS_HIGH_RES_TIMER) && BASE_HAS_HIGH_RES_TIMER != 0

/**
 * Get monotonic time since some unspecified starting point.
 * @param tv	Variable to store the result.
 * @return BASE_SUCCESS if successful.
 */
bstatus_t bgettickcount(btime_val *tv);

/**
 * Acquire high resolution timer value. The time value are stored
 * in cycles.
 *
 * @param ts	    High resolution timer value.
 * @return	    BASE_SUCCESS or the appropriate error code.
 *
 * @see bTimeStampGetFreq().
 */
bstatus_t bTimeStampGet(btimestamp *ts);

/**
 * Get high resolution timer frequency, in cycles per second.
 *
 * @param freq	    Timer frequency, in cycles per second.
 * @return	    BASE_SUCCESS or the appropriate error code.
 */
bstatus_t bTimeStampGetFreq(btimestamp *freq);

/**
 * Set timestamp from 32bit values.
 * @param t	    The timestamp to be set.
 * @param hi	    The high 32bit part.
 * @param lo	    The low 32bit part.
 */
BASE_INLINE_SPECIFIER void bset_timestamp32(btimestamp *t, buint32_t hi,
				   buint32_t lo)
{
    t->u32.hi = hi;
    t->u32.lo = lo;
}


/**
 * Compare timestamp t1 and t2.
 * @param t1	    t1.
 * @param t2	    t2.
 * @return	    -1 if (t1 < t2), 1 if (t1 > t2), or 0 if (t1 == t2)
 */
BASE_INLINE_SPECIFIER int bcmp_timestamp(const btimestamp *t1, const btimestamp *t2)
{
#if BASE_HAS_INT64
    if (t1->u64 < t2->u64)
	return -1;
    else if (t1->u64 > t2->u64)
	return 1;
    else
	return 0;
#else
    if (t1->u32.hi < t2->u32.hi ||
	(t1->u32.hi == t2->u32.hi && t1->u32.lo < t2->u32.lo))
	return -1;
    else if (t1->u32.hi > t2->u32.hi ||
	     (t1->u32.hi == t2->u32.hi && t1->u32.lo > t2->u32.lo))
	return 1;
    else
	return 0;
#endif
}


/**
 * Add timestamp t2 to t1.
 * @param t1	    t1.
 * @param t2	    t2.
 */
BASE_INLINE_SPECIFIER void badd_timestamp(btimestamp *t1, const btimestamp *t2)
{
#if BASE_HAS_INT64
    t1->u64 += t2->u64;
#else
    buint32_t old = t1->u32.lo;
    t1->u32.hi += t2->u32.hi;
    t1->u32.lo += t2->u32.lo;
    if (t1->u32.lo < old)
	++t1->u32.hi;
#endif
}

/**
 * Add timestamp t2 to t1.
 * @param t1	    t1.
 * @param t2	    t2.
 */
BASE_INLINE_SPECIFIER void badd_timestamp32(btimestamp *t1, buint32_t t2)
{
#if BASE_HAS_INT64
    t1->u64 += t2;
#else
    buint32_t old = t1->u32.lo;
    t1->u32.lo += t2;
    if (t1->u32.lo < old)
	++t1->u32.hi;
#endif
}

/**
 * Substract timestamp t2 from t1.
 * @param t1	    t1.
 * @param t2	    t2.
 */
BASE_INLINE_SPECIFIER void bsub_timestamp(btimestamp *t1, const btimestamp *t2)
{
#if BASE_HAS_INT64
    t1->u64 -= t2->u64;
#else
    t1->u32.hi -= t2->u32.hi;
    if (t1->u32.lo >= t2->u32.lo)
	t1->u32.lo -= t2->u32.lo;
    else {
	t1->u32.lo -= t2->u32.lo;
	--t1->u32.hi;
    }
#endif
}

/**
 * Substract timestamp t2 from t1.
 * @param t1	    t1.
 * @param t2	    t2.
 */
BASE_INLINE_SPECIFIER void bsub_timestamp32(btimestamp *t1, buint32_t t2)
{
#if BASE_HAS_INT64
    t1->u64 -= t2;
#else
    if (t1->u32.lo >= t2)
	t1->u32.lo -= t2;
    else {
	t1->u32.lo -= t2;
	--t1->u32.hi;
    }
#endif
}

/**
 * Get the timestamp difference between t2 and t1 (that is t2 minus t1),
 * and return a 32bit signed integer difference.
 */
BASE_INLINE_SPECIFIER bint32_t btimestamp_diff32(const btimestamp *t1,
					  const btimestamp *t2)
{
    /* Be careful with the signess (I think!) */
#if BASE_HAS_INT64
    bint64_t diff = t2->u64 - t1->u64;
    return (bint32_t) diff;
#else
    bint32 diff = t2->u32.lo - t1->u32.lo;
    return diff;
#endif
}


/**
 * Calculate the elapsed time, and store it in btime_val.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time as #btime_val.
 *
 * @see belapsed_usec(), belapsed_cycle(), belapsed_nanosec()
 */
btime_val belapsed_time( const btimestamp *start,
                                      const btimestamp *stop );

/**
 * Calculate the elapsed time as 32-bit miliseconds.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in milisecond.
 *
 * @see belapsed_time(), belapsed_cycle(), belapsed_nanosec()
 */
buint32_t belapsed_msec( const btimestamp *start,
                                      const btimestamp *stop );

/**
 * Variant of #belapsed_msec() which returns 64bit value.
 */
buint64_t belapsed_msec64(const btimestamp *start,
                                       const btimestamp *stop );

/**
 * Calculate the elapsed time in 32-bit microseconds.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in microsecond.
 *
 * @see belapsed_time(), belapsed_cycle(), belapsed_nanosec()
 */
buint32_t belapsed_usec( const btimestamp *start,
                                      const btimestamp *stop );

/**
 * Calculate the elapsed time in 32-bit nanoseconds.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in nanoseconds.
 *
 * @see belapsed_time(), belapsed_cycle(), belapsed_usec()
 */
buint32_t belapsed_nanosec( const btimestamp *start,
                                         const btimestamp *stop );

/**
 * Calculate the elapsed time in 32-bit cycles.
 * This function calculates the elapsed time using highest precision
 * calculation that is available for current platform, considering
 * whether floating point or 64-bit precision arithmetic is available. 
 * For maximum portability, application should prefer to use this function
 * rather than calculating the elapsed time by itself.
 *
 * @param start     The starting timestamp.
 * @param stop      The end timestamp.
 *
 * @return	    Elapsed time in cycles.
 *
 * @see belapsed_usec(), belapsed_time(), belapsed_nanosec()
 */
buint32_t belapsed_cycle( const btimestamp *start,
                                       const btimestamp *stop );


#endif	/* BASE_HAS_HIGH_RES_TIMER */

/**
 * @defgroup BASE_APP_OS Application execution
 * @ingroup BASE_OS
 * @{
 */

/**
 * Type for application main function.
 */
typedef int (*bmain_func_ptr)(int argc, char *argv[]);

/**
 * Run the application. This function has to be called in the main thread
 * and after doing the necessary initialization according to the flags
 * provided, it will call main_func() function.
 *
 * @param main_func Application's main function.
 * @param argc	    Number of arguments from the main() function, which
 * 		    will be passed to main_func() function.
 * @param argv	    The arguments from the main() function, which will
 * 		    be passed to main_func() function.
 * @param flags     Flags for application execution, currently must be 0.
 *
 * @return          main_func()'s return value.
 */
int brun_app(bmain_func_ptr main_func, int argc, char *argv[], unsigned flags);


/**
 * Internal  function to initialize the threading subsystem.
 * @return          BASE_SUCCESS or the appropriate error code.
 */
//bstatus_t _bthreadInit(void);


BASE_END_DECL

#endif

