/*
* merge 2 or more sorted lists, typical 2 lists
*/

#include <stdio.h>
#include <stdlib.h>

struct ListNode
{
    int                 data;
    struct ListNode    *next;
};

typedef struct ListNode SortNode;

SortNode *createSortNode(int data)
{
    SortNode *node = (SortNode *)malloc(sizeof(SortNode));
    if(!node)
        return node;

    node->next = NULL;
    node->data = data;

    return node;
}

// add at head of list
SortNode* listSingleInsert(SortNode *head, int data)
{
    SortNode *node = NULL;

    node = createSortNode(data);
    if(!node)
    {
        return head;
    }
    
    if(head)
    {
        node->next = head;
    }

    return node;
}


void listSinglePrint(SortNode *head)
{
    SortNode *current = head;
    int i = 0;

    while(current)
    {
        printf("#%d: %d\n", i++, current->data);
        current = current->next;
    }
    printf("\n");
}

SortNode* listSortedMerge(SortNode *lista, SortNode *listb)
{
    SortNode *newList = NULL, *tmp = NULL;
    
    if(!lista )
        return listb;
    if(!listb)
        return lista;

    if(lista->data == listb->data)
    {
        newList = lista;
        tmp = listb;
        listb = listb->next;
        free(tmp);
        newList->next = listSortedMerge(lista->next, listb);
    }
    else if(lista->data > listb->data )
    {// sorted: decreasing
        newList = lista;
        newList->next = listSortedMerge(lista->next, listb);
    }
    else
    {
        newList = listb;
        newList->next = listSortedMerge(lista, listb->next);
    }

    return newList;
}


int main()
{
    SortNode *list = listSingleInsert(NULL, 1);
    SortNode *list2 = listSingleInsert(NULL, 1);

    list = listSingleInsert(list, 3);
    list = listSingleInsert(list, 5);
    list = listSingleInsert(list, 7);
    list = listSingleInsert(list, 9);
    list = listSingleInsert(list, 11);
    list = listSingleInsert(list, 13);

    list2 = listSingleInsert(list2, 2);
    list2 = listSingleInsert(list2, 4);
    list2 = listSingleInsert(list2, 6);
    list2 = listSingleInsert(list2, 9);
    list2 = listSingleInsert(list2, 12);

    list2 = listSingleInsert(list2, 16);

    listSinglePrint(list);
    listSinglePrint(list2);

    list = listSortedMerge(list, list2);

    printf("After merged:\n");
    listSinglePrint(list);

    return 0;
}



