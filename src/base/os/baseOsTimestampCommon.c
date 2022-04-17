/* 
 *
 */
#include <baseOs.h>
#include <compat/baseHighPrecision.h>

#if defined(BASE_HAS_HIGH_RES_TIMER) && BASE_HAS_HIGH_RES_TIMER != 0

#define U32MAX  (0xFFFFFFFFUL)
#define NANOSEC (1000000000UL)
#define USEC    (1000000UL)
#define MSEC    (1000)

#define u64tohighprec(u64)	((bhighprec_t)((bint64_t)(u64)))

static bhighprec_t get_elapsed( const btimestamp *start,
                                  const btimestamp *stop )
{
#if defined(BASE_HAS_INT64) && BASE_HAS_INT64!=0
    return u64tohighprec(stop->u64 - start->u64);
#else
    bhighprec_t elapsed_hi, elapsed_lo;

    elapsed_hi = stop->u32.hi - start->u32.hi;
    elapsed_lo = stop->u32.lo - start->u32.lo;

    /* elapsed_hi = elapsed_hi * U32MAX */
    bhighprec_mul(elapsed_hi, U32MAX);

    return elapsed_hi + elapsed_lo;
#endif
}

static bhighprec_t elapsed_msec( const btimestamp *start,
                                   const btimestamp *stop )
{
    btimestamp ts_freq;
    bhighprec_t freq, elapsed;

    if (bTimeStampGetFreq(&ts_freq) != BASE_SUCCESS)
        return 0;

    /* Convert frequency timestamp */
#if defined(BASE_HAS_INT64) && BASE_HAS_INT64!=0
    freq = u64tohighprec(ts_freq.u64);
#else
    freq = ts_freq.u32.hi;
    bhighprec_mul(freq, U32MAX);
    freq += ts_freq.u32.lo;
#endif

    /* Avoid division by zero. */
    if (freq == 0) freq = 1;

    /* Get elapsed time in cycles. */
    elapsed = get_elapsed(start, stop);

    /* usec = elapsed * MSEC / freq */
    bhighprec_mul(elapsed, MSEC);
    bhighprec_div(elapsed, freq);

    return elapsed;
}

static bhighprec_t elapsed_usec( const btimestamp *start,
                                   const btimestamp *stop )
{
    btimestamp ts_freq;
    bhighprec_t freq, elapsed;

    if (bTimeStampGetFreq(&ts_freq) != BASE_SUCCESS)
        return 0;

    /* Convert frequency timestamp */
#if defined(BASE_HAS_INT64) && BASE_HAS_INT64!=0
    freq = u64tohighprec(ts_freq.u64);
#else
    freq = ts_freq.u32.hi;
    bhighprec_mul(freq, U32MAX);
    freq += ts_freq.u32.lo;
#endif

    /* Avoid division by zero. */
    if (freq == 0) freq = 1;

    /* Get elapsed time in cycles. */
    elapsed = get_elapsed(start, stop);

    /* usec = elapsed * USEC / freq */
    bhighprec_mul(elapsed, USEC);
    bhighprec_div(elapsed, freq);

    return elapsed;
}

buint32_t belapsed_nanosec( const btimestamp *start,
                                        const btimestamp *stop )
{
    btimestamp ts_freq;
    bhighprec_t freq, elapsed;

    if (bTimeStampGetFreq(&ts_freq) != BASE_SUCCESS)
        return 0;

    /* Convert frequency timestamp */
#if defined(BASE_HAS_INT64) && BASE_HAS_INT64!=0
    freq = u64tohighprec(ts_freq.u64);
#else
    freq = ts_freq.u32.hi;
    bhighprec_mul(freq, U32MAX);
    freq += ts_freq.u32.lo;
#endif

    /* Avoid division by zero. */
    if (freq == 0) freq = 1;

    /* Get elapsed time in cycles. */
    elapsed = get_elapsed(start, stop);

    /* usec = elapsed * USEC / freq */
    bhighprec_mul(elapsed, NANOSEC);
    bhighprec_div(elapsed, freq);

    return (buint32_t)elapsed;
}

buint32_t belapsed_usec( const btimestamp *start,
                                     const btimestamp *stop )
{
    return (buint32_t)elapsed_usec(start, stop);
}

buint32_t belapsed_msec( const btimestamp *start,
                                     const btimestamp *stop )
{
    return (buint32_t)elapsed_msec(start, stop);
}

buint64_t belapsed_msec64(const btimestamp *start,
                                      const btimestamp *stop )
{
    return (buint64_t)elapsed_msec(start, stop);
}

btime_val belapsed_time( const btimestamp *start,
                                     const btimestamp *stop )
{
    bhighprec_t elapsed = elapsed_msec(start, stop);
    btime_val tv_elapsed;

    if (BASE_HIGHPREC_VALUE_IS_ZERO(elapsed)) {
        tv_elapsed.sec = tv_elapsed.msec = 0;
        return tv_elapsed;
    } else {
        bhighprec_t sec, msec;

        sec = elapsed;
        bhighprec_div(sec, MSEC);
        tv_elapsed.sec = (long)sec;

        msec = elapsed;
        bhighprec_mod(msec, MSEC);
        tv_elapsed.msec = (long)msec;

        return tv_elapsed;
    }
}

buint32_t belapsed_cycle( const btimestamp *start,
                                      const btimestamp *stop )
{
    return stop->u32.lo - start->u32.lo;
}

bstatus_t bgettickcount(btime_val *tv)
{
	btimestamp ts, start;
	bstatus_t status;

	if ((status = bTimeStampGet(&ts)) != BASE_SUCCESS)
		return status;

	bset_timestamp32(&start, 0, 0);
	*tv = belapsed_time(&start, &ts);

	return BASE_SUCCESS;
}

#endif

