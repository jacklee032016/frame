#include <stdio.h>
#include <stdlib.h>

struct ListNode
{
    int                 data;
    struct ListNode    *next;
};

typedef struct ListNode Node;

typedef struct _List
{
    Node *head;
    Node *tail;
}List;

Node *createNode(int data)
{
    Node *node = (Node *)malloc(sizeof(Node));
    if(!node)
        return node;

    node->next = NULL;
    node->data = data;

    return node;
}

// add at head of list
Node* listInsert(Node **head, int data)
{
    Node *node = NULL;
    
    node = createNode(data);
    if(!node)
    {
        return *head;
    }
    
    node->next = *head;
    *head = node;

    return node;
}

// add at head of list
Node* listAppend(List *list, int data)
{
    Node *node = NULL;
    
    node = createNode(data);
    if(!node)
    {
        return NULL;
    }

    if(!list->head)
        list->head = node;

    if(list->tail)
        list->tail->next = node; // last->next point to node
        
    list->tail = node; // now node is new tail

    return node;
}

// find in no-sorted list
Node* listFind(List *list, int data)
{
    Node *cur = list->head;

    while(cur)
    {
        if(cur->data == data)
            return cur;
        
        cur = cur->next;
    }

    return NULL;
}


void listRemove(List *list, int data)
{
    Node *cur = NULL, *prev = NULL, *tmp = NULL;
    
    if(!list)
        return;

    cur = list->head;
    
    while(cur)
    {
        if(cur->data == data )
        {
            if(!prev)
            {// this is first one in list
                list->head = cur->next;
            }
            else
            {
                prev->next = cur->next;
            }
            
            tmp = cur;
            // remove the consecutive one with same data
//            prev = cur->next;
            cur = cur->next;

            // remove all, clear tail
            if(!prev && !cur)
            {
                list->tail = NULL;
            }

            free(tmp);
            tmp = NULL;
        }
        else
        {
            prev = cur;
            cur = cur->next;
        }
    }
}

void listPrint(Node *head)
{
    Node *current = head;
    int i = 0;

    while(current)
    {
        printf("#%d: %d\n", i++, current->data);
        current = current->next;
    }
    printf("\n");
}


int main()
{
    Node *node = NULL;
    int data = 5;
    List    list = {NULL, NULL};
    
    listInsert(&list.head, 1);
    listInsert(&list.head, 2);

    listInsert(&list.head, 4);
    listInsert(&list.head, 6);
    listInsert(&list.head, 7);
    listInsert(&list.head, 9);
    listInsert(&list.head, 11);
    listInsert(&list.head, 12);

    listPrint(list.head);

    List    list2 = {NULL, NULL};
    
    listAppend(&list2, 1);
    listAppend(&list2, 2);

    listAppend(&list2, 4);
    listAppend(&list2, 6);
    listAppend(&list2, 7);
    listAppend(&list2, 7);
    listAppend(&list2, 9);
    listAppend(&list2, 11);
    listAppend(&list2, 12);

    listPrint(list2.head);

    data = 5;
    node = listFind(&list2, data);
    printf("Find %d: %d\n", data, (node)?node->data: -1);

    data = 7;
    node = listFind(&list2, data);
    printf("Find %d: %d\n", data, (node)?node->data: -1);


    data = 1; // remove the first one
    listRemove(&list2, data);
    listPrint(list2.head);

    data = 7; // remove duplicated items with same data
    listRemove(&list2, data);
    listPrint(list2.head);

    data = 8; // remove one not existed
    listRemove(&list2, data);
    listPrint(list2.head);

    data = 12; // remove last one
    listRemove(&list2, data);
    listPrint(list2.head);

    // remove all
    data = 2;
    listRemove(&list2, data);
    data = 4;
    listRemove(&list2, data);
    data = 6;
    listRemove(&list2, data);
    data = 9;
    listRemove(&list2, data);
    data = 11;
    listRemove(&list2, data);

    printf("After remove all:\n");
    listPrint(list2.head);
    printf("head: %d; tail:%d\n", list2.head?list2.head->data:-1, list2.tail?list2.tail->data:-1 );

    return 0;
}


