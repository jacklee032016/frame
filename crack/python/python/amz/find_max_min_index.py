
def find_low_index(arr, key):
    '''
    arr is order array of integers;
    find low index of key 
    '''
    low = 0
    high = len(arr) - 1
    mid = int(high / 2)

    while low <= high:
        mid_elem = arr[mid] # middle value: divid and conquer

        if mid_elem < key:
            low = mid + 1
        else:
            high = mid - 1

        mid = low + int((high - low) / 2)

    if low < len(arr) and arr[low] == key:
        return low

    return -1

def find_high_index(arr, key):
    low = 0
    high = len(arr) - 1
    mid = int(high / 2)

    while low <= high:
        mid_elem = arr[mid]

        if mid_elem <= key:
            low = mid + 1
        else:
            high = mid - 1

        mid = low + int((high - low) / 2);
  
    if high == -1:
        return high

    if high < len(arr) and arr[high] == key:
        return high

    return -1

array = [1, 2, 5, 5, 5, 5, 5, 5, 5, 5, 10 ]
key = [1, 2, 5, 10]

for k in key:
    print(find_high_index(array, k))

for k in key:
    print(find_low_index(array, k))

