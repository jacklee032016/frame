#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct StackOnList
{
    int                 data;
    struct StackOnList  *next;
};


int stackPush(struct StackOnList **stack, int data)
{
    struct StackOnList *node = malloc(sizeof(struct StackOnList));
    if(!node)
        return -1;

    node->data = data;
    if(*stack)
    {
        node->next = *stack;
//        *stack = node;
    }
    else
    {
    }
    *stack = node;

    return 0;
}

struct StackOnList* stackPop(struct StackOnList **stack)
{
    struct StackOnList *head = *stack;
    if(!*stack)
        return NULL;

    *stack = head->next;

    return head;
    
}


int main(int argc, char *argv[])
{
    struct StackOnList *stack = NULL, *node = NULL;

    stackPush(&stack, 9);
    stackPush(&stack, 8);
    stackPush(&stack, 7);
    stackPush(&stack, 6);
    stackPush(&stack, 5);

    node = stackPop(&stack);
    while(node)
    {
        printf("%d ", node->data);
        node = stackPop(&stack);
    }
    printf("\n");

    return 0;
}

