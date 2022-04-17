/* 
 *
 */
#include <baseOs.h>
#include <baseErrno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#if defined(BASE_HAS_UNISTD_H) && BASE_HAS_UNISTD_H != 0
#   include <unistd.h>

#   if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0 && \
       defined(_POSIX_MONOTONIC_CLOCK)
#       define USE_POSIX_TIMERS 1
#   endif

#endif

#if defined(BASE_HAS_PENTIUM) && BASE_HAS_PENTIUM!=0 && \
    defined(BASE_TIMESTAMP_USE_RDTSC) && BASE_TIMESTAMP_USE_RDTSC!=0 && \
    defined(BASE_M_I386) && BASE_M_I386!=0 && \
    defined(BASE_LINUX) && BASE_LINUX!=0
static int machine_speed_mhz;
static btimestamp machine_speed;

static __inline__ unsigned long long int rdtsc()
{
    unsigned long long int x;
    __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

/* Determine machine's CPU MHz to get the counter's frequency.
 */
static int get_machine_speed_mhz()
{
    FILE *strm;
    char buf[512];
    int len;
    char *pos, *end;
	
    BASE_CHECK_STACK();
	
    /* Open /proc/cpuinfo and read the file */
    strm = fopen("/proc/cpuinfo", "r");
    if (!strm)
        return -1;
    len = fread(buf, 1, sizeof(buf), strm);
    fclose(strm);
    if (len < 1) {
        return -1;
    }
    buf[len] = '\0';

    /* Locate the MHz digit. */
    pos = strstr(buf, "cpu MHz");
    if (!pos)
        return -1;
    pos = strchr(pos, ':');
    if (!pos)
        return -1;
    end = (pos += 2);
    while (isdigit(*end)) ++end;
    *end = '\0';

    /* Return the Mhz part, and give it a +1. */
    return atoi(pos)+1;
}

bstatus_t bTimeStampGet(btimestamp *ts)
{
    if (machine_speed_mhz == 0) {
	machine_speed_mhz = get_machine_speed_mhz();
	if (machine_speed_mhz > 0) {
	    machine_speed.u64 = machine_speed_mhz * 1000000.0;
	}
    }
    
    if (machine_speed_mhz == -1) {
	ts->u64 = 0;
	return -1;
    } 
    ts->u64 = rdtsc();
    return 0;
}

bstatus_t bTimeStampGetFreq(btimestamp *freq)
{
	if (machine_speed_mhz == 0)
	{
		machine_speed_mhz = get_machine_speed_mhz();
		if (machine_speed_mhz > 0)
		{
			machine_speed.u64 = machine_speed_mhz * 1000000.0;
		}
	}

	if (machine_speed_mhz == -1)
	{
		freq->u64 = 1;	/* return 1 to prevent division by zero in apps. */
		return -1;
	} 

	freq->u64 = machine_speed.u64;
	return 0;
}

#elif defined(BASE_DARWINOS) && BASE_DARWINOS != 0

/* SYSTEM_CLOCK will stop when the device is in deep sleep, so we use
 * KERN_BOOTTIME instead. 
 * See ticket #2140 for more details.
 */
#define USE_KERN_BOOTTIME 1

#if USE_KERN_BOOTTIME
#   include <sys/sysctl.h>
#else
#   include <mach/mach.h>
#   include <mach/clock.h>
#   include <errno.h>
#endif

#ifndef NSEC_PER_SEC
#	define NSEC_PER_SEC	1000000000
#endif

#if USE_KERN_BOOTTIME
static int64_t get_boottime()
{
    struct timeval boottime;
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
    size_t size = sizeof(boottime);
    int rc;

    rc = sysctl(mib, 2, &boottime, &size, NULL, 0);
    if (rc != 0)
      return 0;

    return (int64_t)boottime.tv_sec * 1000000 + (int64_t)boottime.tv_usec;
}
#endif

bstatus_t bTimeStampGet(btimestamp *ts)
{
#if USE_KERN_BOOTTIME
    int64_t before_now, after_now;
    struct timeval now;

    after_now = get_boottime();
    do {
        before_now = after_now;
        gettimeofday(&now, NULL);
        after_now = get_boottime();
    } while (after_now != before_now);

    ts->u64 = (int64_t)now.tv_sec * 1000000 + (int64_t)now.tv_usec;
    ts->u64 -= before_now;
    ts->u64 *= 1000;
#else
    mach_timespec_t tp;
    int ret;
    clock_serv_t serv;

    ret = host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &serv);
    if (ret != KERN_SUCCESS) {
	return BASE_RETURN_OS_ERROR(EINVAL);
    }

    ret = clock_get_time(serv, &tp);
    if (ret != KERN_SUCCESS) {
	return BASE_RETURN_OS_ERROR(EINVAL);
    }

    ts->u64 = tp.tv_sec;
    ts->u64 *= NSEC_PER_SEC;
    ts->u64 += tp.tv_nsec;
#endif

    return BASE_SUCCESS;
}

bstatus_t bTimeStampGetFreq(btimestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = NSEC_PER_SEC;

    return BASE_SUCCESS;
}

#elif defined(__ANDROID__)

#include <errno.h>
#include <time.h>

#if defined(BASE_HAS_ANDROID_ALARM_H) && BASE_HAS_ANDROID_ALARM_H != 0
#  include <linux/android_alarm.h>
#  include <fcntl.h>
#endif

#define NSEC_PER_SEC	1000000000

#if defined(ANDROID_ALARM_GET_TIME)
static int s_alarm_fd = -1;

void close_alarm_fd(void *data)
{
	if (s_alarm_fd != -1)
		close(s_alarm_fd);
	s_alarm_fd = -1;
}
#endif

bstatus_t bTimeStampGet(btimestamp *ts)
{
	struct timespec tp;
	int err = -1;

#if defined(ANDROID_ALARM_GET_TIME)
	if (s_alarm_fd == -1)
	{
		int fd = open("/dev/alarm", O_RDONLY);
		if (fd >= 0)
		{
			s_alarm_fd = fd;
			libBaseAtExit(&close_alarm_fd, "AlarmExit", NULL);
		}
	}

	if (s_alarm_fd != -1)
	{
		err = ioctl(s_alarm_fd, ANDROID_ALARM_GET_TIME(ANDROID_ALARM_ELAPSED_REALTIME), &tp);
	}
#elif defined(CLOCK_BOOTTIME)
	err = clock_gettime(CLOCK_BOOTTIME, &tp);
#endif

	if (err != 0) {
		/* Fallback to CLOCK_MONOTONIC if /dev/alarm is not found, or
		* getting ANDROID_ALARM_ELAPSED_REALTIME fails, or 
		* CLOCK_BOOTTIME fails.
		*/
		err = clock_gettime(CLOCK_MONOTONIC, &tp);
	}

	if (err != 0) {
		return BASE_RETURN_OS_ERROR(bget_native_os_error());
	}

	ts->u64 = tp.tv_sec;
	ts->u64 *= NSEC_PER_SEC;
	ts->u64 += tp.tv_nsec;

	return BASE_SUCCESS;
}

bstatus_t bTimeStampGetFreq(btimestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = NSEC_PER_SEC;

    return BASE_SUCCESS;
}

#elif defined(USE_POSIX_TIMERS) && USE_POSIX_TIMERS != 0
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define NSEC_PER_SEC	1000000000

bstatus_t bTimeStampGet(btimestamp *ts)
{
	struct timespec tp;

	if (clock_gettime(CLOCK_MONOTONIC, &tp) != 0)
	{
		return BASE_RETURN_OS_ERROR(bget_native_os_error());
	}

	ts->u64 = tp.tv_sec;
	ts->u64 *= NSEC_PER_SEC;
	ts->u64 += tp.tv_nsec;

	return BASE_SUCCESS;
}

bstatus_t bTimeStampGetFreq(btimestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = NSEC_PER_SEC;

    return BASE_SUCCESS;
}

#else
#include <sys/time.h>
#include <errno.h>

#define USEC_PER_SEC	1000000

bstatus_t bTimeStampGet(btimestamp *ts)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) != 0)
	{
		return BASE_RETURN_OS_ERROR(bget_native_os_error());
	}

	ts->u64 = tv.tv_sec;
	ts->u64 *= USEC_PER_SEC;
	ts->u64 += tv.tv_usec;

	return BASE_SUCCESS;
}

bstatus_t bTimeStampGetFreq(btimestamp *freq)
{
	freq->u32.hi = 0;
	freq->u32.lo = USEC_PER_SEC;

	return BASE_SUCCESS;
}

#endif
