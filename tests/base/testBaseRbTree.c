/*
 *
 */
#include "testBaseTest.h"

#if INCLUDE_RBTREE_TEST

#include <libBase.h>

#define LOOP	    32
#define MIN_COUNT   64
#define MAX_COUNT   (LOOP * MIN_COUNT)
#define STRSIZE	    16

typedef struct node_key
{
    buint32_t hash;
    char str[STRSIZE];
} node_key;

static int compare_node(const node_key *k1, const node_key *k2)
{
    if (k1->hash == k2->hash) {
	return strcmp(k1->str, k2->str);
    } else {
	return k1->hash	< k2->hash ? -1 : 1;
    }
}

void randomize_string(char *str, int len)
{
    int i;
    for (i=0; i<len-1; ++i)
	str[i] = (char)('a' + brand() % 26);
    str[len-1] = '\0';
}

static int test(void)
{
    brbtree rb;
    node_key *key;
    brbtree_node *node;
    bpool_t *pool;
    int err=0;
    int count = MIN_COUNT;
    int i;
    unsigned size;

    brbtree_init(&rb, (brbtree_comp*)&compare_node);
    size = MAX_COUNT*(sizeof(*key)+BASE_RBTREE_NODE_SIZE) + 
			   BASE_RBTREE_SIZE + BASE_POOL_SIZE;
    pool = bpool_create( mem, "pool", size, 0, NULL);
    if (!pool) {
	BASE_ERROR("...error: creating pool of %u bytes", size);
	return -10;
    }

    key = (node_key *)bpool_alloc(pool, MAX_COUNT*sizeof(*key));
    if (!key)
	return -20;

    node = (brbtree_node*)bpool_alloc(pool, MAX_COUNT*sizeof(*node));
    if (!node)
	return -30;

    for (i=0; i<LOOP; ++i) {
	int j;
	brbtree_node *prev, *it;
	btimestamp t1, t2, t_setup, t_insert, t_search, t_erase;

	bassert(rb.size == 0);

	t_setup.u32.lo = t_insert.u32.lo = t_search.u32.lo = t_erase.u32.lo = 0;

	for (j=0; j<count; j++) {
	    randomize_string(key[j].str, STRSIZE);

	    bTimeStampGet(&t1);
	    node[j].key = &key[j];
	    node[j].user_data = key[j].str;
	    key[j].hash = bhash_calc(0, key[j].str, BASE_HASH_KEY_STRING);
	    bTimeStampGet(&t2);
	    t_setup.u32.lo += (t2.u32.lo - t1.u32.lo);

	    bTimeStampGet(&t1);
	    brbtree_insert(&rb, &node[j]);
	    bTimeStampGet(&t2);
	    t_insert.u32.lo += (t2.u32.lo - t1.u32.lo);
	}

	bassert(rb.size == (unsigned)count);

	// Iterate key, make sure they're sorted.
	prev = NULL;
	it = brbtree_first(&rb);
	while (it) {
	    if (prev) {
		if (compare_node((node_key*)prev->key,(node_key*)it->key)>=0) {
		    ++err;
		    BASE_ERROR("Error: %s >= %s", (char*)prev->user_data, (char*)it->user_data);
		}
	    }
	    prev = it;
	    it = brbtree_next(&rb, it);
	}

	// Search.
	for (j=0; j<count; j++) {
	    bTimeStampGet(&t1);
	    it = brbtree_find(&rb, &key[j]);
	    bTimeStampGet(&t2);
	    t_search.u32.lo += (t2.u32.lo - t1.u32.lo);

	    bassert(it != NULL);
	    if (it == NULL)
		++err;
	}

	// Erase node.
	for (j=0; j<count; j++) {
	    bTimeStampGet(&t1);
	    it = brbtree_erase(&rb, &node[j]);
	    bTimeStampGet(&t2);
	    t_erase.u32.lo += (t2.u32.lo - t1.u32.lo);
	}

	BASE_INFO("...count:%d, setup:%d, insert:%d, search:%d, erase:%d",
		count,
		t_setup.u32.lo / count, t_insert.u32.lo / count,
		t_search.u32.lo / count, t_erase.u32.lo / count);

	count = 2 * count;
	if (count > MAX_COUNT)
	    break;
    }

    bpool_release(pool);
    return err;
}


int rbtree_test()
{
    return test();
}

#endif


