# https://www.educative.io/blog/crack-amazon-coding-interview-questions

def find_missing(input):
    '''
    You are given an array of positive numbers from 1 to n, such that all numbers from 1 to n are present except one number x. You have to find x

    Runtime Complexity: Linear, O(n)
    Memory Complexity: Constant, O(1)
    '''
    
    # calculate sum of all elements 
    # in input list
    #sum_of_elements = sum(input)
  
    # There is exactly 1 number missing 
    #n = len(input) + 1
    #actual_sum = (n * ( n + 1 ) ) / 2
    
    #return actual_sum - sum_of_elements
    tsum = sum(input)
    n = len(input)+1
    realsum = n*(n+1)/2

    return realsum - tsum
    

print(find_missing([3,7, 1, 2, 8, 4, 5]))

