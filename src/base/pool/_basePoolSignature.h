/* 
 *
 */
#include <baseAssert.h>
#include <baseString.h>

#if BASE_SAFE_POOL
#   define SIG_SIZE		sizeof(buint32_t)

static void apply_signature(void *p, bsize_t size);
static void check_pool_signature(void *p, bsize_t size);

#   define APPLY_SIG(p,sz)	apply_signature(p,sz), \
				p=(void*)(((char*)p)+SIG_SIZE)
#   define REMOVE_SIG(p,sz)	check_pool_signature(p,sz), \
				p=(void*)(((char*)p)-SIG_SIZE)

#   define SIG_BEGIN	    0x600DC0DE
#   define SIG_END	    0x0BADC0DE

static void apply_signature(void *p, bsize_t size)
{
    buint32_t sig;

    sig = SIG_BEGIN;
    bmemcpy(p, &sig, SIG_SIZE);

    sig = SIG_END;
    bmemcpy(((char*)p)+SIG_SIZE+size, &sig, SIG_SIZE);
}

static void check_pool_signature(void *p, bsize_t size)
{
    buint32_t sig;
    buint8_t *mem = (buint8_t*)p;

    /* Check that signature at the start of the block is still intact */
    sig = SIG_BEGIN;
    bassert(!bmemcmp(mem-SIG_SIZE, &sig, SIG_SIZE));

    /* Check that signature at the end of the block is still intact.
     * Note that "mem" has been incremented by SIG_SIZE 
     */
    sig = SIG_END;
    bassert(!bmemcmp(mem+size, &sig, SIG_SIZE));
}

#else
#   define SIG_SIZE	    0
#   define APPLY_SIG(p,sz)
#   define REMOVE_SIG(p,sz)
#endif
