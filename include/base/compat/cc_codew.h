/* $Id: cc_codew.h 4624 2013-10-21 06:37:30Z ming $ */
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
#ifndef __BASE_COMPAT_CC_CODEW_H__
#define __BASE_COMPAT_CC_CODEW_H__

/**
 * @file cc_codew.h
 * @brief Describes MetroWerks Code Warrior compiler specifics.
 */

#ifndef __MWERKS__
#  error "This file is only for Code Warrior!"
#endif

#define BASE_CC_NAME		"codewarrior"
#define BASE_CC_VER_1		((__MWERKS__ & 0xF000) >> 12)
#define BASE_CC_VER_2		((__MWERKS__ & 0x0F00) >> 8)
#define BASE_CC_VER_3		((__MWERKS__ & 0xFF))


#define BASE_INLINE_SPECIFIER	static inline
#define BASE_THREAD_FUNC	
#define BASE_NORETURN		
#define BASE_ATTR_NORETURN	
#define BASE_ATTR_MAY_ALIAS	

#define BASE_HAS_INT64		1

typedef long long bint64_t;
typedef unsigned long long buint64_t;

#define BASE_INT64(val)		val##LL
#define BASE_UINT64(val)		val##LLU
#define BASE_INT64_FMT		"L"

#define BASE_UNREACHED(x)	    	

#endif	/* __BASE_COMPAT_CC_CODEW_H__ */

