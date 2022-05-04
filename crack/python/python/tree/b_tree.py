class Node:
    def __init__(self, data):
        self.data = data
        self.left = None
        self.right = None

    def insert(self, data):
        if self.data is None:
            self.data = data;
        elif data < self.data:
            if self.left is None:
                self.left = Node(data)
            else:
                self.left.insert(data)
        elif data > self.data:
            if self.right is None:
                self.right = Node(data)
            else:
                self.right.insert(data)
                
            
    def printTree(self):
        if self.left is not None:
            self.left.printTree()
            
        print(self.data)
        
        if self.right is not None:
            self.right.printTree()

    def inorderTraversal(self):
        result = []

        if self.left:
            result = self.left.inorderTraversal()

        result.append(self.data)

        if self.right:
            result = result + self.right.inorderTraversal()
            # result.append(self.right.inorderTraversal())

        return result

    def preorderTraversal(self):
        '''
        first is root, then left, then last
        '''
        res = []
        res.append(self.data)

        if self.left:
            res = res + self.left.preorderTraversal()
        if self.right:
            res = res + self.right.preorderTraversal()

        return res

    def postorderTraversal(self):
        '''
         first left, then right, root is last
        '''
        res = []

        if self.left:
            res = self.left.postorderTraversal()

        if self.right:
            res = res + self.right.postorderTraversal()
            
        res.append(self.data)
        
        return res

    def findValue(self, value):
        if self.data is None:
            print(str(value) + " is not found")
            return
            
        if value < self.data:
            if self.left is None:
                print(str(value) + " is not found")
                return
            else:
                self.left.findValue(value)
                return

        if value > self.data:
            if self.right is None:
                print(str(value) + " is not found")
                return
            else:
                self.right.findValue(value)
                return

        print(str(value) + " is found")
        return
        
        

bt = Node(3)

bt.insert(1)
bt.insert(1)
bt.insert(0)
bt.insert(10)
bt.insert(10)
bt.insert(1)

bt.printTree()

inorder = bt.inorderTraversal()
print (inorder)

print(bt.preorderTraversal())

print(bt.postorderTraversal())

root = Node(27)
root.insert(14)
root.insert(35)
root.insert(10)
root.insert(19)
root.insert(31)
root.insert(42)
print(root.preorderTraversal())
print(root.postorderTraversal())

root.findValue(18)
root.findValue(31)


