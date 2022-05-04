Code Test

## Array

** Binary search
 Search one value in the sorted array;
* Divide array in the middle item;
* compare with middle item, and then recurse on the suitable part;

** 2 Pointer Search
find 2 items, which sum is equal to one value in the sorted array;

* 2 pointers, one is first one, another is last;
* sum the first and last, and compare the result;
* iterate with the next first or last one;

** sliding window search
find the max value of sub-array with consecutive K items in unsorted array;

* calculate value of initial sliding window;
* Move start position to k, and sliding window;
* minus the previous one, and add the next one in every sliding operation;
* compare the result;


** kadane algorithm
find max sum of any sub-array in one array
* sum_so_far and sum_till_now
* sum_till_now + a[i]; if > sum_so_far, replace sum_so_far;
* if sum_till_now<0, sum_till_now=0;
* iterate with i;


## String
** Reverse a string
* 2 pointers, point to first and last char;
* swap the first and last char;
* iterate to the next first and next last;

## Linked List

** reverse singly linked list

** reverse doubly linked list

** merge 2 linked lists

** add, delete and search element of an linked list


** find middle of list

** find last nth element of list


** find if there is loop in list


## Stack and Queue

** implement stack using array

** implment stack using linked list

** implement queue using array

** implement queue using linked list

** implement circular buffer

## Bit manipulation

** atoi, itoa, itob, float to bin, atof

** add/sub in binary(without using + operator)

** 2's complement

** endianness swap

** range of 8 bit(-128~127)

** represent float in binary

## Memory

** aligned malloc/free

** malloc/free using static buffers(arrays)

