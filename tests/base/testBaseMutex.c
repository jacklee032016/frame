/* 
 *
 */
#include "testBaseTest.h"
#include <libBase.h>

#if INCLUDE_MUTEX_TEST

/* Test witn non-recursive mutex. */
static int simple_mutex_test(bpool_t *pool)
{
	bstatus_t rc;
	bmutex_t *mutex;

	BASE_INFO("...testing simple mutex");

	/* Create mutex. */
	MTRACE( "....create mutex");
	rc = bmutex_create( pool, "", BASE_MUTEX_SIMPLE, &mutex);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...error: bmutex_create", rc);
		return -10;
	}

	/* Normal lock/unlock cycle. */
	MTRACE("....lock mutex");
	rc = bmutex_lock(mutex);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...error: bmutex_lock", rc);
		return -20;
	}
	
	MTRACE("....unlock mutex");
	rc = bmutex_unlock(mutex);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...error: bmutex_unlock", rc);
		return -30;
	}

	/* Lock again. */
	MTRACE( "....lock mutex");
	rc = bmutex_lock(mutex);
	if (rc != BASE_SUCCESS)
		return -40;

	/* Try-lock should fail. It should not deadlocked. */
	MTRACE( "....trylock mutex");
	rc = bmutex_trylock(mutex);
	if (rc == BASE_SUCCESS)
		BASE_WARN("...info: looks like simple mutex is recursive");

	/* Unlock and done. */
	MTRACE( "....unlock mutex");
	rc = bmutex_unlock(mutex);
		if (rc != BASE_SUCCESS)
			return -50;

	MTRACE("....destroy mutex");
	rc = bmutex_destroy(mutex);
	if (rc != BASE_SUCCESS)
		return -60;

	MTRACE( "....done");
	return BASE_SUCCESS;
}


/* Test with recursive mutex. */
static int recursive_mutex_test(bpool_t *pool)
{
	bstatus_t rc;
	bmutex_t *mutex;

	BASE_INFO("...testing recursive mutex");

	/* Create mutex. */
	MTRACE( "....create mutex");
	rc = bmutex_create( pool, "", BASE_MUTEX_RECURSE, &mutex);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...error: bmutex_create", rc);
		return -10;
	}

	/* Normal lock/unlock cycle. */
	MTRACE( "....lock mutex");
	rc = bmutex_lock(mutex);
	if (rc != BASE_SUCCESS) {
		app_perror("...error: bmutex_lock", rc);
		return -20;
	}
	MTRACE( "....unlock mutex");
	rc = bmutex_unlock(mutex);
	if (rc != BASE_SUCCESS) {
		app_perror("...error: bmutex_unlock", rc);
		return -30;
	}

	/* Lock again. */
	MTRACE( "....lock mutex");
	rc = bmutex_lock(mutex);
	if (rc != BASE_SUCCESS) return -40;

	/* Try-lock should NOT fail. . */
	MTRACE("....trylock mutex");
	rc = bmutex_trylock(mutex);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...error: recursive mutex is not recursive!", rc);
		return -40;
	}

	/* Locking again should not fail. */
	MTRACE("....lock mutex");
	rc = bmutex_lock(mutex);
	if (rc != BASE_SUCCESS) {
		app_perror("...error: recursive mutex is not recursive!", rc);
		return -45;
	}

	/* Unlock several times and done. */
	MTRACE( "....unlock mutex 3x");
	rc = bmutex_unlock(mutex);
	if (rc != BASE_SUCCESS) return -50;

	rc = bmutex_unlock(mutex);
	if (rc != BASE_SUCCESS) return -51;

	rc = bmutex_unlock(mutex);
	if (rc != BASE_SUCCESS) return -52;

	MTRACE( "....destroy mutex");
	rc = bmutex_destroy(mutex);
	if (rc != BASE_SUCCESS) return -60;

	MTRACE( "....done");
	return BASE_SUCCESS;
}

#if BASE_HAS_SEMAPHORE
static int semaphore_test(bpool_t *pool)
	{
	bsem_t *sem;
	bstatus_t status;

	BASE_INFO("...testing semaphore");

	status = bsem_create(pool, NULL, 0, 1, &sem);
	if (status != BASE_SUCCESS)
	{
		app_perror("...error: bsem_create()", status);
		return -151;
	}

	status = bsem_post(sem);
	if (status != BASE_SUCCESS)
	{
		app_perror("...error: bsem_post()", status);
		bsem_destroy(sem);
		return -153;
	}

	status = bsem_trywait(sem);
	if (status != BASE_SUCCESS)
	{
		app_perror("...error: bsem_trywait()", status);
		bsem_destroy(sem);
		return -156;
	}

	status = bsem_post(sem);
	if (status != BASE_SUCCESS) {
		app_perror("...error: bsem_post()", status);
		bsem_destroy(sem);
		return -159;
	}

	status = bsem_wait(sem);
	if (status != BASE_SUCCESS) {
		app_perror("...error: bsem_wait()", status);
		bsem_destroy(sem);
		return -161;
	}

	status = bsem_destroy(sem);
	if (status != BASE_SUCCESS) {
		app_perror("...error: bsem_destroy()", status);
		return -163;
	}

	return 0;
}
#endif	/* BASE_HAS_SEMAPHORE */


int testBaseMutex(void)
{
	bpool_t *pool;
	int rc;

	pool = bpool_create(mem, "", 4000, 4000, NULL);

	rc = simple_mutex_test(pool);
	if (rc != 0)
		return rc;

	rc = recursive_mutex_test(pool);
	if (rc != 0)
		return rc;

#if BASE_HAS_SEMAPHORE
	rc = semaphore_test(pool);
	if (rc != 0)
		return rc;
#endif

	bpool_release(pool);

	return 0;
}

#else
int dummy_mutex_test;
#endif

