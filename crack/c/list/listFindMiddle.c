
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


int listFindMiddle(SNode *head)
{
    SNode *fast = head;
    SNode *slow = head;

    if(!head)
        return -1;

    while(fast && fast->next && slow)
    {
        fast = fast->next->next;
        slow = slow->next;
    }

    return slow->data;
}

// find the nth item from the end of the list
int listFindNthFromEnd(SNode *head, int nth)
{
/*
* find size of list, size> n;
* then loop from beginning for size-n+1 items
*/
    int i = 0;
    SNode *cur = head, *find = head;

    while(cur)
    {
        i++;
        cur = cur->next;
        if(i==nth)
            break;
    }

//    if(!cur && i<nth) // border here: when just n items in list, cur will be null 
    if(!cur || i<nth)
    {// list length is smaller than n
        return -1;
    }

    while(cur)
    {
        cur = cur->next;
        find = find->next;
    }

    return find->data;
}

/* loop at B: next of 2 different nodes point to same node
*     A--> B  --> C
*          ^      |
*          |      
*          E <--  D

*  node->next will never be null
* method:
*   1. fast will catch up with slow node soon or late
*   2. add field into item, indicating the item have been iterated: O(n)
*   3. iterate node, and calculate the distance between current item and the first item; if distance increase, no loop
*      When loop, last come back to one of the node which has been iterated, and distance from first to that node, will be smaller
*      O(n**2)
*/
int listDetectLoop(SNode *head)
{
    SNode *fast = head, *slow = head;

    while(slow && fast && fast->next)
    {
        slow = slow->next;
        fast = fast->next->next;

        /* node->next never be null, so soon or later
        * fast and slow are same node: not fast->next and slow->next point to same node
        */
        if(fast == slow)
        {
            printf("Find looped on node %d\n", fast->data);
            return 1;
        }

        // if not loop, it will return because next is NULL
    }

    return 0;
}

int listRemoveLoop(SNode *head)
{
    SNode *fast = head, *slow = head;
    SNode *prev = NULL;

    while(slow && fast && fast->next)
    {
        prev = slow;
        slow = slow->next;
        fast = fast->next->next;

        if(fast == slow)
        {// ? break the link list
            printf("Remove looped on node %d\n", fast->data);
            slow->next = NULL;
            return 1;
        }

        // if not loop, it will return because next is NULL
    }

    return 0;
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

int testLoop(void)
{
    SNode *loopNode = NULL;
    SNode *list = listSingleInsert(NULL, 13);
    loopNode = list; // last node will be loop node
    list = listSingleInsert(list, 12);
    list = listSingleInsert(list, 11);
    
    list = listSingleInsert(list, 10);
    
    list = listSingleInsert(list, 9);
    loopNode->next = list;
    list = listSingleInsert(list, 1);
    
    printf("Loop %d\n", listDetectLoop(list));

    listRemoveLoop(list);
    listSinglePrint(list);
    return 0;
}

int main()
{
    int nth = 4;
    SNode *list = listSingleInsert(NULL, 1);

    listSinglePrint(list);
    printf("middle %d\n", listFindMiddle(list)); // when only one item, the middle is the only one
    printf("last %dth is %d\n\n", nth, listFindNthFromEnd(list, nth));

    list = listSingleInsert(list, 3);
    listSinglePrint(list);
    printf("middle %d\n\n", listFindMiddle(list)); // when 2 (even) items, the middle is the second

    list = listSingleInsert(list, 5);
    listSinglePrint(list);
    printf("last %dth is %d\n\n", nth, listFindNthFromEnd(list, nth));
    
    list = listSingleInsert(list, 7);
    listSinglePrint(list);
    printf("last %dth is %d\n\n", nth, listFindNthFromEnd(list, nth));
    
    list = listSingleInsert(list, 9);
    listSinglePrint(list);
    printf("last %dth is %d\n\n", nth, listFindNthFromEnd(list, nth));
    
    list = listSingleInsert(list, 11);
    listSinglePrint(list);
    printf("middle %d\n", listFindMiddle(list)); // even number of items, the first one of the last half
    printf("last %dth is %d\n\n", nth, listFindNthFromEnd(list, nth));

    list = listSingleInsert(list, 13);
    listSinglePrint(list);
    printf("middle %d\n", listFindMiddle(list));// when odd number of items, the real middle
    printf("last %dth is %d\n\n", nth, listFindNthFromEnd(list, nth));


    testLoop();
    return 0;
}


