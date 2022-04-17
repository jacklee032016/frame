/*
 *
 */
 
#include <baseLog.h>
#include <baseString.h>
#include <baseOs.h>
#include <compat/stdarg.h>

#if BASE_LOG_MAX_LEVEL >= 1

#if 0
BASE_DEF_DATA(int) blog_max_level = BASE_LOG_MAX_LEVEL;
#else
static int blog_max_level = BASE_LOG_MAX_LEVEL;
#endif

static void *g_last_thread;

#if BASE_HAS_THREADS
static long thread_suspended_tls_id = -1;
#  if BASE_LOG_ENABLE_INDENT
static long thread_indent_tls_id = -1;
#  endif
#endif

#if !BASE_LOG_ENABLE_INDENT || !BASE_HAS_THREADS
static int log_indent;
#endif

static blog_func *log_writer = &blog_write;
static unsigned log_decor = BASE_LOG_HAS_TIME | BASE_LOG_HAS_MICRO_SEC |
			    BASE_LOG_HAS_SENDER | BASE_LOG_HAS_NEWLINE |
			    BASE_LOG_HAS_SPACE | BASE_LOG_HAS_THREAD_SWC |
			    BASE_LOG_HAS_INDENT
#if (defined(BASE_WIN32) && BASE_WIN32!=0) || \
    (defined(BASE_WIN64) && BASE_WIN64!=0)
			    | BASE_LOG_HAS_COLOR
#endif
			    ;

static bcolor_t BASE_LOG_COLOR_0 = BASE_TERM_COLOR_BRIGHT | BASE_TERM_COLOR_R;
static bcolor_t BASE_LOG_COLOR_1 = BASE_TERM_COLOR_BRIGHT | BASE_TERM_COLOR_R;
static bcolor_t BASE_LOG_COLOR_2 = BASE_TERM_COLOR_BRIGHT | BASE_TERM_COLOR_R | BASE_TERM_COLOR_G;
static bcolor_t BASE_LOG_COLOR_3 = BASE_TERM_COLOR_BRIGHT | 
				   BASE_TERM_COLOR_R | 
				   BASE_TERM_COLOR_G | 
				   BASE_TERM_COLOR_B;
static bcolor_t BASE_LOG_COLOR_4 = BASE_TERM_COLOR_R | 
				   BASE_TERM_COLOR_G | 
				   BASE_TERM_COLOR_B;
static bcolor_t BASE_LOG_COLOR_5 = BASE_TERM_COLOR_R | 
				   BASE_TERM_COLOR_G | 
				   BASE_TERM_COLOR_B;
static bcolor_t BASE_LOG_COLOR_6 = BASE_TERM_COLOR_R | 
				   BASE_TERM_COLOR_G | 
				   BASE_TERM_COLOR_B;
/* Default terminal color */
static bcolor_t BASE_LOG_COLOR_77 = BASE_TERM_COLOR_R | 
				    BASE_TERM_COLOR_G | 
				    BASE_TERM_COLOR_B;

#if BASE_LOG_USE_STACK_BUFFER==0
static char log_buffer[BASE_LOG_MAX_SIZE];
#endif

#define LOG_MAX_INDENT		80

#if BASE_HAS_THREADS
static void _loggingShutdown(void *data)
{
	if (thread_suspended_tls_id != -1)
	{
		bthreadLocalFree(thread_suspended_tls_id);
		thread_suspended_tls_id = -1;
	}
#if BASE_LOG_ENABLE_INDENT
	if (thread_indent_tls_id != -1)
	{
		bthreadLocalFree(thread_indent_tls_id);
		thread_indent_tls_id = -1;
	}
#endif
}
#endif	/* BASE_HAS_THREADS */

#if BASE_LOG_ENABLE_INDENT && BASE_HAS_THREADS
static void _logSetIndent(int indent)
{
    if (indent < 0) indent = 0;
    bthreadLocalSet(thread_indent_tls_id, (void*)(bssize_t)indent);
}

static int _getRawIndent(void)
{
    return (long)(bssize_t)bthreadLocalGet(thread_indent_tls_id);
}

#else
static void _logSetIndent(int indent)
{
    log_indent = indent;
    if (log_indent < 0) log_indent = 0;
}

static int _getRawIndent(void)
{
    return log_indent;
}
#endif	/* BASE_LOG_ENABLE_INDENT && BASE_HAS_THREADS */

static int _getIndent(void)
{
    int indent = _getRawIndent();
    return indent > LOG_MAX_INDENT ? LOG_MAX_INDENT : indent;
}

void blog_add_indent(int indent)
{
	_logSetIndent(_getRawIndent() + indent);
}

void blog_push_indent(void)
{
    blog_add_indent(BASE_LOG_INDENT_SIZE);
}

void blog_pop_indent(void)
{
    blog_add_indent(-BASE_LOG_INDENT_SIZE);
}

bstatus_t extLogInit(void)
{
#if BASE_HAS_THREADS
	if (thread_suspended_tls_id == -1)
	{
		bstatus_t status;
		status = bthreadLocalAlloc(&thread_suspended_tls_id);
		if (status != BASE_SUCCESS)
			return status;

#if BASE_LOG_ENABLE_INDENT
		status = bthreadLocalAlloc(&thread_indent_tls_id);
		if (status != BASE_SUCCESS)
		{
			bthreadLocalFree(thread_suspended_tls_id);
			thread_suspended_tls_id = -1;
			return status;
		}
#endif
		libBaseAtExit(&_loggingShutdown, "Logger", NULL);
	}
#endif

	g_last_thread = NULL;
	return BASE_SUCCESS;
}

void blog_set_decor(unsigned decor)
{
    log_decor = decor;
}

unsigned blog_get_decor(void)
{
    return log_decor;
}

void blog_set_color(int level, bcolor_t color)
{
    switch (level) 
    {
	case 0: BASE_LOG_COLOR_0 = color; 
	    break;
	case 1: BASE_LOG_COLOR_1 = color; 
	    break;
	case 2: BASE_LOG_COLOR_2 = color; 
	    break;
	case 3: BASE_LOG_COLOR_3 = color; 
	    break;
	case 4: BASE_LOG_COLOR_4 = color; 
	    break;
	case 5: BASE_LOG_COLOR_5 = color; 
	    break;
	case 6: BASE_LOG_COLOR_6 = color; 
	    break;
	/* Default terminal color */
	case 77: BASE_LOG_COLOR_77 = color; 
	    break;
	default:
	    /* Do nothing */
	    break;
    }
}

bcolor_t blog_get_color(int level)
{
    switch (level) {
	case 0:
	    return BASE_LOG_COLOR_0;
	case 1:
	    return BASE_LOG_COLOR_1;
	case 2:
	    return BASE_LOG_COLOR_2;
	case 3:
	    return BASE_LOG_COLOR_3;
	case 4:
	    return BASE_LOG_COLOR_4;
	case 5:
	    return BASE_LOG_COLOR_5;
	case 6:
	    return BASE_LOG_COLOR_6;
	default:
	    /* Return default terminal color */
	    return BASE_LOG_COLOR_77;
    }
}

void blog_set_level(int level)
{
    blog_max_level = level;
}

#if 1
int blog_get_level(void)
{
    return blog_max_level;
}
#endif

void blog_set_log_func( blog_func *func )
{
    log_writer = func;
}

blog_func* blog_get_log_func(void)
{
    return log_writer;
}

/* Temporarily suspend logging facility for this thread.
 * If thread local storage/variable is not used or not initialized, then
 * we can only suspend the logging globally across all threads. This may
 * happen e.g. when log function is called before  is fully initialized
 * or after  is shutdown.
 */
static void _suspendLogging(int *saved_level)
{
	/* Save the level regardless, just in case  is shutdown
	 * between suspend and resume.
	 */
	*saved_level = blog_max_level;

#if BASE_HAS_THREADS
    if (thread_suspended_tls_id != -1) 
    {
	bthreadLocalSet(thread_suspended_tls_id, 
			    (void*)(bssize_t)BASE_TRUE);
    } 
    else
#endif
    {
	blog_max_level = 0;
    }
}

/* Resume logging facility for this thread */
static void _resumeLogging(int *saved_level)
{
#if BASE_HAS_THREADS
    if (thread_suspended_tls_id != -1) 
    {
	bthreadLocalSet(thread_suspended_tls_id,
			    (void*)(bsize_t)BASE_FALSE);
    }
    else
#endif
    {
	/* Only revert the level if application doesn't change the
	 * logging level between suspend and resume.
	 */
	if (blog_max_level==0 && *saved_level)
	    blog_max_level = *saved_level;
    }
}

/* Is logging facility suspended for this thread? */
static bbool_t _isSuspended(void)
{
#if BASE_HAS_THREADS
    if (thread_suspended_tls_id != -1) 
    {
	return bthreadLocalGet(thread_suspended_tls_id) != NULL;
    }
    else
#endif
    {
	return blog_max_level == 0;
    }
}

void blog( const char *sender, int level, const char *format, va_list marker)
{
	btime_val now;
	bparsed_time ptime;
	char *pre;
#if BASE_LOG_USE_STACK_BUFFER
	char log_buffer[BASE_LOG_MAX_SIZE];
#endif
	int saved_level, len, print_len, indent;

	BASE_CHECK_STACK();

	if (level > blog_max_level)
		return;

	if (_isSuspended())
		return;

	/* Temporarily disable logging for this thread. Some of  APIs that
	* this function calls below will recursively call the logging function 
	* back, hence it will cause infinite recursive calls if we allow that.
	*/
	_suspendLogging(&saved_level);

	/* Get current date/time. */
	bgettimeofday(&now);
	btime_decode(&now, &ptime);

	pre = log_buffer;
	if (log_decor & BASE_LOG_HAS_LEVEL_TEXT)
	{
		static const char *ltexts[] = { "FATAL:", "ERROR:", " WARN:", 	" INFO:", "DEBUG:", "MTRACE:", "DETRC:"};
		bansi_strcpy(pre, ltexts[level]);
		pre += 6;
	}
	
	if (log_decor & BASE_LOG_HAS_DAY_NAME)
	{
		static const char *wdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
		bansi_strcpy(pre, wdays[ptime.wday]);
		pre += 3;
	}
	
	if (log_decor & BASE_LOG_HAS_YEAR)
	{
		if (pre!=log_buffer) *pre++ = ' ';
		pre += butoa(ptime.year, pre);
	}
	
	if (log_decor & BASE_LOG_HAS_MONTH)
	{
		*pre++ = '-';
		pre += butoa_pad(ptime.mon+1, pre, 2, '0');
	}
	
	if (log_decor & BASE_LOG_HAS_DAY_OF_MON)
	{
		*pre++ = '-';
		pre += butoa_pad(ptime.day, pre, 2, '0');
	}
	
	if (log_decor & BASE_LOG_HAS_TIME)
	{
		if (pre!=log_buffer)
			*pre++ = ' ';

		pre += butoa_pad(ptime.hour, pre, 2, '0');
		*pre++ = ':';
		pre += butoa_pad(ptime.min, pre, 2, '0');
		*pre++ = ':';
		pre += butoa_pad(ptime.sec, pre, 2, '0');
	}
	
	if (log_decor & BASE_LOG_HAS_MICRO_SEC)
	{
		*pre++ = '.';
		pre += butoa_pad(ptime.msec, pre, 3, '0');
	}
	
	if (log_decor & BASE_LOG_HAS_SENDER)
	{
		enum { SENDER_WIDTH = BASE_LOG_SENDER_WIDTH };
		bsize_t sender_len = strlen(sender);
		if (pre!=log_buffer)
			*pre++ = ' ';

		if (sender_len <= SENDER_WIDTH)
		{
			while (sender_len < SENDER_WIDTH)
				*pre++ = ' ', ++sender_len;
			while (*sender)
				*pre++ = *sender++;
		}
		else
		{
			int i;
			
			for (i=0; i<SENDER_WIDTH; ++i)
				*pre++ = *sender++;
		}
	}
	
	if (log_decor & BASE_LOG_HAS_THREAD_ID)
	{
		enum { THREAD_WIDTH = BASE_LOG_THREAD_WIDTH };

		const char *thread_name = bthreadGetName(bthreadThis());
		bsize_t thread_len = strlen(thread_name);
		*pre++ = ' ';

		if (thread_len <= THREAD_WIDTH)
		{
			while (thread_len < THREAD_WIDTH)
				*pre++ = ' ', ++thread_len;

			while (*thread_name)
				*pre++ = *thread_name++;
		}
		else
		{
			int i;
			
			for (i=0; i<THREAD_WIDTH; ++i)
				*pre++ = *thread_name++;
		}
	}

	if (log_decor != 0 && log_decor != BASE_LOG_HAS_NEWLINE)
		*pre++ = ' ';

	if (log_decor & BASE_LOG_HAS_THREAD_SWC)
	{
		void *current_thread = (void*)bthreadThis();
		if (current_thread != g_last_thread)
		{
			*pre++ = '!';
			g_last_thread = current_thread;
		}
		else
		{
			*pre++ = ' ';
		}
	}
	else if (log_decor & BASE_LOG_HAS_SPACE)
	{
		*pre++ = ' ';
	}

#if BASE_LOG_ENABLE_INDENT
	if (log_decor & BASE_LOG_HAS_INDENT)
	{
		indent = _getIndent();
		if (indent > 0)
		{
			bmemset(pre, BASE_LOG_INDENT_CHAR, indent);
			pre += indent;
		}
	}
#endif

	len = (int)(pre - log_buffer);

	/* Print the whole message to the string log_buffer. */
	print_len = bansi_vsnprintf(pre, sizeof(log_buffer)-len, format, marker);
	if (print_len < 0)
	{
		level = 1;
		print_len = bansi_snprintf(pre, sizeof(log_buffer)-len, "<logging error: msg too long>");
	}
	
	if (print_len < 1 || print_len >= (int)(sizeof(log_buffer)-len))
	{
		print_len = sizeof(log_buffer) - len - 1;
	}
	
	len = len + print_len;
	if (len > 0 && len < (int)sizeof(log_buffer)-2)
	{
		if (log_decor & BASE_LOG_HAS_CR)
		{
			log_buffer[len++] = '\r';
		}

		if (log_decor & BASE_LOG_HAS_NEWLINE)
		{
			log_buffer[len++] = '\n';
		}
		log_buffer[len] = '\0';
	}
	else
	{
		len = sizeof(log_buffer)-1;

		if (log_decor & BASE_LOG_HAS_CR)
		{
			log_buffer[sizeof(log_buffer)-3] = '\r';
		}

		if (log_decor & BASE_LOG_HAS_NEWLINE)
		{
			log_buffer[sizeof(log_buffer)-2] = '\n';
		}

		log_buffer[sizeof(log_buffer)-1] = '\0';
	}

	/* It should be safe to resume logging at this point. Application can
	* recursively call the logging function inside the callback.
	*/
	_resumeLogging(&saved_level);

	if (log_writer)
		(*log_writer)(level, log_buffer, len);
}

/*
void blog_0(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    blog(obj, 0, format, arg);
    va_end(arg);
}
*/

void blog_1(const char *obj, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	blog(obj, 1, format, arg);
	va_end(arg);
}
#endif	/* BASE_LOG_MAX_LEVEL >= 1 */

#if BASE_LOG_MAX_LEVEL >= 2
void blog_2(const char *obj, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	blog(obj, 2, format, arg);
	va_end(arg);
}
#endif

#if BASE_LOG_MAX_LEVEL >= 3
void blog_3(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    blog(obj, 3, format, arg);
    va_end(arg);
}
#endif

#if BASE_LOG_MAX_LEVEL >= 4
void blog_4(const char *obj, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	blog(obj, 4, format, arg);
	va_end(arg);
}
#endif

#if BASE_LOG_MAX_LEVEL >= 5
void blog_5(const char *obj, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	blog(obj, 5, format, arg);
	va_end(arg);
}
#endif

#if BASE_LOG_MAX_LEVEL >= 6
void blog_6(const char *obj, const char *format, ...)
{
	va_list arg;
	va_start(arg, format);
	blog(obj, 6, format, arg);
	va_end(arg);
}
#endif

