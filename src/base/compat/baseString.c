/* 
 *
 */
#include <_baseTypes.h>
#include <compat/string.h>
#include <baseCtype.h>

int strcasecmp(const char *s1, const char *s2)
{
	while ((*s1==*s2) || (btolower(*s1)==btolower(*s2)))
	{
		if (!*s1++)
			return 0;
		++s2;
	}
	return (btolower(*s1) < btolower(*s2)) ? -1 : 1;
}

int strncasecmp(const char *s1, const char *s2, int len)
{
	if (!len)
		return 0;

	while ((*s1==*s2) || (btolower(*s1)==btolower(*s2)))
	{
		if (!*s1++ || --len <= 0)
			return 0;
		++s2;
	}
	
	return (btolower(*s1) < btolower(*s2)) ? -1 : 1;
}

