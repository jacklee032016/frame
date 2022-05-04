
def reverse_integer(input):
    '''
    #[start:stop:step]
    when step <0, [stop:start:step]
    '''
    res = str(input)

    if res[0] == '-':
        res = int('-'+res[:0:-1]) 
    else:
        res = int(res[::-1]) 

    return res;


print("123:%d"%(reverse_integer(123)))
print("-532:%d"%(reverse_integer(-532)))

    
