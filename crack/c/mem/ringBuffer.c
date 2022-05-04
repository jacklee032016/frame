#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>


struct _RingBuffer
{
    int     read;
    int     write;

    int     capacity;
    int     mask;

    void    **items;
};

typedef struct _RingBuffer  RingBuf;


int RingBufferInit(RingBuf *pBuf, int capacity)
{
    if((capacity&(capacity-1))!=0)
    {
        printf("Capacity must be 2**n, now %u\n");
        return -1;
    }

    pBuf->items = (void **)malloc(sizeof(void *)*capacity);
    if(!pBuf->items)
        return -1;

    pBuf->capacity = capacity;
    pBuf->mask = capacity-1;

    pBuf->read = pBuf->write = 0;

    return 0;
}

int RingBufferClear(RingBuf *pBuf)
{
    while(!RingBufferIsEmpty(pBuf))
    {
        RingBufferGet(pBuf);
    }

    free(pBuf->items);
    return 0;
}

int RingBufferAdd(RingBuf *pBuf, void *data)
{
    if(RingBufferIsFull(pBuf))
        return -1;

    pBuf->items[pBuf->write] = data;
    pBuf->write = (pBuf->write+1)&(pBuf->mask); // modulate operation

    return pBuf->write;
}

void* RingBufferGet(RingBuf *pBuf)
{
    void *data = NULL;
    
    if(RingBufferIsEmpty(pBuf))
        return NULL;

    data = pBuf->items[pBuf->read];
    pBuf->read = (pBuf->read+1) &(pBuf->mask);

    return data;
}

void* RingBufferPeek(RingBuf *pBuf)
{
    if(RingBufferIsEmpty(RingBuf * pBuf))
        return NULL;

    return pBuf->items[pBuf->read];
}

int RingBufferIsEmpty(RingBuf *pBuf)
{
    return (pBuf->read == pBuf->write);
}

int RingBufferIsFull(RingBuf *pBuf)
{// replace modulate operation with '&(mask)'
    return ((pBuf->write+1)&(pBuf->mask) == pBuf->read;
}

int RingBufferSize(RingBuf *pBuf)
{
    return ((pBuf->write - pBuf->read + pBuf->capacity)&(pBuf->mask);
}


int main(int argc, char *argv[])
{

    return 0;
}

