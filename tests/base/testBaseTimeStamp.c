/* 
 *
 */
#include "testBaseTest.h"
#include <baseOs.h>
#include <baseLog.h>
#include <baseRand.h>

/**
 * This tests whether timestamp API works.
 * API tested:
 *  - bTimeStampGetFreq()
 *  - bTimeStampGet()
 *  - belapsed_usec()
 *  - BASE_LOG()
 */

#if INCLUDE_TIMESTAMP_TEST

static int _testTimestampAccuracy()
{
	btimestamp freq, t1, t2;
	btime_val tv1, tv2, tvtmp;
	bint64_t msec, tics;
	bint64_t diff;

	BASE_INFO("...testing frequency accuracy (pls wait)");

	bTimeStampGetFreq(&freq);

	/* Get the start time */
	bgettimeofday(&tvtmp);
	do
	{
		bgettimeofday(&tv1);
	}while (BASE_TIME_VAL_EQ(tvtmp, tv1));
	bTimeStampGet(&t1);

	/* Sleep for 10 seconds */
	bthreadSleepMs(10000);

	/* Get end time */
	bgettimeofday(&tvtmp);
	do {
		bgettimeofday(&tv2);
	} while (BASE_TIME_VAL_EQ(tvtmp, tv2));
	bTimeStampGet(&t2);

	/* Get the elapsed time */
	BASE_TIME_VAL_SUB(tv2, tv1);
	msec = BASE_TIME_VAL_MSEC(tv2);

	/* Check that the frequency match the elapsed time */
	tics = t2.u64 - t1.u64;
	diff = tics - (msec * freq.u64 / 1000);
	if (diff < 0)
		diff = -diff;

	/* Only allow 1 msec mismatch */
	if (diff > (bint64_t)(freq.u64 /1000))
	{
		BASE_ERROR("....error: timestamp drifted by %d usec after %d msec",  (buint32_t)(diff * 1000000 / freq.u64),  msec);
		return -2000;
		/* Otherwise just print warning if timestamp drifted by >1 usec */
	}
	else if (diff > (bint64_t)(freq.u64 / 1000000))
	{
		BASE_WARN("....warning: timestamp drifted by %d usec after %d msec", 	(buint32_t)(diff * 1000000 / freq.u64),  msec);
	}
	else
	{
		BASE_INFO("....good. Timestamp is accurate down to nearest usec.");
	}

	return 0;
}


int testBaseTimestamp(void)
{
	enum { CONSECUTIVE_LOOP = 100 };
	volatile unsigned i;
	btimestamp freq, t1, t2;
	btime_val tv1, tv2;
	unsigned elapsed;
	bstatus_t rc;

	BASE_INFO(TEST_LEVEL_ITEM"...Testing timestamp (high res time)");

	/* Get and display timestamp frequency. */
	if ((rc=bTimeStampGetFreq(&freq)) != BASE_SUCCESS)
	{
		app_perror("...ERROR: get timestamp freq", rc);
		return -1000;
	}

	BASE_INFO(TEST_LEVEL_CASE"frequency: hiword=%lu loword=%lu", freq.u32.hi, freq.u32.lo);


	/*
	* Check if consecutive readings should yield timestamp value that is bigger than previous value.
	* First we get the first timestamp.
	*/
	BASE_INFO(TEST_LEVEL_CASE"checking if time can run backwards (pls wait)..");
	rc = bTimeStampGet(&t1);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...ERROR: bTimeStampGet", rc);
		return -1001;
	}
	
	rc = bgettimeofday(&tv1);
	if (rc != BASE_SUCCESS)
	{
		app_perror("...ERROR: bgettimeofday", rc);
		return -1002;
	}
	
	for (i=0; i<CONSECUTIVE_LOOP; ++i)
	{
		bthreadSleepMs(brand() % 100);

		rc = bTimeStampGet(&t2);
		if (rc != BASE_SUCCESS)
		{
			app_perror("...ERROR: bTimeStampGet", rc);
			return -1003;
		}
		
		rc = bgettimeofday(&tv2);
		if (rc != BASE_SUCCESS)
		{
			app_perror("...ERROR: bgettimeofday", rc);
			return -1004;
		}

		/* compare t2 with t1, expecting t2 >= t1. */
		if (t2.u32.hi < t1.u32.hi ||(t2.u32.hi == t1.u32.hi && t2.u32.lo < t1.u32.lo))
		{
			BASE_ERROR( "...ERROR: timestamp run backwards!");
			return -1005;
		}

		/* compare tv2 with tv1, expecting tv2 >= tv1. */
		if (BASE_TIME_VAL_LT(tv2, tv1))
		{
			BASE_ERROR( "...ERROR: time run backwards!");
			return -1006;
		}
	}

	/* 
	* Simple test to time spending on some loop. 
	*/
	BASE_INFO( TEST_LEVEL_CASE"testing simple 1000000 loop");

	/* Mark start time. */
	if ((rc=bTimeStampGet(&t1)) != BASE_SUCCESS)
	{
		app_perror("....error: cat't get timestamp", rc);
		return -1010;
	}

	for (i=0; i<1000000; ++i)
	{/* Try to do something so that smart compilers wont remove this silly loop.*/
		null_func();
	}

	bthreadSleepMs(0);

	/* Mark end time. */
	bTimeStampGet(&t2);

	/* Get elapsed time in usec. */
	elapsed = belapsed_usec(&t1, &t2);
	BASE_INFO(TEST_LEVEL_RESULT"elapsed: %u usec", (unsigned)elapsed);

	/* See if elapsed time is "reasonable". 
	* This should be good even on 50Mhz embedded powerpc.
	*/
	if (elapsed < 1 || elapsed > 1000000)
	{
		BASE_ERROR( "....error: elapsed time outside window (%u, t1.u32.hi=%u, t1.u32.lo=%u, t2.u32.hi=%u, t2.u32.lo=%u)",
			elapsed, t1.u32.hi, t1.u32.lo, t2.u32.hi, t2.u32.lo);
		return -1030;
	}

	/* Testing time/timestamp accuracy */
	rc = _testTimestampAccuracy();
	if (rc != 0)
		return rc;

	return 0;
}


#else
int dummy_timestamp_test;
#endif

