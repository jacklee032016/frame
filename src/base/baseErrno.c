/* 
 *
 */
#include <baseErrno.h>
#include <baseLog.h>
#include <baseString.h>
#include <compat/string.h>
#include <compat/stdarg.h>
#include <baseAssert.h>

BASE_BEGIN_DECL

/* Prototype for platform specific error message, which will be defined in separate file. */
int platform_strerror(bos_err_type code, char *buf, bsize_t bufsize );
BASE_END_DECL

#ifndef BASE_MAX_ERR_MSG_HANDLER
#	define BASE_MAX_ERR_MSG_HANDLER   10
#endif

/* Error message handler. */
static unsigned err_msg_hnd_cnt;
static struct _err_msg_hnd
{
	bstatus_t	    begin;
	bstatus_t	    end;
	bstr_t	  (*strerror)(bstatus_t, char*, bsize_t);
} _errMsgHnds[BASE_MAX_ERR_MSG_HANDLER];

/* own error codes/messages */
#if defined(BASE_HAS_ERROR_STRING) && BASE_HAS_ERROR_STRING!=0

static const struct 
{
	int code;
	const char *msg;
}_baseLibErrStrs[] = 
{
    BASE_BUILD_ERR(BASE_EUNKNOWN,      "Unknown Error" ),
    BASE_BUILD_ERR(BASE_EPENDING,      "Pending operation" ),
    BASE_BUILD_ERR(BASE_ETOOMANYCONN,  "Too many connecting sockets" ),
    BASE_BUILD_ERR(BASE_EINVAL,        "Invalid value or argument" ),
    BASE_BUILD_ERR(BASE_ENAMETOOLONG,  "Name too long" ),
    BASE_BUILD_ERR(BASE_ENOTFOUND,     "Not found" ),
    BASE_BUILD_ERR(BASE_ENOMEM,        "Not enough memory" ),
    BASE_BUILD_ERR(BASE_EBUG,          "BUG DETECTED!" ),
    BASE_BUILD_ERR(BASE_ETIMEDOUT,     "Operation timed out" ),
    BASE_BUILD_ERR(BASE_ETOOMANY,      "Too many objects of the specified type"),
    BASE_BUILD_ERR(BASE_EBUSY,         "Object is busy"),
    BASE_BUILD_ERR(BASE_ENOTSUP,	   "Option/operation is not supported"),
    BASE_BUILD_ERR(BASE_EINVALIDOP,	   "Invalid operation"),
    BASE_BUILD_ERR(BASE_ECANCELLED,    "Operation cancelled"),
    BASE_BUILD_ERR(BASE_EEXISTS,       "Object already exists" ),
    BASE_BUILD_ERR(BASE_EEOF,	   "End of file" ),
    BASE_BUILD_ERR(BASE_ETOOBIG,	   "Size is too big"),
    BASE_BUILD_ERR(BASE_ERESOLVE,	   "gethostbyname() has returned error"),
    BASE_BUILD_ERR(BASE_ETOOSMALL,	   "Size is too short"),
    BASE_BUILD_ERR(BASE_EIGNORED,	   "Ignored"),
    BASE_BUILD_ERR(BASE_EIPV6NOTSUP,   "IPv6 is not supported"),
    BASE_BUILD_ERR(BASE_EAFNOTSUP,	   "Unsupported address family"),
    BASE_BUILD_ERR(BASE_EGONE,	   "Object no longer exists"),
    BASE_BUILD_ERR(BASE_ESOCKETSTOP,   "Socket is in bad state")
};
#endif	/* BASE_HAS_ERROR_STRING */


/*
 * Retrieve message string for lib's own error code.
 */
static int _baseLibError(bstatus_t code, char *buf, bsize_t size)
{
	int len;

#if defined(BASE_HAS_ERROR_STRING) && BASE_HAS_ERROR_STRING!=0
	unsigned i;

	for (i=0; i<sizeof(_baseLibErrStrs)/sizeof(_baseLibErrStrs[0]); ++i)
	{
		if (_baseLibErrStrs[i].code == code)
		{
			bsize_t len2 = bansi_strlen(_baseLibErrStrs[i].msg);
			if (len2 >= size) len2 = size-1;
				bmemcpy(buf, _baseLibErrStrs[i].msg, len2);
			buf[len2] = '\0';
			return (int)len2;
		}
	}
#endif

	len = bansi_snprintf( buf, size, "Unknown extLib error %d", code);
	if (len < 1 || len >= (int)size)
		len = (int)(size - 1);
	return len;
}

#define IN_RANGE(val,start,end)	    ((val)>=(start) && (val)<(end))

/* Register strerror handle. */
bstatus_t bregister_strerror( bstatus_t start, bstatus_t space, berror_callback f)
{
	unsigned i;

	/* Check arguments. */
	BASE_ASSERT_RETURN(start && space && f, BASE_EINVAL);

	/* Check if there aren't too many handlers registered. */
	BASE_ASSERT_RETURN(err_msg_hnd_cnt < BASE_ARRAY_SIZE(_errMsgHnds), BASE_ETOOMANY);

	/* Start error must be greater than BASE_ERRNO_START_USER */
	BASE_ASSERT_RETURN(start >= BASE_ERRNO_START_USER, BASE_EEXISTS);

	/* Check that no existing handler has covered the specified range. */
	for (i=0; i<err_msg_hnd_cnt; ++i)
	{
		if (IN_RANGE(start, _errMsgHnds[i].begin, _errMsgHnds[i].end) ||	IN_RANGE(start+space-1, _errMsgHnds[i].begin, _errMsgHnds[i].end))
		{
			if (_errMsgHnds[i].begin == start && _errMsgHnds[i].end == (start+space) &&_errMsgHnds[i].strerror == f)
			{/* The same range and handler has already been registered */
				return BASE_SUCCESS;
			}

			return BASE_EEXISTS;
		}
	}

	/* Register the handler. */
	_errMsgHnds[err_msg_hnd_cnt].begin = start;
	_errMsgHnds[err_msg_hnd_cnt].end = start + space;
	_errMsgHnds[err_msg_hnd_cnt].strerror = f;

	++err_msg_hnd_cnt;

	return BASE_SUCCESS;
}

/* Internal  function called by libBaseShutdown() to clear error handlers */
void berrno_clear_handlers(void)
{
	err_msg_hnd_cnt = 0;
	bbzero(_errMsgHnds, sizeof(_errMsgHnds));
}


/*
 * extStrError()
 */
bstr_t extStrError( bstatus_t statcode, char *buf, bsize_t bufsize )
{
	int len = -1;
	bstr_t errstr;

	bassert(buf && bufsize);

	if (statcode == BASE_SUCCESS)
	{
		len = bansi_snprintf( buf, bufsize, "Success");
	}
	else if (statcode < BASE_ERRNO_START + BASE_ERRNO_SPACE_SIZE)
	{
		len = bansi_snprintf( buf, bufsize, "Unknown error %d", statcode);
	}
	else if (statcode < BASE_ERRNO_START_STATUS + BASE_ERRNO_SPACE_SIZE)
	{
		len = _baseLibError(statcode, buf, bufsize);
	}
	else if (statcode < BASE_ERRNO_START_SYS + BASE_ERRNO_SPACE_SIZE)
	{
		len = platform_strerror(BASE_STATUS_TO_OS(statcode), buf, bufsize);
	}
	else
	{
		unsigned i;

		/* Find user handler to get the error message. */
		for (i=0; i<err_msg_hnd_cnt; ++i)
		{
			if (IN_RANGE(statcode, _errMsgHnds[i].begin, _errMsgHnds[i].end))
			{
				return (*_errMsgHnds[i].strerror)(statcode, buf, bufsize);
			}
		}

		/* Handler not found! */
		len = bansi_snprintf( buf, bufsize, "Unknown error %d", statcode);
	}

	if (len < 1 || len >= (int)bufsize)
	{
		len = (int)(bufsize - 1);
		buf[len] = '\0';
	}

	errstr.ptr = buf;
	errstr.slen = len;

	return errstr;
}

#if BASE_LOG_MAX_LEVEL >= 1
static void __invokeLog(const char *sender, int level, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	blog(sender, level, format, arg);
	va_end(arg);
}

static void _perrorImp(int log_level, const char *sender, bstatus_t status, const char *title_fmt, va_list marker)
{
	char titlebuf[BASE_PERROR_TITLE_BUF_SIZE];
	char errmsg[BASE_ERR_MSG_SIZE];
	int len;

	/* Build the title */
	len = bansi_vsnprintf(titlebuf, sizeof(titlebuf), title_fmt, marker);
	if (len < 0 || len >= (int)sizeof(titlebuf))
	bansi_strcpy(titlebuf, "Error");

	/* Get the error */
	extStrError(status, errmsg, sizeof(errmsg));

	/* Send to log */
	__invokeLog(sender, log_level, "%s: %s", titlebuf, errmsg);
}

void bperror(int log_level, const char *sender, bstatus_t status, const char *title_fmt, ...)
{
	va_list marker;
	va_start(marker, title_fmt);
	_perrorImp(log_level, sender, status, title_fmt, marker);
	va_end(marker);
}

void bperror_1(const char *sender, bstatus_t status, const char *title_fmt, ...)
{
	va_list marker;
	va_start(marker, title_fmt);
	_perrorImp(1, sender, status, title_fmt, marker);
	va_end(marker);
}

#else /* #if BASE_LOG_MAX_LEVEL >= 1 */
void bperror(int log_level, const char *sender, bstatus_t status, const char *title_fmt, ...)
{
}
#endif	/* #if BASE_LOG_MAX_LEVEL >= 1 */


#if BASE_LOG_MAX_LEVEL >= 2
void bperror_2(const char *sender, bstatus_t status, const char *title_fmt, ...)
{
	va_list marker;
	va_start(marker, title_fmt);
	_perrorImp(2, sender, status, title_fmt, marker);
	va_end(marker);
}
#endif

#if BASE_LOG_MAX_LEVEL >= 3
void bperror_3(const char *sender, bstatus_t status, const char *title_fmt, ...)
{
	va_list marker;
	va_start(marker, title_fmt);
	_perrorImp(3, sender, status, title_fmt, marker);
	va_end(marker);
}
#endif

#if BASE_LOG_MAX_LEVEL >= 4
void bperror_4(const char *sender, bstatus_t status, const char *title_fmt, ...)
{
	va_list marker;
	va_start(marker, title_fmt);
	_perrorImp(4, sender, status, title_fmt, marker);
	va_end(marker);
}
#endif

#if BASE_LOG_MAX_LEVEL >= 5
void bperror_5(const char *sender, bstatus_t status, const char *title_fmt, ...)
{
	va_list marker;
	va_start(marker, title_fmt);
	_perrorImp(5, sender, status, title_fmt, marker);
	va_end(marker);
}
#endif

#if BASE_LOG_MAX_LEVEL >= 6
void bperror_6(const char *sender, bstatus_t status, const char *title_fmt, ...)
{
	va_list marker;
	va_start(marker, title_fmt);
	_perrorImp(6, sender, status, title_fmt, marker);
	va_end(marker);
}
#endif

