#include <stdio.h>
#include <stdlib.h>

struct QueueNode
{
    int             data;
    struct QueueNode *next;
};

struct QueueOnList
{
    struct QueueNode    *head, *last;
};


int enqueue(struct QueueOnList *queue, int data)
{
    struct QueueNode *node = malloc(sizeof(struct QueueNode));
    if(!node)
        return -1;

    node->data = data;
    node->next = NULL;
    
    if(!queue->head)
    {
        queue->head = node;
    }

    if(!queue->last)
        queue->last = node;
    else
    {
        queue->last->next = node;
        queue->last = node;
    }

    return 0;
}

int dequeue(struct QueueOnList *queue)
{
    struct  QueueNode *node = queue->head;
    
    int data = -1;

    if(!node)
        return -1;
    queue->head = node->next;

    data = node->data;
    free(node);

    return data;
}


int main(int argc, char *argv[])
{
    struct QueueOnList queue = {0};
    int data;

    enqueue(&queue, 9);
    enqueue(&queue, 8);
    enqueue(&queue, 7);
    enqueue(&queue, 6);
    enqueue(&queue, 5);

    data = dequeue(&queue);
    while(data!=-1)
    {
        printf("%d ", data);
        data = dequeue(&queue);
    };
    printf("\n");
    
    return 0;
}

