/* $Id: cc_gcc.h 4704 2014-01-16 05:30:46Z ming $ */
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
#ifndef __BASE_COMPAT_CC_GCC_H__
#define __BASE_COMPAT_CC_GCC_H__

/**
 * @file cc_gcc.h
 * @brief Describes GCC compiler specifics.
 */

#ifndef __GNUC__
#  error "This file is only for gcc!"
#endif

#define BASE_CC_NAME		"gcc"
#define BASE_CC_VER_1		__GNUC__
#define BASE_CC_VER_2		__GNUC_MINOR__

/* __GNUC_PATCHLEVEL__ doesn't exist in gcc-2.9x.x */
#ifdef __GNUC_PATCHLEVEL__
#   define BASE_CC_VER_3		__GNUC_PATCHLEVEL__
#else
#   define BASE_CC_VER_3		0
#endif



#define BASE_THREAD_FUNC	
#define BASE_NORETURN		

#define BASE_HAS_INT64		1

#ifdef __STRICT_ANSI__
  #include <inttypes.h> 
  typedef int64_t		bint64_t;
  typedef uint64_t		buint64_t;
  #define BASE_INLINE_SPECIFIER	static __inline
  #define BASE_ATTR_NORETURN	
  #define BASE_ATTR_MAY_ALIAS	
#else
  typedef long long		bint64_t;
  typedef unsigned long long	buint64_t;
  #define BASE_INLINE_SPECIFIER	static inline
  #define BASE_ATTR_NORETURN	__attribute__ ((noreturn))
  #define BASE_ATTR_MAY_ALIAS	__attribute__((__may_alias__))
#endif

#define BASE_INT64(val)		val##LL
#define BASE_UINT64(val)		val##ULL
#define BASE_INT64_FMT		"L"


#ifdef __GLIBC__
#   define BASE_HAS_BZERO		1
#endif

#define BASE_UNREACHED(x)	    	

#define BASE_ALIGN_DATA(declaration, alignment) declaration __attribute__((aligned (alignment)))


#endif	/* __BASE_COMPAT_CC_GCC_H__ */

