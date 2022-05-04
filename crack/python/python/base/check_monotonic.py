

def check_monotonic(inputInt):
    '''
    all() build-in function
    '''
    return all((inputInt[i]<=(inputInt[i+1])) for i in range(len(inputInt)-1)) or all( (inputInt[i]>=(inputInt[i+1])) for i in range(len(inputInt)-1))


A = [6, 5, 4, 4] 
B = [1,1,1,3,3,4,3,2,4,2]
C = [1,1,2,3,7]

print(check_monotonic(A))
print(check_monotonic(B))
print(check_monotonic(C))
        
