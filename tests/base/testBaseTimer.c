/*
 *
 */
#include "testBaseTest.h"

/**
 * \page page_baselib_testBaseTimer Test: Timer
 *
 * This file provides implementation of \b testBaseTimer(). It tests the
 * functionality of the timer heap.
 *
 */


#if INCLUDE_TIMER_TEST

#include <libBase.h>

#define LOOP				16
#define MIN_COUNT		250
#define MAX_COUNT		(LOOP * MIN_COUNT)
#define MIN_DELAY	2
#define	D		(MAX_COUNT / 32000)
#define DELAY		(D < MIN_DELAY ? MIN_DELAY : D)

static void __timerCallback(btimer_heap_t *ht, btimer_entry *e)
{
	BASE_UNUSED_ARG(ht);
//	BASE_UNUSED_ARG(e);
	BASE_INFO("Timer#%d is called", e->id);
}

static int _testTimerHeap(void)
{
	int i, j;
	btimer_entry *entry;
	bpool_t *pool;
	btimer_heap_t *timer;
	btime_val delay;
	bstatus_t status;
	int err=0;
	bsize_t size;
	unsigned count;

	BASE_INFO(TEST_LEVEL_CASE"Basic test: %d timers", MAX_COUNT);

	size = btimer_heap_mem_size(MAX_COUNT)+MAX_COUNT*sizeof(btimer_entry);
	pool = bpool_create( mem, NULL, size, 4000, NULL);
	if (!pool)
	{
		BASE_ERROR(TEST_LEVEL_RESULT"error: unable to create pool of %u bytes", size);
		return -10;
	}

	entry = (btimer_entry*)bpool_calloc(pool, MAX_COUNT, sizeof(*entry));
	if (!entry)
		return -20;

	for (i=0; i<MAX_COUNT; ++i)
	{
		entry[i].cb = &__timerCallback;
		entry[i].id = i;
	}
	
	status = btimer_heap_create(pool, MAX_COUNT, &timer);
	if (status != BASE_SUCCESS)
	{
		app_perror("...error: unable to create timer heap", status);
		return -30;
	}

	count = MIN_COUNT;
	
	for (i=0; i<LOOP; ++i)
	{
		int early = 0;
		int done=0;
		int cancelled=0;
		int rc;
		btimestamp t1, t2, t_sched, t_cancel, t_poll;
		btime_val now, expire;

		bgettimeofday(&now);
		bsrand(now.sec);
		t_sched.u32.lo = t_cancel.u32.lo = t_poll.u32.lo = 0;

		// Register timers
		BASE_INFO(TEST_LEVEL_RESULT"scheduling %d timers", count );
		for (j=0; j<(int)count; ++j)
		{
			delay.sec = brand() % DELAY;
			delay.msec = brand() % 1000;

			// Schedule timer
			bTimeStampGet(&t1);
			rc = btimer_heap_schedule(timer, &entry[j], &delay);
			if (rc != 0)
				return -40;
			
			bTimeStampGet(&t2);

			t_sched.u32.lo += (t2.u32.lo - t1.u32.lo);

			// Poll timers.
			bTimeStampGet(&t1);
			rc = btimer_heap_poll(timer, NULL);
			bTimeStampGet(&t2);
			if (rc > 0)
			{
				t_poll.u32.lo += (t2.u32.lo - t1.u32.lo);
				early += rc;
			}
		}

		// Set the time where all timers should finish
		bgettimeofday(&expire);
		delay.sec = DELAY; 
		delay.msec = 0;
		BASE_TIME_VAL_ADD(expire, delay);

		// Wait until all timers finish, cancel some of them.
		BASE_INFO(TEST_LEVEL_RESULT"Waiting timers ending" );
		do
		{
			int index = brand() % count;
			bTimeStampGet(&t1);
			
			rc = btimer_heap_cancel(timer, &entry[index]);
			bTimeStampGet(&t2);
			if (rc > 0)
			{
				cancelled += rc;
				t_cancel.u32.lo += (t2.u32.lo - t1.u32.lo);
			}

			bgettimeofday(&now);

			bTimeStampGet(&t1);
#if defined(BASE_SYMBIAN) && BASE_SYMBIAN!=0
		/* On Symbian, we must use OS poll (Active Scheduler poll) since 
		 * timer is implemented using Active Object.
		 */
			rc = 0;
			while (bsymbianos_poll(-1, 0))
				++rc;
#else
			rc = btimer_heap_poll(timer, NULL);
#endif

			bTimeStampGet(&t2);
			if (rc > 0)
			{
				done += rc;
				t_poll.u32.lo += (t2.u32.lo - t1.u32.lo);
			}

		} while (BASE_TIME_VAL_LTE(now, expire)&&btimer_heap_count(timer) > 0);

		if (btimer_heap_count(timer))
		{
			BASE_ERROR("ERROR: %d timers left", btimer_heap_count(timer));
			++err;
		}
		
		t_sched.u32.lo /= count; 
		t_cancel.u32.lo /= count;
		t_poll.u32.lo /= count;
		BASE_INFO(TEST_LEVEL_RESULT"...ok (count:%d, early:%d, cancelled:%d,  sched:%d, cancel:%d poll:%d)", 
			count, early, cancelled, t_sched.u32.lo, t_cancel.u32.lo, t_poll.u32.lo);

		count = count * 2;
		if (count > MAX_COUNT)
			break;
	}

	bpool_release(pool);
	return err;
}


/***************
 * Stress test *
 ***************
 * Test scenario:
 * 1. Create and schedule a number of timer entries.
 * 2. Start threads for polling (simulating normal worker thread).
 *    Each expired entry will try to cancel and re-schedule itself
 *    from within the callback.
 * 3. Start threads for cancelling random entries. Each successfully
 *    cancelled entry will be re-scheduled after some random delay.
 */
#define ST_POLL_THREAD_COUNT			10
#define ST_CANCEL_THREAD_COUNT		10

#define ST_ENTRY_COUNT					1000
#define ST_ENTRY_MAX_TIMEOUT_MS		100

/* Number of group lock, may be zero, shared by timer entries, group lock
 * can be useful to evaluate poll vs cancel race condition scenario, i.e:
 * each group lock must have ref count==1 at the end of the test, otherwise
 * assertion will raise.
 */
#define ST_ENTRY_GROUP_LOCK_COUNT   1


struct thread_param
{
	btimer_heap_t		*timer;
	bbool_t			stopping;
	btimer_entry		*entries;

	batomic_t		*idx;
	struct
	{
		bbool_t		is_poll;
		unsigned		cnt;
	} stat[ST_POLL_THREAD_COUNT + ST_CANCEL_THREAD_COUNT];
};

static bstatus_t st_schedule_entry(btimer_heap_t *ht, btimer_entry *e)
{
	btime_val delay = {0};
	bgrp_lock_t *grp_lock = (bgrp_lock_t*)e->user_data;
	bstatus_t status;

	delay.msec = brand() % ST_ENTRY_MAX_TIMEOUT_MS;
	btime_val_normalize(&delay);
	status = btimer_heap_schedule_w_grp_lock(ht, e, &delay, 1, grp_lock);
	return status;
}

static void st_entry_callback(btimer_heap_t *ht, btimer_entry *e)
{
	/* try to cancel this */
	btimer_heap_cancel_if_active(ht, e, 10);

	/* busy doing something */
	bthreadSleepMs(brand() % 50);

	/* reschedule entry */
	st_schedule_entry(ht, e);
}

/* Poll worker thread function. */
static int poll_worker(void *arg)
{
	struct thread_param *tparam = (struct thread_param*)arg;
	int idx;

	idx = batomic_inc_and_get(tparam->idx);
	tparam->stat[idx].is_poll = BASE_TRUE;

	BASE_INFO("...thread #%d (poll) started", idx);
	while (!tparam->stopping)
	{
		unsigned count;
		count = btimer_heap_poll(tparam->timer, NULL);
		if (count > 0)
		{/* Count expired entries */
			BASE_DEBUG("...thread #%d called %d entries", idx, count);
			tparam->stat[idx].cnt += count;
		}
		else
		{
			bthreadSleepMs(10);
		}
	}
	BASE_INFO("...thread #%d (poll) stopped", idx);

	return 0;
}

/* Cancel worker thread function. */
static int cancel_worker(void *arg)
{
	struct thread_param *tparam = (struct thread_param*)arg;
	int idx;

	idx = batomic_inc_and_get(tparam->idx);
	tparam->stat[idx].is_poll = BASE_FALSE;

	BASE_INFO("...thread #%d (cancel) started", idx);
	while (!tparam->stopping)
	{
		int count;
		btimer_entry *e = &tparam->entries[brand() % ST_ENTRY_COUNT];

		count = btimer_heap_cancel_if_active(tparam->timer, e, 2);
		if (count > 0)
		{/* Count cancelled entries */
			BASE_DEBUG("...thread #%d cancelled %d entries", idx, count);
			tparam->stat[idx].cnt += count;

			/* Reschedule entry after some delay */
			bthreadSleepMs(brand() % 100);
			st_schedule_entry(tparam->timer, e);
		}
	}
	
	BASE_INFO("...thread #%d (cancel) stopped", idx);

	return 0;
}

static int _timerStressTest(void)
{
	int i;
	btimer_entry *entries = NULL;
	bgrp_lock_t **grp_locks = NULL;
	bpool_t *pool;
	btimer_heap_t *timer = NULL;
	block_t *timer_lock;
	bstatus_t status;
	int err=0;
	bthread_t **poll_threads = NULL;
	bthread_t **cancel_threads = NULL;
	struct thread_param tparam = {0};
	btime_val now;

	BASE_INFO("...Stress test");

	bgettimeofday(&now);
	bsrand(now.sec);

	pool = bpool_create( mem, NULL, 128, 128, NULL);
	if (!pool) {
		BASE_ERROR("...error: unable to create pool");
		err = -10;
		goto on_return;
	}

	/* Create timer heap */
	status = btimer_heap_create(pool, ST_ENTRY_COUNT, &timer);
	if (status != BASE_SUCCESS)
	{
		app_perror("...error: unable to create timer heap", status);
		err = -20;
		goto on_return;
	}

	/* Set recursive lock for the timer heap. */
	status = block_create_recursive_mutex( pool, "lock", &timer_lock);
	if (status != BASE_SUCCESS) {
		app_perror("...error: unable to create lock", status);
		err = -30;
		goto on_return;
	}
	btimer_heap_set_lock(timer, timer_lock, BASE_TRUE);

	/* Create group locks for the timer entry. */
	if (ST_ENTRY_GROUP_LOCK_COUNT)
	{
		grp_locks = (bgrp_lock_t**)bpool_calloc(pool, ST_ENTRY_GROUP_LOCK_COUNT, sizeof(bgrp_lock_t*));
	}
	
	for (i=0; i<ST_ENTRY_GROUP_LOCK_COUNT; ++i)
	{    
		status = bgrp_lock_create(pool, NULL, &grp_locks[i]);
		if (status != BASE_SUCCESS)
		{
			app_perror("...error: unable to create group lock", status);
			err = -40;
			goto on_return;
		}
		
		bgrp_lock_add_ref(grp_locks[i]);
	}

	/* Create and schedule timer entries */
	entries = (btimer_entry*)bpool_calloc(pool, ST_ENTRY_COUNT, sizeof(*entries));
	if (!entries)
	{
		err = -50;
		goto on_return;
	}

	for (i=0; i<ST_ENTRY_COUNT; ++i)
	{
		bgrp_lock_t *grp_lock = NULL;

		if (ST_ENTRY_GROUP_LOCK_COUNT && brand() % 10)
		{/* About 90% of entries should have group lock */
			grp_lock = grp_locks[brand() % ST_ENTRY_GROUP_LOCK_COUNT];
		}

		btimer_entry_init(&entries[i], 0, grp_lock, &st_entry_callback);

		status = st_schedule_entry(timer, &entries[i]);
		if (status != BASE_SUCCESS)
		{
			app_perror("...error: unable to schedule entry", status);
			err = -60;
			goto on_return;
		}
	}

	tparam.stopping = BASE_FALSE;
	tparam.timer = timer;
	tparam.entries = entries;
	status = batomic_create(pool, -1, &tparam.idx);
	if (status != BASE_SUCCESS)
	{
		app_perror("...error: unable to create atomic", status);
		err = -70;
		goto on_return;
	}

	/* Start poll worker threads */
	if (ST_POLL_THREAD_COUNT)
	{
		poll_threads = (bthread_t**)bpool_calloc(pool, ST_POLL_THREAD_COUNT, sizeof(bthread_t*));
	}
	
	for (i=0; i<ST_POLL_THREAD_COUNT; ++i)
	{
		status = bthreadCreate( pool, "poll", &poll_worker, &tparam, 0, 0, &poll_threads[i]);
		if (status != BASE_SUCCESS)
		{
			app_perror("...error: unable to create poll thread", status);
			err = -80;
			goto on_return;
		}
	}

	/* Start cancel worker threads */
	if (ST_CANCEL_THREAD_COUNT)
	{
		cancel_threads = (bthread_t**)bpool_calloc(pool, ST_CANCEL_THREAD_COUNT, sizeof(bthread_t*));
	}
	
	for (i=0; i<ST_CANCEL_THREAD_COUNT; ++i)
	{
		status = bthreadCreate( pool, "cancel", &cancel_worker, &tparam, 0, 0, &cancel_threads[i]);
		if (status != BASE_SUCCESS)
		{
			app_perror("...error: unable to create cancel thread", status);
			err = -90;
			goto on_return;
		}
	}

	/* Wait 30s, then stop all threads */
	bthreadSleepMs(30*1000);

on_return:
	BASE_INFO("...Cleaning up resources");
	tparam.stopping = BASE_TRUE;

	for (i=0; i<ST_POLL_THREAD_COUNT; ++i)
	{
		if (!poll_threads[i])
			continue;
		
		bthreadJoin(poll_threads[i]);
		bthreadDestroy(poll_threads[i]);
	}

	for (i=0; i<ST_CANCEL_THREAD_COUNT; ++i)
	{
		if (!cancel_threads[i])
			continue;
		
		bthreadJoin(cancel_threads[i]);
		bthreadDestroy(cancel_threads[i]);
	}

	for (i=0; i<ST_POLL_THREAD_COUNT+ST_CANCEL_THREAD_COUNT; ++i)
	{
		BASE_INFO("...Thread #%d (%s) executed %d entries",	i, (tparam.stat[i].is_poll? "poll":"cancel"), tparam.stat[i].cnt);
	}

	for (i=0; i<ST_ENTRY_COUNT; ++i)
	{
		btimer_heap_cancel_if_active(timer, &entries[i], 10);
	}

	for (i=0; i<ST_ENTRY_GROUP_LOCK_COUNT; ++i)
	{
		/* Ref count must be equal to 1 */
		if (bgrp_lock_get_ref(grp_locks[i]) != 1)
		{
			bassert(!"Group lock ref count must be equal to 1");
			if (!err)
				err = -100;
		}

		bgrp_lock_dec_ref(grp_locks[i]);
	}

	if (timer)
		btimer_heap_destroy(timer);

	if (tparam.idx)
		batomic_destroy(tparam.idx);

	bpool_safe_release(&pool);

	return err;
}

int testBaseTimer()
{
	int rc = 0;

//	rc = _testTimerHeap();
	if (rc != 0)
		return rc;

	rc = _timerStressTest();
	if (rc != 0)
		return rc;

	return 0;
}

#else
/* To prevent warning about "translation unit is empty" when this test is disabled */
int dummy_testBaseTimer;
#endif


