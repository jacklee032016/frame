/*
* find maximum sum in any sub-array of this array
*/

#include <stdio.h>
const int INT_MIN = -1e9;

int maximumSubarraySum(int arr[], int n)
{
    int maxSum = INT_MIN;
    int i=0;

    for(; i <= n - 1; i++)
    {
        int currSum = 0;
        int j=i;
        for (; j <= n - 1; j++)
        {
            currSum += arr[j];
            if (currSum > maxSum)
            {
                maxSum = currSum;
            }
        }

    }

    return maxSum;
}


int maximumSubArrarSumKadane(int arrry[], int n)
{
    int max_so_far = INT_MIN;
    int max_till_now = 0;
    int i;

    for(i=0; i<n; i++)
    {
        max_till_now = max_till_now + arrry[i];

        if(max_till_now > max_so_far)
            max_so_far = max_till_now;

        if(max_till_now <0)
            max_till_now = 0;
    }

    return max_so_far;
}

int main()
{
   int a[] = {1, 3, 8, -2, 6, -8, 5};

   int a2[] = {-2, -3, 4, -1, -2, 1, 5, -3}; // max 7

   printf("%d\n", maximumSubarraySum(a, sizeof(a)/sizeof(a[0])));
   printf("%d\n", maximumSubArrarSumKadane(a, sizeof(a)/sizeof(a[0])));

   printf("%d\n", maximumSubarraySum(a2, sizeof(a2)/sizeof(a2[0])));
   printf("%d\n", maximumSubArrarSumKadane(a2, sizeof(a2)/sizeof(a2[0])));
   
   return 0;
}

