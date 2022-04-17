/* 
 *
 */
#include <baseOs.h>
#include <baseString.h>
#include <baseLog.h>
#include <windows.h>

#define SECS_TO_FT_MULT 10000000

static LARGE_INTEGER btime;

#if defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE
#   define WINCE_TIME
#endif

#ifdef WINCE_TIME
/* Note:
 *  In Windows CE/Windows Mobile platforms, the availability of milliseconds
 *  time resolution in SYSTEMTIME.wMilliseconds depends on the OEM, and most
 *  likely it won't be available. When it's not available, the 
 *  SYSTEMTIME.wMilliseconds will contain a constant arbitrary value.
 *
 *  Because of that, we need to emulate the milliseconds time resolution
 *  using QueryPerformanceCounter() (via bTimeStampGet() API). However 
 *  there is limitation on using this, i.e. the time returned by 
 *  bgettimeofday() may be off by up to plus/minus 999 msec (the second
 *  part will be correct, however the msec part may be off), because we're 
 *  not synchronizing the msec field with the change of value of the "second"
 *  field of the system time.
 *
 *  Also there is other caveat which need to be handled (and they are 
 *  handled by this implementation):
 *   - user may change system time, so bgettimeofday() needs to periodically
 *     checks if system time has changed. The period on which system time is
 *     checked is controlled by BASE_WINCE_TIME_CHECK_INTERVAL macro.
 */
static LARGE_INTEGER g_start_time;  /* Time gettimeofday() is first called  */
static btimestamp  g_start_tick;  /* TS gettimeofday() is first called  */
static btimestamp  g_last_update; /* Last time check_system_time() is 
				       called, to periodically synchronize
				       with up-to-date system time (in case
				       user changes system time).	    */
static buint64_t   g_update_period; /* Period (in TS) check_system_time()
				         should be called.		    */

/* Period on which check_system_time() is called, in seconds		    */
#ifndef BASE_WINCE_TIME_CHECK_INTERVAL
#   define BASE_WINCE_TIME_CHECK_INTERVAL (10)
#endif

#endif

#ifdef WINCE_TIME
static bstatus_t init_start_time(void)
{
    SYSTEMTIME st;
    FILETIME ft;
    btimestamp freq;
    bstatus_t status;

    GetLocalTime(&st);
    SystemTimeToFileTime(&st, &ft);

    g_start_time.LowPart = ft.dwLowDateTime;
    g_start_time.HighPart = ft.dwHighDateTime;
    g_start_time.QuadPart /= SECS_TO_FT_MULT;
    g_start_time.QuadPart -= btime.QuadPart;

    status = bTimeStampGet(&g_start_tick);
    if (status != BASE_SUCCESS)
	return status;

    g_last_update.u64 = g_start_tick.u64;

    status = bTimeStampGetFreq(&freq);
    if (status != BASE_SUCCESS)
	return status;

    g_update_period = BASE_WINCE_TIME_CHECK_INTERVAL * freq.u64;

    BASE_INFO("WinCE time (re)started");

    return BASE_SUCCESS;
}

static bstatus_t check_system_time(buint64_t ts_elapsed)
{
    enum { MIS = 5 };
    SYSTEMTIME st;
    FILETIME ft;
    LARGE_INTEGER cur, calc;
    DWORD diff;
    btimestamp freq;
    bstatus_t status;

    /* Get system's current time */
    GetLocalTime(&st);
    SystemTimeToFileTime(&st, &ft);
    
    cur.LowPart = ft.dwLowDateTime;
    cur.HighPart = ft.dwHighDateTime;
    cur.QuadPart /= SECS_TO_FT_MULT;
    cur.QuadPart -= btime.QuadPart;

    /* Get our calculated system time */
    status = bTimeStampGetFreq(&freq);
    if (status != BASE_SUCCESS)
	return status;

    calc.QuadPart = g_start_time.QuadPart + ts_elapsed / freq.u64;

    /* See the difference between calculated and actual system time */
    if (calc.QuadPart >= cur.QuadPart) {
	diff = (DWORD)(calc.QuadPart - cur.QuadPart);
    } else {
	diff = (DWORD)(cur.QuadPart - calc.QuadPart);
    }

    if (diff > MIS) {
	/* System time has changed */
	BASE_ERROR("WinCE system time changed detected (diff=%u)", diff);
	status = init_start_time();
    } else {
	status = BASE_SUCCESS;
    }

    return status;
}

#endif

// Find 1st Jan 1970 as a FILETIME 
static bstatus_t get_btime(void)
{
    SYSTEMTIME st;
    FILETIME ft;
    bstatus_t status = BASE_SUCCESS;

    memset(&st,0,sizeof(st));
    st.wYear=1970;
    st.wMonth=1;
    st.wDay=1;
    SystemTimeToFileTime(&st, &ft);
    
    btime.LowPart = ft.dwLowDateTime;
    btime.HighPart = ft.dwHighDateTime;
    btime.QuadPart /= SECS_TO_FT_MULT;

#ifdef WINCE_TIME
    benter_critical_section();
    status = init_start_time();
    bleave_critical_section();
#endif

    return status;
}

bstatus_t bgettimeofday(btime_val *tv)
{
#ifdef WINCE_TIME
    btimestamp tick;
    buint64_t msec_elapsed;
#else
    SYSTEMTIME st;
    FILETIME ft;
    LARGE_INTEGER li;
#endif
    bstatus_t status;

    if (btime.QuadPart == 0) {
	status = get_btime();
	if (status != BASE_SUCCESS)
	    return status;
    }

#ifdef WINCE_TIME
    do {
	status = bTimeStampGet(&tick);
	if (status != BASE_SUCCESS)
	    return status;

	if (tick.u64 - g_last_update.u64 >= g_update_period) {
	    benter_critical_section();
	    if (tick.u64 - g_last_update.u64 >= g_update_period) {
		g_last_update.u64 = tick.u64;
		check_system_time(tick.u64 - g_start_tick.u64);
	    }
	    bleave_critical_section();
	} else {
	    break;
	}
    } while (1);

    msec_elapsed = belapsed_msec64(&g_start_tick, &tick);

    tv->sec = (long)(g_start_time.QuadPart + msec_elapsed/1000);
    tv->msec = (long)(msec_elapsed % 1000);
#else
    /* Standard Win32 GetLocalTime */
    GetLocalTime(&st);
    SystemTimeToFileTime(&st, &ft);

    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    li.QuadPart /= SECS_TO_FT_MULT;
    li.QuadPart -= btime.QuadPart;

    tv->sec = li.LowPart;
    tv->msec = st.wMilliseconds;
#endif	/* WINCE_TIME */

    return BASE_SUCCESS;
}

bstatus_t btime_decode(const btime_val *tv, bparsed_time *pt)
{
    LARGE_INTEGER li;
    FILETIME ft;
    SYSTEMTIME st;

    li.QuadPart = tv->sec;
    li.QuadPart += btime.QuadPart;
    li.QuadPart *= SECS_TO_FT_MULT;

    ft.dwLowDateTime = li.LowPart;
    ft.dwHighDateTime = li.HighPart;
    FileTimeToSystemTime(&ft, &st);

    pt->year = st.wYear;
    pt->mon = st.wMonth-1;
    pt->day = st.wDay;
    pt->wday = st.wDayOfWeek;

    pt->hour = st.wHour;
    pt->min = st.wMinute;
    pt->sec = st.wSecond;
    pt->msec = tv->msec;

    return BASE_SUCCESS;
}

/**
 * Encode parsed time to time value.
 */
bstatus_t btime_encode(const bparsed_time *pt, btime_val *tv)
{
    SYSTEMTIME st;
    FILETIME ft;
    LARGE_INTEGER li;

    bbzero(&st, sizeof(st));
    st.wYear = (buint16_t) pt->year;
    st.wMonth = (buint16_t) (pt->mon + 1);
    st.wDay = (buint16_t) pt->day;
    st.wHour = (buint16_t) pt->hour;
    st.wMinute = (buint16_t) pt->min;
    st.wSecond = (buint16_t) pt->sec;
    st.wMilliseconds = (buint16_t) pt->msec;
    
    SystemTimeToFileTime(&st, &ft);

    li.LowPart = ft.dwLowDateTime;
    li.HighPart = ft.dwHighDateTime;
    li.QuadPart /= SECS_TO_FT_MULT;
    li.QuadPart -= btime.QuadPart;

    tv->sec = li.LowPart;
    tv->msec = st.wMilliseconds;

    return BASE_SUCCESS;
}

/**
 * Convert local time to GMT.
 */
bstatus_t btime_local_to_gmt(btime_val *tv);

/**
 * Convert GMT to local time.
 */
bstatus_t btime_gmt_to_local(btime_val *tv);


