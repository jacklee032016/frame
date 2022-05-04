
#include <stdlib.h>
#include <stdio.h>

struct Node
{
    int         data;
    struct Node *next;
};

struct Stack
{
    struct Node     *node;
    struct Stack    *next;
};

void printList(struct Node *head)
{
    struct Node *node = head;

    while(node)
    {
        printf("%d ", node->data);
        node = node->next;
    }

    printf("\n");
}

//int pushStack(struct Stack **stack, str)

int push(struct Node **head, int data)
{
    struct Node *node = malloc(sizeof(struct Node));
    if(!node)
        return -1;

    node->data = data;

    node->next = *head;
    *head = node;

    return 0;
}

struct Node *pop(struct Node **head)
{
    struct Node *node = NULL;
    if(*head)
    {
        node = *head;
        *head = node->next;
    }

    return node;
}

struct Node* reverse(struct Node **head, int pos)
{
    struct Node *node = NULL, *last = NULL, *node2;
    struct Node *current = NULL, *newHead = NULL;
    struct Node *stack = NULL;
    int index = 0;

    current = *head;

    while(current)
    {
        node = pop(&current);
        if(index >= pos || !node )
        {
            break;
        }
//        current = current->next;

//        node->next = NULL;
        // this is strict object stack, so 
        push(&stack, node->data);
        
        index++;
    }

    node2 = pop(&stack);
    index = 0;

    while(node2)
    {
        if(index ==0)
        {
            newHead = node2;
            last = node2;
        }
        else
        {
            last->next = node2;
            last = node2;
        }
        node2 = pop(&stack);
        index++;
    };

    last->next = node;

    return newHead;
}


int main(void)
{
    /* Start with the empty list */
    struct Node* head = NULL;
  
     /* Created Linked list is 1->2->3->4->5->6->7->8->9 */
     push(&head, 9);
     push(&head, 8);
     push(&head, 7);
     push(&head, 6);
     push(&head, 5);
     push(&head, 4);
     push(&head, 3);
     push(&head, 2);
     push(&head, 1);          
 
     printf("\nGiven linked list \n");
     printList(head);
     head = reverse(&head, 5);
 
     printf("\nReversed Linked list \n");
     printList(head);
 
     return(0);
}



