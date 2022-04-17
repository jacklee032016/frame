/* 
 *
 */

#ifndef __BASE_TIMER_H__
#define __BASE_TIMER_H__

/**
 * @brief Timer Heap
 */

#include <_baseTypes.h>
#include <baseLock.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_TIMER Timer Heap Management.
 * The timer scheduling implementation here is based on ACE library's 
 * ACE_Timer_Heap, with only little modification to suit our library's style
 * (I even left most of the comments in the original source).
 *
 * To quote the original quote in ACE_Timer_Heap_T class:
 *
 *      This implementation uses a heap-based callout queue of
 *      absolute times.  Therefore, in the average and worst case,
 *      scheduling, canceling, and expiring timers is O(log N) (where
 *      N is the total number of timers).  In addition, we can also
 *      preallocate as many \a ACE_Timer_Nodes as there are slots in
 *      the heap.  This allows us to completely remove the need for
 *      dynamic memory allocation, which is important for real-time
 *      systems.
 *
 * You can find the fine ACE library at:
 *  http://www.cs.wustl.edu/~schmidt/ACE.html
 *
 * ACE is Copyright (C)1993-2006 Douglas C. Schmidt <d.schmidt@vanderbilt.edu>
 *
 */


/**
 * The type for internal timer ID.
 */
typedef int btimer_id_t;

/** 
 * Forward declaration for btimer_entry. 
 */
struct _btimer_entry;

/**
 * The type of callback function to be called by timer scheduler when a timer
 * has expired.
 *
 * @param timer_heap    The timer heap.
 * @param entry         Timer entry which timer's has expired.
 */
typedef void btimer_heap_callback(btimer_heap_t *timer_heap, struct _btimer_entry *entry);


/* This structure represents an entry to the timer */
typedef struct _btimer_entry
{
	/** 
	* User data to be associated with this entry. 
	* Applications normally will put the instance of object that
	* owns the timer entry in this field.
	*/
	void *user_data;

	/** 
	* Arbitrary ID assigned by the user/owner of this entry. 
	* Applications can use this ID to distinguish multiple
	* timer entries that share the same callback and user_data.
	*/
	int					id;

	/* Callback to be called when the timer expires */
	btimer_heap_callback	*cb;

	/** 
	* Internal unique timer ID, which is assigned by the timer heap. 
	* Application should not touch this ID.
	*/
	btimer_id_t			_timer_id;

	/** 
	* The future time when the timer expires, which the value is updated
	* by timer heap when the timer is scheduled.
	*/
	btime_val			_timer_value;

	/**
	* Internal: the group lock used by this entry, set when
	* btimer_heap_schedule_w_lock() is used.
	*/
	bgrp_lock_t			*_grp_lock;

#if BASE_TIMER_DEBUG
	const char			*src_file;
	int					src_line;
#endif
} btimer_entry;


/**
 * Calculate memory size required to create a timer heap.
 *
 * @param count     Number of timer entries to be supported.
 * @return          Memory size requirement in bytes.
 */
bsize_t btimer_heap_mem_size(bsize_t count);

/**
 * Create a timer heap.
 *
 * @param pool      The pool where allocations in the timer heap will be 
 *                  allocated. The timer heap will dynamicly allocate 
 *                  more storate from the pool if the number of timer 
 *                  entries registered is more than the size originally 
 *                  requested when calling this function.
 * @param count     The maximum number of timer entries to be supported 
 *                  initially. If the application registers more entries 
 *                  during runtime, then the timer heap will resize.
 * @param ht        Pointer to receive the created timer heap.
 *
 * @return          BASE_SUCCESS, or the appropriate error code.
 */
bstatus_t btimer_heap_create( bpool_t *pool,  bsize_t count, btimer_heap_t **ht);

/** Destroy the timer heap */
void btimer_heap_destroy( btimer_heap_t *ht );


/**
 * Set lock object to be used by the timer heap. By default, the timer heap
 * uses dummy synchronization.
 *
 * @param ht        The timer heap.
 * @param lock      The lock object to be used for synchronization.
 * @param auto_del  If nonzero, the lock object will be destroyed when
 *                  the timer heap is destroyed.
 */
void btimer_heap_set_lock( btimer_heap_t *ht, block_t *lock, bbool_t auto_del );

/**
 * Set maximum number of timed out entries to process in a single poll.
 *
 * @param ht        The timer heap.
 * @param count     Number of entries.
 *
 * @return          The old number.
 */
unsigned btimer_heap_set_max_timed_out_per_poll(btimer_heap_t *ht, unsigned count );

/**
 * Initialize a timer entry. Application should call this function at least
 * once before scheduling the entry to the timer heap, to properly initialize
 * the timer entry.
 *
 * @param entry     The timer entry to be initialized.
 * @param id        Arbitrary ID assigned by the user/owner of this entry.
 *                  Applications can use this ID to distinguish multiple
 *                  timer entries that share the same callback and user_data.
 * @param user_data User data to be associated with this entry. 
 *                  Applications normally will put the instance of object that
 *                  owns the timer entry in this field.
 * @param cb        Callback function to be called when the timer elapses.
 *
 * @return          The timer entry itself.
 */
btimer_entry* btimer_entry_init( btimer_entry *entry,
                                              int id,
                                              void *user_data,
                                              btimer_heap_callback *cb );

/**
 * Queries whether a timer entry is currently running.
 *
 * @param entry     The timer entry to query.
 *
 * @return          BASE_TRUE if the timer is running.  BASE_FALSE if not.
 */
bbool_t btimer_entry_running( btimer_entry *entry );

/**
 * Schedule a timer entry which will expire AFTER the specified delay.
 *
 * @param ht        The timer heap.
 * @param entry     The entry to be registered. 
 * @param delay     The interval to expire.
 * @return          BASE_SUCCESS, or the appropriate error code.
 */
#if BASE_TIMER_DEBUG
#define btimer_heap_schedule(ht,e,d) \
			btimer_heap_schedule_dbg(ht,e,d,__FILE__,__LINE__)

  bstatus_t btimer_heap_schedule_dbg( btimer_heap_t *ht,
        					   btimer_entry *entry,
        					   const btime_val *delay,
        					   const char *src_file,
        					   int src_line);
#else
bstatus_t btimer_heap_schedule( btimer_heap_t *ht,
					     btimer_entry *entry, 
					     const btime_val *delay);
#endif	/* BASE_TIMER_DEBUG */

/**
 * Schedule a timer entry which will expire AFTER the specified delay, and
 * increment the reference counter of the group lock while the timer entry
 * is active. The group lock reference counter will automatically be released
 * after the timer callback is called or when the timer is cancelled.
 *
 * @param ht        The timer heap.
 * @param entry     The entry to be registered.
 * @param delay     The interval to expire.
 * @param id_val    The value to be set to the "id" field of the timer entry
 * 		    once the timer is scheduled.
 * @param grp_lock  The group lock.
 *
 * @return          BASE_SUCCESS, or the appropriate error code.
 */
#if BASE_TIMER_DEBUG
#define btimer_heap_schedule_w_grp_lock(ht,e,d,id,g) \
	btimer_heap_schedule_w_grp_lock_dbg(ht,e,d,id,g,__FILE__,__LINE__)

  bstatus_t btimer_heap_schedule_w_grp_lock_dbg(
						   btimer_heap_t *ht,
        					   btimer_entry *entry,
        					   const btime_val *delay,
        					   int id_val,
        					   bgrp_lock_t *grp_lock,
        					   const char *src_file,
        					   int src_line);
#else
bstatus_t btimer_heap_schedule_w_grp_lock(
						    btimer_heap_t *ht,
						    btimer_entry *entry,
						    const btime_val *delay,
						    int id_val,
						    bgrp_lock_t *grp_lock);
#endif	/* BASE_TIMER_DEBUG */


/**
 * Cancel a previously registered timer. This will also decrement the
 * reference counter of the group lock associated with the timer entry,
 * if the entry was scheduled with one.
 *
 * @param ht        The timer heap.
 * @param entry     The entry to be cancelled.
 * @return          The number of timer cancelled, which should be one if the
 *                  entry has really been registered, or zero if no timer was
 *                  cancelled.
 */
int btimer_heap_cancel( btimer_heap_t *ht,
				   btimer_entry *entry);

/**
 * Cancel only if the previously registered timer is active. This will
 * also decrement the reference counter of the group lock associated
 * with the timer entry, if the entry was scheduled with one. In any
 * case, set the "id" to the specified value.
 *
 * @param ht        The timer heap.
 * @param entry     The entry to be cancelled.
 * @param id_val    Value to be set to "id"
 *
 * @return          The number of timer cancelled, which should be one if the
 *                  entry has really been registered, or zero if no timer was
 *                  cancelled.
 */
int btimer_heap_cancel_if_active(btimer_heap_t *ht,
                                            btimer_entry *entry,
                                            int id_val);

/**
 * Get the number of timer entries.
 *
 * @param ht        The timer heap.
 * @return          The number of timer entries.
 */
bsize_t btimer_heap_count( btimer_heap_t *ht );

/**
 * Get the earliest time registered in the timer heap. The timer heap
 * MUST have at least one timer being scheduled (application should use
 * #btimer_heap_count() before calling this function).
 *
 * @param ht        The timer heap.
 * @param timeval   The time deadline of the earliest timer entry.
 *
 * @return          BASE_SUCCESS, or BASE_ENOTFOUND if no entry is scheduled.
 */
bstatus_t btimer_heap_earliest_time( btimer_heap_t *ht, 
					          btime_val *timeval);

/**
 * Poll the timer heap, check for expired timers and call the callback for each of the expired timers.
 *
 * Note: polling the timer heap is not necessary in Symbian. Please see
 * @ref BASE_SYMBIAN_OS for more info.
 *
 * @param ht         The timer heap.
 * @param nbdelay If this parameter is not NULL, it will be filled up with
 *		     the time delay until the next timer elapsed, or 
 *		     BASE_MAXINT32 in the sec part if no entry exist.
 *
 * @return           The number of timers expired.
 */
unsigned btimer_heap_poll( btimer_heap_t *ht, btime_val *nbdelay);

#if BASE_TIMER_DEBUG
/**
 * Dump timer heap entries.
 */
void btimer_heap_dump(btimer_heap_t *ht);
#endif

BASE_END_DECL

#endif

