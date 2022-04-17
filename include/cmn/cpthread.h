/*
* 
*/


#ifndef __CPTHREAD_H__
#define __CPTHREAD_H__

//#ifdef _MSC_VER//_WIN32
#if _WIN32 || _WIN64
#include <stdbool.h>
#include <windows.h>
//#include <synchapi.h>
#include <processthreadsapi.h>

typedef	CRITICAL_SECTION	pthread_mutex_t;
typedef void		pthread_attr_t;
typedef	void		pthread_mutexattr_t;
typedef void		pthread_condattr_t;
typedef void		pthread_rwlockattr_t;

typedef CONDITION_VARIABLE		pthread_cond_t;
typedef HANDLE					pthread_t;

typedef struct _rwlock_t
{
	SRWLOCK		lock;
	unsigned char			exclusive;
}pthread_rwlock_t;

typedef void (*_thread_method)(void *);

#else
#include <pthread.h>
#endif

#if 0//ndef struct timespec
struct timespec
{
	long		tv_sec;
	long		tv_nsec;
};
#endif


#ifdef _MSC_VER //_WIN32

int pthread_setname_np(pthread_t thId, char *threadName);
int pthread_getname_np(pthread_t thId, char *threadName, int size);
pthread_t pthread_self(void );

int pthread_create(pthread_t *thread, pthread_attr_t *attr, void *(start_routine)(void *data), void *arg);
int pthread_join(pthread_t thread, void **value_ptr);
int pthread_detach(pthread_t);

int pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);

int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr);
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
int pthread_rwlock_trywrlock(pthread_rwlock_t  *rwlock);
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
#endif

unsigned int pcthread_get_num_procs();

void ms_to_timespec(struct timespec *ts, unsigned int ms);

#endif /* __WPTHREAD_H__ */


