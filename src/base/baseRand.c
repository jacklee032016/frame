/* 
 *
 */
#include <baseRand.h>
#include <baseOs.h>
#include <compat/rand.h>

void bsrand(unsigned int seed)
{
	BASE_CHECK_STACK();
	platform_srand(seed);
}

int brand(void)
{
	BASE_CHECK_STACK();
	return platform_rand();
}

