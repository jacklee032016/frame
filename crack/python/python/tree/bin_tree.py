

class BinTree:
    
    def __init__(self, data):
        '''
        data must be argument, so recurse operation can be used
        '''
        self.data = data
        # left/right must be BinTree type, so the recurse operation can be used
        self.left = None
        self.right = None

    def __str__(self):
        return str(self.data)


    def insert(self, data):
        if data == self.data:
            return

        if data < self.data:
            if self.left is None:
                self.left = BinTree(data)
            else:
                self.left.insert(data)
        else:  #if data > self.data
            if self.right is None:
                self.right = BinTree(data)
            else:
                self.right.insert(data)
        

    def printTree(self):
        if self.left is not None:
            self.left.printTree()

        print(self.data)

        if self.right is not None:
            self.right.printTree()

'''
tree = BinTree(20)            

tree.insert(23)
tree.insert(7)
tree.insert(4)
tree.insert(45)
tree.insert(18)

tree.printTree()
'''

