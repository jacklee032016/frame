/*
* use sliding window to find the maximum value in k consecutive items in an array
* time complexity from O(n*k) (brutal force) to O(n)
*/

#include <stdio.h>

int arrayFindMaxSumBySlidingWindow(int array[], size_t size, int k)
{
    int maxSum = 0, value = 0;
    int i;

    for(i=0; i<k; i++)
    {
        maxSum += array[i];
    }
    value = maxSum;

#if 0
    // border
    for(i=1; i< size-k+1; i++)
    {
        printf("%d %d %d\n", i, i+k-1, array[i+k-1]);
        // 2*N operations
        value = value - array[i-1] + array[i+k-1];
        if(value > maxSum)
            maxSum = value;
    }
#else
    // border is very clean, like usuall for loop, loop size-k-1 times
    for(i=k; i<size; i++)
    {
        value = value - array[i-k] + array[i];
        if(value>maxSum)
            maxSum = value;
    }
#endif
    return maxSum;
}


int main()
{
    int arr[] = { 1, 4, 2, 10, 2, 3, 1, 0, 20 };// border is the last item, so put it big
    int k = 4;
    int n = sizeof(arr) / sizeof(arr[0]);
    
    printf("max sum with %d items is %d\n", k, arrayFindMaxSumBySlidingWindow(arr, n, k));
    return 0;
}

