
class Node:
    def __init__(self, data, arbiNode):
        self.next = None;
        self.data = data
        self.arbiNode = arbiNode # arbiNode: any node in its list

    def printNode(self):
        print(self.data)
        if self.arbiNode is not None:
            print("arbi:%d"%(self.arbiNode.data))

class ArbiList:
    def __init__(self):
        '''
        head is a Node
        '''
        self.head = None


    def insert(self, data, arbiNode = None):
        nn = Node(data, arbiNode)
        if self.head is None:
            self.head = nn
            return nn

        nn.next = self.head
        self.head = nn
        return nn

    def append(self, data, arbiNode = None):
        nn = Node(data, arbiNode)
        if self.head is None:
            self.head = nn
            return nn

        n = self.head
        while n.next is not None:
            n = n.next

        n.next = nn
        return nn

    def printArbi(self):
        n = self.head
        while n is not None:
            n.printNode()
            n = n.next


def copy_arbitrary_list(list):
    newlist = ArbiList()

    newHead = newlist.head
    lastNode = newlist.head
    n = list.head
    dd = {}

    if n is None:
        return n

    while n is not None:
        nn = newlist.append(n.data, n.arbiNode)
        # relationship between new node and old node, and nn.arbiNode point to old node
        dd[n] = nn
        #if n.arbiNode is not None:
        #    nn.arbiNode = n.arbiNode
        #    dd[n] = n.arbiNode
        n = n.next

    nn = newlist.head
    while nn is not None:
        if nn.arbiNode is not None:
            arbi = dd[nn.arbiNode] # nn.arbiNode is old node, and its value is the new node
            nn.arbiNode = arbi # now nn.arbiNode point to new node
        nn = nn.next
        
    return newlist


list = ArbiList()

n1 = list.insert(1)
n2 = list.insert(2, n1)

n3 = list.insert(3)

n10 = list.insert(10, n2)

n15 = list.insert(15, n10)

list.printArbi()            

print("new list:")
list2 = copy_arbitrary_list(list)
list2.printArbi()
        
