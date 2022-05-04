#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define     ALIGNED_BOARD       16

#define     MEM_ALIGN(size)         (((size)+ALIGNED_BOARD-1)&(~(ALIGNED_BOARD-1)))

struct MBlock
{
    struct MBlock   *next;
};


struct MemBuffer
{
    void                *start;
    void                *end;

    void                *current;

    struct MemBuffer    *next;
};


struct MemPool
{
    int     buffer_size;
    int     num;
    int     block_size;

    struct  MemBuffer   *first;
    struct  MemBuffer   *last;
    
    struct  MBlock      *blocks;
};

struct MemBuffer* memBufferCreate(int size)
{
    struct MemBuffer *buf = NULL;

    buf = (struct MemBuffer*)malloc(sizeof(struct MemBuffer));
    if(!buf)
        return NULL;

    buf->start = (void *)malloc(size);
    if(!buf->start)
    {
        free(buf);
        return NULL;
    }

    buf->end = buf->start + size;
    buf->current = buf->start;
    buf->next = NULL;

    return buf;
}


struct MemPool* memPoolCreate(int blocks, int block_size)
{
    struct MemPool* pMpool = NULL;

    pMpool = (struct MemPool *)malloc(sizeof(struct MemPool));
    if(!pMpool)
        return NULL;
    
    memset(pMpool, 0, sizeof(struct MemPool));

    pMpool->num = blocks;
    pMpool->block_size = MEM_ALIGN(block_size);
    pMpool->buffer_size = blocks*pMpool->block_size;

    pMpool->first = memBufferCreate(pMpool->buffer_size);
    if(!pMpool->first)
    {
        free(pMpool);
        return NULL;
    }
    
    pMpool->last = pMpool->first;

    pMpool->blocks = NULL;

    return pMpool;
}

void memPoolDestroy(struct MemPool *pMpool)
{
    struct MemBuffer *buf = pMpool->first;
    struct MemBuffer *next = NULL;

    while(buf)
    {
        next = buf->next;
        free(buf->start);
        free(buf);
        buf = next;
    }
    
}

void *memPoolAllocate(struct MemPool *pMpool)
{
    struct MBlock *mb = pMpool->blocks;
    struct MemBuffer *buf = NULL;
    void *ptr = NULL;
    
    if(mb)
    {
        pMpool->blocks = mb->next;
        return (void *)mb;
    }

    if(pMpool->last->current >= pMpool->last->end)
    {
        buf = memBufferCreate(pMpool->buffer_size);
        if(!buf)
        {
            return NULL;
        }
        pMpool->last->next = buf;
        pMpool->last = buf;
    }

    ptr = pMpool->last->current;
    pMpool->last->current += pMpool->block_size;

    return ptr;
}


int memPoolFree(struct MemPool *pMpool, void *ptr)
{
    struct MBlock *mb = (struct MBlock *)ptr;
    struct MemBuffer *buf = pMpool->first;
    int isFound = 0;

    while(buf)
    {
        if(ptr >= buf->start && ptr < buf->end)
        {
            isFound = 1;
        }
        
        buf = buf->next;
    }
    
    if(!isFound)
    {
        printf("%p is not in the pool\n", ptr);
        return -1;
    }

    mb = pMpool->blocks;
    while(mb)
    {
        if( (void*)mb == ptr)
        {
            printf("%p has been freed, ignore it\n", ptr);
            return -1;
        }
        mb = mb->next;
    }

    mb = (struct MBlock *)ptr;
#if 0    
    mb->next = NULL;
    if(!pMpool->blocks)
    {
        pMpool->blocks = mb;
    }
    else
    {
        mb->next = pMpool->blocks;
        pMpool->blocks = mb;
    }
#else
    mb->next = pMpool->blocks;
    pMpool->blocks = mb;
#endif

    return 0;
}


int main(int argc, char *argv[])
{
//    struct MemPool *pMpool = memPoolCreate(128, 64);
    struct MemPool *pMpool = memPoolCreate(2, 62);
    int block_size = MEM_ALIGN(62);
    
    void *p1, *p2, *p3,*p4, *prev;
    if(!pMpool)
        return -1;

    p1 = memPoolAllocate(pMpool);
    assert(p1!=NULL);

    p2 = memPoolAllocate(pMpool);
    assert(p2!=NULL);
    assert( (p2-p1)==block_size);

    p3 = memPoolAllocate(pMpool);
    assert(p3!=NULL);

    assert( (p3-p2)>block_size);

    prev = p1;
    memPoolFree(pMpool, p1);
    p4 = memPoolAllocate(pMpool);
    assert(prev==p4);
    
    p1 = memPoolAllocate(pMpool);
    assert(p1-p3 == block_size);
    
    memPoolFree(pMpool, p2);
    memPoolFree(pMpool, p3);

    // duplicated free
    assert(memPoolFree(pMpool, p3)<0);

    // free invalidate pointer
    p3 = &block_size;
    assert(memPoolFree(pMpool, p3) <0);

    memPoolDestroy(pMpool);

    return 0;
}

