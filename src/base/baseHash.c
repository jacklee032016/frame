/* 
 *
 */
#include <baseHash.h>
#include <baseLog.h>
#include <baseString.h>
#include <basePool.h>
#include <baseOs.h>
#include <baseCtype.h>
#include <baseAssert.h>

/**
 * The hash multiplier used to calculate hash value.
 */
#define BASE_HASH_MULTIPLIER	33


struct _bhash_entry
{
	struct _bhash_entry		*next;
	void						*key;
	buint32_t				hash;
	buint32_t				keylen;
	void						*value;
};


struct _bhash_table_t
{
    bhash_entry     **table;
    unsigned		count, rows;
    bhash_iterator_t	iterator;
};



buint32_t bhash_calc(buint32_t hash, const void *key, 
				 unsigned keylen)
{
    BASE_CHECK_STACK();

    if (keylen==BASE_HASH_KEY_STRING) {
	const buint8_t *p = (const buint8_t*)key;
	for ( ; *p; ++p ) {
	    hash = (hash * BASE_HASH_MULTIPLIER) + *p;
	}
    } else {
	const buint8_t *p = (const buint8_t*)key,
			      *end = p + keylen;
	for ( ; p!=end; ++p) {
	    hash = (hash * BASE_HASH_MULTIPLIER) + *p;
	}
    }
    return hash;
}

buint32_t bhash_calc_tolower( buint32_t hval,
                                          char *result,
                                          const bstr_t *key)
{
    long i;

    for (i=0; i<key->slen; ++i) {
        int lower = btolower(key->ptr[i]);
	if (result)
	    result[i] = (char)lower;

	hval = hval * BASE_HASH_MULTIPLIER + lower;
    }

    return hval;
}


bhash_table_t* bhash_create(bpool_t *pool, unsigned size)
{
    bhash_table_t *h;
    unsigned table_size;
    
    /* Check that BASE_HASH_ENTRY_BUF_SIZE is correct. */
    BASE_ASSERT_RETURN(sizeof(bhash_entry)<=BASE_HASH_ENTRY_BUF_SIZE, NULL);

    h = BASE_POOL_ALLOC_T(pool, bhash_table_t);
    h->count = 0;

    BASE_INFO("hash table %p created from pool %s", h, bpool_getobjname(pool));

    /* size must be 2^n - 1.
       round-up the size to this rule, except when size is 2^n, then size
       will be round-down to 2^n-1.
     */
    table_size = 8;
    do {
	table_size <<= 1;    
    } while (table_size < size);
    table_size -= 1;
    
    h->rows = table_size;
    h->table = (bhash_entry**)
    	       bpool_calloc(pool, table_size+1, sizeof(bhash_entry*));
    return h;
}

static bhash_entry **find_entry( bpool_t *pool, bhash_table_t *ht, 
				   const void *key, unsigned keylen,
				   void *val, buint32_t *hval,
				   void *entry_buf, bbool_t lower)
{
    buint32_t hash;
    bhash_entry **p_entry, *entry;

    if (hval && *hval != 0) {
	hash = *hval;
	if (keylen==BASE_HASH_KEY_STRING) {
	    keylen = (unsigned)bansi_strlen((const char*)key);
	}
    } else {
	/* This slightly differs with bhash_calc() because we need 
	 * to get the keylen when keylen is BASE_HASH_KEY_STRING.
	 */
	hash=0;
	if (keylen==BASE_HASH_KEY_STRING) {
	    const buint8_t *p = (const buint8_t*)key;
	    for ( ; *p; ++p ) {
                if (lower)
                    hash = hash * BASE_HASH_MULTIPLIER + btolower(*p);
                else 
		    hash = hash * BASE_HASH_MULTIPLIER + *p;
	    }
	    keylen = (unsigned)(p - (const unsigned char*)key);
	} else {
	    const buint8_t *p = (const buint8_t*)key,
				  *end = p + keylen;
	    for ( ; p!=end; ++p) {
		if (lower)
                    hash = hash * BASE_HASH_MULTIPLIER + btolower(*p);
                else
                    hash = hash * BASE_HASH_MULTIPLIER + *p;
	    }
	}

	/* Report back the computed hash. */
	if (hval)
	    *hval = hash;
    }

    /* scan the linked list */
    for (p_entry = &ht->table[hash & ht->rows], entry=*p_entry; 
	 entry; 
	 p_entry = &entry->next, entry = *p_entry)
    {
	if (entry->hash==hash && entry->keylen==keylen &&
            ((lower && bansi_strnicmp((const char*)entry->key,
        			        (const char*)key, keylen)==0) ||
	     (!lower && bmemcmp(entry->key, key, keylen)==0)))
	{
	    break;
	}
    }

    if (entry || val==NULL)
	return p_entry;

    /* Entry not found, create a new one. 
     * If entry_buf is specified, use it. Otherwise allocate from pool.
     */
    if (entry_buf) {
	entry = (bhash_entry*)entry_buf;
    } else {
	/* Pool must be specified! */
	BASE_ASSERT_RETURN(pool != NULL, NULL);

	entry = BASE_POOL_ALLOC_T(pool, bhash_entry);
	BASE_INFO( "%p: New p_entry %p created, pool used=%u, cap=%u", ht, entry,  bpool_get_used_size(pool), bpool_get_capacity(pool));
    }
    entry->next = NULL;
    entry->hash = hash;
    if (pool) {
	entry->key = bpool_alloc(pool, keylen);
	bmemcpy(entry->key, key, keylen);
    } else {
	entry->key = (void*)key;
    }
    entry->keylen = keylen;
    entry->value = val;
    *p_entry = entry;
    
    ++ht->count;
    
    return p_entry;
}

void * bhash_get( bhash_table_t *ht,
			    const void *key, unsigned keylen,
			    buint32_t *hval)
{
    bhash_entry *entry;
    entry = *find_entry( NULL, ht, key, keylen, NULL, hval, NULL, BASE_FALSE);
    return entry ? entry->value : NULL;
}

void * bhash_get_lower( bhash_table_t *ht,
			          const void *key, unsigned keylen,
			          buint32_t *hval)
{
    bhash_entry *entry;
    entry = *find_entry( NULL, ht, key, keylen, NULL, hval, NULL, BASE_TRUE);
    return entry ? entry->value : NULL;
}

static void hash_set( bpool_t *pool, bhash_table_t *ht,
	              const void *key, unsigned keylen, buint32_t hval,
		      void *value, void *entry_buf, bbool_t lower )
{
    bhash_entry **p_entry;

    p_entry = find_entry( pool, ht, key, keylen, value, &hval, entry_buf,
                          lower);
    if (*p_entry) {
	if (value == NULL) {
	    /* delete entry */
	    BASE_INFO("%p: p_entry %p deleted", ht, *p_entry);
		*p_entry = (*p_entry)->next;
	    --ht->count;
	    
	} else {
	    /* overwrite */
	    (*p_entry)->value = value;
	    BASE_INFO("%p: p_entry %p value set to %p", ht, *p_entry, value);
	}
    }
}

void bhash_set( bpool_t *pool, bhash_table_t *ht,
			  const void *key, unsigned keylen, buint32_t hval,
			  void *value )
{
    hash_set(pool, ht, key, keylen, hval, value, NULL, BASE_FALSE);
}

void bhash_set_lower( bpool_t *pool, bhash_table_t *ht,
			        const void *key, unsigned keylen,
                                buint32_t hval, void *value )
{
    hash_set(pool, ht, key, keylen, hval, value, NULL, BASE_TRUE);
}

void bhash_set_np( bhash_table_t *ht,
			     const void *key, unsigned keylen, 
			     buint32_t hval, bhash_entry_buf entry_buf, 
			     void *value)
{
    hash_set(NULL, ht, key, keylen, hval, value, (void *)entry_buf, BASE_FALSE);
}

void bhash_set_np_lower( bhash_table_t *ht,
			           const void *key, unsigned keylen,
			           buint32_t hval,
                                   bhash_entry_buf entry_buf,
			           void *value)
{
    hash_set(NULL, ht, key, keylen, hval, value, (void *)entry_buf, BASE_TRUE);
}

unsigned bhash_count( bhash_table_t *ht )
{
    return ht->count;
}

bhash_iterator_t* bhash_first( bhash_table_t *ht,
					   bhash_iterator_t *it )
{
    it->index = 0;
    it->entry = NULL;

    for (; it->index <= ht->rows; ++it->index) {
	it->entry = ht->table[it->index];
	if (it->entry) {
	    break;
	}
    }

    return it->entry ? it : NULL;
}

bhash_iterator_t* bhash_next( bhash_table_t *ht, 
					  bhash_iterator_t *it )
{
    it->entry = it->entry->next;
    if (it->entry) {
	return it;
    }

    for (++it->index; it->index <= ht->rows; ++it->index) {
	it->entry = ht->table[it->index];
	if (it->entry) {
	    break;
	}
    }

    return it->entry ? it : NULL;
}

void* bhash_this( bhash_table_t *ht, bhash_iterator_t *it )
{
    BASE_CHECK_STACK();
    BASE_UNUSED_ARG(ht);
    return it->entry->value;
}

#if 0
void bhash_dump_collision( bhash_table_t *ht )
{
    unsigned min=0xFFFFFFFF, max=0;
    unsigned i;
    char line[120];
    int len, totlen = 0;

    for (i=0; i<=ht->rows; ++i) {
	unsigned count = 0;    
	bhash_entry *entry = ht->table[i];
	while (entry) {
	    ++count;
	    entry = entry->next;
	}
	if (count < min)
	    min = count;
	if (count > max)
	    max = count;
	len = bsnprintf( line+totlen, sizeof(line)-totlen, "%3d:%3d ", i, count);
	if (len < 1)
	    break;
	totlen += len;

	if ((i+1) % 10 == 0) {
	    line[totlen] = '\0';
	    BASE_INFO( line);
	}
    }

    BASE_INFO("Count: %d, min: %d, max: %d\n", ht->count, min, max);
}
#endif


