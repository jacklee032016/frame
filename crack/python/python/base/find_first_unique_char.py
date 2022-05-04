# Given a string, find the first non-repeating character in it and return its index. 
# If it doesn't exist, return -1. # Note: all the input strings are already lowercase.

def find_first_unique_char(strInput):
    frequ = {}
    
    for c in strInput:
        # if frequ[c] is None:
        if c not in frequ:
            frequ[c] = 1
        else:
            frequ[c] += 1

    for i in range(len(strInput)):
        if frequ[strInput[i]] == 1:
            return i

    return -1

print(find_first_unique_char('alphabet'))    
print(find_first_unique_char('barbados'))    
print(find_first_unique_char('crunchy'))    


