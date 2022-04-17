/* $Id: setjmp.h 5692 2017-11-13 06:06:25Z ming $ */
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
#ifndef __BASE_COMPAT_SETJMP_H__
#define __BASE_COMPAT_SETJMP_H__

/**
 * @file setjmp.h
 * @brief Provides setjmp.h functionality.
 */

#if defined(BASE_HAS_SETJMP_H) && BASE_HAS_SETJMP_H != 0
#  include <setjmp.h>
   typedef jmp_buf bjmp_buf;
#  ifndef bsetjmp
#    define bsetjmp(buf)	setjmp(buf)
#  endif
#  ifndef blongjmp
#    define blongjmp(buf,d)	longjmp(buf,d)
#  endif

#elif defined(BASE_SYMBIAN) && BASE_SYMBIAN!=0
    /* Symbian framework don't use setjmp/longjmp */
    
#else
#  warning "setjmp()/longjmp() is not implemented"
   typedef int bjmp_buf[1];
#  define bsetjmp(buf)	0
#  define blongjmp(buf,d)	0
#endif


#endif	/* __BASE_COMPAT_SETJMP_H__ */

