def solve_coin_change(denominations, amount):
    '''
    Runtime Complexity: Quadratic, O(mxn)
    Memory Complexity: Linear, O(n)
    '''
    # solution[index] : resolution when amount is index
    solution = [0] * (amount + 1) # array length amount+1, with initial value 0
    solution[0] = 1 #  store the solution for the 0 amount

    for den in denominations:
        print("den:%d"%(den))
        for i in range(den, amount + 1):
            solution[i] += solution[i - den] 
            print("\tresolution[%d]: %d"%(i, solution[i]))

    print(solution)
    return solution[len(solution) - 1]

denominations = [ 2, 3, 5]
amount = 8

print(solve_coin_change(denominations, amount))


