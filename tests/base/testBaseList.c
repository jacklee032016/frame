/* $Id: list.c 4537 2013-06-19 06:47:43Z riza $ */
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
#include "testBaseTest.h"

/**
 * \page page_baselib_list_test Test: Linked List
 *
 * This file provides implementation of \b list_test(). It tests the
 * functionality of the linked-list API.
 *
 * \section list_test_sec Scope of the Test
 *
 * API tested:
 *  - blist_init()
 *  - blist_insert_before()
 *  - blist_insert_after()
 *  - blist_merge_last()
 *  - blist_empty()
 *  - blist_insert_nodes_before()
 *  - blist_erase()
 *  - blist_find_node()
 *  - blist_search()
 */

#if INCLUDE_LIST_TEST

#include <libBase.h>

typedef struct _list_node
{
	BASE_DECL_LIST_MEMBER(struct _list_node);
	int value;
} list_node;

static int compare_node(void *value, const blist_type *nd)
{
	list_node *node = (list_node*)nd;
	return ((long)(bssize_t)value == node->value) ? 0 : -1;
}

#define BASE_SIGNED_ARRAY_SIZE(a)	((int)BASE_ARRAY_SIZE(a))

int list_test()
{
	list_node nodes[4];    // must be even number of nodes
	list_node list;
	list_node list2;
	list_node *p;
	int i; // don't change to unsigned!

	// Test insert_before().
	list.value = (unsigned)-1;
	blist_init(&list);
	for (i=0; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i)
	{
		nodes[i].value = i;
		blist_insert_before(&list, &nodes[i]);
	}
	
	// check.
	for (i=0, p=list.next; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i, p=p->next)
	{
		bassert(p->value == i);
		if (p->value != i)
		{
			return -1;
		}
	}


	// Test insert_after()
	blist_init(&list);
	for (i=BASE_SIGNED_ARRAY_SIZE(nodes)-1; i>=0; --i)
	{
		blist_insert_after(&list, &nodes[i]);
	}
	
	// check.
	for (i=0, p=list.next; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i, p=p->next)
	{
		bassert(p->value == i);
		if (p->value != i)
		{
			return -1;
		}
	}


	// Test merge_last()

	// Init lists
	blist_init(&list);
	blist_init(&list2);
	for (i=0; i<BASE_SIGNED_ARRAY_SIZE(nodes)/2; ++i)
	{
		blist_insert_before(&list, &nodes[i]);
	}
	
	for (i=BASE_SIGNED_ARRAY_SIZE(nodes)/2; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i)
	{
		blist_insert_before(&list2, &nodes[i]);
	}
	
	// merge
	blist_merge_last(&list, &list2);
	// check.
	for (i=0, p=list.next; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i, p=p->next)
	{
		bassert(p->value == i);
		if (p->value != i)
		{
			return -1;
		}
	}
	
	// check list is empty
	bassert( blist_empty(&list2) );
	if (!blist_empty(&list2))
	{
		return -1;
	}


	// Check merge_first()
	//
	blist_init(&list);
	blist_init(&list2);
	for (i=0; i<BASE_SIGNED_ARRAY_SIZE(nodes)/2; ++i)
	{
		blist_insert_before(&list, &nodes[i]);
	}
	
	for (i=BASE_SIGNED_ARRAY_SIZE(nodes)/2; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i)
	{
		blist_insert_before(&list2, &nodes[i]);
	}
	
	// merge
	blist_merge_first(&list2, &list);
	// check (list2).
	for (i=0, p=list2.next; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i, p=p->next)
	{
		bassert(p->value == i);
		if (p->value != i)
		{
			return -1;
		}
	}
	
	// check list is empty
	bassert( blist_empty(&list) );
	if (!blist_empty(&list))
	{
		return -1;
	}


	// Test insert_nodes_before()
	//
	// init list
	blist_init(&list);
	for (i=0; i<BASE_SIGNED_ARRAY_SIZE(nodes)/2; ++i)
	{
		blist_insert_before(&list, &nodes[i]);
	}
	
	// chain remaining nodes
	blist_init(&nodes[BASE_SIGNED_ARRAY_SIZE(nodes)/2]);
	for (i=BASE_SIGNED_ARRAY_SIZE(nodes)/2+1; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i)
	{
		blist_insert_before(&nodes[BASE_SIGNED_ARRAY_SIZE(nodes)/2], &nodes[i]);
	}
	
	// insert nodes
	blist_insert_nodes_before(&list, &nodes[BASE_SIGNED_ARRAY_SIZE(nodes)/2]);
	// check
	for (i=0, p=list.next; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i, p=p->next)
	{
		bassert(p->value == i);
		if (p->value != i)
		{
			return -1;
		}
	}

	// erase test.
	blist_init(&list);
	for (i=0; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i)
	{
		nodes[i].value = i;
		blist_insert_before(&list, &nodes[i]);
	}
	
	for (i=BASE_SIGNED_ARRAY_SIZE(nodes)-1; i>=0; --i)
	{
		int j;
		blist_erase(&nodes[i]);
		for (j=0, p=list.next; j<i; ++j, p=p->next)
		{
			bassert(p->value == j);
			if (p->value != j)
			{
				return -1;
			}
		}
	}


	// find and search
	blist_init(&list);
	for (i=0; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i)
	{
		nodes[i].value = i;
		blist_insert_before(&list, &nodes[i]);
	}
	
	for (i=0; i<BASE_SIGNED_ARRAY_SIZE(nodes); ++i)
	{
		p = (list_node*) blist_find_node(&list, &nodes[i]);
		bassert( p == &nodes[i] );
		if (p != &nodes[i])
		{
			return -1;
		}

		p = (list_node*) blist_search(&list, (void*)(bssize_t)i, &compare_node);
		bassert( p == &nodes[i] );
		if (p != &nodes[i])
		{
			return -1;
		}
	}
	
	return 0;
}

#else
/* To prevent warning about "translation unit is empty"
 * when this test is disabled. 
 */
int dummy_list_test;
#endif	/* INCLUDE_LIST_TEST */


