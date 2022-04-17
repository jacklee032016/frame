/* 
/* 
 */

#ifndef __BASE_LOG_H__
#define __BASE_LOG_H__


#include <_baseTypes.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Log decoration flag, to be specified with #blog_set_decor().
 */
enum blog_decoration
{
	BASE_LOG_HAS_DAY_NAME   =    1, /**< Include day name [default: no] 	      */
	BASE_LOG_HAS_YEAR       =    2, /**< Include year digit [no]		      */
	BASE_LOG_HAS_MONTH	  =    4, /**< Include month [no]		      */
	BASE_LOG_HAS_DAY_OF_MON =    8, /**< Include day of month [no]	      */
	BASE_LOG_HAS_TIME	  =   16, /**< Include time [yes]		      */
	BASE_LOG_HAS_MICRO_SEC  =   32, /**< Include microseconds [yes]             */
	BASE_LOG_HAS_SENDER	  =   64, /**< Include sender in the log [yes] 	      */
	BASE_LOG_HAS_NEWLINE	  =  128, /**< Terminate each call with newline [yes] */
	BASE_LOG_HAS_CR	  =  256, /**< Include carriage return [no] 	      */
	BASE_LOG_HAS_SPACE	  =  512, /**< Include two spaces before log [yes]    */
	BASE_LOG_HAS_COLOR	  = 1024, /**< Colorize logs [yes on win32]	      */
	BASE_LOG_HAS_LEVEL_TEXT = 2048, /**< Include level text string [no]	      */
	BASE_LOG_HAS_THREAD_ID  = 4096, /**< Include thread identification [no]     */
	BASE_LOG_HAS_THREAD_SWC = 8192, /**< Add mark when thread has switched [yes]*/
	BASE_LOG_HAS_INDENT     =16384  /**< Indentation. Say yes! [yes]            */
};

typedef enum
{
	BASE_LOG_EMERG = 0,		/* system is unusable */
	BASE_LOG_ALERT,			/* action must be taken immediately */
	BASE_LOG_CRIT ,			/* critical conditions */
	BASE_LOG_ERR , 				/* error conditions */
	BASE_LOG_WARNING ,		/* warning conditions */
	BASE_LOG_NOTICE ,			/* normal but significant condition */
	BASE_LOG_INFO,				/* informational */
	BASE_LOG_DEBUG			/* 7 : debug-level messages */
}log_level_t;

/**
 * Write log message.
 * This is the main macro used to write text to the logging backend. 
 *
 * @param level	    The logging verbosity level. Lower number indicates higher
 *		    importance, with level zero indicates fatal error. Only
 *		    numeral argument is permitted (e.g. not variable).
 * @param arg	    Enclosed 'printf' like arguments, with the first 
 *		    argument is the sender, the second argument is format 
 *		    string and the following arguments are variable number of 
 *		    arguments suitable for the format string.
 *
 * Sample:
 * \verbatim
   BASE_LOG(2, (__FILE__, "current value is %d", value));
   \endverbatim
 * @hideinitializer
 */
#define BASE_LOG(level,arg)	do { \
				    if (level <= blog_get_level()) { \
					blog_wrapper_##level(arg); \
				    } \
				} while (0)

/**
 * Signature for function to be registered to the logging subsystem to
 * write the actual log message to some output device.
 *
 * @param level	    Log level.
 * @param data	    Log message, which will be NULL terminated.
 * @param len	    Message length.
 */
typedef void blog_func(int level, const char *data, int len);

/**
 * Default logging writer function used by front end logger function.
 * This function will print the log message to stdout only.
 * Application normally should NOT need to call this function, but
 * rather use the BASE_LOG macro.
 *
 * @param level	    Log level.
 * @param buffer    Log message.
 * @param len	    Message length.
 */
void blog_write(int level, const char *buffer, int len);


#if BASE_LOG_MAX_LEVEL >= 1

/**
 * Write to log.
 *
 * @param sender    Source of the message.
 * @param level	    Verbosity level.
 * @param format    Format.
 * @param marker    Marker.
 */
void blog(const char *sender, int level, const char *format, va_list marker);

/**
 * Change log output function. The front-end logging functions will call
 * this function to write the actual message to the desired device. 
 * By default, the front-end functions use blog_write() to write
 * the messages, unless it's changed by calling this function.
 *
 * @param func	    The function that will be called to write the log messages to the desired device.
 */
void blog_set_log_func( blog_func *func );

/**
 * Get the current log output function that is used to write log messages.
 * @return	    Current log output function.
 */
blog_func* blog_get_log_func(void);

/**
 * Set maximum log level. Application can call this function to set 
 * the desired level of verbosity of the logging messages. The bigger the
 * value, the more verbose the logging messages will be printed. However,
 * the maximum level of verbosity can not exceed compile time value of
 * BASE_LOG_MAX_LEVEL.
 *
 * @param level	    The maximum level of verbosity of the logging
 *		    messages (6=very detailed..1=error only, 0=disabled)
 */
void blog_set_level(int level);

/**
 * Get current maximum log verbositylevel.
 *
 * @return	    Current log maximum level.
 */
#if 1
int blog_get_level(void);
#else
BASE_DECL_DATA(int) blog_max_level;
#define blog_get_level()  blog_max_level
#endif

/**
 * Set log decoration. The log decoration flag controls what are printed
 * to output device alongside the actual message. For example, application
 * can specify that date/time information should be displayed with each
 * log message.
 *
 * @param decor	    Bitmask combination of #blog_decoration to control
 *		    the layout of the log message.
 */
void blog_set_decor(unsigned decor);

/**
 * Get current log decoration flag.
 *
 * @return	    Log decoration flag.
 */
unsigned blog_get_decor(void);

/**
 * Add indentation to log message. Indentation will add BASE_LOG_INDENT_CHAR
 * before the message, and is useful to show the depth of function calls.
 *
 * @param indent    The indentation to add or substract. Positive value
 * 		    adds current indent, negative value subtracts current
 * 		    indent.
 */
void blog_add_indent(int indent);

/**
 * Push indentation to the right by default value (BASE_LOG_INDENT).
 */
void blog_push_indent(void);

/**
 * Pop indentation (to the left) by default value (BASE_LOG_INDENT).
 */
void blog_pop_indent(void);

/**
 * Set color of log messages.
 *
 * @param level	    Log level which color will be changed.
 * @param color	    Desired color.
 */
void blog_set_color(int level, bcolor_t color);

/**
 * Get color of log messages.
 *
 * @param level	    Log level which color will be returned.
 * @return	    Log color.
 */
bcolor_t blog_get_color(int level);

/**
 * Internal function to be called by libBaseInit()
 */
bstatus_t extLogInit(void);

#else	/* #if BASE_LOG_MAX_LEVEL >= 1 */

#define blog_set_log_func(func)

#define blog(sender, level, format, marker)

#define blog_set_level(level)

#define blog_set_decor(decor)

#define blog_add_indent(indent)

#define blog_push_indent()

#define blog_pop_indent()

#define blog_set_color(level, color)

#define blog_get_level()	0

#define blog_get_decor()	0

#define blog_get_color(level) 0

#define extLogInit()	BASE_SUCCESS

#endif	/* #if BASE_LOG_MAX_LEVEL >= 1 */


/*
 * Log functions implementation prototypes.
 * These functions are called by BASE_LOG macros according to verbosity
 * level specified when calling the macro. Applications should not normally
 * need to call these functions directly.
 */

/**
 * @def blog_wrapper_1(arg)
 * Internal function to write log with verbosity 1. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 1.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 1
    #define blog_wrapper_1(arg)	blog_1 arg
    /** Internal function. */
    void blog_1(const char *src, const char *format, ...);
#else
    #define blog_wrapper_1(arg)
#endif

/**
 * @def blog_wrapper_2(arg)
 * Internal function to write log with verbosity 2. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 2.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 2
    #define blog_wrapper_2(arg)	blog_2 arg
    /** Internal function. */
    void blog_2(const char *src, const char *format, ...);
#else
    #define blog_wrapper_2(arg)
#endif

/**
 * @def blog_wrapper_3(arg)
 * Internal function to write log with verbosity 3. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 3.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 3
    #define blog_wrapper_3(arg)	blog_3 arg
    /** Internal function. */
    void blog_3(const char *src, const char *format, ...);
#else
    #define blog_wrapper_3(arg)
#endif

/**
 * @def blog_wrapper_4(arg)
 * Internal function to write log with verbosity 4. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 4.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 4
    #define blog_wrapper_4(arg)	blog_4 arg
    /** Internal function. */
    void blog_4(const char *src, const char *format, ...);
#else
    #define blog_wrapper_4(arg)
#endif

/**
 * @def blog_wrapper_5(arg)
 * Internal function to write log with verbosity 5. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 5.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 5
    #define blog_wrapper_5(arg)	blog_5 arg
    /** Internal function. */
    void blog_5(const char *src, const char *format, ...);
#else
    #define blog_wrapper_5(arg)
#endif

/**
 * @def blog_wrapper_6(arg)
 * Internal function to write log with verbosity 6. Will evaluate to
 * empty expression if BASE_LOG_MAX_LEVEL is below 6.
 * @param arg       Log expression.
 */
#if BASE_LOG_MAX_LEVEL >= 6
    #define blog_wrapper_6(arg)	blog_6 arg
    /** Internal function. */
    void blog_6(const char *src, const char *format, ...);
#else
    #define blog_wrapper_6(arg)
#endif

#ifdef __cplusplus
}
#endif

#endif

