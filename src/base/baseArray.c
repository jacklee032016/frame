/* 
 *
 */
#include <baseArray.h>
#include <baseString.h>
#include <baseAssert.h>
#include <baseErrno.h>

void barray_insert( void *array,
			      unsigned elem_size,
			      unsigned count,
			      unsigned pos,
			      const void *value)
{
    if (count && pos < count) {
	bmemmove( (char*)array + (pos+1)*elem_size,
		    (char*)array + pos*elem_size,
		    (count-pos)*elem_size);
    }
    bmemmove((char*)array + pos*elem_size, value, elem_size);
}

void barray_erase( void *array,
			     unsigned elem_size,
			     unsigned count,
			     unsigned pos)
{
    bassert(count != 0);
    if (pos < count-1) {
	bmemmove( (char*)array + pos*elem_size,
		    (char*)array + (pos+1)*elem_size,
		    (count-pos-1)*elem_size);
    }
}

bstatus_t barray_find( const void *array, 
				   unsigned elem_size, 
				   unsigned count, 
				   bstatus_t (*matching)(const void *value),
				   void **result)
{
    unsigned i;
    const char *char_array = (const char*)array;
    for (i=0; i<count; ++i) {
	if ( (*matching)(char_array) == BASE_SUCCESS) {
	    if (result) {
		*result = (void*)char_array;
	    }
	    return BASE_SUCCESS;
	}
	char_array += elem_size;
    }
    return BASE_ENOTFOUND;
}

