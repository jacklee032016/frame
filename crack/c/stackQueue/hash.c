#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#include <assert.h>

/*
* Important points in Hash Table
*   1. key type: int, string, pointer, pointer with type; key must be different for any data
*   2. hash algorithm to get hash value from key
*   3. bucket: contains list or only one items; the first one is prefered
*   4. expand the buckets: double the size of buckets, not size of items; when number of items in one bucket is bigger than threshold
*   5. check expand result, eg. check the hash algorithm is effective or not
*        if more than half threshold items in one bucket, bad hash algorithm
*        if more than half of existed buckets are used, bad hash algorithm
*   6. iterator of hash table
*/

// when number of items in any bucket is bigger than this, then expand the buckets in hash table
#define HASH_EXPAND_THRESHOLD       5

struct hash_bucket
{
    struct hash_bucket  *next;

    void                *key;   // key must be unique
    // useless
//    int                 index;  // save index, to retrive it
    
    void                *data;
};

struct hash_table
{
    uint32_t            capacity;
    
//    int                 size;
    int                 length;

    struct hash_bucket  **buckets;
};


struct  hash_iterator
{
    struct hash_table   *ht;
    int                 index;
    struct hash_bucket  *bucket;
};


uint32_t    _hash_value(void *key)
{
    return (uintptr_t)key;
}

static int _hash_index(struct hash_table *ht, void *key)
{
    int index = -1;
    uint32_t hash = _hash_value(key);
    index = hash&(ht->capacity-1);
    assert( (index < ht->capacity));
    printf("Index %d\n", index);

    return index;
}

struct hash_table*  hash_table_create(uint32_t capacity)
{
    struct hash_table *ht = NULL;
    int size = 0;

    // capacity must be 2**N, so index can be get by AND, not division
    if((capacity &(capacity-1) ) != 0 )
    {
        printf("Capacity must be 2**N, now it's %u\n", capacity);
        return NULL;
    }

    ht = (struct hash_table *)malloc(sizeof(struct hash_table));
    if(!ht)
        return NULL;

    ht->capacity = capacity;
    while(0) //capacity!=0)
    {
        size++;
        capacity >>=1;
    }
    
    ht->length = 0;
    
    printf("Capacity %u\n", capacity);

    ht->buckets = (struct hash_bucket **)malloc(sizeof(struct hash_bucket*)*ht->capacity);
    if(!ht->buckets)
    {
        free(ht);
        ht = NULL;
        return NULL;
    }
    memset(ht->buckets, 0, sizeof(struct hash_bucket*)*ht->capacity );

    return ht;
}

void hash_table_free(struct hash_table *ht)
{
    free(ht->buckets);
    ht->buckets = NULL;
    free(ht);
}

static int _hash_table_expand(struct hash_table *ht)
{
    struct hash_bucket **buckets = NULL;
    struct hash_bucket *current = NULL, *next;
//    struct hash_bucket *newBucket = NULL;
    int i, newIndex;
    int capacity = 0;
    
    if(ht->capacity > ht->capacity*2)
    {// overflow
        assert(0);
        return -1;
    }

    buckets = (struct hash_bucket **)malloc(sizeof(struct hash_bucket*)*ht->capacity*2);
    if(!buckets)
    {
        return -1;
    }
    memset(buckets, 0, sizeof(struct hash_bucket*)*ht->capacity*2);

    capacity = ht->capacity;
    ht->capacity = ht->capacity*2;
    printf("expand from %u to %u...\n", capacity, ht->capacity);
    for(i=0; i< capacity; i++)
    {
        for(current= ht->buckets[i]; current; current= next)
        {
            next = current->next;
            
            newIndex = _hash_index(ht, current->key);
#if 1
            current->next = buckets[newIndex];
            buckets[newIndex] = current;
#else
            newBucket = buckets[newIndex];

            // cut old relationship when move the item from old chains to new chains
            if(!newBucket)
            {
                buckets[newIndex] = current;
                newBucket = current;
            }
            else
            {
                while(newBucket->next)
                {
                    newBucket = newBucket->next;
                }
                newBucket->next = current;
            }
            
            newBucket->next = NULL; // cut the old relation
#endif            
        }
    }

    printf("expand from %u to %u\n", capacity, ht->capacity);
    free(ht->buckets);
    ht->buckets = buckets;

    return 0;
}


void *hash_table_get(struct hash_table *ht, void *key, int *ret_items)
{
    struct hash_bucket *bucket = NULL;
    int index = _hash_index(ht, key); //

    if(ret_items)
        *ret_items = 0;
    
    for(bucket = ht->buckets[index]; bucket; bucket=bucket->next)
    {
        if(memcmp(bucket->key, key, sizeof(void *)) == 0 )
        {
            return bucket->data;
        }
        
        if(ret_items)
            (*ret_items) ++;
    }

    return NULL;
}


void *hash_table_add(struct hash_table *ht, void *key, void *value)
{
    struct hash_bucket *newBucket;
    int item_num_in_bucket = 0; // how many items are in this bucket
    
    int index = _hash_index(ht, key); //

    if(hash_table_get(ht, key, &item_num_in_bucket) )
    {// has existed
        return NULL;
    }

//    if(ht->length > ht->capacity - 20 )
#if 0    
    if(item_num_in_bucket > HASH_EXPAND_THRESHOLD)
#else        
    if(ht->length >= ht->capacity/2 ) // use to test expand algorithm
#endif        
    {
        _hash_table_expand(ht);
        index = _hash_index(ht, key);
    }

    newBucket = (struct hash_bucket *)malloc(sizeof(struct hash_bucket));
    if(!newBucket)
    {
        return NULL;
    }
    
    newBucket->data = value;
    newBucket->key = key;
//    newBucket->index = index;
    newBucket->next = ht->buckets[index];
    ht->buckets[index] = newBucket;
    ht->length++;

    return newBucket->data;
}

void *hash_table_remove(struct hash_table *ht, void *key)
{
    struct hash_bucket *bucket = NULL, *last = NULL;
    int index = _hash_index(ht, key); //
    void *value = NULL;
    
    for(bucket = ht->buckets[index]; bucket; bucket=bucket->next)
    {
        if(memcmp(bucket->key, key, sizeof(void *)) == 0 )
        {
            if(!last)
            {
                ht->buckets[index] = bucket->next;
            }
            else
            {
                last->next = bucket->next;
            }

            value = bucket->data;
            
            free(bucket);
            ht->length--;
            break;
        }
        
        last = bucket;
    }

    return value;
}

uint32_t hash_table_size(struct hash_table *ht)
{
    if(ht)
        return ht->length;
    return 0;
}

uint32_t hash_table_flush(struct hash_table *ht)
{
    int i;
    struct hash_bucket *bucket = NULL, *next = NULL;
    int count = 0;

    // how to accelebrate the speed
//    for(i=0; i< ht->length;i++)
    for(i=0; i< ht->capacity;i++)
    {
        for(bucket = ht->buckets[i]; bucket; bucket = next)
        {
            printf("flush index= %d\n", i);
            next = bucket->next;
            free(bucket);
            bucket = NULL;
            count++;
        }
    }
    
    return count;
}

struct hash_iterator *hash_table_iter(struct hash_table *ht, struct hash_iterator *iter)
{
    if(!iter || !ht )
        return NULL;
    
    iter->ht = ht;
    iter->index = 0;
    iter->bucket = NULL;

    for(; iter->index< ht->capacity; iter->index++)
    {
        iter->bucket = ht->buckets[iter->index];
        if(iter->bucket)
            break;
    }
    
    return iter->bucket?iter:NULL;
}


struct hash_iterator *hash_table_next(struct hash_iterator *iter)
{
    if(iter->bucket->next)
    {
        iter->bucket = iter->bucket->next;
        return iter;
    }

    iter->index++;
    for(; iter->index < iter->ht->capacity; iter->index++)
    {
        iter->bucket = iter->ht->buckets[iter->index];
        if(iter->bucket)
        {
            printf("iter index:%u\n", iter->index);
            break;
        }
    }

    return iter->bucket?iter:NULL;
}


int main(int argc, char *argv[])
{
    int value = 10, value2= 102345, val3 = 908766;
    int *data = NULL;
    int count = 0;
    struct hash_iterator _iter;
    struct hash_iterator *iter = &_iter;
    
    struct hash_table *ht = hash_table_create(255);
    assert(ht==NULL);

//    ht = hash_table_create(256);
    ht = hash_table_create(4);

    hash_table_add(ht, &value, &value);
    hash_table_add(ht, &value2, &value2);
    hash_table_add(ht, &val3, &val3);
    
    assert(hash_table_size(ht) == 3);
    printf("length: %d\n", hash_table_size(ht));

    printf("iterate %u items\n", hash_table_size(ht));
    iter = hash_table_iter(ht, iter);
    while(iter)
    {
        printf("\t#%d: %d\n", count++, (int)*(int *)(iter->bucket->data));
        iter = hash_table_next(iter);
    }

#if 0    
    data = hash_table_get(ht, &value, NULL);
    printf("Get: %d\n", (int)*data);
    data = hash_table_get(ht, &value2, NULL);
    printf("Get: %d\n", (int)*data);
#endif

    printf("remove the second item\n" );
    hash_table_remove(ht, &value2);
    assert(hash_table_size(ht) == 2);

    printf("After removed the second item\n" );
    data = hash_table_get(ht, &value, NULL);
    printf("Get: %d\n", (int)*data);
    data = hash_table_get(ht, &val3, NULL);
    printf("Get: %d\n", (int)*data);

    printf("Flush: %d\n", hash_table_flush(ht));
    hash_table_free(ht);
    printf("OK!%d %d\n", (256)&(-256), (255)&(~255));

    return 0;
}

