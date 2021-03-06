/* $Id: string_compat.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <_baseTypes.h>
#include <compat/string.h>
#include <baseCtype.h>
#include <baseAssert.h>


#if defined(BASE_HAS_STRING_H) && BASE_HAS_STRING_H != 0
/* Nothing to do */
#else
int strcasecmp(const char *s1, const char *s2)
{
    while ((*s1==*s2) || (btolower(*s1)==btolower(*s2))) {
	if (!*s1++)
	    return 0;
	++s2;
    }
    return (btolower(*s1) < btolower(*s2)) ? -1 : 1;
}

int strncasecmp(const char *s1, const char *s2, int len)
{
    if (!len) return 0;

    while ((*s1==*s2) || (btolower(*s1)==btolower(*s2))) {
	if (!*s1++ || --len <= 0)
	    return 0;
	++s2;
    }
    return (btolower(*s1) < btolower(*s2)) ? -1 : 1;
}
#endif

#if defined(BASE_HAS_NO_SNPRINTF) && BASE_HAS_NO_SNPRINTF != 0

int snprintf(char *s1, bsize_t len, const char *s2, ...)
{
    int ret;
    va_list arg;

    BASE_UNUSED_ARG(len);

    va_start(arg, s2);
    ret = vsprintf(s1, s2, arg);
    va_end(arg);
    
    return ret;
}

int vsnprintf(char *s1, bsize_t len, const char *s2, va_list arg)
{
#define MARK_CHAR   ((char)255)
    int rc;

    s1[len-1] = MARK_CHAR;

    rc = vsprintf(s1,s2,arg);

    bassert(s1[len-1] == MARK_CHAR || s1[len-1] == '\0');

    return rc;
}

#endif

