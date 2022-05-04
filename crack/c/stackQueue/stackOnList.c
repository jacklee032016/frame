/*
* Stack: FILO, push at the beginning of list; pop also at beginning of list
*/
#include <stdio.h>
#include <stdlib.h>

struct StackItem
{
    int                 data;
    struct StackItem    *next;
};

typedef struct StackItem    SItem;

typedef struct _StackOnList
{
    SItem   *head;
//    SItem   *tail;
}StackOnList;

SItem *createItem(int data)
{
    SItem *node = (SItem *)malloc(sizeof(SItem));
    if(!node)
        return node;

    node->next = NULL;
    node->data = data;

    return node;
}


// push at head of list
SItem* stackPush(StackOnList *stack, int data)
{
    SItem *item = NULL;
    
    item = createItem(data);
    if(!item)
    {
        return NULL;
    }

    if(stack->head)
        item->next = stack->head;

    stack->head = item;

    return item;
}

int stackIsEmpty(StackOnList *stack)
{
    return (stack->head == NULL);
}

// FOLI, pop from head of list
SItem* stackPop(StackOnList *stack)
{
    SItem *cur = NULL;
    
    if(!stack)
        return NULL;

    if(stackIsEmpty(stack))
        return NULL;

    cur = stack->head;
    stack->head = cur->next;

    printf("Pop %d\n", cur->data);
    return cur;
}

void stackOnListPrint(StackOnList *stack)
{
    SItem *cur = stack->head;
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
    SItem *item = NULL;
    StackOnList    stack = {NULL};//, NULL};
    
    stackPush(&stack, 1);
    stackPush(&stack, 2);

#if 0
    stackPush(&stack, 4);
    stackPush(&stack, 6);
    stackPush(&stack, 7);
    stackPush(&stack, 7);
    stackPush(&stack, 9);
    stackPush(&stack, 11);
    stackPush(&stack, 12);
#endif

    stackOnListPrint(&stack);

    item = stackPop(&stack);
    free(item);
    stackOnListPrint(&stack);

    item = stackPop(&stack);
    free(item);
    stackOnListPrint(&stack);

    printf("Stack Is Empty: %d\n", stackIsEmpty(&stack));

    return 0;
}



