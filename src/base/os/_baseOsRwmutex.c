/* 
 * this file is included in osCoreXXX.c
 */

/* 
 * Note: 
 * 
 * DO NOT BUILD THIS FILE DIRECTLY. THIS FILE WILL BE INCLUDED BY os_core_*.c
 * WHEN MACRO BASE_EMULATE_RWMUTEX IS SET.
 */

/*
 * Implementation of Read-Write mutex for platforms that lack it (e.g. Win32, RTEMS).
 */


struct brwmutex_t
{
	bmutex_t *read_lock;
	/* write_lock must use semaphore, because write_lock may be released
	* by thread other than the thread that acquire the write_lock in the
	* first place.
	*/
	bsem_t   *write_lock;
	bint32_t  reader_count;
};

/*
 * Create reader/writer mutex.
 *
 */
bstatus_t brwmutex_create(bpool_t *pool, const char *name,
				      brwmutex_t **p_mutex)
{
    bstatus_t status;
    brwmutex_t *rwmutex;

    BASE_ASSERT_RETURN(pool && p_mutex, BASE_EINVAL);

    *p_mutex = NULL;
    rwmutex = BASE_POOL_ALLOC_T(pool, brwmutex_t);

    status = bmutex_create_simple(pool, name, &rwmutex ->read_lock);
    if (status != BASE_SUCCESS)
	return status;

    status = bsem_create(pool, name, 1, 1, &rwmutex->write_lock);
    if (status != BASE_SUCCESS) {
	bmutex_destroy(rwmutex->read_lock);
	return status;
    }

    rwmutex->reader_count = 0;
    *p_mutex = rwmutex;
    return BASE_SUCCESS;
}

/*
 * Lock the mutex for reading.
 *
 */
bstatus_t brwmutex_lock_read(brwmutex_t *mutex)
{
    bstatus_t status;

    BASE_ASSERT_RETURN(mutex, BASE_EINVAL);

    status = bmutex_lock(mutex->read_lock);
    if (status != BASE_SUCCESS) {
	bassert(!"This pretty much is unexpected");
	return status;
    }

    mutex->reader_count++;

    bassert(mutex->reader_count < 0x7FFFFFF0L);

    if (mutex->reader_count == 1)
	bsem_wait(mutex->write_lock);

    status = bmutex_unlock(mutex->read_lock);
    return status;
}

/*
 * Lock the mutex for writing.
 *
 */
bstatus_t brwmutex_lock_write(brwmutex_t *mutex)
{
    BASE_ASSERT_RETURN(mutex, BASE_EINVAL);
    return bsem_wait(mutex->write_lock);
}

/*
 * Release read lock.
 *
 */
bstatus_t brwmutex_unlock_read(brwmutex_t *mutex)
{
    bstatus_t status;

    BASE_ASSERT_RETURN(mutex, BASE_EINVAL);

    status = bmutex_lock(mutex->read_lock);
    if (status != BASE_SUCCESS) {
	bassert(!"This pretty much is unexpected");
	return status;
    }

    bassert(mutex->reader_count >= 1);

    --mutex->reader_count;
    if (mutex->reader_count == 0)
	bsem_post(mutex->write_lock);

    status = bmutex_unlock(mutex->read_lock);
    return status;
}

/*
 * Release write lock.
 *
 */
bstatus_t brwmutex_unlock_write(brwmutex_t *mutex)
{
    BASE_ASSERT_RETURN(mutex, BASE_EINVAL);
    bassert(mutex->reader_count <= 1);
    return bsem_post(mutex->write_lock);
}


/*
 * Destroy reader/writer mutex.
 *
 */
bstatus_t brwmutex_destroy(brwmutex_t *mutex)
{
    BASE_ASSERT_RETURN(mutex, BASE_EINVAL);
    bmutex_destroy(mutex->read_lock);
    bsem_destroy(mutex->write_lock);
    return BASE_SUCCESS;
}

