# If input vector is empty return result vector
# block_size = (n-1)! ['n' is the size of vector]
# Figure out which block k will lie in and select the first element of that block
# (this can be done by doing (k-1)/block_size)
# Append selected element to result vector and remove it from original input vector
# Deduce from k the blocks that are skipped i.e k = k - selected*block_size and goto step 1

# permutation: arrangement

def factorial(n):
    if n == 0 or n == 1:
        return 1
    return n * factorial(n -1 )

def find_kth_permutation(v, k):
    result = ""
    if not v:
        return result
  
    n = len(v)
    # select first digit in the n digit when Kth
    
    # count is number of permutations when number of digits is n-1
    count = factorial(n - 1) #
    selected = (k - 1) // count  ## ?? the point
    print("count:%d; k:%d; selected:%d"%(count, k, selected))

    # add the selected digit to result, and remove it from v, and recursing operation
    result += str(v[selected])
    del v[selected]
    k = k - (count * selected)  ## ? new K in recurse
    print(result)
    result += find_kth_permutation(v, k)

    return result


v=[1,2,3]
k = 6

result = find_kth_permutation(v, k)
print("result of k %d: %s"%(k, result))

v=[1,2,3]
k = 3
result = find_kth_permutation(v, k)
print("result of k %d: %s"%(k, result))

