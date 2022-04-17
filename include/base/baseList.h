/* 
 *
 */
 
#ifndef __BASE_LIST_H__
#define __BASE_LIST_H__

/**
 * @brief Linked List data structure.
 */

#include <_baseTypes.h>

BASE_BEGIN_DECL

/*
 * @defgroup BASE_DS Data Structure.
 */

/**
 * @defgroup BASE_LIST Linked List
 * @ingroup BASE_DS
 * @{
 *
 * List in  is implemented as doubly-linked list, and it won't require
 * dynamic memory allocation (just as all  data structures). The list here
 * should be viewed more like a low level C list instead of high level C++ list
 * (which normally are easier to use but require dynamic memory allocations),
 * therefore all caveats with C list apply here too (such as you can NOT put
 * a node in more than one lists).
 *
 * \section blist_example_sec Examples
 *
 * See below for examples on how to manipulate linked list:
 *  - @ref page_baselib_samples_list_c
 *  - @ref page_baselib_list_test
 */


/**
 * Use this macro in the start of the structure declaration to declare that
 * the structure can be used in the linked list operation. This macro simply
 * declares additional member @a prev and @a next to the structure.
 * @hideinitializer
 */
#define BASE_DECL_LIST_MEMBER(type)	\
                                   type *prev;		\
                                   type *next 


/**
 * This structure describes generic list node and list. The owner of this list
 * must initialize the 'value' member to an appropriate value (typically the
 * owner itself).
 */
struct _blist
{
	BASE_DECL_LIST_MEMBER(void);
} BASE_ATTR_MAY_ALIAS; /* may_alias avoids warning with gcc-4.4 -Wall -O2 */


/**
 * Initialize the list.
 * Initially, the list will have no member, and function blist_empty() will
 * always return nonzero (which indicates TRUE) for the newly initialized 
 * list.
 *
 * @param node The list head.
 */
BASE_INLINE_SPECIFIER void blist_init(blist_type * node)
{
	((blist*)node)->next = ((blist*)node)->prev = node;
}


/**
 * Check that the list is empty.
 *
 * @param node	The list head.
 *
 * @return Non-zero if the list is empty, or zero if it is not empty.
 *
 */
BASE_INLINE_SPECIFIER int blist_empty(const blist_type * node)
{
	return ((blist*)node)->next == node;
}


/**
 * Insert the node to the list before the specified element position.
 *
 * @param pos	The element to which the node will be inserted before. 
 * @param node	The element to be inserted.
 *
 * @return void.
 */
void	blist_insert_before(blist_type *pos, blist_type *node);


/**
 * Insert the node to the back of the list. This is just an alias for
 * #blist_insert_before().
 *
 * @param list	The list. 
 * @param node	The element to be inserted.
 */
BASE_INLINE_SPECIFIER void blist_push_back(blist_type *list, blist_type *node)
{
    blist_insert_before(list, node);
}


/**
 * Inserts all nodes in \a nodes to the target list.
 * @param lst	    The target list.
 * @param nodes	    Nodes list.
 */
void blist_insert_nodes_before(blist_type *lst, blist_type *nodes);

/**
 * Insert a node to the list after the specified element position.
 *
 * @param pos	    The element in the list which will precede the inserted element.
 * @param node	    The element to be inserted after the position element.
 *
 * @return void.
 */
void blist_insert_after(blist_type *pos, blist_type *node);


/**
 * Insert the node to the front of the list. This is just an alias for
 * #blist_insert_after().
 *
 * @param list	The list. 
 * @param node	The element to be inserted.
 */
BASE_INLINE_SPECIFIER void blist_push_front(blist_type *list, blist_type *node)
{
	blist_insert_after(list, node);
}


/**
 * Insert all nodes in \a nodes to the target list.
 *
 * @param lst	    The target list.
 * @param nodes	    Nodes list.
 */
void blist_insert_nodes_after(blist_type *lst, blist_type *nodes);


/**
 * Remove elements from the source list, and insert them to the destination
 * list. The elements of the source list will occupy the
 * front elements of the target list. Note that the node pointed by \a list2
 * itself is not considered as a node, but rather as the list descriptor, so
 * it will not be inserted to the \a list1. The elements to be inserted starts
 * at \a list2->next. If \a list2 is to be included in the operation, use
 * \a blist_insert_nodes_before.
 *
 * @param list1	The destination list.
 * @param list2	The source list.
 *
 * @return void.
 */
void blist_merge_first(blist_type *list1, blist_type *list2);


/**
 * Remove elements from the second list argument, and insert them to the list 
 * in the first argument. The elements from the second list will be appended
 * to the first list. Note that the node pointed by \a list2
 * itself is not considered as a node, but rather as the list descriptor, so
 * it will not be inserted to the \a list1. The elements to be inserted starts
 * at \a list2->next. If \a list2 is to be included in the operation, use
 * \a blist_insert_nodes_before.
 *
 * @param list1	    The element in the list which will precede the inserted element.
 * @param list2	    The element in the list to be inserted.
 *
 * @return void.
 */
void blist_merge_last( blist_type *list1, blist_type *list2);


/**
 * Erase the node from the list it currently belongs.
 *
 * @param node	    The element to be erased.
 */
void blist_erase(blist_type *node);


/**
 * Find node in the list.
 *
 * @param list	    The list head.
 * @param node	    The node element to be searched.
 *
 * @return The node itself if it is found in the list, or NULL if it is not found in the list.
 */
blist_type* blist_find_node(blist_type *list, blist_type *node);


/**
 * Search the list for the specified value, using the specified comparison
 * function. This function iterates on nodes in the list, started with the
 * first node, and call the user supplied comparison function until the
 * comparison function returns ZERO.
 *
 * @param list	    The list head.
 * @param value	    The user defined value to be passed in the comparison function
 * @param comp	    The comparison function, which should return ZERO to 
 *		    indicate that the searched value is found.
 *
 * @return The first node that matched, or NULL if it is not found.
 */
blist_type* blist_search(blist_type *list, void *value, int (*comp)(void *value, const blist_type *node) );


/**
 * Traverse the list to get the number of elements in the list.
 *
 * @param list	    The list head.
 *
 * @return	    Number of elements.
 */
bsize_t blist_size(const blist_type *list);


#if BASE_FUNCTIONS_ARE_INLINED
#include "extList_i.h"
#endif

BASE_END_DECL

#endif

