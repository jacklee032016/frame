/*
* queue: FIFO, based on array
*/

#include <stdio.h>
#include <stdlib.h>

typedef struct _StackOnArray
{
    int     *data;

    int     capacity;
    int     size;

    int     index;
}StackOnArray;

StackOnArray *stackArrayCreate(int capacity)
{
    StackOnArray *stack = (StackOnArray *)malloc(sizeof(StackOnArray));
    if(!stack)
        return NULL;

    stack->data = (int *)malloc(sizeof(int)*capacity);
    if(!stack->data)
    {
        free(stack);
        return NULL;
    }
    
    stack->capacity = capacity;
    stack->size = 0;
    stack->index = 0;

    return stack;
}

void stackArrayFree(StackOnArray **stack)
{
    free( (*stack)->data);
    free(*stack);
    
    *stack = NULL;
}


// 
int stackArrayIsEmpty(StackOnArray *stack)
{
    return (stack->size == 0);
}

int stackArrayIsFull(StackOnArray *stack)
{
    return (stack->size == stack->capacity );
}

int stackArrayPush(StackOnArray *stack, int data)
{
    if( stackArrayIsFull(stack))
    {
        printf("full, failed when PUSH %d\n", data);
        return -1;
    }

    stack->data[stack->index] = data;
    printf("PUSH #%d: %d\n", stack->index, data);
    stack->index++;
    stack->size++;

    return data;
}


int stackArrayPop(StackOnArray *stack)
{
    int data = -1;
    
    if( stackArrayIsEmpty(stack))
    {
        printf("empty, failed when POP\n");
        return -1;
    }

    data = stack->data[(stack->index-1)];
    stack->index--;
    stack->size--;

    printf("POP %d\n", data);
    return data;
}


int main()
{
    StackOnArray    *stack = NULL;

    stack = stackArrayCreate(5);
    
    stackArrayPush(stack, 1);
    stackArrayPush(stack, 2);
    stackArrayPush(stack, 3);
    stackArrayPush(stack, 4);

//    queueArrayGet(queue);
    stackArrayPush(stack, 5);
    stackArrayPush(stack, 6);


    stackArrayPop(stack);
    stackArrayPop(stack);
    stackArrayPop(stack);
    stackArrayPop(stack);

    stackArrayPush(stack, 7);
    stackArrayPush(stack, 8);
    stackArrayPush(stack, 9);
    
    stackArrayPop(stack);
    stackArrayPop(stack);
    stackArrayPop(stack);
    stackArrayPop(stack);

    printf("stack Is Empty: %d\n", stackArrayIsEmpty(stack));
    printf("stack Is Full: %d\n", stackArrayIsFull(stack));

    stackArrayFree(&stack);
    return 0;
}


