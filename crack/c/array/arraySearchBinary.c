/*
* search one value in the sorted array:
*  Search begin at the middle point, and divide arrary into 2 parts(binary)
* time complexity: O(n); from O(n**2) ()
*/

#include <stdio.h>

int arraySearchBinary(int array[], int start, int end, int X)
{
    int middle = (end - start)/2 + start;
    
    if(end-start == 1)
    {
        return 0;
    }

    if(array[middle] == X)
        return 1;
    else if(array[middle] <X)
    {
        return arraySearchBinary(array, middle, end, X);
    }
    else
    {
        return arraySearchBinary(array, start, middle, X);
    }
}


int main(void)
{
    int arr[] = { 2, 3, 4, 10, 40 };
    int n = sizeof(arr) / sizeof(arr[0]);
    int x = 10;

    printf("%d is: %d\n", x, arraySearchBinary(arr, 0, n - 1, x));

    x = 20;
    printf("%d is: %d\n", x, arraySearchBinary(arr, 0, n - 1, x));
    return 0;
}

