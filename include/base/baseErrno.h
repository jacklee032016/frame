/* 
 *
 */
#ifndef __BASE_ERRNO_H__
#define __BASE_ERRNO_H__

/**
 * @brief  Error Subsystem
 */
#include <_baseTypes.h>
#include <compat/errno.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup berrno Error Subsystem
 * @{
 *
 * The  Error Subsystem is a framework to unify all error codes
 * produced by all components into a single error space, and provide
 * uniform set of APIs to access them. With this framework, any error
 * codes are encoded as bstatus_t value. The framework is extensible,
 * application may register new error spaces to be recognized by
 * the framework.
 *
 * @section berrno_retval Return Values
 *
 * All functions that returns @a bstatus_t returns @a BASE_SUCCESS if the
 * operation was completed successfully, or non-zero value to indicate 
 * error. If the error came from operating system, then the native error
 * code is translated/folded into 's error namespace by using
 * #BASE_STATUS_FROM_OS() macro. The function will do this automatically
 * before returning the error to caller.
 *
 * @section err_services Retrieving and Displaying Error Messages
 *
 * The framework provides the following APIs to retrieve and/or display
 * error messages:
 *
 *   - #extStrError(): this is the base API to retrieve error string
 *      description for the specified bstatus_t error code.
 *
 *   - #BASE_PERROR() macro: use this macro similar to BASE_LOG to format
 *      an error message and display them to the log
 *
 *   - #bperror(): this function is similar to BASE_PERROR() but unlike
 *      #BASE_PERROR(), this function will always be included in the
 *      link process. Due to this reason, prefer to use #BASE_PERROR()
 *      if the application is concerned about the executable size.
 *
 * Application MUST NOT pass native error codes (such as error code from
 * functions like GetLastError() or errno) to  functions expecting
 * @a bstatus_t.
 *
 * @section err_baseending Extending the Error Space
 *
 * Application may register new error space to be recognized by the
 * framework by using #bregister_strerror(). Use the range started
 * from BASE_ERRNO_START_USER to avoid conflict with existing error
 * spaces.
 *
 */

/**
 * Guidelines on error message length.
 */
#define BASE_ERR_MSG_SIZE  80

/**
 * Buffer for title string of #BASE_PERROR().
 */
#ifndef BASE_PERROR_TITLE_BUF_SIZE
#   define BASE_PERROR_TITLE_BUF_SIZE	120
#endif


/**
 * Get the last platform error/status, folded into bstatus_t.
 * @return	OS dependent error code, folded into bstatus_t.
 * @remark	This function gets errno, or calls GetLastError() function and
 *		convert the code into bstatus_t with BASE_STATUS_FROM_OS. Do
 *		not call this for socket functions!
 * @see	bget_netos_error()
 */
bstatus_t bget_os_error(void);

/**
 * Set last error.
 * @param code	bstatus_t
 */
void bset_os_error(bstatus_t code);

/**
 * Get the last error from socket operations.
 * @return	Last socket error, folded into bstatus_t.
 */
bstatus_t bget_netos_error(void);

/**
 * Set error code.
 * @param code	bstatus_t.
 */
void bset_netos_error(bstatus_t code);


/**
 * Get the error message for the specified error code. The message
 * string will be NULL terminated.
 *
 * @param statcode  The error code.
 * @param buf	    Buffer to hold the error message string.
 * @param bufsize   Size of the buffer.
 *
 * @return	    The error message as NULL terminated string,
 *                  wrapped with bstr_t.
 */
bstr_t extStrError( bstatus_t statcode, 
			       char *buf, bsize_t bufsize);

/**
 * A utility macro to print error message pertaining to the specified error 
 * code to the log. This macro will construct the error message title 
 * according to the 'title_fmt' argument, and add the error string pertaining
 * to the error code after the title string. A colon (':') will be added 
 * automatically between the title and the error string.
 *
 * This function is similar to bperror() function, but has the advantage
 * that the function call can be omitted from the link process if the
 * log level argument is below BASE_LOG_MAX_LEVEL threshold.
 *
 * Note that the title string constructed from the title_fmt will be built on
 * a string buffer which size is BASE_PERROR_TITLE_BUF_SIZE, which normally is
 * allocated from the stack. By default this buffer size is small (around
 * 120 characters). Application MUST ensure that the constructed title string
 * will not exceed this limit, since not all platforms support truncating
 * the string.
 *
 * @see bperror()
 *
 * @param level	    The logging verbosity level, valid values are 0-6. Lower
 *		    number indicates higher importance, with level zero 
 *		    indicates fatal error. Only numeral argument is 
 *		    permitted (e.g. not variable).
 * @param arg	    Enclosed 'printf' like arguments, with the following
 *		    arguments:
 *		     - the sender (NULL terminated string),
 *		     - the error code (bstatus_t)
 *		     - the format string (title_fmt), and 
 *		     - optional variable number of arguments suitable for the 
 *		       format string.
 *
 * Sample:
 * \verbatim
   BASE_PERROR(2, (__FILE__, BASE_EBUSY, "Error making %s", "coffee"));
   \endverbatim
 * @hideinitializer
 */
#define BASE_PERROR(level,arg)	do { \
				    bperror_wrapper_##level(arg); \
				} while (0)

/**
 * A utility function to print error message pertaining to the specified error 
 * code to the log. This function will construct the error message title 
 * according to the 'title_fmt' argument, and add the error string pertaining
 * to the error code after the title string. A colon (':') will be added 
 * automatically between the title and the error string.
 *
 * Unlike the BASE_PERROR() macro, this function takes the \a log_level argument
 * as a normal argument, unlike in BASE_PERROR() where a numeral value must be
 * given. However this function will always be linked to the executable,
 * unlike BASE_PERROR() which can be omitted when the level is below the 
 * BASE_LOG_MAX_LEVEL.
 *
 * Note that the title string constructed from the title_fmt will be built on
 * a string buffer which size is BASE_PERROR_TITLE_BUF_SIZE, which normally is
 * allocated from the stack. By default this buffer size is small (around
 * 120 characters). Application MUST ensure that the constructed title string
 * will not exceed this limit, since not all platforms support truncating
 * the string.
 *
 * @see BASE_PERROR()
 */
void bperror(int log_level, const char *sender, bstatus_t status,
		        const char *title_fmt, ...);


/**
 * Type of callback to be specified in #bregister_strerror()
 *
 * @param e	    The error code to lookup.
 * @param msg	    Buffer to store the error message.
 * @param max	    Length of the buffer.
 *
 * @return	    The error string.
 */
typedef bstr_t (*berror_callback)(bstatus_t e, char *msg, bsize_t max);


/**
 * Register strerror message handler for the specified error space.
 * Application can register its own handler to supply the error message
 * for the specified error code range. This handler will be called
 * by #extStrError().
 *
 * @param start_code	The starting error code where the handler should
 *			be called to retrieve the error message.
 * @param err_space	The size of error space. The error code range then
 *			will fall in start_code to start_code+err_space-1
 *			range.
 * @param f		The handler to be called when #extStrError() is
 *			supplied with error code that falls into this range.
 *
 * @return		BASE_SUCCESS or the specified error code. The 
 *			registration may fail when the error space has been
 *			occupied by other handler, or when there are too many
 *			handlers registered to .
 */
bstatus_t bregister_strerror(bstatus_t start_code,
					  bstatus_t err_space,
					  berror_callback f);

/**
 * @hideinitializer
 * Return platform os error code folded into bstatus_t code. This is
 * the macro that is used throughout the library for all 's functions
 * that returns error from operating system. Application may override
 * this macro to reduce size (e.g. by defining it to always return 
 * #BASE_EUNKNOWN).
 *
 * Note:
 *  This macro MUST return non-zero value regardless whether zero is
 *  passed as the argument. The reason is to protect logic error when
 *  the operating system doesn't report error codes properly.
 *
 * @param os_code   Platform OS error code. This value may be evaluated
 *		    more than once.
 * @return	    The platform os error code folded into bstatus_t.
 */
#ifndef BASE_RETURN_OS_ERROR
#   define BASE_RETURN_OS_ERROR(os_code)   (os_code ? \
					    BASE_STATUS_FROM_OS(os_code) : -1)
#endif


/**
 * @hideinitializer
 * Fold a platform specific error into an bstatus_t code.
 *
 * @param e	The platform os error code.
 * @return	bstatus_t
 * @warning	Macro implementation; the syserr argument may be evaluated
 *		multiple times.
 */
#if BASE_NATIVE_ERR_POSITIVE
#   define BASE_STATUS_FROM_OS(e) (e == 0 ? BASE_SUCCESS : e + BASE_ERRNO_START_SYS)
#else
#   define BASE_STATUS_FROM_OS(e) (e == 0 ? BASE_SUCCESS : BASE_ERRNO_START_SYS - e)
#endif

/**
 * @hideinitializer
 * Fold an bstatus_t code back to the native platform defined error.
 *
 * @param e	The bstatus_t folded platform os error code.
 * @return	bos_err_type
 * @warning	macro implementation; the statcode argument may be evaluated
 *		multiple times.  If the statcode was not created by 
 *		bget_os_error or BASE_STATUS_FROM_OS, the results are undefined.
 */
#if BASE_NATIVE_ERR_POSITIVE
#   define BASE_STATUS_TO_OS(e) (e == 0 ? BASE_SUCCESS : e - BASE_ERRNO_START_SYS)
#else
#   define BASE_STATUS_TO_OS(e) (e == 0 ? BASE_SUCCESS : BASE_ERRNO_START_SYS - e)
#endif


/**
 * @defgroup berrnum 's Own Error Codes
 * @ingroup berrno
 * @{
 */

/**
 * Use this macro to generate error message text for your error code,
 * so that they look uniformly as the rest of the libraries.
 *
 * @param code	The error code
 * @param msg	The error test.
 */
#ifndef BASE_BUILD_ERR
#   define BASE_BUILD_ERR(code,msg) { code, msg " (" #code ")" }
#endif


/**
 * @hideinitializer
 * Unknown error has been reported.
 */
#define BASE_EUNKNOWN	    (BASE_ERRNO_START_STATUS + 1)	/* 70001 */
/**
 * @hideinitializer
 * The operation is pending and will be completed later.
 */
#define BASE_EPENDING	    (BASE_ERRNO_START_STATUS + 2)	/* 70002 */
/**
 * @hideinitializer
 * Too many connecting sockets.
 */
#define BASE_ETOOMANYCONN	    (BASE_ERRNO_START_STATUS + 3)	/* 70003 */
/**
 * @hideinitializer
 * Invalid argument.
 */
#define BASE_EINVAL	    (BASE_ERRNO_START_STATUS + 4)	/* 70004 */
/**
 * @hideinitializer
 * Name too long (eg. hostname too long).
 */
#define BASE_ENAMETOOLONG	    (BASE_ERRNO_START_STATUS + 5)	/* 70005 */
/**
 * @hideinitializer
 * Not found.
 */
#define BASE_ENOTFOUND	    (BASE_ERRNO_START_STATUS + 6)	/* 70006 */
/**
 * @hideinitializer
 * Not enough memory.
 */
#define BASE_ENOMEM	    (BASE_ERRNO_START_STATUS + 7)	/* 70007 */
/**
 * @hideinitializer
 * Bug detected!
 */
#define BASE_EBUG             (BASE_ERRNO_START_STATUS + 8)	/* 70008 */
/**
 * @hideinitializer
 * Operation timed out.
 */
#define BASE_ETIMEDOUT        (BASE_ERRNO_START_STATUS + 9)	/* 70009 */
/**
 * @hideinitializer
 * Too many objects.
 */
#define BASE_ETOOMANY         (BASE_ERRNO_START_STATUS + 10)/* 70010 */
/**
 * @hideinitializer
 * Object is busy.
 */
#define BASE_EBUSY            (BASE_ERRNO_START_STATUS + 11)/* 70011 */
/**
 * @hideinitializer
 * The specified option is not supported.
 */
#define BASE_ENOTSUP	    (BASE_ERRNO_START_STATUS + 12)/* 70012 */
/**
 * @hideinitializer
 * Invalid operation.
 */
#define BASE_EINVALIDOP	    (BASE_ERRNO_START_STATUS + 13)/* 70013 */
/**
 * @hideinitializer
 * Operation is cancelled.
 */
#define BASE_ECANCELLED	    (BASE_ERRNO_START_STATUS + 14)/* 70014 */
/**
 * @hideinitializer
 * Object already exists.
 */
#define BASE_EEXISTS          (BASE_ERRNO_START_STATUS + 15)/* 70015 */
/**
 * @hideinitializer
 * End of file.
 */
#define BASE_EEOF		    (BASE_ERRNO_START_STATUS + 16)/* 70016 */
/**
 * @hideinitializer
 * Size is too big.
 */
#define BASE_ETOOBIG	    (BASE_ERRNO_START_STATUS + 17)/* 70017 */
/**
 * @hideinitializer
 * Error in gethostbyname(). This is a generic error returned when
 * gethostbyname() has returned an error.
 */
#define BASE_ERESOLVE	    (BASE_ERRNO_START_STATUS + 18)/* 70018 */
/**
 * @hideinitializer
 * Size is too small.
 */
#define BASE_ETOOSMALL	    (BASE_ERRNO_START_STATUS + 19)/* 70019 */
/**
 * @hideinitializer
 * Ignored
 */
#define BASE_EIGNORED	    (BASE_ERRNO_START_STATUS + 20)/* 70020 */
/**
 * @hideinitializer
 * IPv6 is not supported
 */
#define BASE_EIPV6NOTSUP	    (BASE_ERRNO_START_STATUS + 21)/* 70021 */
/**
 * @hideinitializer
 * Unsupported address family
 */
#define BASE_EAFNOTSUP	    (BASE_ERRNO_START_STATUS + 22)/* 70022 */
/**
 * @hideinitializer
 * Object no longer exists
 */
#define BASE_EGONE	    (BASE_ERRNO_START_STATUS + 23)/* 70023 */
/**
 * @hideinitializer
 * Socket is stopped
 */
#define BASE_ESOCKETSTOP	    (BASE_ERRNO_START_STATUS + 24)/* 70024 */

/** @} */   /* berrnum */

/** @} */   /* berrno */


/**
 * BASE_ERRNO_START is where  specific error values start.
 */
#define BASE_ERRNO_START		20000

/**
 * BASE_ERRNO_SPACE_SIZE is the maximum number of errors in one of 
 * the error/status range below.
 */
#define BASE_ERRNO_SPACE_SIZE	50000

/**
 * BASE_ERRNO_START_STATUS is where  specific status codes start.
 * Effectively the error in this class would be 70000 - 119000.
 */
#define BASE_ERRNO_START_STATUS	(BASE_ERRNO_START + BASE_ERRNO_SPACE_SIZE)

/**
 * BASE_ERRNO_START_SYS converts platform specific error codes into
 * bstatus_t values.
 * Effectively the error in this class would be 120000 - 169000.
 */
#define BASE_ERRNO_START_SYS	(BASE_ERRNO_START_STATUS + BASE_ERRNO_SPACE_SIZE)

/**
 * BASE_ERRNO_START_USER are reserved for applications that use error
 * codes along with  codes.
 * Effectively the error in this class would be 170000 - 219000.
 */
#define BASE_ERRNO_START_USER	(BASE_ERRNO_START_SYS + BASE_ERRNO_SPACE_SIZE)


/*
 * Below are list of error spaces that have been taken so far:
 *  - PJSIP_ERRNO_START		(BASE_ERRNO_START_USER)
 *  - PJMEDIA_ERRNO_START	(BASE_ERRNO_START_USER + BASE_ERRNO_SPACE_SIZE)
 *  - PJSIP_SIMPLE_ERRNO_START	(BASE_ERRNO_START_USER + BASE_ERRNO_SPACE_SIZE*2)
 *  - UTIL_ERRNO_START	(BASE_ERRNO_START_USER + BASE_ERRNO_SPACE_SIZE*3)
 *  - PJNATH_ERRNO_START	(BASE_ERRNO_START_USER + BASE_ERRNO_SPACE_SIZE*4)
 *  - PJMEDIA_AUDIODEV_ERRNO_START (BASE_ERRNO_START_USER + BASE_ERRNO_SPACE_SIZE*5)
 *  - BASE_SSL_ERRNO_START	   (BASE_ERRNO_START_USER + BASE_ERRNO_SPACE_SIZE*6)
 *  - PJMEDIA_VIDEODEV_ERRNO_START (BASE_ERRNO_START_USER + BASE_ERRNO_SPACE_SIZE*7)
 */

/* Internal */
void berrno_clear_handlers(void);


/****** Internal for BASE_PERROR *******/

/**
 * @def bperror_wrapper_1(arg)
 * Internal function to write log with verbosity 1. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 1.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 1
    #define bperror_wrapper_1(arg)	bperror_1 arg
    /** Internal function. */
    void bperror_1(const char *sender, bstatus_t status, 
			      const char *title_fmt, ...);
#else
    #define bperror_wrapper_1(arg)
#endif

/**
 * @def bperror_wrapper_2(arg)
 * Internal function to write log with verbosity 2. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 2.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 2
    #define bperror_wrapper_2(arg)	bperror_2 arg
    /** Internal function. */
    void bperror_2(const char *sender, bstatus_t status, 
			      const char *title_fmt, ...);
#else
    #define bperror_wrapper_2(arg)
#endif

/**
 * @def bperror_wrapper_3(arg)
 * Internal function to write log with verbosity 3. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 3.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 3
    #define bperror_wrapper_3(arg)	bperror_3 arg
    /** Internal function. */
    void bperror_3(const char *sender, bstatus_t status, 
			      const char *title_fmt, ...);
#else
    #define bperror_wrapper_3(arg)
#endif

/**
 * @def bperror_wrapper_4(arg)
 * Internal function to write log with verbosity 4. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 4.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 4
    #define bperror_wrapper_4(arg)	bperror_4 arg
    /** Internal function. */
    void bperror_4(const char *sender, bstatus_t status, 
			      const char *title_fmt, ...);
#else
    #define bperror_wrapper_4(arg)
#endif

/**
 * @def bperror_wrapper_5(arg)
 * Internal function to write log with verbosity 5. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 5.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 5
    #define bperror_wrapper_5(arg)	bperror_5 arg
    /** Internal function. */
    void bperror_5(const char *sender, bstatus_t status, 
			      const char *title_fmt, ...);
#else
    #define bperror_wrapper_5(arg)
#endif

/**
 * @def bperror_wrapper_6(arg)
 * Internal function to write log with verbosity 6. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 6.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 6
    #define bperror_wrapper_6(arg)	bperror_6 arg
    /** Internal function. */
    void bperror_6(const char *sender, bstatus_t status, 
			      const char *title_fmt, ...);
#else
    #define bperror_wrapper_6(arg)
#endif

#ifdef __cplusplus
}
#endif

#endif

