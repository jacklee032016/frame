/* 
 *
 */
#include <baseLog.h>
#include <baseOs.h>
#include <compat/stdfileio.h>


static void _termSetColor(int level)
{
#if defined(BASE_TERM_HAS_COLOR) && BASE_TERM_HAS_COLOR != 0
	bterm_set_color(blog_get_color(level));
#else
	BASE_UNUSED_ARG(level);
#endif
}

static void _termRestoreColor(void)
{
#if defined(BASE_TERM_HAS_COLOR) && BASE_TERM_HAS_COLOR != 0
	/* Set terminal to its default color */
	bterm_set_color(blog_get_color(77));
#endif
}


void blog_write(int level, const char *buffer, int len)
{
	BASE_CHECK_STACK();
	BASE_UNUSED_ARG(len);

	/* Copy to terminal/file. */
	if (blog_get_decor() & BASE_LOG_HAS_COLOR)
	{
		_termSetColor(level);
		printf("%s", buffer);
		_termRestoreColor();
	}
	else
	{
		printf("%s", buffer);
	}
}

