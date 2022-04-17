/*
 *
 */
#ifndef __BASE_ARRAY_H__
#define __BASE_ARRAY_H__

#include <_baseTypes.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_ARRAY Array helper.
 * @ingroup BASE_DS
 * @{
 *
 * This module provides helper to manipulate array of elements of any size.
 * It provides most used array operations such as insert, erase, and search.
 */

/**
 * Insert value to the array at the given position, and rearrange the
 * remaining nodes after the position.
 *
 * @param array	    the array.
 * @param elem_size the size of the individual element.
 * @param count	    the CURRENT number of elements in the array.
 * @param pos	    the position where the new element is put.
 * @param value	    the value to copy to the new element.
 */
void barray_insert( void *array,
			       unsigned elem_size,
			       unsigned count,
			       unsigned pos,
			       const void *value);

/**
 * Erase a value from the array at given position, and rearrange the remaining
 * elements post the erased element.
 *
 * @param array	    the array.
 * @param elem_size the size of the individual element.
 * @param count	    the current number of elements in the array.
 * @param pos	    the index/position to delete.
 */
void barray_erase( void *array,
			      unsigned elem_size,
			      unsigned count,
			      unsigned pos);

/**
 * Search the first value in the array according to matching function.
 *
 * @param array	    the array.
 * @param elem_size the individual size of the element.
 * @param count	    the number of elements.
 * @param matching  the matching function, which MUST return BASE_SUCCESS if 
 *		    the specified element match.
 * @param result    the pointer to the value found.
 *
 * @return	    BASE_SUCCESS if value is found, otherwise the error code.
 */
bstatus_t barray_find(   const void *array, 
				      unsigned elem_size, 
				      unsigned count, 
				      bstatus_t (*matching)(const void *value),
				      void **result);

BASE_END_DECL


#endif

