# Given a Binary Tree, figure out whether it's a Binary Search Tree. In a binary search tree, 
# each node's key value is smaller than the key value of all nodes in the right subtree, and 
# is greater than the key values of all nodes in the left subtree. Below is an example of a 
# binary tree that is a valid BST.

'''
def is_bst_rec(root, min_value, max_value):
  if root == None:
    return True

  if root.data < min_value or root.data > max_value:
    return False

  return is_bst_rec(root.left, min_value, root.data) and is_bst_rec(root.right, root.data, max_value)

def is_bst(root):
  return is_bst_rec(root, -sys.maxsize - 1, sys.maxsize)
'''

from bin_tree import BinTree
import sys

def check_bst_with_value(tree, min, max):
    if tree is None:
        return True     # why?
        
    if tree.data <= min or tree.data >= max:
        return False;

    return check_bst_with_value(tree.left, min, tree.data) and check_bst_with_value(tree.right, tree.data, max);


def check_bst(tree):
    '''
    Binary search Tree
    '''
    return check_bst_with_value(tree, -sys.maxsize-1, sys.maxsize)


tree = BinTree(100)            

tree.insert(50)
tree.insert(200)
tree.insert(25)
tree.insert(75)
tree.insert(125)
tree.insert(350)

tree.printTree()
  
print(check_bst(tree))

tree2 = BinTree(100)            

tree2.insert(50)
tree2.insert(200)
tree2.insert(25)
tree2.insert(75)
tree2.insert(90)
tree2.insert(350)

tree2.printTree()
  
print(check_bst(tree2))

