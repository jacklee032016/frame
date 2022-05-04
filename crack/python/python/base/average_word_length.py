# For a given sentence, return the average word length. 
# Note: Remember to remove punctuation first.

sentence1 = "Hi all, my name is Tom...I am originally from Australia."
sentence2 = "I need to work very hard to learn more about algorithms in Python!"

def average_word_length(sentence):
    for c in "?.,!;":
       sentence = sentence.replace(c, ' ')
    words = sentence.split()

    print("words: %d"%(len(words)))
    print("length: %d"%(sum(len(w) for w in words)))
    return round(sum(len(w) for w in words)/len(words), 2)


print(average_word_length(sentence1))    
print(average_word_length(sentence2))    

