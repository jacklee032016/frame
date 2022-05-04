/*
* queue: FIFO, based on array
*/

#include <stdio.h>
#include <stdlib.h>

typedef struct _QueueOnArray
{
    int     *data;
    int     capacity;
    int     mask;
    int     readIndex;
    int     writeIndex;
}QueueOnArray;

#if 1
#define     NEXT_INDEX(queue, index)        (( (index)+1)% (queue)->capacity)
#else
#define     NEXT_INDEX(queue, index)        (( (index)+1)&(queue)->mask)
#endif

QueueOnArray *queueArrayCreate(int capacity)
{
    QueueOnArray *queue = NULL;

    if(capacity&(capacity-1))
    {
        printf("capacity %d is 2**n, please select correct capacity\n", capacity);
        return NULL;
    }
    
    queue = (QueueOnArray *)malloc(sizeof(QueueOnArray));
    if(!queue)
        return NULL;

    queue->data = (int *)malloc(sizeof(int)*capacity);
    if(!queue->data)
    {
        free(queue);
        return NULL;
    }

    // for example, when capacity is 4, and writeIndex=3, eg. writeIndex is next writing position; 
    // if no capacity+1, then next index will be (3+1)%4=0, eg. isFull; if (3+1)%5=4, is not full, write to index=3
    queue->capacity = capacity+1; // must add 1, otherwise, the last one in array always is not used
    queue->mask = capacity-1; // so n lsb are 1
    queue->readIndex = 0;
    queue->writeIndex = 0;

    return queue;
}

void queueArrayFree(QueueOnArray **queue)
{
    free( (*queue)->data);
    free(*queue);
    
    *queue = NULL;
}

int nextIndex(QueueOnArray *queue, int index)
{
    int _index = (index+1)%queue->capacity;
    printf("%d next index: %d\n", index, _index);
    return _index;
}

// 
int queueArrayIsEmpty(QueueOnArray *queue)
{
    return (queue->readIndex == queue->writeIndex);
}

int queueArrayIsFull(QueueOnArray *queue)
{
    return (queue->readIndex == NEXT_INDEX(queue, queue->writeIndex) );
//    return (queue->readIndex == nextIndex(queue, queue->writeIndex) );
}

int queueArrayPut(QueueOnArray *queue, int data)
{
    if( queueArrayIsFull(queue))
    {
        printf("full, failed when put %d\n", data);
        return -1;
    }

    queue->data[queue->writeIndex] = data;
    printf("put #%d: %d\n", queue->writeIndex, data);
    queue->writeIndex = NEXT_INDEX(queue, queue->writeIndex);

    return data;
}


int queueArrayGet(QueueOnArray *queue)
{
    int data = -1;
    
    if( queueArrayIsEmpty(queue))
    {
        printf("empty, failed when get\n");
        return -1;
    }

    data = queue->data[queue->readIndex];
    queue->readIndex = NEXT_INDEX(queue, queue->readIndex);

    printf("Get %d\n", data);
    return data;
}


int main()
{
    QueueOnArray    *queue = NULL;

    queue = queueArrayCreate(4);
    if(!queue)
        return -1;
    
    queueArrayPut(queue, 1);
    queueArrayPut(queue, 2);
    queueArrayPut(queue, 3);
    queueArrayPut(queue, 4);

//    queueArrayGet(queue);
    queueArrayPut(queue, 5);
    queueArrayPut(queue, 6);


    queueArrayGet(queue);
    queueArrayGet(queue);
    queueArrayGet(queue);
    queueArrayGet(queue);
    queueArrayGet(queue);
    queueArrayGet(queue);

    printf("Queue Is Empty: %d\n", queueArrayIsEmpty(queue));
    printf("Queue Is Full: %d\n", queueArrayIsFull(queue));

    queueArrayFree(&queue);
    return 0;
}

