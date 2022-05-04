/*
* Tree: search perforamnce from O(N) to O(logN); basis of set and map
* Binary Tree: only 2 child nodes
* Binary Search Tree: left node is smaller than right one
*/

/*
                10
             5
          3     8
       1   

or
                10
             1
                 5
             3       8

both are binary search tree, both 4 levels
so the actual tree depends on the order to insert the nodes
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct BinSearchNode
{
    int                     data;
    
    struct BinSearchNode    *left;
    struct BinSearchNode    *right;
};


typedef struct BinSearchNode BinNode;

void debugNode(BinNode *node)
{
    if(!node)
    {
        printf("null node\n");
        return;
    }
    printf("node: %d: %d %d\n", node->data, node->left?node->left->data:-1, node->right?node->right->data:-1);
}

static BinNode* _findMinNode(BinNode *tree)
{
    if(tree->left)
        return _findMinNode(tree->left);
    else
        return tree;
}

// after recurseing ends, alwlays return root of tree
BinNode*     binSearchTreeInsert(BinNode *tree, int value)
{
    if(!tree)
    {
        tree = malloc(sizeof(BinNode));
        memset(tree, 0, sizeof(BinNode));
        tree->data = value;

        return tree;
    }

    if(value < tree->data )
    {// when tree->left is null, return new child; 
     // when tree->left is not null, return tree->left itself, so assign it back to tree->left
        tree->left = binSearchTreeInsert(tree->left, value);
    }
    else if(value > tree->data )
    {
        tree->right = binSearchTreeInsert(tree->right, value);
    }
    // value == tree->data: ignore

    return tree;
}

// return next node used to replace current right|left node
BinNode* binSearchTreeRemove(BinNode *tree, int value)
{
    if(!tree) // this value is not in the range of tree
        return NULL;

    if(value < tree->data)
    {
        tree->left = binSearchTreeRemove(tree->left, value);
    }
    else if(value > tree->data)
    {
        tree->right = binSearchTreeRemove(tree->right, value);
    }
    else
    {
        BinNode *tmp = NULL;
        if(tree->left && tree->right)
        {// replace this node with the smallest on in the right subtree
            tmp = _findMinNode(tree->right);
            tree->data = tmp->data;
            tree->right = binSearchTreeRemove(tree->right, tree->data); // now tree->data is the smallest node of right subtree
//            tree->left
        }
#if 0
        else if(tree->left)
        {// right is null
            printf("remove left %d :%d:%d\n", tree->left->data, tree->data, value);
            tmp = tree->left;
            tree->data = tmp->data;
            tree->left = tmp->left;
            tree->right = tmp->right;
            free(tree->left);
        }
        else if(tree->right)
        {// left is null
            BinNode *tmp = tree->right;
            debugNode(tmp);
            
            tree->data = tmp->data;
            tree->left = tmp->left;
            tree->right = tmp->right;
            debugNode(tree);
            free(tmp);
            debugNode(tree);
        }
        else
        {
            printf("remove %d :%d \n", tree->data, value);
            free(tree);
            tree = NULL;
        }
        
        return tree;
#else
        else
        {
            if(tree->left)
                tmp = tree->left;
            else if(tree->right)
                tmp = tree->right;

            free(tree);
            return tmp;
        }
#endif
    }

    // if not this node, return this node itself
    return tree;
}


void binSearchTreeIterate(BinNode *tree)
{
    if(tree->left)
        binSearchTreeIterate(tree->left);

    printf("%d ", tree->data);

    if(tree->right)
        binSearchTreeIterate(tree->right);

}


int binSearchTreeFindMin(BinNode *tree)
{
    if(tree->left)
        return binSearchTreeFindMin(tree->left);
    else
        return tree->data;
}

int binSearchTreeFindMax(BinNode * tree)
{
    if (tree->right)
        return binSearchTreeFindMax(tree->right);
    else
        return tree->data;
}

int binSearchTreeFind(BinNode * tree, int value)
{
    if(tree->data == value)
        return tree->data;

    if(value < tree->data)
    {
        return binSearchTreeFind(tree->left, value);
    }
    else if(value > tree->data)
    {
        return binSearchTreeFind(tree->right, value);
    }
    else
    {
        return -1;
    }
}

// here, tree is always the root of tree
void binTreePrintLevel(BinNode *tree, int level)
{
    if(!tree)
        return;
    if(level == 1)
    {
        printf("%d ", tree->data);
//        debugNode(tree);
    }
    else
    {
        binTreePrintLevel(tree->left, level-1);
        binTreePrintLevel(tree->right, level-1);
    }
}


int binTreeLevel(BinNode *tree)
{
    int level = 1;
    int levelLeft = 0, levelRight = 0;
    if(!tree)
        return 0;

    levelLeft += binTreeLevel(tree->left);
    levelRight += binTreeLevel(tree->right);

    return (levelLeft>levelRight)? level+levelLeft: level+levelRight;
}

// breadth first search
int binTreePrint(BinNode *tree)
{
    int i;
    int level = binTreeLevel(tree);

    for(i=1; i< level+1; i++)
    {
        printf("Level %d: ", i);
        binTreePrintLevel(tree, i);
        printf("\n");
    }

    return 0;
}

// following 3 functions are depth first search
// root node is previous to others
void binTreePrintPreorder(BinNode *tree)
{
    if (tree)
    {
        printf("%d\n", tree->data);
        binTreePrintPreorder(tree->left);
        binTreePrintPreorder(tree->right);
    }

}

// root node is in the middle of others
void binTreePrintInorder(BinNode *tree)
{
    if (tree)
    {
        binTreePrintInorder(tree->left);
        printf("%d\n",tree->data);
        binTreePrintInorder(tree->right);
    }
}

// root node is after others
void binTreePrintPostorder(node * tree)
{
    if (tree)
    {
        binTreePrintPostorder(tree->left);
        binTreePrintPostorder(tree->right);
        printf("%d\n",tree->data);
    }
}


int main(int argc, char *argv[])
{
    BinNode *tree = NULL;
    int max, min;

    tree = binSearchTreeInsert(tree, 10);
    printf("level: %d\n", binTreeLevel(tree));
    binSearchTreeIterate(tree);
    printf("\n");
    binTreePrint(tree);
    
    binSearchTreeInsert(tree, 1);
    binSearchTreeInsert(tree, 20);
    binSearchTreeInsert(tree, 5);
    binSearchTreeInsert(tree, 15);
    printf("level: %d\n", binTreeLevel(tree));
    binSearchTreeIterate(tree);
    printf("\n");
    binTreePrint(tree);
    
    binSearchTreeInsert(tree, 3);
    binSearchTreeInsert(tree, 6);
    binSearchTreeInsert(tree, 4);
    binSearchTreeInsert(tree, 2);
    printf("level: %d\n", binTreeLevel(tree));
    binSearchTreeIterate(tree);
    printf("\n");
    binTreePrint(tree);
    
    binSearchTreeInsert(tree, 24);
    binSearchTreeInsert(tree, 8);
    binSearchTreeInsert(tree, 28);
    binSearchTreeInsert(tree, 30);
    binSearchTreeInsert(tree, 13);
    binSearchTreeInsert(tree, 18);
    printf("level: %d\n", binTreeLevel(tree));
    binSearchTreeIterate(tree);
    printf("\n");
    binTreePrint(tree);

//    binSearchTreeInsert(tree, 10); // duplicate value
    
    binSearchTreeIterate(tree);
    printf("\n");
    min = binSearchTreeFindMin(tree);
    max = binSearchTreeFindMax(tree);
    printf("Min:%d\n", min);
    printf("Max:%d\n", max);
    
    binSearchTreeIterate(tree);
    printf("\n");

//    printf("8:%d\n", binSearchTreeFind(tree, 8));
//    printf("9:%d\n", binSearchTreeFind(tree, 9));

    // remove smallest and biggest
    printf("Remove %d %d\n", min, max);
    binSearchTreeRemove(tree, min);
    binSearchTreeRemove(tree, max);
    binSearchTreeRemove(tree, max+2); // remove non-existed node
    binSearchTreeIterate(tree);
    printf("\n");
    printf("Min:%d\n", binSearchTreeFindMin(tree));
    printf("Max:%d\n", binSearchTreeFindMax(tree));

    binSearchTreeRemove(tree, 5); // remove middle node
    binSearchTreeIterate(tree);
    printf("\n");

    // remove root
    tree = binSearchTreeRemove(tree, 10);
    debugNode(tree); // new root
    binSearchTreeIterate(tree);
    printf("\n");

    return 0;
}

