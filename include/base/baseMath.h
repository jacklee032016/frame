/* $Id: math.h 3553 2011-05-05 06:14:19Z nanang $ */
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

#ifndef __BASE_MATH_H__
#define __BASE_MATH_H__

/**
 * @file math.h
 * @brief Mathematics and Statistics.
 */

#include <baseString.h>
#include <compat/baseHighPrecision.h>

BASE_BEGIN_DECL

/**
 * @defgroup bmath Mathematics and Statistics
 * @ingroup BASE_MISC
 * @{
 *
 * Provides common mathematics constants and operations, and also standard
 * statistics calculation (min, max, mean, standard deviation). Statistics
 * calculation is done in realtime (statistics state is updated on time each
 * new sample comes).
 */

/**
 * Mathematical constants
 */
#define BASE_PI		    3.14159265358979323846	/* pi	    */
#define BASE_1_PI		    0.318309886183790671538	/* 1/pi	    */

/**
 * Mathematical macro
 */
#define	BASE_ABS(x)	((x) >  0 ? (x) : -(x))
#define	BASE_MAX(x, y)	((x) > (y)? (x) : (y))
#define	BASE_MIN(x, y)	((x) < (y)? (x) : (y))

/**
 * This structure describes statistics state.
 */
typedef struct bmath_stat
{
    int		     n;		/* number of samples	*/
    int		     max;	/* maximum value	*/
    int		     min;	/* minimum value	*/
    int		     last;	/* last value		*/
    int		     mean;	/* mean			*/

    /* Private members */
#if BASE_HAS_FLOATING_POINT
    float	     fmean_;	/* mean(floating point) */
#else
    int		     mean_res_;	/* mean residu		*/
#endif
    bhighprec_t    m2_;	/* variance * n		*/
} bmath_stat;

/**
 * Calculate integer square root of an integer.
 *
 * @param i         Integer to be calculated.
 *
 * @return          Square root result.
 */
BASE_INLINE_SPECIFIER unsigned bisqrt(unsigned i)
{
    unsigned res = 1, prev;
    
    /* Rough guess, calculate half bit of input */
    prev = i >> 2;
    while (prev) {
	prev >>= 2;
	res <<= 1;
    }

    /* Babilonian method */
    do {
	prev = res;
	res = (prev + i/prev) >> 1;
    } while ((prev+res)>>1 != res);

    return res;
}

/**
 * Initialize statistics state.
 *
 * @param stat	    Statistic state.
 */
BASE_INLINE_SPECIFIER void bmath_stat_init(bmath_stat *stat)
{
    bbzero(stat, sizeof(bmath_stat));
}

/**
 * Update statistics state as a new sample comes.
 *
 * @param stat	    Statistic state.
 * @param val	    The new sample data.
 */
BASE_INLINE_SPECIFIER void bmath_stat_update(bmath_stat *stat, int val)
{
#if BASE_HAS_FLOATING_POINT
    float	     delta;
#else
    int		     delta;
#endif

    stat->last = val;
    
    if (stat->n++) {
	if (stat->min > val)
	    stat->min = val;
	if (stat->max < val)
	    stat->max = val;
    } else {
	stat->min = stat->max = val;
    }

#if BASE_HAS_FLOATING_POINT
    delta = val - stat->fmean_;
    stat->fmean_ += delta/stat->n;
    
    /* Return mean value with 'rounding' */
    stat->mean = (int) (stat->fmean_ + 0.5);

    stat->m2_ += (int)(delta * (val-stat->fmean_));
#else
    delta = val - stat->mean;
    stat->mean += delta/stat->n;
    stat->mean_res_ += delta % stat->n;
    if (stat->mean_res_ >= stat->n) {
	++stat->mean;
	stat->mean_res_ -= stat->n;
    } else if (stat->mean_res_ <= -stat->n) {
	--stat->mean;
	stat->mean_res_ += stat->n;
    }

    stat->m2_ += delta * (val-stat->mean);
#endif
}

/**
 * Get the standard deviation of specified statistics state.
 *
 * @param stat	    Statistic state.
 *
 * @return	    The standard deviation.
 */
BASE_INLINE_SPECIFIER unsigned bmath_stat_get_stddev(const bmath_stat *stat)
{
    if (stat->n == 0) return 0;
    return (bisqrt((unsigned)(stat->m2_/stat->n)));
}

/**
 * Set the standard deviation of statistics state. This is useful when
 * the statistic state is operated in 'read-only' mode as a storage of 
 * statistical data.
 *
 * @param stat	    Statistic state.
 *
 * @param dev	    The standard deviation.
 */
BASE_INLINE_SPECIFIER void bmath_stat_set_stddev(bmath_stat *stat, unsigned dev)
{
    if (stat->n == 0) 
	stat->n = 1;
    stat->m2_ = dev*dev*stat->n;
}

/** @} */

BASE_END_DECL

#endif

