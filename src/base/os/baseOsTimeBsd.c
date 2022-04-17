/* 
 *
 */
 
#include <baseOs.h>
#include <compat/time.h>

bstatus_t bgettimeofday(btime_val *tv)
{
	struct timeb tb;

	BASE_CHECK_STACK();

	ftime(&tb);
	tv->sec = tb.time;
	tv->msec = tb.millitm;
	return BASE_SUCCESS;
}

