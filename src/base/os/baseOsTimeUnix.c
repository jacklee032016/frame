/* 
 *
 */
#include <baseOs.h>
#include <baseErrno.h>
#include <compat/time.h>

#if defined(BASE_HAS_UNISTD_H) && BASE_HAS_UNISTD_H!=0
#    include <unistd.h>
#endif

#include <errno.h>

bstatus_t bgettimeofday(btime_val *p_tv)
{
	struct timeval the_time;
	int rc;

	BASE_CHECK_STACK();

	rc = gettimeofday(&the_time, NULL);
	if (rc != 0)
		return BASE_RETURN_OS_ERROR(bget_native_os_error());

	p_tv->sec = the_time.tv_sec;
	p_tv->msec = the_time.tv_usec / 1000;
	return BASE_SUCCESS;
}

