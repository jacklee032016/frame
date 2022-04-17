/* 
 *
 */
#include "testBaseTest.h"

/**
 * \page page_baselib_sleep_test Test: Sleep, Time, and Timestamp
 *
 * This file provides implementation of \b sleep_test().
 *
 * \section sleep_test_sec Scope of the Test
 *
 * This tests:
 *  - whether bthreadSleepMs() works.
 *  - whether bgettimeofday() works.
 *  - whether bTimeStampGet() and friends works.
 *
 * API tested:
 *  - bthreadSleepMs()
 *  - bgettimeofday()
 *  - BASE_TIME_VAL_SUB()
 *  - BASE_TIME_VAL_LTE()
 *  - bTimeStampGet()
 *  - bTimeStampGetFreq() (implicitly)
 *  - belapsed_time()
 *  - belapsed_usec()
 */

#if INCLUDE_SLEEP_TEST

#include <libBase.h>

static int simple_sleep_test(void)
{
    enum { COUNT = 10 };
    int i;
    bstatus_t rc;
    
    BASE_INFO( "..will write messages every 1 second:");
    
    for (i=0; i<COUNT; ++i) {
	btime_val tv;
	bparsed_time pt;

	rc = bthreadSleepMs(1000);
	if (rc != BASE_SUCCESS) {
	    app_perror("...error: bthreadSleepMs()", rc);
	    return -10;
	}

	rc = bgettimeofday(&tv);
	if (rc != BASE_SUCCESS) {
	    app_perror("...error: bgettimeofday()", rc);
	    return -11;
	}

	btime_decode(&tv, &pt);

	BASE_INFO(
		  "...%04d-%02d-%02d %02d:%02d:%02d.%03d",
		  pt.year, pt.mon, pt.day,
		  pt.hour, pt.min, pt.sec, pt.msec);

    }

    return 0;
}

static int sleep_duration_test(void)
{
    enum { MIS = 20};
    unsigned duration[] = { 2000, 1000, 500, 200, 100 };
    unsigned i;
    bstatus_t rc;

    BASE_INFO( "..running sleep duration test");

    /* Test bthreadSleepMs() and bgettimeofday() */
    for (i=0; i<BASE_ARRAY_SIZE(duration); ++i) {
        btime_val start, stop;
	buint32_t msec;

        /* Mark start of test. */
        rc = bgettimeofday(&start);
        if (rc != BASE_SUCCESS) {
            app_perror("...error: bgettimeofday()", rc);
            return -10;
        }

        /* Sleep */
        rc = bthreadSleepMs(duration[i]);
        if (rc != BASE_SUCCESS) {
            app_perror("...error: bthreadSleepMs()", rc);
            return -20;
        }

        /* Mark end of test. */
        rc = bgettimeofday(&stop);

        /* Calculate duration (store in stop). */
        BASE_TIME_VAL_SUB(stop, start);

	/* Convert to msec. */
	msec = BASE_TIME_VAL_MSEC(stop);

	/* Check if it's within range. */
	if (msec < duration[i] * (100-MIS)/100 ||
	    msec > duration[i] * (100+MIS)/100)
	{
	   BASE_ERROR("...error: slept for %d ms instead of %d ms "
		      "(outside %d%% err window)", msec, duration[i], MIS);
	    return -30;
	}
    }


    /* Test bthreadSleepMs() and bTimeStampGet() and friends */
    for (i=0; i<BASE_ARRAY_SIZE(duration); ++i) {
	btime_val t1, t2;
        btimestamp start, stop;
	buint32_t msec;

	bthreadSleepMs(0);

        /* Mark start of test. */
        rc = bTimeStampGet(&start);
        if (rc != BASE_SUCCESS) {
            app_perror("...error: bTimeStampGet()", rc);
            return -60;
        }

	/* ..also with gettimeofday() */
	bgettimeofday(&t1);

        /* Sleep */
        rc = bthreadSleepMs(duration[i]);
        if (rc != BASE_SUCCESS) {
            app_perror("...error: bthreadSleepMs()", rc);
            return -70;
        }

        /* Mark end of test. */
        bTimeStampGet(&stop);

	/* ..also with gettimeofday() */
	bgettimeofday(&t2);

	/* Compare t1 and t2. */
	if (BASE_TIME_VAL_LT(t2, t1)) {
	    BASE_ERROR( "...error: t2 is less than t1!!");
	    return -75;
	}

        /* Get elapsed time in msec */
        msec = belapsed_msec(&start, &stop);

	/* Check if it's within range. */
	if (msec < duration[i] * (100-MIS)/100 ||
	    msec > duration[i] * (100+MIS)/100)
	{
	    BASE_ERROR("...error: slept for %d ms instead of %d ms "
		      "(outside %d%% err window)",
		      msec, duration[i], MIS);
	    BASE_TIME_VAL_SUB(t2, t1);
	    BASE_ERROR(  "...info: gettimeofday() reported duration is "
		      "%d msec",
		      BASE_TIME_VAL_MSEC(t2));

	    return -76;
	}
    }

    /* All done. */
    return 0;
}

int sleep_test()
{
    int rc;

    rc = simple_sleep_test();
    if (rc != BASE_SUCCESS)
	return rc;

    rc = sleep_duration_test();
    if (rc != BASE_SUCCESS)
	return rc;

    return 0;
}

#else
int dummy_sleep_test;
#endif

