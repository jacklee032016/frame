#include <stdio.h>
#include <stdlib.h>


struct ListSNode
{
    int                 data;
    struct ListSNode    *next;
};

typedef struct ListSNode SNode;

SNode *createNode(int data)
{
    SNode *node = (SNode *)malloc(sizeof(SNode));
    if(!node)
        return node;

    node->next = NULL;
    node->data = data;

    return node;
}

// add at head of list
SNode* listSingleInsert(SNode *head, int data)
{
    SNode *node = NULL;
    if(!head)
    {
        head = createNode(data);
        return head;
    }

    node = createNode(data);
    if(node)
    {
        node->next = head;
        return node;
    }

    return head;
}

SNode* listSingleReverse(SNode *head)
{
    SNode *prev = NULL, *next;
    SNode *current = head;

    while(current)
    {
        next = current->next;
        current->next = prev;
        prev = current;
        current = next;
    }

    return prev;
}

void listSinglePrint(SNode *head)
{
    SNode *current = head;
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
    SNode *list = listSingleInsert(NULL, 1);

    list = listSingleInsert(list, 3);
    list = listSingleInsert(list, 5);
    list = listSingleInsert(list, 7);
    list = listSingleInsert(list, 9);
    list = listSingleInsert(list, 11);
    list = listSingleInsert(list, 13);

    listSinglePrint(list);

    list = listSingleReverse(list);

    listSinglePrint(list);

    return 0;
}

