/* 
 *
 */
#include <baseCtype.h>
/*
char bhex_digits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
			'8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
*/


#include <baseOs.h>

void btime_val_normalize(btime_val *t)
{
	BASE_CHECK_STACK();

	if (t->msec >= 1000)
	{
		t->sec += (t->msec / 1000);
		t->msec = (t->msec % 1000);
	}
	else if (t->msec <= -1000)
	{
		do
		{
			t->sec--;
			t->msec += 1000;
		} while (t->msec <= -1000);
	}

	if (t->sec >= 1 && t->msec < 0)
	{
		t->sec--;
		t->msec += 1000;
	}
	else if (t->sec < 0 && t->msec > 0)
	{
		t->sec++;
		t->msec -= 1000;
	}
}


