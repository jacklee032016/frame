
/*
* "Given an array of integers nums and an integer target, return indices of the two numbers 
* such that they add up to target. You may assume that each input would have exactly one 
* solution, and you may not use the same element twice." 
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct int_array
{
    int     index;
    int     value;
};

int find_sum_of_2(int *argv, int size, int sum)
{
    struct  int_array *array = NULL;
    int i, j, diff;
    
    array = malloc(sizeof(struct int_array)*size);
    if(!array)
        return -1;

    memset(array, 0, sizeof(struct int_array)*size);

    for(i=0; i< size; i++)
    {
        diff = sum - argv[i];
        for(j =0; j< i; j++)
        {
            if (array[j].value == diff)
            {
                printf("Found sum of %d, index is [%d(%d), %d(%d)], diff %d\n", sum, array[j].value, j, argv[i], i, diff);
                free(array);
                return 1;
            }
        }

        array[i].value = argv[i];
    }

    free(array);
    printf("Not found\n");
    return 0;
}


int main(int argc, char *argv[])
{
    int array1[] = {5, 7, 1, 2, 8, 4, 3};
    int array2[] = {5, 7, 1, 2, 8, 4, 3};
    int ret = 0;
    ret = find_sum_of_2(array1, sizeof(array1)/sizeof(array1[0]), 10);
    
    ret = find_sum_of_2(array2, sizeof(array2)/sizeof(array2[0]), 9);

    ret = find_sum_of_2(array2, sizeof(array2)/sizeof(array2[0]), 19);
    return 0;
}

