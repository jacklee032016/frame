def binary_search_rotated(arr, key):
    start = 0
    end = len(arr) - 1

    if start > end:
        return -1
    
    while start <= end:
        mid = start + (end - start) // 2

        if arr[mid] == key:
            return mid

        if arr[start] <= arr[mid] and key <= arr[mid] and key >= arr[start]:
            end = mid - 1
    
        elif (arr[mid] <= arr[end] and key >= arr[mid] and key <= arr[end]):
            start = mid + 1

        elif arr[start] <= arr[mid] and arr[mid] <= arr[end] and key > arr[end]:
            start = mid + 1 

        elif arr[end] <= arr[mid]:
            start = mid + 1  

        elif arr[start] >= arr[mid]:
            end = mid - 1
    
        else:
            return -1
    
    return -1
  

