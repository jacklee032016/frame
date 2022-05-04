#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

/*
* static memory pool
*    memory block for pool has been allocated when building, no malloc in system
*    no dynamic memory is support, only used in memory limited system
*/

#define     MEM_ALIGNED_BORDER      8

#define ALIGNED_MEM(size)          (( (size)+MEM_ALIGNED_BORDER-1)&(~(MEM_ALIGNED_BORDER-1)) )

struct  MemBlock
{
    struct MemBlock *next;
};

#define    MEMB_HEADER_SIZE           ALIGNED_MEM(sizeof(struct MemBlock))

struct  MemDescrip
{
    char                *desc;
    
    uint16_t            num;
    uint16_t            size;

    uint8_t             *baseAddress;

    struct MemBlock     **blocks;   // can by pointer, not pointer to pointer??
};


#define     MEMB_NUM        128
#define     MEMB_SIZE       64


static      uint8_t              _memp_base[MEMB_NUM*ALIGNED_MEM(MEMB_SIZE+MEMB_HEADER_SIZE)];

static      struct  MemBlock     *_memb;

static      struct  MemDescrip   _memDesc = 
{
    .desc = "StaticMemPool",
    .num  = MEMB_NUM,
    .size = MEMB_SIZE,
    .baseAddress = _memp_base,
    .blocks      = &_memb       // assign a pointer to blocks
};


int staticMemPoolInit(struct MemDescrip *memDesc)
{
    int i;
    struct MemBlock     *memB = NULL;

    *memDesc->blocks = NULL;
    memB = (struct MemBlock *)memDesc->baseAddress;
    for(i=0; i< memDesc->num; i++)
    {
        memB->next = *memDesc->blocks;
        *memDesc->blocks = memB;

        memB = (struct MemBlock *)(void *)( (uintptr_t)memB + MEMB_HEADER_SIZE+ memDesc->size);
    }

    return 0;
}


void* staticMemPoolAllocate(struct MemDescrip *memDesc)
{
    struct MemBlock *memb = NULL;

    if(!memDesc || !memDesc->blocks)
    {
        return NULL;
    }

    memb = *memDesc->blocks;

    *memDesc->blocks = memb->next;
    memb->next = NULL; // cut the old relationship

    return (void *)((uintptr_t)memb + MEMB_HEADER_SIZE);
}

void staticMemPoolFree(struct MemDescrip *memDesc, void *ptr)
{
    struct MemBlock *memb = NULL;

    if(!memDesc || !ptr)
        return;

    memb = (struct MemBlock *)((uintptr_t)ptr - MEMB_HEADER_SIZE);

    memb->next = *memDesc->blocks;
    *memDesc->blocks = memb;
}

int main(int argc, char *argv[])
{
    void    *ptr = NULL;
    
    staticMemPoolInit(&_memDesc);

    ptr = staticMemPoolAllocate(&_memDesc);

    staticMemPoolFree(&_memDesc, ptr);
    
    return 0;
}

