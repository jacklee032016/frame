
def find_match_unmatch_words(sentence1, sentence2):
    '''
    return 2 arrays: one: words in on array, not in another; another: words in both array
    '''

    # set(): make array into set, so set operator can be used
    set1 = set(sentence1.split())
    set2 = set(sentence2.split())

    # ^:XOR; &:AND
    return sorted(list(set1^set2)), sorted(list(set1&set2))

sentence1 = 'We are really pleased to meet you in our city'
sentence2 = 'The city was hit by a really heavy storm'

print(find_match_unmatch_words(sentence1, sentence2))

