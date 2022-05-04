README

https://www.educative.io/blog/crack-amazon-coding-interview-questions

** 1. Find the missing number in the array

You are given an array of positive numbers from 1 to n, such that all numbers from 1 to 
n are present except one number x. You have to find x. The input array is not sorted. 
Look at the below array and give it a try before checking the solution.

** 2. Determine if the sum of two integers is equal to the given value

Given an array of integers and a value, determine if there are any two integers in the 
array whose sum is equal to the given value. Return true if the sum exists and return 
false if it does not. 


** 3. Merge two sorted linked lists

Given two sorted linked lists, merge them so that the resulting linked list is also sorted


** 4. Copy linked list with arbitrary pointer

You are given a linked list where the node has two pointers. The first is the regular next pointer. 
The second pointer is called arbitrary_pointer and it can point to any node in the linked list. 
Your job is to write code to make a deep copy of the given linked list. Here, deep copy means that 
any operations on the original list should not affect the copied list.


** 5. Level Order Traversal of Binary Tree

Given the root of a binary tree, display the node values at each level. Node values for all 
levels should be displayed on separate lines. Let’s take a look at the below binary tree.

** 6. Determine if a binary tree is a binary search tree

Given a Binary Tree, figure out whether it’s a Binary Search Tree. In a binary search tree, 
each node’s key value is smaller than the key value of all nodes in the right subtree, and 
is greater than the key values of all nodes in the left subtree. Below is an example of a 
binary tree that is a valid BST.


** 7. String segmentation 

You are given a dictionary of words and a large input string. You have to find out whether the 
input string can be completely segmented into the words of a given dictionary.

** 8: check binary search tree


** 9. How many ways can you make change with coins and a total amount: XXXXX


** 10. Find Kth permutation: XXXXX

Given a set of ‘n’ elements, find their Kth permutation. 

** 11. Find all subsets of a given set of integers

We are given a set of integers and we have to find all the possible subsets of this set of integers. 


** 12. Print balanced brace combinations
Print all braces combinations for a given value n so that they are balanced. For this solution, we will be using recursion.


** 13. Clone a Directed Graph

Given the root node of a directed graph, clone this graph by creating its deep copy so that the cloned 
graph has the same vertices and edges as the original graph.

Let’s look at the below graphs as an example. If the input graph is G=(V,E)G = (V, E)G=(V,E) where V is 
set of vertices and E is set of edges, then the output graph (cloned graph) G’ = (V’, E’) such that V = V’ 
and E = E’. We are assuming that all vertices are reachable from the root vertex, i.e. we have a connected 
graph.

** 14. Find Low/High Index

Given a sorted array of integers, return the low and high index of the given key. You must return 
-1 if the indexes are not found. The array length can be in the millions with many duplicates.

** 15. Search Rotated Array

Search for a given number in a sorted array, with unique elements, that has been rotated by some 
arbitrary number. Return -1 if the number does not exist. Assume that the array does not contain 
duplicates.



    Get the two numbers (if any) in array those sum up to certain value.
    Balanced brackets
    Set intersection (with different variation)

For the second question you can expect something a bit harder, where you should apply certain algorithm (mostly straight forward implementation of an algorithm) 90% of them (in my opinion, and previous experience) are BFS/DFS, sorting, string manipulation:

    Get number of disjoint sets (variations: number of islands in metrics, number of continents … either given matrix or graph).
    Find the first k frequent numbers in (large) array.
    Maximum sub-array

Of course you better get 100% of test cases passed. But if you couldn’t, I think its OK to have 1 or 2 failing tests.


The test had 2 questions. For each question, there is a coding part and an additional part where we have to explain our approach to the question and share details about time and memory complexity.
    