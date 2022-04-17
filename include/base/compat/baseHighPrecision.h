/* $Id: high_precision.h 5692 2017-11-13 06:06:25Z ming $ */
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
#ifndef __BASE_COMPAT_HIGH_PRECISION_H__
#define __BASE_COMPAT_HIGH_PRECISION_H__


#if defined(BASE_HAS_FLOATING_POINT) && BASE_HAS_FLOATING_POINT != 0
    /*
     * The first choice for high precision math is to use double.
     */
#   include <math.h>
    typedef double bhighprec_t;

#   define BASE_HIGHPREC_VALUE_IS_ZERO(a)     (a==0)
#   define bhighprec_mod(a,b)             (a=fmod(a,b))

#elif defined(BASE_HAS_INT64) && BASE_HAS_INT64 != 0
    /*
     * Next choice is to use 64-bit arithmatics.
     */
    typedef bint64_t bhighprec_t;

#else
#   warning "High precision math is not available"

    /*
     * Last, fallback to 32-bit arithmetics.
     */
    typedef bint32_t bhighprec_t;

#endif

/**
 * @def bhighprec_mul
 * bhighprec_mul(a1, a2) - High Precision Multiplication
 * Multiply a1 and a2, and store the result in a1.
 */
#ifndef bhighprec_mul
#   define bhighprec_mul(a1,a2)   (a1 = a1 * a2)
#endif

/**
 * @def bhighprec_div
 * bhighprec_div(a1, a2) - High Precision Division
 * Divide a2 from a1, and store the result in a1.
 */
#ifndef bhighprec_div
#   define bhighprec_div(a1,a2)   (a1 = a1 / a2)
#endif

/**
 * @def bhighprec_mod
 * bhighprec_mod(a1, a2) - High Precision Modulus
 * Get the modulus a2 from a1, and store the result in a1.
 */
#ifndef bhighprec_mod
#   define bhighprec_mod(a1,a2)   (a1 = a1 % a2)
#endif


/**
 * @def BASE_HIGHPREC_VALUE_IS_ZERO(a)
 * Test if the specified high precision value is zero.
 */
#ifndef BASE_HIGHPREC_VALUE_IS_ZERO
#   define BASE_HIGHPREC_VALUE_IS_ZERO(a)     (a==0)
#endif


#endif	/* __BASE_COMPAT_HIGH_PRECISION_H__ */

