/*
 *
 */
#include <baseLock.h>
#include <baseOs.h>
#include <baseAssert.h>
#include <baseLog.h>
#include <basePool.h>
#include <baseString.h>
#include <baseErrno.h>


typedef void LOCK_OBJ;

/*
 * Lock structure.
 */
struct block_t
{
	LOCK_OBJ *lock_object;

	bstatus_t	(*acquire)(LOCK_OBJ*);
	bstatus_t	(*tryacquire)(LOCK_OBJ*);
	bstatus_t	(*release)(LOCK_OBJ*);
	bstatus_t	(*destroy)(LOCK_OBJ*);
};

typedef bstatus_t (*FPTR)(LOCK_OBJ*);

/** Implementation of lock object with mutex. */
static block_t mutex_lock_template = 
{
	NULL,
	(FPTR) &bmutex_lock,
	(FPTR) &bmutex_trylock,
	(FPTR) &bmutex_unlock,
	(FPTR) &bmutex_destroy
};

static bstatus_t create_mutex_lock( bpool_t *pool, const char *name, int type, block_t **lock )
{
	block_t *p_lock;
	bmutex_t *mutex;
	bstatus_t rc;

	BASE_ASSERT_RETURN(pool && lock, BASE_EINVAL);

	p_lock = BASE_POOL_ALLOC_T(pool, block_t);
	if (!p_lock)
		return BASE_ENOMEM;

	bmemcpy(p_lock, &mutex_lock_template, sizeof(block_t));
	rc = bmutex_create(pool, name, type, &mutex);
	if (rc != BASE_SUCCESS)
		return rc;

	p_lock->lock_object = mutex;
	*lock = p_lock;
	return BASE_SUCCESS;
}


bstatus_t block_create_simple_mutex( bpool_t *pool, const char *name, block_t **lock )
{
	return create_mutex_lock(pool, name, BASE_MUTEX_SIMPLE, lock);
}

bstatus_t block_create_recursive_mutex( bpool_t *pool, 	const char *name, block_t **lock )
{
	return create_mutex_lock(pool, name, BASE_MUTEX_RECURSE, lock);
}


/*
 * Implementation of NULL lock object.
 */
static bstatus_t null_op(void *arg)
{
	BASE_UNUSED_ARG(arg);
	return BASE_SUCCESS;
}

static block_t null_lock_template = 
{
	NULL,
	&null_op,
	&null_op,
	&null_op,
	&null_op
};

bstatus_t block_create_null_mutex( bpool_t *pool,
					       const char *name,
					       block_t **lock )
{
    BASE_UNUSED_ARG(name);
    BASE_UNUSED_ARG(pool);

    BASE_ASSERT_RETURN(lock, BASE_EINVAL);

    *lock = &null_lock_template;
    return BASE_SUCCESS;
}


/** Implementation of semaphore lock object. */
#if defined(BASE_HAS_SEMAPHORE) && BASE_HAS_SEMAPHORE != 0

static block_t sem_lock_template = 
{
    NULL,
    (FPTR) &bsem_wait,
    (FPTR) &bsem_trywait,
    (FPTR) &bsem_post,
    (FPTR) &bsem_destroy
};

bstatus_t block_create_semaphore(  bpool_t *pool,
					       const char *name,
					       unsigned initial,
					       unsigned max,
					       block_t **lock )
{
    block_t *p_lock;
    bsem_t *sem;
    bstatus_t rc;

    BASE_ASSERT_RETURN(pool && lock, BASE_EINVAL);

    p_lock = BASE_POOL_ALLOC_T(pool, block_t);
    if (!p_lock)
	return BASE_ENOMEM;

    bmemcpy(p_lock, &sem_lock_template, sizeof(block_t));
    rc = bsem_create( pool, name, initial, max, &sem);
    if (rc != BASE_SUCCESS)
        return rc;

    p_lock->lock_object = sem;
    *lock = p_lock;

    return BASE_SUCCESS;
}


#endif	/* BASE_HAS_SEMAPHORE */


bstatus_t block_acquire( block_t *lock )
{
    BASE_ASSERT_RETURN(lock != NULL, BASE_EINVAL);
    return (*lock->acquire)(lock->lock_object);
}

bstatus_t block_tryacquire( block_t *lock )
{
    BASE_ASSERT_RETURN(lock != NULL, BASE_EINVAL);
    return (*lock->tryacquire)(lock->lock_object);
}

bstatus_t block_release( block_t *lock )
{
	BASE_ASSERT_RETURN(lock != NULL, BASE_EINVAL);
	return (*lock->release)(lock->lock_object);
}

bstatus_t block_destroy( block_t *lock )
{
	BASE_ASSERT_RETURN(lock != NULL, BASE_EINVAL);
	return (*lock->destroy)(lock->lock_object);
}


/** Group lock */

/* Individual lock in the group lock */
typedef struct grp_lock_item
{
    BASE_DECL_LIST_MEMBER(struct grp_lock_item);
    int		 prio;
    block_t	*lock;

} grp_lock_item;

/* Destroy callbacks */
typedef struct grp_destroy_callback
{
    BASE_DECL_LIST_MEMBER(struct grp_destroy_callback);
    void	*comp;
    void	(*handler)(void*);
} grp_destroy_callback;

#if BASE_GRP_LOCK_DEBUG
/* Store each add_ref caller */
typedef struct grp_lock_ref
{
    BASE_DECL_LIST_MEMBER(struct grp_lock_ref);
    const char	*file;
    int		 line;
} grp_lock_ref;
#endif

/* The group lock */
struct bgrp_lock_t
{
    block_t	 	 base;

    bpool_t		*pool;
    batomic_t		*ref_cnt;
    block_t		*own_lock;

    bthread_t		*owner;
    int			 owner_cnt;

    grp_lock_item	 lock_list;
    grp_destroy_callback destroy_list;

#if BASE_GRP_LOCK_DEBUG
    grp_lock_ref	 ref_list;
    grp_lock_ref	 ref_free_list;
#endif
};


void bgrp_lock_config_default(bgrp_lock_config *cfg)
{
    bbzero(cfg, sizeof(*cfg));
}

static void grp_lock_set_owner_thread(bgrp_lock_t *glock)
{
    if (!glock->owner) {
	glock->owner = bthreadThis();
	glock->owner_cnt = 1;
    } else {
	bassert(glock->owner == bthreadThis());
	glock->owner_cnt++;
    }
}

static void grp_lock_unset_owner_thread(bgrp_lock_t *glock)
{
    bassert(glock->owner == bthreadThis());
    bassert(glock->owner_cnt > 0);
    if (--glock->owner_cnt <= 0) {
	glock->owner = NULL;
	glock->owner_cnt = 0;
    }
}

static bstatus_t grp_lock_acquire(LOCK_OBJ *p)
{
    bgrp_lock_t *glock = (bgrp_lock_t*)p;
    grp_lock_item *lck;

    bassert(batomic_get(glock->ref_cnt) > 0);

    lck = glock->lock_list.next;
    while (lck != &glock->lock_list) {
	block_acquire(lck->lock);
	lck = lck->next;
    }
    grp_lock_set_owner_thread(glock);
    bgrp_lock_add_ref(glock);
    return BASE_SUCCESS;
}

static bstatus_t grp_lock_tryacquire(LOCK_OBJ *p)
{
    bgrp_lock_t *glock = (bgrp_lock_t*)p;
    grp_lock_item *lck;

    bassert(batomic_get(glock->ref_cnt) > 0);

    lck = glock->lock_list.next;
    while (lck != &glock->lock_list) {
	bstatus_t status = block_tryacquire(lck->lock);
	if (status != BASE_SUCCESS) {
	    lck = lck->prev;
	    while (lck != &glock->lock_list) {
		block_release(lck->lock);
		lck = lck->prev;
	    }
	    return status;
	}
	lck = lck->next;
    }
    grp_lock_set_owner_thread(glock);
    bgrp_lock_add_ref(glock);
    return BASE_SUCCESS;
}

static bstatus_t grp_lock_release(LOCK_OBJ *p)
{
    bgrp_lock_t *glock = (bgrp_lock_t*)p;
    grp_lock_item *lck;

    grp_lock_unset_owner_thread(glock);

    lck = glock->lock_list.prev;
    while (lck != &glock->lock_list) {
	block_release(lck->lock);
	lck = lck->prev;
    }
    return bgrp_lock_dec_ref(glock);
}

static bstatus_t grp_lock_add_handler( bgrp_lock_t *glock,
                             		 bpool_t *pool,
                             		 void *comp,
                             		 void (*destroy)(void *comp),
                             		 bbool_t acquire_lock)
{
    grp_destroy_callback *cb;

    if (acquire_lock)
        grp_lock_acquire(glock);

    if (pool == NULL)
	pool = glock->pool;

    cb = BASE_POOL_ZALLOC_T(pool, grp_destroy_callback);
    cb->comp = comp;
    cb->handler = destroy;
    blist_push_back(&glock->destroy_list, cb);

    if (acquire_lock)
        grp_lock_release(glock);

    return BASE_SUCCESS;
}

static bstatus_t grp_lock_destroy(LOCK_OBJ *p)
{
    bgrp_lock_t *glock = (bgrp_lock_t*)p;
    bpool_t *pool = glock->pool;
    grp_lock_item *lck;
    grp_destroy_callback *cb;

    if (!glock->pool) {
	/* already destroyed?! */
	return BASE_EINVAL;
    }

    /* Release all chained locks */
    lck = glock->lock_list.next;
    while (lck != &glock->lock_list) {
	if (lck->lock != glock->own_lock) {
	    int i;
	    for (i=0; i<glock->owner_cnt; ++i)
		block_release(lck->lock);
	}
	lck = lck->next;
    }

    /* Call callbacks */
    cb = glock->destroy_list.next;
    while (cb != &glock->destroy_list) {
	grp_destroy_callback *next = cb->next;
	cb->handler(cb->comp);
	cb = next;
    }

    block_destroy(glock->own_lock);
    batomic_destroy(glock->ref_cnt);
    glock->pool = NULL;
    bpool_release(pool);

    return BASE_SUCCESS;
}


bstatus_t bgrp_lock_create( bpool_t *pool,
                                        const bgrp_lock_config *cfg,
                                        bgrp_lock_t **p_grp_lock)
{
    bgrp_lock_t *glock;
    grp_lock_item *own_lock;
    bstatus_t status;

    BASE_ASSERT_RETURN(pool && p_grp_lock, BASE_EINVAL);

    BASE_UNUSED_ARG(cfg);

    pool = bpool_create(pool->factory, "glck%p", 512, 512, NULL);
    if (!pool)
	return BASE_ENOMEM;

    glock = BASE_POOL_ZALLOC_T(pool, bgrp_lock_t);
    glock->base.lock_object = glock;
    glock->base.acquire = &grp_lock_acquire;
    glock->base.tryacquire = &grp_lock_tryacquire;
    glock->base.release = &grp_lock_release;
    glock->base.destroy = &grp_lock_destroy;

    glock->pool = pool;
    blist_init(&glock->lock_list);
    blist_init(&glock->destroy_list);
#if BASE_GRP_LOCK_DEBUG
    blist_init(&glock->ref_list);
    blist_init(&glock->ref_free_list);
#endif

    status = batomic_create(pool, 0, &glock->ref_cnt);
    if (status != BASE_SUCCESS)
	goto on_error;

    status = block_create_recursive_mutex(pool, pool->objName,
                                            &glock->own_lock);
    if (status != BASE_SUCCESS)
	goto on_error;

    own_lock = BASE_POOL_ZALLOC_T(pool, grp_lock_item);
    own_lock->lock = glock->own_lock;
    blist_push_back(&glock->lock_list, own_lock);

    *p_grp_lock = glock;
    return BASE_SUCCESS;

on_error:
    grp_lock_destroy(glock);
    return status;
}

bstatus_t bgrp_lock_create_w_handler( bpool_t *pool,
                                        	  const bgrp_lock_config *cfg,
                                        	  void *member,
                                                  void (*handler)(void *member),
                                        	  bgrp_lock_t **p_grp_lock)
{
    bstatus_t status;

    status = bgrp_lock_create(pool, cfg, p_grp_lock);
    if (status == BASE_SUCCESS) {
        grp_lock_add_handler(*p_grp_lock, pool, member, handler, BASE_FALSE);
    }
    
    return status;
}

bstatus_t bgrp_lock_destroy( bgrp_lock_t *grp_lock)
{
    return grp_lock_destroy(grp_lock);
}

bstatus_t bgrp_lock_acquire( bgrp_lock_t *grp_lock)
{
    return grp_lock_acquire(grp_lock);
}

bstatus_t bgrp_lock_tryacquire( bgrp_lock_t *grp_lock)
{
    return grp_lock_tryacquire(grp_lock);
}

bstatus_t bgrp_lock_release( bgrp_lock_t *grp_lock)
{
    return grp_lock_release(grp_lock);
}

bstatus_t bgrp_lock_replace( bgrp_lock_t *old_lock,
                                         bgrp_lock_t *new_lock)
{
    grp_destroy_callback *ocb;

    /* Move handlers from old to new */
    ocb = old_lock->destroy_list.next;
    while (ocb != &old_lock->destroy_list) {
	grp_destroy_callback *ncb;

	ncb = BASE_POOL_ALLOC_T(new_lock->pool, grp_destroy_callback);
	ncb->comp = ocb->comp;
	ncb->handler = ocb->handler;
	blist_push_back(&new_lock->destroy_list, ncb);

	ocb = ocb->next;
    }

    blist_init(&old_lock->destroy_list);

    grp_lock_destroy(old_lock);
    return BASE_SUCCESS;
}

bstatus_t bgrp_lock_add_handler( bgrp_lock_t *glock,
                                             bpool_t *pool,
                                             void *comp,
                                             void (*destroy)(void *comp))
{
    return grp_lock_add_handler(glock, pool, comp, destroy, BASE_TRUE);
}

bstatus_t bgrp_lock_del_handler( bgrp_lock_t *glock,
                                             void *comp,
                                             void (*destroy)(void *comp))
{
    grp_destroy_callback *cb;

    grp_lock_acquire(glock);

    cb = glock->destroy_list.next;
    while (cb != &glock->destroy_list) {
	if (cb->comp == comp && cb->handler == destroy)
	    break;
	cb = cb->next;
    }

    if (cb != &glock->destroy_list)
	blist_erase(cb);

    grp_lock_release(glock);
    return BASE_SUCCESS;
}

static bstatus_t grp_lock_add_ref(bgrp_lock_t *glock)
{
    batomic_inc(glock->ref_cnt);
    return BASE_SUCCESS;
}

static bstatus_t grp_lock_dec_ref(bgrp_lock_t *glock)
{
    int cnt; /* for debugging */
    if ((cnt=batomic_dec_and_get(glock->ref_cnt)) == 0) {
	grp_lock_destroy(glock);
	return BASE_EGONE;
    }
    bassert(cnt > 0);
    return BASE_SUCCESS;
}

#if BASE_GRP_LOCK_DEBUG
static bstatus_t grp_lock_dec_ref_dump(bgrp_lock_t *glock)
{
    bstatus_t status;

    status = grp_lock_dec_ref(glock);
    if (status == BASE_SUCCESS) {
	bgrp_lock_dump(glock);
    } else if (status == BASE_EGONE) {
	BASE_INFO("Group lock %p destroyed.", glock);
    }

    return status;
}

bstatus_t bgrp_lock_add_ref_dbg(bgrp_lock_t *glock,
                                            const char *file,
                                            int line)
{
    grp_lock_ref *ref;
    bstatus_t status;

    benter_critical_section();
    if (!blist_empty(&glock->ref_free_list)) {
	ref = glock->ref_free_list.next;
	blist_erase(ref);
    } else {
	ref = BASE_POOL_ALLOC_T(glock->pool, grp_lock_ref);
    }

    ref->file = file;
    ref->line = line;
    blist_push_back(&glock->ref_list, ref);

    bleave_critical_section();

    status = grp_lock_add_ref(glock);

    if (status != BASE_SUCCESS) {
	benter_critical_section();
	blist_erase(ref);
	blist_push_back(&glock->ref_free_list, ref);
	bleave_critical_section();
    }

    return status;
}

bstatus_t bgrp_lock_dec_ref_dbg(bgrp_lock_t *glock,
                                            const char *file,
                                            int line)
{
    grp_lock_ref *ref;

    BASE_UNUSED_ARG(line);

    benter_critical_section();
    /* Find the same source file */
    ref = glock->ref_list.next;
    while (ref != &glock->ref_list) {
	if (strcmp(ref->file, file) == 0) {
	    blist_erase(ref);
	    blist_push_back(&glock->ref_free_list, ref);
	    break;
	}
	ref = ref->next;
    }
    bleave_critical_section();

    if (ref == &glock->ref_list) {
	BASE_ERROR("bgrp_lock_dec_ref_dbg() could not find matching ref for %s", file);
    }

    return grp_lock_dec_ref_dump(glock);
}
#else
bstatus_t bgrp_lock_add_ref(bgrp_lock_t *glock)
{
    return grp_lock_add_ref(glock);
}

bstatus_t bgrp_lock_dec_ref(bgrp_lock_t *glock)
{
    return grp_lock_dec_ref(glock);
}
#endif

int bgrp_lock_get_ref(bgrp_lock_t *glock)
{
    return batomic_get(glock->ref_cnt);
}

bstatus_t bgrp_lock_chain_lock( bgrp_lock_t *glock,
                                            block_t *lock,
                                            int pos)
{
    grp_lock_item *lck, *new_lck;
    int i;

    grp_lock_acquire(glock);

    for (i=0; i<glock->owner_cnt; ++i)
	block_acquire(lock);

    lck = glock->lock_list.next;
    while (lck != &glock->lock_list) {
	if (lck->prio >= pos)
	    break;
	lck = lck->next;
    }

    new_lck = BASE_POOL_ZALLOC_T(glock->pool, grp_lock_item);
    new_lck->prio = pos;
    new_lck->lock = lock;
    blist_insert_before(lck, new_lck);

    /* this will also release the new lock */
    grp_lock_release(glock);
    return BASE_SUCCESS;
}

bstatus_t bgrp_lock_unchain_lock( bgrp_lock_t *glock,
                                              block_t *lock)
{
    grp_lock_item *lck;

    grp_lock_acquire(glock);

    lck = glock->lock_list.next;
    while (lck != &glock->lock_list) {
	if (lck->lock == lock)
	    break;
	lck = lck->next;
    }

    if (lck != &glock->lock_list) {
	int i;

	blist_erase(lck);
	for (i=0; i<glock->owner_cnt; ++i)
	    block_release(lck->lock);
    }

    grp_lock_release(glock);
    return BASE_SUCCESS;
}

void bgrp_lock_dump(bgrp_lock_t *grp_lock)
{
#if BASE_GRP_LOCK_DEBUG
    grp_lock_ref *ref;
    char info_buf[1000];
    bstr_t info;

    info.ptr = info_buf;
    info.slen = 0;

    grp_lock_add_ref(grp_lock);
    benter_critical_section();

    ref = grp_lock->ref_list.next;
    while (ref != &grp_lock->ref_list && info.slen < sizeof(info_buf)) {
	char *start = info.ptr + info.slen;
	int max_len = sizeof(info_buf) - info.slen;
	int len;

	len = bansi_snprintf(start, max_len, "\t%s:%d\n", ref->file, ref->line);
	if (len < 1 || len >= max_len) {
	    len = strlen(ref->file);
	    if (len > max_len - 1)
		len = max_len - 1;

	    memcpy(start, ref->file, len);
	    start[len++] = '\n';
	}

	info.slen += len;

	ref = ref->next;
    }

    if (ref != &grp_lock->ref_list) {
	int i;
	for (i=0; i<4; ++i)
	    info_buf[sizeof(info_buf)-i-1] = '.';
    }
    info.ptr[info.slen-1] = '\0';

    bleave_critical_section();

    BASE_INFO("Group lock %p, ref_cnt=%d. Reference holders:\n%s", grp_lock, bgrp_lock_get_ref(grp_lock)-1, info.ptr);

    grp_lock_dec_ref(grp_lock);
#else
    BASE_INFO("Group lock %p, ref_cnt=%d.", grp_lock, bgrp_lock_get_ref(grp_lock));
#endif
}

