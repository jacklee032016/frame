
def find_primary_numbers(n):
    '''
    find all the primary numbers which is less than n
    '''
    pn = [1]
    
    for i in range(2, n):
        for k in range(2, i):
            if i%k == 0:
                break
                
        #? else: used with 'for'
        else:
            pn.append(i)

    return pn 


print(find_primary_numbers(35))    

        
