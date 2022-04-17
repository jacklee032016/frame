/*
 *
 */

#ifndef __CMN_TIME_H__
#define __CMN_TIME_H__

#include <sys/time.h>
#include <limits.h>
#include <string.h>
#include <stdbool.h>

typedef struct timeval timeval_t;

/* Global vars */
extern timeval_t time_now;

#ifdef _TIMER_CHECK_
extern bool do_timer_check;
#endif

/* Some defines */
#define TIMER_HZ				1000000
#define TIMER_HZ_FLOAT			1000000.0F
#define TIMER_HZ_DOUBLE		((double)1000000.0F)
#define TIMER_CENTI_HZ			10000
#define TIMER_MAX_SEC			1000U
#define TIMER_NEVER				ULONG_MAX	/* Used with time intervals in TIMER_HZ units */
#define TIMER_DISABLED			LONG_MIN	/* Value in timeval_t tv_sec */

#define	NSEC_PER_SEC			1000000000	/* nanoseconds per second. Avoids typos by having a definition */


#ifdef _TIMER_CHECK_
#define cmnTimeNow()	cmnTimeNowDebug((__FILE__), (__func__), (__LINE__))
#define cmnTimeSetNow()	cmnTimeSetNowDebug((__FILE__), (__func__), (__LINE__))
#endif

#define RB_TIMER_CMP(obj)					\
static inline int						\
obj##_timer_cmp(const obj##_t *r1, const obj##_t *r2)		\
{								\
	if (r1->sands.tv_sec == TIMER_DISABLED) {		\
		if (r2->sands.tv_sec == TIMER_DISABLED)		\
			return 0;				\
		return 1;					\
	}							\
								\
	if (r2->sands.tv_sec == TIMER_DISABLED)			\
		return -1;					\
								\
	if (r1->sands.tv_sec != r2->sands.tv_sec)		\
		return r1->sands.tv_sec - r2->sands.tv_sec;	\
								\
	return r1->sands.tv_usec - r2->sands.tv_usec;		\
}

/* timer sub from current time */
static inline timeval_t cmnTimeSubNow(timeval_t a)
{
	timersub(&a, &time_now, &a);

	return a;
}

/* timer add to current time */
static inline timeval_t cmnTimeAddNow(timeval_t a)
{
	timeradd(&time_now, &a, &a/* result*/);

	return a;
}

/* Returns true if time a + diff_hz < time_now */
static inline bool cmnTimeCmpNowDiff(timeval_t a, unsigned long diff_hz)
{
	timeval_t b = { .tv_sec = diff_hz / TIMER_HZ, .tv_usec = diff_hz % TIMER_HZ };

	timeradd(&b, &a, &b);

	return !!timercmp(&b, &time_now, <);
}

/* Return time as unsigned long */
static inline unsigned long cmnTimeLong(timeval_t a)
{
	return (unsigned long)a.tv_sec * TIMER_HZ + (unsigned long)a.tv_usec;
}

#ifdef _INCLUDE_UNUSED_CODE_
/* print timer value */
static inline void timer_dump(timeval_t a)
{
	printf("=> %lu (usecs)\n", timer_tol(a));
}
#endif

/* prototypes */
#ifdef _TIMER_CHECK_
extern timeval_t cmnTimeNowDebug(const char *, const char *, int);
extern timeval_t cmnTimeSetNowDebug(const char *, const char *, int);
#else
extern timeval_t cmnTimeNow(void);
extern timeval_t cmnTimeSetNow(void);
#endif
extern timeval_t cmnTimeAddLong(timeval_t, unsigned long);
extern timeval_t cmnTimeSubLong(timeval_t, unsigned long);

#endif

