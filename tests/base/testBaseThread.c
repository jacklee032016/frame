/* 
 *
 */
#include "testBaseTest.h"

/**
 * \  Test: Thread Test
 *
 * This file contains \a thread_test() definition.
 *
 * \section thread_test_scope_sec Scope of Test
 * This tests:
 *  - whether BASE_THREAD_SUSPENDED flag works.
 *  - whether multithreading works.
 *  - whether thread timeslicing works, and threads have equal
 *    time-slice proportion.
 *
 * APIs tested:
 *  - bthreadCreate()
 *  - bthreadRegister()
 *  - bthreadThis()
 *  - bthreadGetName()
 *  - bthreadDestroy()
 *  - bthreadResume()
 *  - bthreadSleepMs()
 *  - bthreadJoin()
 *  - bthreadDestroy()
 *
 */
#if INCLUDE_THREAD_TEST

#include <libBase.h>

static volatile int quit_flag=0;

/*
 * Each of the thread mainly will just execute the loop which increments a variable.
 */
static void* __threadProc(buint32_t *pcounter)
{
	/* Test that bthreadRegister() works. */
	bthread_desc desc;
	bthread_t *this_thread;
	unsigned id;
	bstatus_t rc;

	id = *pcounter;
	BASE_UNUSED_ARG(id); /* Warning about unused var if MTRACE_ is disabled */
	MTRACE( "     thread %d running..", id);

	bbzero(desc, sizeof(desc));

	rc = bthreadRegister("thread", desc, &this_thread);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...error in bthreadRegister", rc);
		return NULL;
	}

	/* Test that bthreadThis() works */
	this_thread = bthreadThis();
	if (this_thread == NULL)
	{
		BASE_ERROR("...error: bthreadThis() returns NULL!");
		return NULL;
	}

	/* Test that bthreadGetName() works */
	if (bthreadGetName(this_thread) == NULL)
	{
		BASE_ERROR("...error: bthreadGetName() returns NULL!");
		return NULL;
	}

	/* Main loop */
	for (;!quit_flag;)
	{
		(*pcounter)++;
		//Must sleep if platform doesn't do time-slicing.
		//bthreadSleepMs(0);
	}

	MTRACE( "     thread %d quitting..", id);
	return NULL;
}

static int _simpleThread(const char *title, unsigned flags)
{
	bpool_t *pool;
	bthread_t *thread;
	bstatus_t rc;
	buint32_t counter = 0;

	BASE_INFO("..%s", title);

	pool = bpool_create(mem, NULL, 4000, 4000, NULL);
	if (!pool)
		return -1000;

	quit_flag = 0;

	MTRACE("    Creating thread 0..");
	rc = bthreadCreate(pool, "thread", (bthread_proc*)&__threadProc, &counter, BASE_THREAD_DEFAULT_STACK_SIZE, flags, &thread);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...error: unable to create thread", rc);
		return -1010;
	}

	MTRACE("    Main thread waiting..");
	bthreadSleepMs(1500);
	MTRACE( "    Main thread resuming..");

	if (flags & BASE_THREAD_SUSPENDED)
	{/* Check that counter is still zero */
		if (counter != 0)
		{
			BASE_ERROR("...error: thread is not suspended");
			return -1015;
		}

		rc = bthreadResume(thread);
		if (rc != BASE_SUCCESS)
		{
			app_perror("...error: resume thread error", rc);
			return -1020;
		}
	}

	BASE_INFO("..waiting for thread to quit..");
	bthreadSleepMs(1500);

	quit_flag = 1;
	bthreadJoin(thread);

	bpool_release(pool);

	if (counter == 0)
	{
		BASE_ERROR("...error: thread is not running");
		return -1025;
	}

	BASE_INFO("...%s success", title);
	return BASE_SUCCESS;
}


/*
 * timeslice_test()
 */
static int _timeSliceTest(void)
{
	enum { NUM_THREADS = 4 };
	bpool_t *pool;
	buint32_t counter[NUM_THREADS], lowest, highest, diff;
	bthread_t *thread[NUM_THREADS];
	unsigned i;
	bstatus_t rc;

	quit_flag = 0;

	pool = bpool_create(mem, NULL, 4000, 4000, NULL);
	if (!pool)
		return -10;

	BASE_INFO("..timeslice testing with %d threads", NUM_THREADS);

	/* Create all threads in suspended mode. */
	for (i=0; i<NUM_THREADS; ++i)
	{
		counter[i] = i;
		rc = bthreadCreate(pool, "thread", (bthread_proc*)&__threadProc, &counter[i], BASE_THREAD_DEFAULT_STACK_SIZE, BASE_THREAD_SUSPENDED, &thread[i]);
		if (rc!=BASE_SUCCESS)
		{
			app_perror("...ERROR in bthreadCreate()", rc);
			return -20;
		}
	}

	/* Sleep for 1 second.
	* The purpose of this is to test whether all threads are suspended.
	*/
	MTRACE( "    Main thread waiting..");
	bthreadSleepMs(1000);
	MTRACE( "    Main thread resuming..");

	/* Check that all counters are still zero. */
	for (i=0; i<NUM_THREADS; ++i)
	{
		if (counter[i] > i)
		{
			BASE_ERROR("....ERROR! Thread %d-th is not suspended!", i);
			return -30;
		}
	}

	/* Now resume all threads. */
	for (i=0; i<NUM_THREADS; ++i)
	{
		MTRACE( "    Resuming thread %d [%p]..", i, thread[i]);
		rc = bthreadResume(thread[i]);
		if (rc != BASE_SUCCESS)
		{
			app_perror("...ERROR in bthreadResume()", rc);
			return -40;
		}
	}

	/* Main thread sleeps for some time to allow threads to run.
	* The longer we sleep, the more accurate the calculation will be,
	* but it'll make user waits for longer for the test to finish.
	*/
	MTRACE( "    Main thread waiting (5s)..");
	bthreadSleepMs(5000);
	MTRACE( "    Main thread resuming..");

	/* Signal all threads to quit. */
	quit_flag = 1;

	/* Wait until all threads quit, then destroy. */
	for (i=0; i<NUM_THREADS; ++i)
	{
		MTRACE( "    Main thread joining thread %d [%p]..", i, thread[i]);
		rc = bthreadJoin(thread[i]);
		if (rc != BASE_SUCCESS)
		{
			app_perror("...ERROR in bthreadJoin()", rc);
			return -50;
		}

		MTRACE( "    Destroying thread %d [%p]..", i, thread[i]);
		rc = bthreadDestroy(thread[i]);
		if (rc != BASE_SUCCESS)
		{
			app_perror("...ERROR in bthreadDestroy()", rc);
			return -60;
		}
	}

	MTRACE("    Main thread calculating time slices..");

	/* Now examine the value of the counters.
	* Check that all threads had equal proportion of processing.
	*/
	lowest = 0xFFFFFFFF;
	highest = 0;
	for (i=0; i<NUM_THREADS; ++i)
	{
		if (counter[i] < lowest)
			lowest = counter[i];
		if (counter[i] > highest)
			highest = counter[i];
	}

	/* Check that all threads are running. */
	if (lowest < 2)
	{
		BASE_ERROR("...ERROR: not all threads were running!");
		return -70;
	}

	/* The difference between lowest and higest should be lower than 50%.
	*/
	diff = (highest-lowest)*100 / ((highest+lowest)/2);
	if ( diff >= 50)
	{
		BASE_ERROR("...ERROR: thread didn't have equal timeslice!");
		BASE_ERROR(".....lowest counter=%u, highest counter=%u, diff=%u%%", lowest, highest, diff);
		return -80;
	}
	else
	{
		BASE_INFO("...info: timeslice diff between lowest & highest=%u%%",  diff);
	}

	bpool_release(pool);
	return 0;
}

int testBaseThread(void)
{
	int rc;

	rc = _simpleThread("simple thread test", 0);
	if (rc != BASE_SUCCESS)
		return rc;

	rc = _simpleThread("suspended thread test", BASE_THREAD_SUSPENDED);
	if (rc != BASE_SUCCESS)
		return rc;

	rc = _timeSliceTest();
	if (rc != BASE_SUCCESS)
		return rc;

	return rc;
}

#else
/* To prevent warning about "translation unit is empty" when this test is disabled */
int dummy_thread_test;
#endif


