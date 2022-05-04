# https://www.educative.io/blog/crack-amazon-coding-interview-questions

def find_sum_of_two(A, val):
    '''
    Given an array of integers and a value, determine if there are any two integers in the array whose sum is equal to the given value. 
    Return true if the sum exists and return false if it does not

    memory complexity: linear O(N)
    runtime complexity: linear, O(N) 
    '''
#  found_values = set()
#  for a in A:
#    if val - a in found_values:
#      return True

#    found_values.add(a)
    
#  return False

    used_v = []
    for a in A:
        if (val -a) in used_v:
            return True
        else:
            used_v.append(a)

    return False        


print(find_sum_of_two([5,7,1, 2, 8, 4, 3], 10))            
print(find_sum_of_two([5, 7, 1, 2, 8, 4, 3], 19))            

