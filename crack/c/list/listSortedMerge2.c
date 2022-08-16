#include <stdio.h>
#include <stdlib.h>

struct  SortedNode
{
    int                 data;
    struct SortedNode   *next;
};

struct SortedNode* createNode(int data)
{
    struct SortedNode *node = malloc(sizeof(struct SortedNode));
    if(!node)
        return NULL;

    node->data = data;
    node->next = NULL;
    return node;
}

void printSortedList(struct SortedNode *head)
{
    struct SortedNode *node = head;

    while(node)
    {
        printf("%d ", node->data);
        node = node->next;
    }

    printf("\n");
}


struct SortedNode* merge2SortedList(struct SortedNode *lista, struct SortedNode *listb)
{
    struct SortedNode *result = NULL;

    if(!lista)
        return listb;
    else if(!listb)
        return lista;

    if(lista->data == listb->data)
    {
        result = lista;
        result->next = merge2SortedList(lista->next, listb->next);
        free(listb);
    }
    else if(lista->data > listb->data)
    {
        result = listb;
        result->next = merge2SortedList(lista, listb->next);
    }
    else
    {
        result = lista;
        result->next = merge2SortedList(lista->next, listb);
    }

    return result;
}


struct SortedNode* mergeSortedList(struct SortedNode *arrays[], int count)
{
//    struct SortedNode *res = NULL;
    struct SortedNode *lista = arrays[0];
    int i;

    for(i=1; i< count; i++)
    {
        lista = merge2SortedList(lista, arrays[i]);
    }

    return lista;
}

int main(int argc, char *argv[])
{
    struct SortedNode *arrays[3] = {0};

    arrays[0] = createNode(1);
    arrays[0]->next = createNode(3);
    arrays[0]->next->next = createNode(5);
    arrays[0]->next->next->next = createNode(7);

    arrays[1] = createNode(2);
    arrays[1]->next = createNode(7);
    arrays[1]->next->next = createNode(11);

    arrays[2] = createNode(3);
    arrays[2]->next = createNode(6);
    arrays[2]->next->next = createNode(10);

    arrays[0] = mergeSortedList(arrays, 3);

    printSortedList(arrays[0]);
}

