# n = length of input string
# for i = 0 to n - 1
#  first_word = substring (input string from index [0, i] )
#  second_word = substring (input string from index [i + 1, n - 1] )
#  if dictionary has first_word
#    if second_word is in dictionary OR second_word is of zero length, then return true
#    recursively call this method with second_word as input and return true if it can be segmented

'''
def can_segment_string(s, dictionary):
  for i in range(1, len(s) + 1):
    first = s[0:i]
    if first in dictionary:
      second = s[i:]
      if not second or second in dictionary or can_segment_string(second, dictionary):
        return True
  return False
'''

def check_segment_string(s, dictionary):
    #print("string %s: %d"%(s, len(s)))
    
    # must be len()+1, then first get the whole string
    # i in range(1, n) is 1,2, n-1
    for i in range(1, len(s)+1): 
        first = s[:i]
        # print("#%d: %s"%(i, first))
        if first in dictionary:
            second = s[i:]
            # not second: the second is empty, so only whole word in this dictionary
            if not second or second in dictionary:
                return True

    return False

dictionary =['pie', 'apple', 'peach']
s1 = "applepie"
print(check_segment_string(s1, dictionary))

s2 = "applepeech"
print(check_segment_string(s2, dictionary))

s1 = "apple"
print(check_segment_string(s1, dictionary))

    
