/* 
 *
 */
#include "testBaseTest.h"

/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_fifobuf_test;

#if INCLUDE_FIFOBUF_TEST

#include <libBase.h>

int fifobuf_test()
{
    enum { SIZE = 1024, MAX_ENTRIES = 128, 
	   MIN_SIZE = 4, MAX_SIZE = 64, 
	   LOOP=10000 };
    bpool_t *pool;
    bfifobuf_t fifo;
    unsigned available = SIZE;
    void *entries[MAX_ENTRIES];
    void *buffer;
    int i;

    pool = bpool_create(mem, NULL, SIZE+256, 0, NULL);
    if (!pool)
	return -10;

    buffer = bpool_alloc(pool, SIZE);
    if (!buffer)
	return -20;

    bfifobuf_init (&fifo, buffer, SIZE);
    
    // Test 1
    for (i=0; i<LOOP*MAX_ENTRIES; ++i)
    {
        	int size;
        	int c, f;
        	c = i%2;
        	f = (i+1)%2;

            do {
        	    size = MIN_SIZE+(brand() % MAX_SIZE);
        	    entries[c] = bfifobuf_alloc (&fifo, size);
        	} while (entries[c] == 0);
            
        	if ( i!=0) {
        	    bfifobuf_free(&fifo, entries[f]);
        	}
    }
    
    if (entries[(i+1)%2])
    	bfifobuf_free(&fifo, entries[(i+1)%2]);

    if (bfifobuf_max_size(&fifo) < SIZE-4) {
    	bassert(0);
    	return -1;
    }

    // Test 2
    entries[0] = bfifobuf_alloc (&fifo, MIN_SIZE);
    if (!entries[0]) return -1;
    for (i=0; i<LOOP*MAX_ENTRIES; ++i) {
        	int size = MIN_SIZE+(brand() % MAX_SIZE);
        	entries[1] = bfifobuf_alloc (&fifo, size);
        	if (entries[1])
        	    bfifobuf_unalloc(&fifo, entries[1]);
    }
    
    bfifobuf_unalloc(&fifo, entries[0]);
    
    if (bfifobuf_max_size(&fifo) < SIZE-4) {
    	bassert(0);
    	return -2;
    }

    // Test 3
    for (i=0; i<LOOP; ++i) {
	int count, j;
	for (count=0; available>=MIN_SIZE+4 && count < MAX_ENTRIES;) {
	    int size = MIN_SIZE+(brand() % MAX_SIZE);
	    entries[count] = bfifobuf_alloc (&fifo, size);
	    if (entries[count]) {
		available -= (size+4);
		++count;
	    }
	}
	for (j=0; j<count; ++j) {
	    bfifobuf_free (&fifo, entries[j]);
	}
	available = SIZE;
    }

    if (bfifobuf_max_size(&fifo) < SIZE-4) {
	bassert(0);
	return -3;
    }
    bpool_release(pool);
    return 0;
}

#endif	/* INCLUDE_FIFOBUF_TEST */


