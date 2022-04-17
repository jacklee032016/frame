/* $Id: string.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __BASE_COMPAT_STRING_H__
#define __BASE_COMPAT_STRING_H__

/**
 * @file string.h
 * @brief Provides string manipulation functions found in ANSI string.h.
 */


#if defined(BASE_HAS_STRING_H) && BASE_HAS_STRING_H != 0
#   include <string.h>
#else

    int strcasecmp(const char *s1, const char *s2);
    int strncasecmp(const char *s1, const char *s2, int len);

#endif

/* For sprintf family */
#include <stdio.h>

/* On WinCE, string stuffs are declared in stdlib.h */
#if defined(BASE_HAS_STDLIB_H) && BASE_HAS_STDLIB_H!=0
#   include <stdlib.h>
#endif

#if defined(_MSC_VER)
#   define strcasecmp	_stricmp
#   define strncasecmp	_strnicmp
#   define snprintf	_snprintf
#   define vsnprintf	_vsnprintf
#   define snwprintf	_snwprintf
#   define wcsicmp	_wcsicmp
#   define wcsnicmp	_wcsnicmp
#else
#   define stricmp	strcasecmp
#   define strnicmp	strncasecmp

#   if defined(BASE_NATIVE_STRING_IS_UNICODE) && BASE_NATIVE_STRING_IS_UNICODE!=0
#	error "Implement Unicode string functions"
#   endif
#endif

#define bansi_strcmp		strcmp
#define bansi_strncmp		strncmp
#define bansi_strlen		strlen
#define bansi_strcpy		strcpy
#define bansi_strncpy		strncpy
#define bansi_strcat		strcat
#define bansi_strstr		strstr
#define bansi_strchr		strchr
#define bansi_strcasecmp	strcasecmp
#define bansi_stricmp		strcasecmp
#define bansi_strncasecmp	strncasecmp
#define bansi_strnicmp	strncasecmp
#define bansi_sprintf		sprintf

#if defined(BASE_HAS_NO_SNPRINTF) && BASE_HAS_NO_SNPRINTF != 0
#   include <_baseTypes.h>
#   include <compat/stdarg.h>
    BASE_BEGIN_DECL
    int snprintf(char*s1, bsize_t len, const char*s2, ...);
    int vsnprintf(char*s1, bsize_t len, const char*s2, va_list arg);
    BASE_END_DECL
#endif
    
#define bansi_snprintf	snprintf
#define bansi_vsprintf	vsprintf
#define bansi_vsnprintf	vsnprintf

#define bunicode_strcmp	wcscmp
#define bunicode_strncmp	wcsncmp
#define bunicode_strlen	wcslen
#define bunicode_strcpy	wcscpy
#define	bunicode_strncpy	wcsncpy
#define bunicode_strcat	wcscat
#define bunicode_strstr	wcsstr
#define bunicode_strchr	wcschr
#define bunicode_strcasecmp	wcsicmp
#define bunicode_stricmp	wcsicmp
#define bunicode_strncasecmp	wcsnicmp
#define bunicode_strnicmp	wcsnicmp
#define bunicode_sprintf	swprintf
#define bunicode_snprintf	snwprintf
#define bunicode_vsprintf	vswprintf
#define bunicode_vsnprintf	vsnwprintf

#if defined(BASE_NATIVE_STRING_IS_UNICODE) && BASE_NATIVE_STRING_IS_UNICODE!=0
#   define bnative_strcmp	    bunicode_strcmp
#   define bnative_strncmp	    bunicode_strncmp
#   define bnative_strlen	    bunicode_strlen
#   define bnative_strcpy	    bunicode_strcpy
#   define bnative_strncpy	    bunicode_strncpy
#   define bnative_strcat	    bunicode_strcat
#   define bnative_strstr	    bunicode_strstr
#   define bnative_strchr	    bunicode_strchr
#   define bnative_strcasecmp	    bunicode_strcasecmp
#   define bnative_stricmp	    bunicode_stricmp
#   define bnative_strncasecmp    bunicode_strncasecmp
#   define bnative_strnicmp	    bunicode_strnicmp
#   define bnative_sprintf	    bunicode_sprintf
#   define bnative_snprintf	    bunicode_snprintf
#   define bnative_vsprintf	    bunicode_vsprintf
#   define bnative_vsnprintf	    bunicode_vsnprintf
#else
#   define bnative_strcmp	    bansi_strcmp
#   define bnative_strncmp	    bansi_strncmp
#   define bnative_strlen	    bansi_strlen
#   define bnative_strcpy	    bansi_strcpy
#   define bnative_strncpy	    bansi_strncpy
#   define bnative_strcat	    bansi_strcat
#   define bnative_strstr	    bansi_strstr
#   define bnative_strchr	    bansi_strchr
#   define bnative_strcasecmp	    bansi_strcasecmp
#   define bnative_stricmp	    bansi_stricmp
#   define bnative_strncasecmp    bansi_strncasecmp
#   define bnative_strnicmp	    bansi_strnicmp
#   define bnative_sprintf	    bansi_sprintf
#   define bnative_snprintf	    bansi_snprintf
#   define bnative_vsprintf	    bansi_vsprintf
#   define bnative_vsnprintf	    bansi_vsnprintf
#endif


#endif	/* __BASE_COMPAT_STRING_H__ */
