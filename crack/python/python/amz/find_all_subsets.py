
def get_bit(num, bit):
    temp = (1 << bit)
    temp = temp & num
    if temp == 0:
        return 0
    return 1
        
def get_all_subsets(v, sets):
    '''
    n = size of given integer set
    subsets_count = 2^n
    for i = 0 to subsets_count
        form a subset using the value of 'i' as following:
            bits in number 'i' represent index of elements to choose from original set,
            if a specific bit is 1 choose that number from original set and add it to current subset,
            e.g. if i = 6 i.e 110 in binary means that 1st and 2nd elements in original array need to be picked.
        add current subset to list of all subsets

    '''
    subsets_count = 2 ** len(v)  # total number of subset
    
    for i in range(0, subsets_count):
        # st = set([]) # set() or {}
        st = []  # array, list or []
        for j in range(0, len(v)):
            if get_bit(i, j) == 1:
                #st.add(v[j]) set.add()
                st.append(v[j])

        sets.append(st)


v = [2,3,4]      
sets = []

get_all_subsets(v, sets)
print(sets)


