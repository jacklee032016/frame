/* 
 *
 */
#ifndef __BASE_HASH_H__
#define __BASE_HASH_H__

/**
 * @brief Hash Table.
 */

#include <_baseTypes.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_HASH Hash Table
 * @ingroup BASE_DS
 * @{
 * A hash table is a dictionary in which keys are mapped to array positions by
 * hash functions. Having the keys of more than one item map to the same 
 * position is called a collision. In this library, we will chain the nodes
 * that have the same key in a list.
 */

/**
 * If this constant is used as keylen, then the key is interpreted as
 * NULL terminated string.
 */
#define BASE_HASH_KEY_STRING	((unsigned)-1)

/**
 * This indicates the size of of each hash entry.
 */
#define BASE_HASH_ENTRY_BUF_SIZE	(3*sizeof(void*) + 2*sizeof(buint32_t))

/**
 * Type declaration for entry buffer, used by #bhash_set_np()
 */
typedef void *bhash_entry_buf[(BASE_HASH_ENTRY_BUF_SIZE+sizeof(void*)-1)/(sizeof(void*))];

/**
 * This is the function that is used by the hash table to calculate hash value
 * of the specified key.
 *
 * @param hval	    the initial hash value, or zero.
 * @param key	    the key to calculate.
 * @param keylen    the length of the key, or BASE_HASH_KEY_STRING to treat 
 *		    the key as null terminated string.
 *
 * @return          the hash value.
 */
buint32_t bhash_calc(buint32_t hval, const void *key, unsigned keylen);


/**
 * Convert the key to lowercase and calculate the hash value. The resulting
 * string is stored in \c result.
 *
 * @param hval      The initial hash value, normally zero.
 * @param result    Optional. Buffer to store the result, which must be enough
 *                  to hold the string.
 * @param key       The input key to be converted and calculated.
 *
 * @return          The hash value.
 */
buint32_t bhash_calc_tolower(buint32_t hval,
                                          char *result,
                                          const bstr_t *key);

/**
 * Create a hash table with the specified 'bucket' size.
 *
 * @param pool	the pool from which the hash table will be allocated from.
 * @param size	the bucket size, which will be round-up to the nearest 2^n-1
 *
 * @return the hash table.
 */
bhash_table_t* bhash_create(bpool_t *pool, unsigned size);


/**
 * Get the value associated with the specified key.
 *
 * @param ht	    the hash table.
 * @param key	    the key to look for.
 * @param keylen    the length of the key, or BASE_HASH_KEY_STRING to use the
 *		    string length of the key.
 * @param hval	    if this argument is not NULL and the value is not zero,
 *		    the value will be used as the computed hash value. If
 *		    the argument is not NULL and the value is zero, it will
 *		    be filled with the computed hash upon return.
 *
 * @return the value associated with the key, or NULL if the key is not found.
 */
void * bhash_get( bhash_table_t *ht,
			     const void *key, unsigned keylen,
			     buint32_t *hval );


/**
 * Variant of #bhash_get() with the key being converted to lowercase when
 * calculating the hash value.
 *
 * @see bhash_get()
 */
void * bhash_get_lower( bhash_table_t *ht,
			           const void *key, unsigned keylen,
			           buint32_t *hval );


/**
 * Associate/disassociate a value with the specified key. If value is not
 * NULL and entry already exists, the entry's value will be overwritten.
 * If value is not NULL and entry does not exist, a new one will be created
 * with the specified pool. Otherwise if value is NULL, entry will be
 * deleted if it exists.
 *
 * @param pool	    the pool to allocate the new entry if a new entry has to be
 *		    created.
 * @param ht	    the hash table.
 * @param key	    the key. If pool is not specified, the key MUST point to
 * 		    buffer that remains valid for the duration of the entry.
 * @param keylen    the length of the key, or BASE_HASH_KEY_STRING to use the 
 *		    string length of the key.
 * @param hval	    if the value is not zero, then the hash table will use
 *		    this value to search the entry's index, otherwise it will
 *		    compute the key. This value can be obtained when calling
 *		    #bhash_get().
 * @param value	    value to be associated, or NULL to delete the entry with
 *		    the specified key.
 */
void bhash_set( bpool_t *pool, bhash_table_t *ht,
			   const void *key, unsigned keylen, buint32_t hval,
			   void *value );


/**
 * Variant of #bhash_set() with the key being converted to lowercase when
 * calculating the hash value.
 *
 * @see bhash_set()
 */
void bhash_set_lower( bpool_t *pool, bhash_table_t *ht,
			         const void *key, unsigned keylen,
                                 buint32_t hval, void *value );


/**
 * Associate/disassociate a value with the specified key. This function works
 * like #bhash_set(), except that it doesn't use pool (hence the np -- no 
 * pool suffix). If new entry needs to be allocated, it will use the entry_buf.
 *
 * @param ht	    the hash table.
 * @param key	    the key.
 * @param keylen    the length of the key, or BASE_HASH_KEY_STRING to use the 
 *		    string length of the key.
 * @param hval	    if the value is not zero, then the hash table will use
 *		    this value to search the entry's index, otherwise it will
 *		    compute the key. This value can be obtained when calling
 *		    #bhash_get().
 * @param entry_buf Buffer which will be used for the new entry, when one needs
 *		    to be created.
 * @param value	    value to be associated, or NULL to delete the entry with
 *		    the specified key.
 */
void bhash_set_np(bhash_table_t *ht,
			     const void *key, unsigned keylen, 
			     buint32_t hval, bhash_entry_buf entry_buf, 
			     void *value);

/**
 * Variant of #bhash_set_np() with the key being converted to lowercase
 * when calculating the hash value.
 *
 * @see bhash_set_np()
 */
void bhash_set_np_lower(bhash_table_t *ht,
			           const void *key, unsigned keylen,
			           buint32_t hval,
                                   bhash_entry_buf entry_buf,
			           void *value);

/**
 * Get the total number of entries in the hash table.
 *
 * @param ht	the hash table.
 *
 * @return the number of entries in the hash table.
 */
unsigned bhash_count( bhash_table_t *ht );


/**
 * Get the iterator to the first element in the hash table. 
 *
 * @param ht	the hash table.
 * @param it	the iterator for iterating hash elements.
 *
 * @return the iterator to the hash element, or NULL if no element presents.
 */
bhash_iterator_t* bhash_first( bhash_table_t *ht,
					    bhash_iterator_t *it );


/**
 * Get the next element from the iterator.
 *
 * @param ht	the hash table.
 * @param it	the hash iterator.
 *
 * @return the next iterator, or NULL if there's no more element.
 */
bhash_iterator_t* bhash_next( bhash_table_t *ht, 
					   bhash_iterator_t *it );

/**
 * Get the value associated with a hash iterator.
 *
 * @param ht	the hash table.
 * @param it	the hash iterator.
 *
 * @return the value associated with the current element in iterator.
 */
void* bhash_this( bhash_table_t *ht,
			     bhash_iterator_t *it );

BASE_END_DECL

#endif


