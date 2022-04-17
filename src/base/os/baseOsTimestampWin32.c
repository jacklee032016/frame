/* 
 *
 */
#include <baseOs.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseLog.h>
#include <windows.h>


/////////////////////////////////////////////////////////////////////////////

#if defined(BASE_TIMESTAMP_USE_RDTSC) && BASE_TIMESTAMP_USE_RDTSC!=0 && \
    defined(BASE_M_I386) && BASE_M_I386 != 0 && \
    defined(BASE_HAS_PENTIUM) && BASE_HAS_PENTIUM!=0 && \
    defined(_MSC_VER)

/*
 * Use rdtsc to get the OS timestamp.
 */
static LONG CpuMhz;
static bint64_t CpuHz;
 
static bstatus_t GetCpuHz(void)
{
    HKEY key;
    LONG rc;
    DWORD size;

#if defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE!=0
    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		      L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
		      0, 0, &key);
#else
    rc = RegOpenKey( HKEY_LOCAL_MACHINE,
		     "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
		     &key);
#endif

    if (rc != ERROR_SUCCESS)
	return BASE_RETURN_OS_ERROR(rc);

    size = sizeof(CpuMhz);
    rc = RegQueryValueEx(key, "~MHz", NULL, NULL, (BYTE*)&CpuMhz, &size);
    RegCloseKey(key);

    if (rc != ERROR_SUCCESS) {
	return BASE_RETURN_OS_ERROR(rc);
    }

    CpuHz = CpuMhz;
    CpuHz = CpuHz * 1000000;

    return BASE_SUCCESS;
}

/* __int64 is nicely returned in EDX:EAX */
__declspec(naked) __int64 rdtsc() 
{
    __asm 
    {
	RDTSC
	RET
    }
}

bstatus_t bTimeStampGet(btimestamp *ts)
{
    ts->u64 = rdtsc();
    return BASE_SUCCESS;
}

bstatus_t bTimeStampGetFreq(btimestamp *freq)
{
	bstatus_t status;

TRACE();
	if (CpuHz == 0)
	{
		status = GetCpuHz();
		if (status != BASE_SUCCESS)
			return status;
	}

	freq->u64 = CpuHz;
	return BASE_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////

#elif defined(BASE_TIMESTAMP_WIN32_USE_SAFE_QPC) && \
         BASE_TIMESTAMP_WIN32_USE_SAFE_QPC!=0

/* Use safe QueryPerformanceCounter.
 * This implementation has some protection against bug in KB Q274323:
 *   Performance counter value may unexpectedly leap forward
 *   http://support.microsoft.com/default.aspx?scid=KB;EN-US;Q274323
 *
 * THIS SHOULD NOT BE USED YET AS IT DOESN'T HANDLE SYSTEM TIME
 * CHANGE.
 */

static btimestamp g_ts_freq;
static btimestamp g_ts_base;
static bint64_t   g_time_base;

bstatus_t bTimeStampGet(btimestamp *ts)
{
    enum { MAX_RETRY = 10 };
    unsigned i;


    /* bTimeStampGetFreq() must have been called before.
     * This is done when application called libBaseInit().
     */
    bassert(g_ts_freq.u64 != 0);

    /* Retry QueryPerformanceCounter() until we're sure that the
     * value returned makes sense.
     */
    i = 0;
    do {
	LARGE_INTEGER val;
	bint64_t counter64, time64, diff;
	btime_val time_now;

	/* Retrieve the counter */
	if (!QueryPerformanceCounter(&val))
	    return BASE_RETURN_OS_ERROR(GetLastError());

	/* Regardless of the goodness of the value, we should put
	 * the counter here, because normally application wouldn't
	 * check the error result of this function.
	 */
	ts->u64 = val.QuadPart;

	/* Retrieve time */
	bgettimeofday(&time_now);

	/* Get the counter elapsed time in miliseconds */
	counter64 = (val.QuadPart - g_ts_base.u64) * 1000 / g_ts_freq.u64;
	
	/* Get the time elapsed in miliseconds. 
	 * We don't want to use BASE_TIME_VAL_MSEC() since it's using
	 * 32bit calculation, which limits the maximum elapsed time
	 * to around 49 days only.
	 */
	time64 = time_now.sec;
	time64 = time64 * 1000 + time_now.msec;
	//time64 = GetTickCount();

	/* It's good if the difference between two clocks are within
	 * some compile time constant (default: 20ms, which to allow
	 * context switch happen between QueryPerformanceCounter and
	 * bgettimeofday()).
	 */
	diff = (time64 - g_time_base) - counter64;
	if (diff >= -20 && diff <= 20) {
	    /* It's good */
	    return BASE_SUCCESS;
	}

	++i;

    } while (i < MAX_RETRY);

    MTRACE( "QueryPerformanceCounter returned bad value");
    return BASE_ETIMEDOUT;
}

static bstatus_t init_performance_counter(void)
{
    LARGE_INTEGER val;
    btime_val time_base;
    bstatus_t status;

    /* Get the frequency */
    if (!QueryPerformanceFrequency(&val))
	return BASE_RETURN_OS_ERROR(GetLastError());

    g_ts_freq.u64 = val.QuadPart;

    /* Get the base timestamp */
    if (!QueryPerformanceCounter(&val))
	return BASE_RETURN_OS_ERROR(GetLastError());

    g_ts_base.u64 = val.QuadPart;


    /* Get the base time */
    status = bgettimeofday(&time_base);
    if (status != BASE_SUCCESS)
	return status;

    /* Convert time base to 64bit value in msec */
    g_time_base = time_base.sec;
    g_time_base  = g_time_base * 1000 + time_base.msec;
    //g_time_base = GetTickCount();

    return BASE_SUCCESS;
}

bstatus_t bTimeStampGetFreq(btimestamp *freq)
{
    if (g_ts_freq.u64 == 0) {
	enum { MAX_REPEAT = 10 };
	unsigned i;
	bstatus_t status;

	/* Make unellegant compiler happy */
	status = 0;

	/* Repeat initializing performance counter until we're sure
	 * the base timing is correct. It is possible that the system
	 * returns bad counter during this initialization!
	 */
	for (i=0; i<MAX_REPEAT; ++i) {

	    btimestamp dummy;

	    /* Init base time */
	    status = init_performance_counter();
	    if (status != BASE_SUCCESS)
		return status;

	    /* Try the base time */
	    status = bTimeStampGet(&dummy);
	    if (status == BASE_SUCCESS)
		break;
	}

	if (status != BASE_SUCCESS)
	    return status;
    }

    freq->u64 = g_ts_freq.u64;
    return BASE_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////

#else

/*
 * Use QueryPerformanceCounter and QueryPerformanceFrequency.
 * This should be the default implementation to be used on Windows.
 */
bstatus_t bTimeStampGet(btimestamp *ts)
{
    LARGE_INTEGER val;

    if (!QueryPerformanceCounter(&val))
	return BASE_RETURN_OS_ERROR(GetLastError());

    ts->u64 = val.QuadPart;
    return BASE_SUCCESS;
}

bstatus_t bTimeStampGetFreq(btimestamp *freq)
{
	LARGE_INTEGER val;
	
	/* current value of the performance counter, high precision, < 1us. Jack  */
	if (!QueryPerformanceFrequency(&val))
		return BASE_RETURN_OS_ERROR(GetLastError());

	freq->u64 = val.QuadPart;
	return BASE_SUCCESS;
}


#endif

