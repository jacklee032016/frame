/* 
 *
 */
#include <baseUnicode.h>
#include <baseAssert.h>
#include <baseString.h>
#include <windows.h>


wchar_t* bansi_to_unicode(const char *s, int len, wchar_t *buf, int buf_count)
{
	BASE_ASSERT_RETURN(s && buf, NULL);

	len = MultiByteToWideChar(CP_ACP, 0, s, len, buf, buf_count);
	if (buf_count)
	{
		if (len < buf_count)
			buf[len] = 0;
		else
			buf[len-1] = 0;
	}

	return buf;
}


char* bunicode_to_ansi( const wchar_t *wstr, bssize_t len, char *buf, int buf_size)
{
	BASE_ASSERT_RETURN(wstr && buf, NULL);

	len = WideCharToMultiByte(CP_ACP, 0, wstr, (int)len, buf, buf_size, NULL, NULL);
	if (buf_size)
	{
		if (len < buf_size)
			buf[len] = '\0';
		else
			buf[len-1] = '\0';
	}

	return buf;
}

