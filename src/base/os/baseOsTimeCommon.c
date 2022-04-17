/* 
 *
 */
#include <baseOs.h>
#include <compat/time.h>
#include <baseErrno.h>


///////////////////////////////////////////////////////////////////////////////

#if !defined(BASE_WIN32) || BASE_WIN32==0

bstatus_t btime_decode(const btime_val *tv, bparsed_time *pt)
{
    struct tm local_time;

    BASE_CHECK_STACK();

#if defined(BASE_HAS_LOCALTIME_R) && BASE_HAS_LOCALTIME_R != 0
    localtime_r((time_t*)&tv->sec, &local_time);
#else
    /* localtime() is NOT thread-safe. */
    local_time = *localtime((time_t*)&tv->sec);
#endif

    pt->year = local_time.tm_year+1900;
    pt->mon = local_time.tm_mon;
    pt->day = local_time.tm_mday;
    pt->hour = local_time.tm_hour;
    pt->min = local_time.tm_min;
    pt->sec = local_time.tm_sec;
    pt->wday = local_time.tm_wday;
    pt->msec = tv->msec;

    return BASE_SUCCESS;
}

/**
 * Encode parsed time to time value.
 */
bstatus_t btime_encode(const bparsed_time *pt, btime_val *tv)
{
    struct tm local_time;

    local_time.tm_year = pt->year-1900;
    local_time.tm_mon = pt->mon;
    local_time.tm_mday = pt->day;
    local_time.tm_hour = pt->hour;
    local_time.tm_min = pt->min;
    local_time.tm_sec = pt->sec;
    local_time.tm_isdst = 0;
    
    tv->sec = mktime(&local_time);
    tv->msec = pt->msec;

    return BASE_SUCCESS;
}

#endif /* !BASE_WIN32 */


/**
 * Convert local time to GMT.
 */
bstatus_t btime_local_to_gmt(btime_val *tv)
{
    BASE_UNUSED_ARG(tv);
    return BASE_EBUG;
}

/**
 * Convert GMT to local time.
 */
bstatus_t btime_gmt_to_local(btime_val *tv)
{
    BASE_UNUSED_ARG(tv);
    return BASE_EBUG;
}


