/* 
 *
 */
#ifndef __BASE_RAND_H__
#define __BASE_RAND_H__

/**
 * @brief Random Number Generator.
 */

#include <baseConfig.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_RAND Random Number Generator
 * @ingroup BASE_MISC
 * @{
 * This module contains functions for generating random numbers.
 * This abstraction is needed not only because not all platforms have
 * \a rand() and \a srand(), but also on some platforms \a rand()
 * only has 16-bit randomness, which is not good enough.
 */

/**
 * Put in seed to random number generator.
 *
 * @param seed	    Seed value.
 */
void bsrand(unsigned int seed);


/**
 * Generate random integer with 32bit randomness.
 *
 * @return a random integer.
 */
int brand(void);

BASE_END_DECL

#endif

