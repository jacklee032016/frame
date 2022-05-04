/*
* queue: FIFO, put at the tail of list; get from head of list
*/

#include <stdio.h>
#include <stdlib.h>

struct QueueItem
{
    int                 data;
    struct QueueItem    *next;
};

typedef struct QueueItem    QItem;

typedef struct _QueueOnList
{
    QItem   *head;
    QItem   *tail;
}QueueOnList;

QItem *createQItem(int data)
{
    QItem *node = (QItem *)malloc(sizeof(QItem));
    if(!node)
        return node;

    node->next = NULL;
    node->data = data;

    return node;
}


// add at tail of list
QItem* queuePut(QueueOnList *queue, int data)
{
    QItem *item = NULL;
    
    item = createQItem(data);
    if(!item)
    {
        return NULL;
    }

    if(queue->tail)
        queue->tail->next = item;
    if(!queue->head)
        queue->head = item;
        
    queue->tail = item;

    return item;
}

// 
int queueIsEmpty(QueueOnList *queue)
{// queue->head == queue->tail: this is the last one
    return (queue->head == NULL);// queue->tail);
}

// get from head of list
QItem* queueGet(QueueOnList *queue)
{
    QItem *cur = NULL;
    
    if(!queue)
        return NULL;

    if(queueIsEmpty(queue))
        return NULL;

    cur = queue->head;
    queue->head = cur->next;

    printf("Get %d\n", cur->data);
    return cur;
}

void queueOnListPrint(QueueOnList *queue)
{
    QItem *cur = queue->head;
    int i = 0;

    while(cur)
    {
        printf("#%d: %d\n", i++, cur->data);
        cur = cur->next;
    }
    printf("\n");
}


int main()
{
    QItem *item = NULL;
    QueueOnList    queue = {NULL, NULL};
    
    queuePut(&queue, 1);
    queuePut(&queue, 2);

#if 0
    queuePut(&queue, 4);
    queuePut(&queue, 6);
    queuePut(&queue, 7);
    queuePut(&queue, 7);
    queuePut(&queue, 9);
    queuePut(&queue, 11);
    queuePut(&queue, 12);
#endif

    queueOnListPrint(&queue);

    item = queueGet(&queue);
    free(item);
    queueOnListPrint(&queue);

    item = queueGet(&queue);
    free(item);
    queueOnListPrint(&queue);

    printf("Stack Is Empty: %d\n", queueIsEmpty(&queue));

    return 0;
}




