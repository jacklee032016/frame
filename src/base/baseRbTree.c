/* 
 *
 */
#include <baseRbTree.h>
#include <baseOs.h>

static void left_rotate( brbtree *tree, brbtree_node *node ) 
{
    brbtree_node *rnode, *parent;

    BASE_CHECK_STACK();

    rnode = node->right;
    if (rnode == tree->null)
        return;
    
    node->right = rnode->left;
    if (rnode->left != tree->null)
        rnode->left->parent = node;
    parent = node->parent;
    rnode->parent = parent;
    if (parent != tree->null) {
        if (parent->left == node)
	   parent->left = rnode;
        else
	   parent->right = rnode;
    } else {
        tree->root = rnode;
    }
    rnode->left = node;
    node->parent = rnode;
}

static void right_rotate( brbtree *tree, brbtree_node *node ) 
{
    brbtree_node *lnode, *parent;

    BASE_CHECK_STACK();

    lnode = node->left;
    if (lnode == tree->null)
        return;

    node->left = lnode->right;
    if (lnode->right != tree->null)
	lnode->right->parent = node;
    parent = node->parent;
    lnode->parent = parent;

    if (parent != tree->null) {
        if (parent->left == node)
	    parent->left = lnode;
	else
	    parent->right = lnode;
    } else {
        tree->root = lnode;
    }
    lnode->right = node;
    node->parent = lnode;
}

static void insert_fixup( brbtree *tree, brbtree_node *node ) 
{
    brbtree_node *temp, *parent;

    BASE_CHECK_STACK();

    while (node != tree->root && node->parent->color == BASE_RBCOLOR_RED) {
        parent = node->parent;
        if (parent == parent->parent->left) {
	    temp = parent->parent->right;
	    if (temp->color == BASE_RBCOLOR_RED) {
	        temp->color = BASE_RBCOLOR_BLACK;
	        node = parent;
	        node->color = BASE_RBCOLOR_BLACK;
	        node = node->parent;
	        node->color = BASE_RBCOLOR_RED;
	    } else {
	        if (node == parent->right) {
		   node = parent;
		   left_rotate(tree, node);
	        }
	        temp = node->parent;
	        temp->color = BASE_RBCOLOR_BLACK;
	        temp = temp->parent;
	        temp->color = BASE_RBCOLOR_RED;
	        right_rotate( tree, temp);
	    }
        } else {
	    temp = parent->parent->left;
	    if (temp->color == BASE_RBCOLOR_RED) {
	        temp->color = BASE_RBCOLOR_BLACK;
	        node = parent;
	        node->color = BASE_RBCOLOR_BLACK;
	        node = node->parent;
	        node->color = BASE_RBCOLOR_RED;
	    } else {
	        if (node == parent->left) {
		    node = parent;
		    right_rotate(tree, node);
	        }
	        temp = node->parent;
	        temp->color = BASE_RBCOLOR_BLACK;
	        temp = temp->parent;
	        temp->color = BASE_RBCOLOR_RED;
	        left_rotate(tree, temp);
	   }
        }
    }
	
    tree->root->color = BASE_RBCOLOR_BLACK;
}


static void delete_fixup( brbtree *tree, brbtree_node *node )
{
    brbtree_node *temp;

    BASE_CHECK_STACK();

    while (node != tree->root && node->color == BASE_RBCOLOR_BLACK) {
        if (node->parent->left == node) {
	    temp = node->parent->right;
	    if (temp->color == BASE_RBCOLOR_RED) {
	        temp->color = BASE_RBCOLOR_BLACK;
	        node->parent->color = BASE_RBCOLOR_RED;
	        left_rotate(tree, node->parent);
	        temp = node->parent->right;
	    }
	    if (temp->left->color == BASE_RBCOLOR_BLACK && 
	        temp->right->color == BASE_RBCOLOR_BLACK) 
	    {
	        temp->color = BASE_RBCOLOR_RED;
	        node = node->parent;
	    } else {
	        if (temp->right->color == BASE_RBCOLOR_BLACK) {
		    temp->left->color = BASE_RBCOLOR_BLACK;
		    temp->color = BASE_RBCOLOR_RED;
		    right_rotate( tree, temp);
		    temp = node->parent->right;
	        }
	        temp->color = node->parent->color;
	        temp->right->color = BASE_RBCOLOR_BLACK;
	        node->parent->color = BASE_RBCOLOR_BLACK;
	        left_rotate(tree, node->parent);
	        node = tree->root;
	    }
        } else {
	    temp = node->parent->left;
	    if (temp->color == BASE_RBCOLOR_RED) {
	        temp->color = BASE_RBCOLOR_BLACK;
	        node->parent->color = BASE_RBCOLOR_RED;
	        right_rotate( tree, node->parent);
	        temp = node->parent->left;
	    }
	    if (temp->right->color == BASE_RBCOLOR_BLACK && 
		temp->left->color == BASE_RBCOLOR_BLACK) 
	    {
	        temp->color = BASE_RBCOLOR_RED;
	        node = node->parent;
	    } else {
	        if (temp->left->color == BASE_RBCOLOR_BLACK) {
		    temp->right->color = BASE_RBCOLOR_BLACK;
		    temp->color = BASE_RBCOLOR_RED;
		    left_rotate( tree, temp);
		    temp = node->parent->left;
	        }
	        temp->color = node->parent->color;
	        node->parent->color = BASE_RBCOLOR_BLACK;
	        temp->left->color = BASE_RBCOLOR_BLACK;
	        right_rotate(tree, node->parent);
	        node = tree->root;
	    }
        }
    }
	
    node->color = BASE_RBCOLOR_BLACK;
}


void brbtree_init( brbtree *tree, brbtree_comp *comp )
{
    BASE_CHECK_STACK();

    tree->null = tree->root = &tree->null_node;
    tree->null->key = NULL;
    tree->null->user_data = NULL;
    tree->size = 0;
    tree->null->left = tree->null->right = tree->null->parent = tree->null;
    tree->null->color = BASE_RBCOLOR_BLACK;
    tree->comp = comp;
}

brbtree_node* brbtree_first( brbtree *tree )
{
    register brbtree_node *node = tree->root;
    register brbtree_node *null = tree->null;
    
    BASE_CHECK_STACK();

    while (node->left != null)
	node = node->left;
    return node != null ? node : NULL;
}

brbtree_node* brbtree_last( brbtree *tree )
{
    register brbtree_node *node = tree->root;
    register brbtree_node *null = tree->null;
    
    BASE_CHECK_STACK();

    while (node->right != null)
	node = node->right;
    return node != null ? node : NULL;
}

brbtree_node* brbtree_next( brbtree *tree, 
					register brbtree_node *node )
{
    register brbtree_node *null = tree->null;
    
    BASE_CHECK_STACK();

    if (node->right != null) {
	for (node=node->right; node->left!=null; node = node->left)
	    /* void */;
    } else {
        register brbtree_node *temp = node->parent;
        while (temp!=null && temp->right==node) {
	    node = temp;
	    temp = temp->parent;
	}
	node = temp;
    }    
    return node != null ? node : NULL;
}

brbtree_node* brbtree_prev( brbtree *tree, 
					register brbtree_node *node )
{
    register brbtree_node *null = tree->null;
    
    BASE_CHECK_STACK();

    if (node->left != null) {
        for (node=node->left; node->right!=null; node=node->right)
	   /* void */;
    } else {
        register brbtree_node *temp = node->parent;
        while (temp!=null && temp->left==node) {
	    node = temp;
	    temp = temp->parent;
        }
        node = temp;
    }    
    return node != null ? node : NULL;
}

int brbtree_insert( brbtree *tree, 
			      brbtree_node *element )
{
    int rv = 0;
    brbtree_node *node, *parent = tree->null, 
		   *null = tree->null;
    brbtree_comp *comp = tree->comp;
	
    BASE_CHECK_STACK();

    node = tree->root;	
    while (node != null) {
        rv = (*comp)(element->key, node->key);
        if (rv == 0) {
	    /* found match, i.e. entry with equal key already exist */
	    return -1;
	}    
	parent = node;
        node = rv < 0 ? node->left : node->right;
    }

    element->color = BASE_RBCOLOR_RED;
    element->left = element->right = null;

    node = element;
    if (parent != null) {
        node->parent = parent;
        if (rv < 0)
	   parent->left = node;
        else
	   parent->right = node;
        insert_fixup( tree, node);
    } else {
        tree->root = node;
        node->parent = null;
        node->color = BASE_RBCOLOR_BLACK;
    }
	
    ++tree->size;
    return 0;
}


brbtree_node* brbtree_find( brbtree *tree,
					const void *key )
{
    int rv;
    brbtree_node *node = tree->root;
    brbtree_node *null = tree->null;
    brbtree_comp *comp = tree->comp;
    
    while (node != null) {
        rv = (*comp)(key, node->key);
        if (rv == 0)
	    return node;
        node = rv < 0 ? node->left : node->right;
    }
    return node != null ? node : NULL;
}

brbtree_node* brbtree_erase( brbtree *tree,
					 brbtree_node *node )
{
    brbtree_node *succ;
    brbtree_node *null = tree->null;
    brbtree_node *child;
    brbtree_node *parent;
    
    BASE_CHECK_STACK();

    if (node->left == null || node->right == null) {
        succ = node;
    } else {
        for (succ=node->right; succ->left!=null; succ=succ->left)
	   /* void */;
    }

    child = succ->left != null ? succ->left : succ->right;
    parent = succ->parent;
    child->parent = parent;
    
    if (parent != null) {
	if (parent->left == succ)
	    parent->left = child;
        else
	   parent->right = child;
    } else
        tree->root = child;

    if (succ != node) {
        succ->parent = node->parent;
        succ->left = node->left;
        succ->right = node->right;
        succ->color = node->color;

        parent = node->parent;
        if (parent != null) {
	   if (parent->left==node)
	        parent->left=succ;
	   else
		parent->right=succ;
        }
        if (node->left != null)
	   node->left->parent = succ;;
        if (node->right != null)
	    node->right->parent = succ;

        if (tree->root == node)
	   tree->root = succ;
    }

    if (succ->color == BASE_RBCOLOR_BLACK) {
	if (child != null) 
	    delete_fixup(tree, child);
        tree->null->color = BASE_RBCOLOR_BLACK;
    }

    --tree->size;
    return node;
}


unsigned brbtree_max_height( brbtree *tree,
				       brbtree_node *node )
{
    unsigned l, r;
    
    BASE_CHECK_STACK();

    if (node==NULL) 
	node = tree->root;
    
    l = node->left != tree->null ? brbtree_max_height(tree,node->left)+1 : 0;
    r = node->right != tree->null ? brbtree_max_height(tree,node->right)+1 : 0;
    return l > r ? l : r;
}

unsigned brbtree_min_height( brbtree *tree,
				       brbtree_node *node )
{
    unsigned l, r;
    
    BASE_CHECK_STACK();

    if (node==NULL) 
	node=tree->root;
    
    l = (node->left != tree->null) ? brbtree_max_height(tree,node->left)+1 : 0;
    r = (node->right != tree->null) ? brbtree_max_height(tree,node->right)+1 : 0;
    return l > r ? r : l;
}


