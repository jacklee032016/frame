/* $Id: limits.h 5682 2017-11-08 02:58:18Z riza $ */
/*
 * Copyright (C) 2017 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2017 George Joseph <gjoseph@digium.com>
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
#ifndef __BASE_LIMITS_H__
#define __BASE_LIMITS_H__

/**
 * @file limits.h
 * @brief Common min and max values
 */

#include <compat/limits.h>

/** Maximum value for signed 32-bit integer. */
#define BASE_MAXINT32	0x7fffffff

/** Minimum value for signed 32-bit integer. */
#define BASE_MININT32	0x80000000

/** Maximum value for unsigned 16-bit integer. */
#define BASE_MAXUINT16	0xffff

/** Maximum value for unsigned char. */
#define BASE_MAXUINT8	0xff

/** Maximum value for long. */
#define BASE_MAXLONG	LONG_MAX

/** Minimum value for long. */
#define BASE_MINLONG	LONG_MIN

/** Minimum value for unsigned long. */
#define BASE_MAXULONG	ULONG_MAX

#endif  /* __BASE_LIMITS_H__ */
