#include <stdio.h>
#include <stdlib.h>


struct ListDblNode
{
    int                   data;
    
    struct ListDblNode    *prev;
    struct ListDblNode    *next;
};

typedef struct ListDblNode DNode;

DNode *createDblNode(int data)
{
    DNode *node = (DNode *)malloc(sizeof(DNode));
    if(!node)
        return node;

    node->prev = NULL;
    node->next = NULL;
    node->data = data;

    return node;
}

// add at head of list
DNode* listDblInsert(DNode *head, int data)
{
    DNode *node = NULL;
    if(!head)
    {
        head = createDblNode(data);
        return head;
    }

    node = createDblNode(data);
    if(node)
    {
        head->prev = node;
        node->next = head;
        return node;
    }

    return head;
}

DNode* listDbleReverse(DNode *head)
{
    DNode *prev = NULL, *next = NULL;
    DNode *current = head;

    while(current)
    {
        next = current->next;
#if 0        
        if(next)
        {
            next->prev = current;
        }
#endif        
        // assign double link
        current->prev = next;
        current->next = prev;

        // get prev, current, and next
        prev = current;
        current = next;
    }

    return prev;
}

void listDblPrint(DNode *head)
{
    DNode *current = head;
    int i = 0;

    while(current)
    {
        printf("#%d: %d<-- %d -->%d\n", i++, 
            current->prev?current->prev->data:-1, current->data, current->next?current->next->data:-1);
        current = current->next;
    }
    printf("\n");
}


int main()
{
    DNode *list = listDblInsert(NULL, 1);

    list = listDblInsert(list, 3);
    list = listDblInsert(list, 5);
    list = listDblInsert(list, 7);
    list = listDblInsert(list, 9);
    list = listDblInsert(list, 11);
    list = listDblInsert(list, 13);

    listDblPrint(list);

    list = listDbleReverse(list);

    listDblPrint(list);

    return 0;
}

