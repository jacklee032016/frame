
def replace_none_with_recent(val):
    res = []
    valid = 0
    
    for v in val:
        if v is None:
            res.append(valid)
        else:
            res.append(v)
            valid = v

    return res        


array1 = [1,None,2,3,None,None,5,None]

print(array1)
print(replace_none_with_recent(array1))


