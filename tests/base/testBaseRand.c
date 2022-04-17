/* 
 *
 */
#include <baseRand.h>
#include <baseLog.h>
#include "testBaseTest.h"

#if INCLUDE_RAND_TEST

#define COUNT  1024
static int values[COUNT];

/*
 * rand_test(), simply generates COUNT number of random number and
 * check that there's no duplicate numbers.
 */
int testBaseRand(void)
{
	int i;

	for (i=0; i<COUNT; ++i)
	{
		int j;

		values[i] = brand();
		for (j=0; j<i; ++j)
		{
			if (values[i] == values[j])
			{
				BASE_ERROR("error: duplicate value %d at %d-th index", values[i], i);
				return -10;
			}
		}
	}

	return 0;
}

#endif

