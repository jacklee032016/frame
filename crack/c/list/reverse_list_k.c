
#include <stdlib.h>
#include <stdio.h>

struct Node
{
    int         data;
    struct Node *next;
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

struct Node* reverse(struct Node *head, int pos)
{
    struct Node *prev = NULL, *next = NULL, *current, *first;
    struct Node *newHead = NULL;
    int index = 0;

    current = head;

    // at beginning, don't know which one is the first out of the range, so prev is null
//    prev = head->next;
    while(current)
    {
        if(index >= pos)
        {
//            current->next = next->next;
            first->next = current;
            newHead = prev;
            break;
        }

        next = current->next;
        current->next = prev;
        prev = current; // 

        current = next;//current->next;
        printf("%d %d; ", prev->data, current->data);
        index++;
        if(index == 1)
        {
            first = prev;
        }
    }

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
     head = reverse(head, 5);
 
     printf("\nReversed Linked list \n");
     printList(head);
 
     return(0);
}


