#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>


struct _RingBuffer
{
    int     read;
    int     write;

    int     blockSize;
    int     capacity; // block count
    int     mask;

    void    *buffer;
};

typedef struct _RingBuffer  RingBuffer;


int RingBuffer2Init(RingBuffer *pBuf, int size, int capacity)
{
    if((capacity&(capacity-1))!=0)
    {
        printf("Capacity must be 2**n, now %u\n");
        return -1;
    }

    pBuf->buffer = (void **)malloc(size*capacity);
    if(!pBuf->buffer)
        return -1;

    pBuf->blockSize = size;
    pBuf->capacity = capacity;
    pBuf->mask = capacity-1;

    pBuf->read = pBuf->write = 0;

    return 0;
}

int RingBufferClear(RingBuffer *pBuf)
{
    while(!RingBufferIsEmpty(pBuf))
    {
        RingBufferGet(pBuf);
    }

    free(pBuf->buffer);
    return 0;
}

int RingBufferAdd(RingBuffer *pBuf, void *data)
{
    if(RingBufferIsFull(pBuf))
        return -1;

    memcpy(pBuf->buffer+ pBuf->write*pBuf->blockSize, data, pBuf->blockSize);
    pBuf->write = (pBuf->write+1)&(pBuf->mask); // modulate operation

    return pBuf->write;
}

void* RingBufferGet(RingBuffer *pBuf)
{
    void *data = NULL;
    
    if(RingBufferIsEmpty(pBuf))
        return NULL;

    data = pBuf->buffer+ pBuf->read*pBuf->blockSize;
    pBuf->read = (pBuf->read+1) &(pBuf->mask);

    return data;
}

void* RingBufferPeek(RingBuffer *pBuf)
{
    if(RingBufferIsEmpty(RingBuf * pBuf))
        return NULL;

    return pBuf->buffer + pBuf->read*pBuf->blockSize;
}

int RingBufferIsEmpty(RingBuffer *pBuf)
{
    return (pBuf->read == pBuf->write);
}

int RingBufferIsFull(RingBuffer *pBuf)
{// replace modulate operation with '&(mask)'
    return ((pBuf->write+1)&(pBuf->mask) == pBuf->read;
}

int RingBufferSize(RingBuffer *pBuf)
{
    return ((pBuf->write - pBuf->read + pBuf->capacity)&(pBuf->mask);
}


int main(int argc, char *argv[])
{

    return 0;
}

