
def check_palindrome(inputStr):
    for i in range(len(inputStr)):
        tmp = inputStr[0:i] + inputStr[i+1:]
        if tmp == tmp[::-1]:
            return True

    return  inputStr == inputStr[::-1]


print(check_palindrome("radkar"))
print(check_palindrome("radkkar"))
    
