
from collections import deque
from bin_tree import BinTree

def level_order_traversal(root):
    '''
    Given the root of a binary tree, display the node values at each level. Node values for all levels 
    should be displayed on separate lines. Let's take a look at the below binary tree.
    Using two queues
    '''
    if root == None:
        return

    result = ""
    queues = [deque(), deque()]

    current_queue = queues[0]
    next_queue = queues[1]

    # add whole Btree as one item
    current_queue.append(root)
    level_number = 0
    print(current_queue)

    while current_queue:
        temp = current_queue.popleft() # when all items are pop up, current_queue is None
        print(temp)
        result += str(temp.data) + " "

        if temp.left != None:
            print("append left:%s"%(temp.left))
            next_queue.append(temp.left)

        if temp.right != None:
            print("append right:%s"%(temp.right))
            next_queue.append(temp.right)

        print(current_queue)
        # check container is empty: not current_queue
        if current_queue.__len__() == 0: # current_queue is empty
        # if not current_queue: # current_queue is empty, current_queue False
        # if current_queue == False: X
        # if current_queue is None: X
            level_number += 1
            print("move level:%d"%(level_number))
            result += ";\n"
            current_queue = queues[level_number % 2]
            next_queue = queues[(level_number + 1) % 2]

    return result


tree = BinTree(100)            

tree.insert(50)
tree.insert(200)
tree.insert(25)
tree.insert(75)
tree.insert(350)

tree.printTree()
  
print(level_order_traversal(tree))

