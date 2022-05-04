/*
* search i, j in a sorted array, to make a[i]+a[j]=X
* time complexity: O(n); from O(n**2) ()
*/


#include <stdlib.h>
#include <stdio.h>

int arraySearch2Pointer(int array[], size_t size, int X)
{
    int i = 0;
    int j = size-1;

    while(i<j)
    {
        if(array[i]+array[j] == X)
        {
            return 1; // found
        }
        else if(array[i]+array[j] < X)
        {
            i++;
        }
        else
        {
            j--;
        }
    }
    
    return 0;// not found
}

int main(int argc, char *argv[])
{
    int a[] = {1, 2, 3, 4, 6, 7, 8, 10, 20, 23};

    printf("find 15: %d\n", arraySearch2Pointer(a, sizeof(a)/sizeof(a[0]), 15));
    printf("find 16: %d\n", arraySearch2Pointer(a, sizeof(a)/sizeof(a[0]), 16));
    printf("find 19: %d\n", arraySearch2Pointer(a, sizeof(a)/sizeof(a[0]), 19));

    return 0;
}

