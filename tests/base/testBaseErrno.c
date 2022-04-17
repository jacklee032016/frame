/* 
 *
 */
#include "testBaseTest.h"
#include <baseErrno.h>
#include <baseLog.h>
#include <baseCtype.h>
#include <compat/socket.h>
#include <baseString.h>

#if INCLUDE_ERRNO_TEST

#if (defined(BASE_WIN32) && BASE_WIN32 != 0) || \
    (defined(BASE_WIN64) && BASE_WIN64 != 0)
#include <windows.h>
#endif

#if defined(BASE_HAS_ERRNO_H) && BASE_HAS_ERRNO_H != 0
#include <errno.h>
#endif

static void trim_newlines(char *s)
{
	while (*s)
	{
		if (*s == '\r' || *s == '\n')
			*s = ' ';
		++s;
	}
}

int my_strncasecmp(const char *s1, const char *s2, int max_len)
{
	while (*s1 && *s2 && max_len > 0)
	{
		if (btolower(*s1) != btolower(*s2))
			return -1;
		
		++s1;
		++s2;
		--max_len;
	}
	return 0;
}

const char *my_stristr(const char *whole, const char *part)
{
	int part_len = (int)strlen(part);
	while (*whole)
	{
		if (my_strncasecmp(whole, part, part_len) == 0)
			return whole;
		++whole;
	}
	return NULL;
}

int testBaseErrno(void)
{
	enum { CUT = 6 };
	bstatus_t rc = 0;
	char errbuf[256];

	BASE_INFO("...errno test: check the msg carefully");

	BASE_UNUSED_ARG(rc);

	/* 
	* Windows platform error. 
	*/
#ifdef ERROR_INVALID_DATA
	rc = BASE_STATUS_FROM_OS(ERROR_INVALID_DATA);
	bset_os_error(rc);

	/* Whole */
	extStrError(rc, errbuf, sizeof(errbuf));
	trim_newlines(errbuf);
	BASE_INFO("...msg for rc=ERROR_INVALID_DATA: '%s'", errbuf);

	if (my_stristr(errbuf, "invalid") == NULL)
	{
		BASE_ERROR( "...error: expecting \"invalid\" string in the msg");
#ifndef BASE_WIN32_WINCE
		return -20;
#endif
	}

	/* Cut version. */
	extStrError(rc, errbuf, CUT);
	BASE_INFO("...msg for rc=ERROR_INVALID_DATA (cut): '%s'", errbuf);
#endif

	/*
	* Unix errors
	*/
#if defined(EINVAL) && !defined(BASE_SYMBIAN) && !defined(BASE_WIN32) && !defined(BASE_WIN64)
	rc = BASE_STATUS_FROM_OS(EINVAL);
	bset_os_error(rc);

	/* Whole */
	extStrError(rc, errbuf, sizeof(errbuf));
	trim_newlines(errbuf);
	BASE_INFO("...msg for rc=EINVAL: '%s'", errbuf);
	if (my_stristr(errbuf, "invalid") == NULL)
	{
		BASE_ERROR( "...error: expecting \"invalid\" string in the msg");
		return -30;
	}

	/* Cut */
	extStrError(rc, errbuf, CUT);
	BASE_INFO("...msg for rc=EINVAL (cut): '%s'", errbuf);
#endif

	/* Windows WSA errors 	*/
#ifdef WSAEINVAL
	rc = BASE_STATUS_FROM_OS(WSAEINVAL);
	bset_os_error(rc);

	/* Whole */
	extStrError(rc, errbuf, sizeof(errbuf));
	trim_newlines(errbuf);
	BASE_INFO("...msg for rc=WSAEINVAL: '%s'", errbuf);
	if (my_stristr(errbuf, "invalid") == NULL)
	{
		BASE_ERROR("...error: expecting \"invalid\" string in the msg");
		return -40;
	}

	/* Cut */
	extStrError(rc, errbuf, CUT);
	BASE_INFO("...msg for rc=WSAEINVAL (cut): '%s'", errbuf);
#endif

	extStrError(BASE_EBUG, errbuf, sizeof(errbuf));
	BASE_INFO("...msg for rc=BASE_EBUG: '%s'", errbuf);
	if (my_stristr(errbuf, "BUG") == NULL)
	{
		BASE_ERROR("...error: expecting \"BUG\" string in the msg");
		return -20;
	}

	extStrError(BASE_EBUG, errbuf, CUT);
	BASE_INFO("...msg for rc=BASE_EBUG, cut at %d chars: '%s'", CUT, errbuf);

	/* Perror */
	bperror(3, THIS_FILE, BASE_SUCCESS, "...testing %s", "bperror");
	BASE_PERROR(3,(THIS_FILE, BASE_SUCCESS, "...testing %s", "BASE_PERROR"));

	return 0;
}
#endif

