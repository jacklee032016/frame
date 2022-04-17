/* $Id: hash_test.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <baseHash.h>
#include <baseRand.h>
#include <baseLog.h>
#include <basePool.h>
#include "testBaseTest.h"

#if INCLUDE_HASH_TEST

#define HASH_COUNT  31

static int hash_test_with_key(bpool_t *pool, unsigned char key)
{
    bhash_table_t *ht;
    unsigned value = 0x12345;
    bhash_iterator_t it_buf, *it;
    unsigned *entry;

    ht = bhash_create(pool, HASH_COUNT);
    if (!ht)
	return -10;

    bhash_set(pool, ht, &key, sizeof(key), 0, &value);

    entry = (unsigned*) bhash_get(ht, &key, sizeof(key), NULL);
    if (!entry)
	return -20;

    if (*entry != value)
	return -30;

    if (bhash_count(ht) != 1)
	return -30;

    it = bhash_first(ht, &it_buf);
    if (it == NULL)
	return -40;

    entry = (unsigned*) bhash_this(ht, it);
    if (!entry)
	return -50;

    if (*entry != value)
	return -60;

    it = bhash_next(ht, it);
    if (it != NULL)
	return -70;

    /* Erase item */

    bhash_set(NULL, ht, &key, sizeof(key), 0, NULL);

    if (bhash_get(ht, &key, sizeof(key), NULL) != NULL)
	return -80;

    if (bhash_count(ht) != 0)
	return -90;

    it = bhash_first(ht, &it_buf);
    if (it != NULL)
	return -100;

    return 0;
}


static int hash_collision_test(bpool_t *pool)
{
    enum {
	COUNT = HASH_COUNT * 4
    };
    bhash_table_t *ht;
    bhash_iterator_t it_buf, *it;
    unsigned char *values;
    unsigned i;

    ht = bhash_create(pool, HASH_COUNT);
    if (!ht)
	return -200;

    values = (unsigned char*) bpool_alloc(pool, COUNT);

    for (i=0; i<COUNT; ++i) {
	values[i] = (unsigned char)i;
	bhash_set(pool, ht, &i, sizeof(i), 0, &values[i]);
    }

    if (bhash_count(ht) != COUNT)
	return -210;

    for (i=0; i<COUNT; ++i) {
	unsigned char *entry;
	entry = (unsigned char*) bhash_get(ht, &i, sizeof(i), NULL);
	if (!entry)
	    return -220;
	if (*entry != values[i])
	    return -230;
    }

    i = 0;
    it = bhash_first(ht, &it_buf);
    while (it) {
	++i;
	it = bhash_next(ht, it);
    }

    if (i != COUNT)
	return -240;

    return 0;
}


/*
 * Hash table test.
 */
int hash_test(void)
{
    bpool_t *pool = bpool_create(mem, "hash", 512, 512, NULL);
    int rc;
    unsigned i;

    /* Test to fill in each row in the table */
    for (i=0; i<=HASH_COUNT; ++i) {
	rc = hash_test_with_key(pool, (unsigned char)i);
	if (rc != 0) {
	    bpool_release(pool);
	    return rc;
	}
    }

    /* Collision test */
    rc = hash_collision_test(pool);
    if (rc != 0) {
	bpool_release(pool);
	return rc;
    }

    bpool_release(pool);
    return 0;
}

#endif	/* INCLUDE_HASH_TEST */

