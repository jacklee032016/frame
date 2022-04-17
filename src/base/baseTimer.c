/* 
 * The 's timer heap is based (or more correctly, copied and modied)
 * from ACE library by Douglas C. Schmidt. ACE is an excellent OO framework
 * that implements many core patterns for concurrent communication software.
 * If you're looking for C++ alternative of , then ACE is your best
 * solution.
 *
 * You can find the fine ACE library at:
 *  http://www.cs.wustl.edu/~schmidt/ACE.html
 */
#include <baseTimer.h>
#include <basePool.h>
#include <baseOs.h>
#include <baseString.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseLock.h>
#include <baseLog.h>
#include <baseRand.h>
#include <_baseLimits.h>

#define HEAP_PARENT(X)	(X == 0 ? 0 : (((X) - 1) / 2))
#define HEAP_LEFT(X)	(((X)+(X))+1)


#define DEFAULT_MAX_TIMED_OUT_PER_POLL  (64)

enum
{
	F_DONT_CALL = 1,
	F_DONT_ASSERT = 2,
	F_SET_ID = 4
};


/**
 * The implementation of timer heap.
 */
struct _btimer_heap_t
{
	/** Pool from which the timer heap resize will get the storage from */
	bpool_t *pool;

	/** Maximum size of the heap. */
	bsize_t max_size;

	/** Current size of the heap. */
	bsize_t cur_size;

	/** Max timed out entries to process per poll. */
	unsigned max_entries_per_poll;

	/** Lock object. */
	block_t *lock;

	/** Autodelete lock. */
	bbool_t auto_delete_lock;

	/**
	* Current contents of the Heap, which is organized as a "heap" of
	* btimer_entry *'s.  In this context, a heap is a "partially
	* ordered, almost complete" binary tree, which is stored in an
	* array.
	*/
	btimer_entry **heap;

	/**
	* An array of "pointers" that allows each btimer_entry in the
	* <heap_> to be located in O(1) time.  Basically, <timer_id_[i]>
	* contains the slot in the <heap_> array where an btimer_entry
	* with timer id <i> resides.  Thus, the timer id passed back from
	* <schedule_entry> is really an slot into the <timer_ids> array.  The
	* <timer_ids_> array serves two purposes: negative values are
	* treated as "pointers" for the <freelist_>, whereas positive
	* values are treated as "pointers" into the <heap_> array.
	*/
	btimer_id_t *timer_ids;

	/**
	* "Pointer" to the first element in the freelist contained within
	* the <timer_ids_> array, which is organized as a stack.
	*/
	btimer_id_t timer_ids_freelist;

	/** Callback to be called when a timer expires. */
	btimer_heap_callback *callback;

};



BASE_INLINE_SPECIFIER void lock_timer_heap( btimer_heap_t *ht )
{
    if (ht->lock) {
	block_acquire(ht->lock);
    }
}

BASE_INLINE_SPECIFIER void unlock_timer_heap( btimer_heap_t *ht )
{
    if (ht->lock) {
	block_release(ht->lock);
    }
}


static void copy_node( btimer_heap_t *ht, bsize_t slot, 
		       btimer_entry *moved_node )
{
    BASE_CHECK_STACK();

    // Insert <moved_node> into its new location in the heap.
    ht->heap[slot] = moved_node;
    
    // Update the corresponding slot in the parallel <timer_ids_> array.
    ht->timer_ids[moved_node->_timer_id] = (int)slot;
}

static btimer_id_t pop_freelist( btimer_heap_t *ht )
{
    // We need to truncate this to <int> for backwards compatibility.
    btimer_id_t new_id = ht->timer_ids_freelist;
    
    BASE_CHECK_STACK();

    // The freelist values in the <timer_ids_> are negative, so we need
    // to negate them to get the next freelist "pointer."
    ht->timer_ids_freelist =
	-ht->timer_ids[ht->timer_ids_freelist];
    
    return new_id;
    
}

static void push_freelist (btimer_heap_t *ht, btimer_id_t old_id)
{
    BASE_CHECK_STACK();

    // The freelist values in the <timer_ids_> are negative, so we need
    // to negate them to get the next freelist "pointer."
    ht->timer_ids[old_id] = -ht->timer_ids_freelist;
    ht->timer_ids_freelist = old_id;
}


static void reheap_down(btimer_heap_t *ht, btimer_entry *moved_node, size_t slot, size_t child)
{
    BASE_CHECK_STACK();

    // Restore the heap property after a deletion.
    
    while (child < ht->cur_size)
    {
	// Choose the smaller of the two children.
	if (child + 1 < ht->cur_size
	    && BASE_TIME_VAL_LT(ht->heap[child + 1]->_timer_value, ht->heap[child]->_timer_value))
	    child++;
	
	// Perform a <copy> if the child has a larger timeout value than
	// the <moved_node>.
	if (BASE_TIME_VAL_LT(ht->heap[child]->_timer_value, moved_node->_timer_value))
        {
	    copy_node( ht, slot, ht->heap[child]);
	    slot = child;
	    child = HEAP_LEFT(child);
        }
	else
	    // We've found our location in the heap.
	    break;
    }
    
    copy_node( ht, slot, moved_node);
}

static void reheap_up( btimer_heap_t *ht, btimer_entry *moved_node,
		       size_t slot, size_t parent)
{
    // Restore the heap property after an insertion.
    
    while (slot > 0)
    {
	// If the parent node is greater than the <moved_node> we need
	// to copy it down.
	if (BASE_TIME_VAL_LT(moved_node->_timer_value, ht->heap[parent]->_timer_value))
        {
	    copy_node(ht, slot, ht->heap[parent]);
	    slot = parent;
	    parent = HEAP_PARENT(slot);
        }
	else
	    break;
    }
    
    // Insert the new node into its proper resting place in the heap and
    // update the corresponding slot in the parallel <timer_ids> array.
    copy_node(ht, slot, moved_node);
}


static btimer_entry * remove_node( btimer_heap_t *ht, size_t slot)
{
    btimer_entry *removed_node = ht->heap[slot];
    
    // Return this timer id to the freelist.
    push_freelist( ht, removed_node->_timer_id );
    
    // Decrement the size of the heap by one since we're removing the
    // "slot"th node.
    ht->cur_size--;
    
    // Set the ID
    removed_node->_timer_id = -1;

    // Only try to reheapify if we're not deleting the last entry.
    
    if (slot < ht->cur_size)
    {
	bsize_t parent;
	btimer_entry *moved_node = ht->heap[ht->cur_size];
	
	// Move the end node to the location being removed and update
	// the corresponding slot in the parallel <timer_ids> array.
	copy_node( ht, slot, moved_node);
	
	// If the <moved_node->time_value_> is great than or equal its
	// parent it needs be moved down the heap.
	parent = HEAP_PARENT (slot);
	
	if (BASE_TIME_VAL_GTE(moved_node->_timer_value, ht->heap[parent]->_timer_value))
	    reheap_down( ht, moved_node, slot, HEAP_LEFT(slot));
	else
	    reheap_up( ht, moved_node, slot, parent);
    }
    
    return removed_node;
}

static void grow_heap(btimer_heap_t *ht)
{
    // All the containers will double in size from max_size_
    size_t new_size = ht->max_size * 2;
    btimer_id_t *new_timer_ids;
    bsize_t i;
    
    // First grow the heap itself.
    
    btimer_entry **new_heap = 0;
    
    new_heap = (btimer_entry**) 
    	       bpool_alloc(ht->pool, sizeof(btimer_entry*) * new_size);
    memcpy(new_heap, ht->heap, ht->max_size * sizeof(btimer_entry*));
    //delete [] this->heap_;
    ht->heap = new_heap;
    
    // Grow the array of timer ids.
    
    new_timer_ids = 0;
    new_timer_ids = (btimer_id_t*)
    		    bpool_alloc(ht->pool, new_size * sizeof(btimer_id_t));
    
    memcpy( new_timer_ids, ht->timer_ids, ht->max_size * sizeof(btimer_id_t));
    
    //delete [] timer_ids_;
    ht->timer_ids = new_timer_ids;
    
    // And add the new elements to the end of the "freelist".
    for (i = ht->max_size; i < new_size; i++)
	ht->timer_ids[i] = -((btimer_id_t) (i + 1));
    
    ht->max_size = new_size;
}

static void insert_node(btimer_heap_t *ht, btimer_entry *new_node)
{
    if (ht->cur_size + 2 >= ht->max_size)
	grow_heap(ht);
    
    reheap_up( ht, new_node, ht->cur_size, HEAP_PARENT(ht->cur_size));
    ht->cur_size++;
}


static bstatus_t schedule_entry( btimer_heap_t *ht, 	btimer_entry *entry, const btime_val *future_time )
{
	if (ht->cur_size < ht->max_size)
	{
		// Obtain the next unique sequence number.
		// Set the entry
		entry->_timer_id = pop_freelist(ht);
		entry->_timer_value = *future_time;
		
		insert_node( ht, entry);
		return 0;
	}
	else
		return -1;
}


static int cancel( btimer_heap_t *ht, btimer_entry *entry, unsigned flags)
{
	long timer_node_slot;

	BASE_CHECK_STACK();

	// Check to see if the timer_id is out of range
	if (entry->_timer_id < 0 || (bsize_t)entry->_timer_id > ht->max_size)
	{
		entry->_timer_id = -1;
		return 0;
	}

	timer_node_slot = ht->timer_ids[entry->_timer_id];

	if (timer_node_slot < 0)
	{ // Check to see if timer_id is still valid.
		entry->_timer_id = -1;
		return 0;
	}

	if (entry != ht->heap[timer_node_slot])
	{
		if ((flags & F_DONT_ASSERT) == 0)
			bassert(entry == ht->heap[timer_node_slot]);
		entry->_timer_id = -1;
		return 0;
	}
	else
	{
		remove_node( ht, timer_node_slot);

		if ((flags & F_DONT_CALL) == 0)
			// Call the close hook.
			(*ht->callback)(ht, entry);
		return 1;
	}
}


/*
 * Calculate memory size required to create a timer heap.
 */
bsize_t btimer_heap_mem_size(bsize_t count)
{
    return /* size of the timer heap itself: */
           sizeof(btimer_heap_t) + 
           /* size of each entry: */
           (count+2) * (sizeof(btimer_entry*)+sizeof(btimer_id_t)) +
           /* lock, pool etc: */
           132;
}

/*
 * Create a new timer heap.
 */
bstatus_t btimer_heap_create( bpool_t *pool, bsize_t size, btimer_heap_t **p_heap)
{
	btimer_heap_t *ht;
	bsize_t i;

	BASE_ASSERT_RETURN(pool && p_heap, BASE_EINVAL);

	*p_heap = NULL;

	/* Magic? */
	size += 2;

	/* Allocate timer heap data structure from the pool */
	ht = BASE_POOL_ALLOC_T(pool, btimer_heap_t);
	if (!ht)
		return BASE_ENOMEM;

	/* Initialize timer heap sizes */
	ht->max_size = size;
	ht->cur_size = 0;
	ht->max_entries_per_poll = DEFAULT_MAX_TIMED_OUT_PER_POLL;
	ht->timer_ids_freelist = 1;
	ht->pool = pool;

	/* Lock. */
	ht->lock = NULL;
	ht->auto_delete_lock = 0;

	// Create the heap array.
	ht->heap = (btimer_entry**)bpool_alloc(pool, sizeof(btimer_entry*) * size);
	if (!ht->heap)
		return BASE_ENOMEM;

	// Create the parallel
	ht->timer_ids = (btimer_id_t *)bpool_alloc( pool, sizeof(btimer_id_t) * size);
	if (!ht->timer_ids)
	{
		/* free ht->heap ??? */
		return BASE_ENOMEM;
	}

	// Initialize the "freelist," which uses negative values to
	// distinguish freelist elements from "pointers" into the <heap_>
	// array.
	for (i=0; i<size; ++i)
		ht->timer_ids[i] = -((btimer_id_t) (i + 1));

	*p_heap = ht;
	return BASE_SUCCESS;
}


void btimer_heap_destroy( btimer_heap_t *ht )
{
	if (ht->lock && ht->auto_delete_lock)
	{
		block_destroy(ht->lock);
		ht->lock = NULL;
	}
}

void btimer_heap_set_lock(  btimer_heap_t *ht, block_t *lock, bbool_t auto_del )
{
	if (ht->lock && ht->auto_delete_lock)
		block_destroy(ht->lock);

	ht->lock = lock;
	ht->auto_delete_lock = auto_del;
}


unsigned btimer_heap_set_max_timed_out_per_poll(btimer_heap_t *ht, unsigned count )
{
	unsigned old_count = ht->max_entries_per_poll;
	ht->max_entries_per_poll = count;
	return old_count;
}

btimer_entry* btimer_entry_init( btimer_entry *entry, int id, void *user_data, btimer_heap_callback *cb )
{
	bassert(entry && cb);

	entry->_timer_id = -1;
	entry->id = id;
	entry->user_data = user_data;
	entry->cb = cb;
	entry->_grp_lock = NULL;

	return entry;
}

bbool_t btimer_entry_running( btimer_entry *entry )
{
	return (entry->_timer_id >= 1);
}

#if BASE_TIMER_DEBUG
static bstatus_t schedule_w_grp_lock_dbg(btimer_heap_t *ht,
                                           btimer_entry *entry,
                                           const btime_val *delay,
                                           bbool_t set_id,
                                           int id_val,
					   bgrp_lock_t *grp_lock,
					   const char *src_file,
					   int src_line)
#else
static bstatus_t schedule_w_grp_lock(btimer_heap_t *ht,
                                       btimer_entry *entry,
                                       const btime_val *delay,
                                       bbool_t set_id,
                                       int id_val,
                                       bgrp_lock_t *grp_lock)
#endif
{
	bstatus_t status;
	btime_val expires;

	BASE_ASSERT_RETURN(ht && entry && delay, BASE_EINVAL);
	BASE_ASSERT_RETURN(entry->cb != NULL, BASE_EINVAL);

	/* Prevent same entry from being scheduled more than once */
	//BASE_ASSERT_RETURN(entry->_timer_id < 1, BASE_EINVALIDOP);
#if BASE_TIMER_DEBUG
	entry->src_file = src_file;
	entry->src_line = src_line;
#endif
	bgettickcount(&expires);
	BASE_TIME_VAL_ADD(expires, *delay);

	lock_timer_heap(ht);

	/* Prevent same entry from being scheduled more than once */
	if (btimer_entry_running(entry))
	{
		unlock_timer_heap(ht);
		BASE_CRIT("Bug! Rescheduling outstanding entry (%p)",entry);
		return BASE_EINVALIDOP;
	}

	status = schedule_entry(ht, entry, &expires);
	if (status == BASE_SUCCESS)
	{
		if (set_id)
			entry->id = id_val;

		entry->_grp_lock = grp_lock;
		if (entry->_grp_lock)
		{
			bgrp_lock_add_ref(entry->_grp_lock);
		}
	}
	
	unlock_timer_heap(ht);

	return status;
}


#if BASE_TIMER_DEBUG
bstatus_t btimer_heap_schedule_dbg( btimer_heap_t *ht,
						btimer_entry *entry,
						const btime_val *delay,
						const char *src_file,
						int src_line)
{
    return schedule_w_grp_lock_dbg(ht, entry, delay, BASE_FALSE, 1, NULL,
                                   src_file, src_line);
}

bstatus_t btimer_heap_schedule_w_grp_lock_dbg(
						btimer_heap_t *ht,
						btimer_entry *entry,
						const btime_val *delay,
						int id_val,
                                                bgrp_lock_t *grp_lock,
						const char *src_file,
						int src_line)
{
    return schedule_w_grp_lock_dbg(ht, entry, delay, BASE_TRUE, id_val,
                                   grp_lock, src_file, src_line);
}

#else
bstatus_t btimer_heap_schedule( btimer_heap_t *ht,
                                            btimer_entry *entry,
                                            const btime_val *delay)
{
    return schedule_w_grp_lock(ht, entry, delay, BASE_FALSE, 1, NULL);
}

bstatus_t btimer_heap_schedule_w_grp_lock(btimer_heap_t *ht,
                                                      btimer_entry *entry,
                                                      const btime_val *delay,
                                                      int id_val,
                                                      bgrp_lock_t *grp_lock)
{
    return schedule_w_grp_lock(ht, entry, delay, BASE_TRUE, id_val, grp_lock);
}
#endif

static int _cancelTimer(btimer_heap_t *ht, btimer_entry *entry, unsigned flags, int id_val)
{
	int count;

	BASE_ASSERT_RETURN(ht && entry, BASE_EINVAL);

	lock_timer_heap(ht);
	count = cancel(ht, entry, flags | F_DONT_CALL);

	if (count > 0)
	{/* Timer entry found & cancelled */
		if (flags & F_SET_ID)
		{
			entry->id = id_val;
		}

		if (entry->_grp_lock)
		{
			bgrp_lock_t *grp_lock = entry->_grp_lock;
			entry->_grp_lock = NULL;
			bgrp_lock_dec_ref(grp_lock);
		}
	}
	
	unlock_timer_heap(ht);

	return count;
}

int btimer_heap_cancel( btimer_heap_t *ht, btimer_entry *entry)
{
	return _cancelTimer(ht, entry, 0, 0);
}

int btimer_heap_cancel_if_active(btimer_heap_t *ht,	btimer_entry *entry, int id_val)
{
	return _cancelTimer(ht, entry, F_SET_ID | F_DONT_ASSERT, id_val);
}

unsigned btimer_heap_poll( btimer_heap_t *ht, btime_val *nbdelay )
{
	btime_val now;
	unsigned count;

	BASE_ASSERT_RETURN(ht, 0);

	lock_timer_heap(ht);
	if (!ht->cur_size && nbdelay)
	{
		nbdelay->sec = nbdelay->msec = BASE_MAXINT32;
		unlock_timer_heap(ht);
		return 0;
	}

	count = 0;
	bgettickcount(&now);

	while ( ht->cur_size &&  BASE_TIME_VAL_LTE(ht->heap[0]->_timer_value, now) && count < ht->max_entries_per_poll ) 
	{
		btimer_entry *node = remove_node(ht, 0);
		/* Avoid re-use of this timer until the callback is done. */
		///Not necessary, even causes problem (see also #2176).
		///btimer_id_t node_timer_id = pop_freelist(ht);
		bgrp_lock_t *grp_lock;

		++count;

		grp_lock = node->_grp_lock;
		node->_grp_lock = NULL;

		unlock_timer_heap(ht);

		BASE_RACE_ME(5);

		if (node->cb)
			(*node->cb)(ht, node);

		if (grp_lock)
			bgrp_lock_dec_ref(grp_lock);

		lock_timer_heap(ht);
		/* Now, the timer is really free for re-use. */
		///push_freelist(ht, node_timer_id);
	}
	
	if (ht->cur_size && nbdelay)
	{
		*nbdelay = ht->heap[0]->_timer_value;
		BASE_TIME_VAL_SUB(*nbdelay, now);
		
		if (nbdelay->sec < 0 || nbdelay->msec < 0)
			nbdelay->sec = nbdelay->msec = 0;
	}
	else if (nbdelay)
	{
		nbdelay->sec = nbdelay->msec = BASE_MAXINT32;
	}
	unlock_timer_heap(ht);

	return count;
}

bsize_t btimer_heap_count( btimer_heap_t *ht )
{
	BASE_ASSERT_RETURN(ht, 0);

	return ht->cur_size;
}

bstatus_t btimer_heap_earliest_time( btimer_heap_t * ht, btime_val *timeval)
{
	bassert(ht->cur_size != 0);
	if (ht->cur_size == 0)
		return BASE_ENOTFOUND;

	lock_timer_heap(ht);
	*timeval = ht->heap[0]->_timer_value;
	unlock_timer_heap(ht);

	return BASE_SUCCESS;
}

#if BASE_TIMER_DEBUG
void btimer_heap_dump(btimer_heap_t *ht)
{
	lock_timer_heap(ht);

	BASE_INFO("Dumping timer heap:");
	BASE_INFO("  Cur size: %d entries, max: %d", (int)ht->cur_size, (int)ht->max_size);

	if (ht->cur_size)
	{
		unsigned i;
		btime_val now;

		BASE_INFO("  Entries: ");
		BASE_INFO("    _id\tId\tElapsed\tSource");
		BASE_INFO("    ----------------------------------");

		bgettickcount(&now);

		for (i=0; i<(unsigned)ht->cur_size; ++i)
		{
			btimer_entry *e = ht->heap[i];
			btime_val delta;

			if (BASE_TIME_VAL_LTE(e->_timer_value, now))
				delta.sec = delta.msec = 0;
			else
			{
				delta = e->_timer_value;
				BASE_TIME_VAL_SUB(delta, now);
			}

			BASE_INFO( "    %d\t%d\t%d.%03d\t%s:%d", e->_timer_id, e->id, (int)delta.sec, (int)delta.msec, e->src_file, e->src_line);
		}
	}

	unlock_timer_heap(ht);
}

#endif

