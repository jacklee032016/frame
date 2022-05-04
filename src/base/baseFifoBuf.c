/* 
 *
 */
#include <baseFifoBuf.h>
#include <baseLog.h>
#include <baseAssert.h>
#include <baseOs.h>


#define SZ  sizeof(unsigned)

void bfifobuf_init (bfifobuf_t *fifobuf, void *buffer, unsigned size)
{
    BASE_CHECK_STACK();

    BASE_INFO("fifobuf_init fifobuf=%p buffer=%p, size=%d", fifobuf, buffer, size);

    fifobuf->first = (char*)buffer;
    fifobuf->last = fifobuf->first + size;
    fifobuf->ubegin = fifobuf->uend = fifobuf->first;
    fifobuf->full = 0;
}

unsigned bfifobuf_max_size (bfifobuf_t *fifobuf)
{
    unsigned s1, s2;

    BASE_CHECK_STACK();

    if (fifobuf->uend >= fifobuf->ubegin)
    {
      	s1 = (unsigned)(fifobuf->last - fifobuf->uend);
       	s2 = (unsigned)(fifobuf->ubegin - fifobuf->first);
    }
    else {
    	s1 = s2 = (unsigned)(fifobuf->ubegin - fifobuf->uend);// after init, max_size is 0??
    }
    
    return s1<s2 ? s2 : s1;
}

void* bfifobuf_alloc (bfifobuf_t *fifobuf, unsigned size)
{
    unsigned available;
    char *start;

    BASE_CHECK_STACK();

    if (fifobuf->full) {
	    BASE_INFO("fifobuf_alloc fifobuf=%p, size=%d: full!", fifobuf, size);
    	return NULL;
    }

    /* try to allocate from the end part of the fifo */
    if (fifobuf->uend >= fifobuf->ubegin)
    {
    	available = (unsigned)(fifobuf->last - fifobuf->uend);
    	if (available >= size+SZ)
        {
    	    char *ptr = fifobuf->uend;
    	    fifobuf->uend += (size+SZ);
            
    	    if (fifobuf->uend == fifobuf->last)
        		fifobuf->uend = fifobuf->first;
            
    	    if (fifobuf->uend == fifobuf->ubegin)
        		fifobuf->full = 1;
            
    	    *(unsigned*)ptr = size+SZ; // save length in ptr; length|return pointer position
    	    ptr += SZ;

    	    BASE_INFO("fifobuf_alloc fifobuf=%p, size=%d: returning %p, p1=%p, p2=%p", fifobuf, size, ptr, fifobuf->ubegin, fifobuf->uend);
    	    return ptr;
    	}
    }

    /* try to allocate from the start part of the fifo */
    start = (fifobuf->uend <= fifobuf->ubegin) ? fifobuf->uend : fifobuf->first;
    available = (unsigned)(fifobuf->ubegin - start);
    if (available >= size+SZ)
    {
    	char *ptr = start;
        
    	fifobuf->uend = start + size + SZ;
    	if (fifobuf->uend == fifobuf->ubegin)
    	    fifobuf->full = 1;
        
    	*(unsigned*)ptr = size+SZ;
    	ptr += SZ;

    	BASE_INFO("fifobuf_alloc fifobuf=%p, size=%d: returning %p, p1=%p, p2=%p", fifobuf, size, ptr, fifobuf->ubegin, fifobuf->uend);
    	return ptr;
    }

    BASE_INFO("fifobuf_alloc fifobuf=%p, size=%d: no space left! p1=%p, p2=%p", 
	       fifobuf, size, fifobuf->ubegin, fifobuf->uend);
    return NULL;
}

bstatus_t bfifobuf_unalloc (bfifobuf_t *fifobuf, void *buf)
{
    char *ptr = (char*)buf;
    char *endptr;
    unsigned sz;

    BASE_CHECK_STACK();

    ptr -= SZ;
    sz = *(unsigned*)ptr; // get length of buf

    endptr = fifobuf->uend;
    if (endptr == fifobuf->first)
    	endptr = fifobuf->last;

    if (ptr+sz != endptr) {
    	bassert(!"Invalid pointer to undo alloc");
    	return -1;
    }

    fifobuf->uend = ptr;
    fifobuf->full = 0;

    BASE_INFO("fifobuf_unalloc fifobuf=%p, ptr=%p, size=%d, p1=%p, p2=%p", fifobuf, buf, sz, fifobuf->ubegin, fifobuf->uend);

    return 0;
}

bstatus_t bfifobuf_free (bfifobuf_t *fifobuf, void *buf)
{
    char *ptr = (char*)buf;
    char *end;
    unsigned sz;

    BASE_CHECK_STACK();

    ptr -= SZ;
    if (ptr < fifobuf->first || ptr >= fifobuf->last) {
    	bassert(!"Invalid pointer to free");
    	return -1;
    }

    if (ptr != fifobuf->ubegin && ptr != fifobuf->first) {
    	bassert(!"Invalid free() sequence!");
    	return -1;
    }

    end = (fifobuf->uend > fifobuf->ubegin) ? fifobuf->uend : fifobuf->last;
    sz = *(unsigned*)ptr;
    if (ptr+sz > end) {
    	bassert(!"Invalid size!");
    	return -1;
    }

    fifobuf->ubegin = ptr + sz;

    /* Rollover */
    if (fifobuf->ubegin == fifobuf->last)
    	fifobuf->ubegin = fifobuf->first;

    /* Reset if fifobuf is empty */
    if (fifobuf->ubegin == fifobuf->uend)
    	fifobuf->ubegin = fifobuf->uend = fifobuf->first;

    fifobuf->full = 0;

    BASE_INFO("fifobuf_free fifobuf=%p, ptr=%p, size=%d, p1=%p, p2=%p", fifobuf, buf, sz, fifobuf->ubegin, fifobuf->uend);

    return 0;
}

