/* 
 *
 */
#ifndef __BASE_COMPAT_RAND_H__
#define __BASE_COMPAT_RAND_H__

/**
 * @file rand.h
 * @brief Provides platform_rand() and platform_srand() functions.
 */

#if defined(BASE_HAS_STDLIB_H) && BASE_HAS_STDLIB_H != 0
   /*
    * Use stdlib based rand() and srand().
    */
#  include <stdlib.h>
#  define platform_srand    srand
#  if defined(RAND_MAX) && RAND_MAX <= 0xFFFF
       /*
        * When rand() is only 16 bit strong, double the strength
	* by calling it twice!
	*/
       BASE_INLINE_SPECIFIER int platform_rand(void)
       {
	   return ((rand() & 0xFFFF) << 16) | (rand() & 0xFFFF);
       }
#  else
#      define platform_rand rand
#  endif

#else
#  warning "platform_rand() is not implemented"
#  define platform_rand()	1
#  define platform_srand(seed)

#endif


#endif	/* __BASE_COMPAT_RAND_H__ */

