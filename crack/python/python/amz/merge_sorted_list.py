
class Node:
    def __init__(self, value, next=None):
        self.data = value
        self.next = next

    def printnode(self):
        print(self.data)

class LList:
    def __init__(self):
        self.head = None

    def insert(self, value):
        n = Node(value, self.head)
        self.head = n

    def append(self, value):
        nn = Node(value)
        if self.head is None:
            self.head = nn
            return
            
        n = self.head
        last = n
        while n is not None:
            last = n
            n = n.next
        last.next = nn
            

    def printlist(self):
        n = self.head
        while n is not None:
            n.printnode()
            n = n.next


def merge_sorted(head1, head2):
    '''
    Given two sorted linked lists, merge them so that the resulting linked list is also sorted.
    
    Runtime Complexity: Linear, O(m+n) where m and n are lengths of both linked lists. eg. iterate m+n times
    Memory Complexity: Constant, O(1). eg. no extra memory is needed, and no memory copy
    '''
    
    mergedHead = None
    if head1 is None:
        return head2
    elif head2 is None:
        return head1

    nl_1 = head1.head
    nl_2 = head2.head
    if nl_1.data < nl_2.data:
        mergedHead = head1;
        nl_1 = nl_1.next
    else:
        mergedHead = head2
        nl_2 = nl_2.next

    nextNode = mergedHead.head
    while  nl_1 is not None and nl_2 is not None:
        if nl_1.data < nl_2.data:
            nextNode.next = nl_1
            nl_1 = nl_1.next
        else:
            nextNode.next = nl_2
            nl_2 = nl_2.next
        nextNode = nextNode.next
            
    if nl_1 is None:
        nextNode = nl_2
    else:
        nextNode = nl_1

    return mergedHead    
  # if both lists are empty then merged list is also empty
  # if one of the lists is empty then other is the merged list
    '''
  if head1 == None:
    return head2
  elif head2 == None:
    return head1

  mergedHead = None;
  if head1.data <= head2.data:
    mergedHead = head1
    head1 = head1.next
  else:
    mergedHead = head2
    head2 = head2.next

  mergedTail = mergedHead
  
  while head1 != None and head2 != None:
    temp = None
    if head1.data <= head2.data:
      temp = head1
      head1 = head1.next
    else:
      temp = head2
      head2 = head2.next

    mergedTail.next = temp
    mergedTail = temp

  if head1 != None:
    mergedTail.next = head1
  elif head2 != None:
    mergedTail.next = head2

  return mergedHead
    '''

list = LList()
list.insert(4)
list.insert(8)
list.insert(15)
list.insert(19)
# list.printlist()

list2 = LList()
list2.append(4)
list2.append(8)
list2.append(15)
list2.append(19)
list2.printlist()

list3 = LList()
list3.append(1)
list3.append(5)
list3.append(6)
list3.append(9)
list3.append(13)
list3.append(15)

list3.printlist()

#list1 = [4, 8, 15, 19, None]
#list2 = [7, 9, 10, 16, None]

print("merged:")
mergedlist = merge_sorted(list2, list3)
mergedlist.printlist()

