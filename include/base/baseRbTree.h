/* 
 *
 */
#ifndef __BASE_RBTREE_H__
#define __BASE_RBTREE_H__

/**
 * @brief Red/Black Tree
 */

#include <_baseTypes.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_RBTREE Red/Black Balanced Tree
 * @ingroup BASE_DS
 * @brief
 * Red/Black tree is the variant of balanced tree, where the search, insert, 
 * and delete operation is \b guaranteed to take at most \a O( lg(n) ).
 * @{
 */
/**
 * Color type for Red-Black tree.
 */ 
typedef enum brbcolor_t
{
	BASE_RBCOLOR_BLACK,
	BASE_RBCOLOR_RED
} brbcolor_t;

/**
 * The type of the node of the R/B Tree.
 */
typedef struct brbtree_node 
{
	/** Pointers to the node's parent, and left and right siblings. */
	struct brbtree_node *parent, *left, *right;

	/** Key associated with the node. */
	const void *key;

	/** User data associated with the node. */
	void *user_data;

	/** The R/B Tree node color. */
	brbcolor_t color;

} brbtree_node;


/**
 * The type of function use to compare key value of tree node.
 * @return
 *  0 if the keys are equal
 * <0 if key1 is lower than key2
 * >0 if key1 is greater than key2.
 */
typedef int brbtree_comp(const void *key1, const void *key2);


/**
 * Declaration of a red-black tree. All elements in the tree must have UNIQUE
 * key.
 * A red black tree always maintains the balance of the tree, so that the
 * tree height will not be greater than lg(N). Insert, search, and delete
 * operation will take lg(N) on the worst case. But for insert and delete,
 * there is additional time needed to maintain the balance of the tree.
 */
typedef struct brbtree
{
	brbtree_node null_node;   /**< Constant to indicate NULL node.    */
	brbtree_node *null;       /**< Constant to indicate NULL node.    */
	brbtree_node *root;       /**< Root tree node.                    */
	unsigned size;              /**< Number of elements in the tree.    */
	brbtree_comp *comp;       /**< Key comparison function.           */
} brbtree;


/**
 * Guidance on how much memory required for each of the node.
 */
#define BASE_RBTREE_NODE_SIZE	    (sizeof(brbtree_node))


/**
 * Guidance on memory required for the tree.
 */
#define BASE_RBTREE_SIZE		    (sizeof(brbtree))


/**
 * Initialize the tree.
 * @param tree the tree to be initialized.
 * @param comp key comparison function to be used for this tree.
 */
void brbtree_init( brbtree *tree, brbtree_comp *comp);

/**
 * Get the first element in the tree.
 * The first element always has the least value for the key, according to
 * the comparison function.
 * @param tree the tree.
 * @return the tree node, or NULL if the tree has no element.
 */
brbtree_node* brbtree_first( brbtree *tree );

/**
 * Get the last element in the tree.
 * The last element always has the greatest key value, according to the
 * comparison function defined for the tree.
 * @param tree the tree.
 * @return the tree node, or NULL if the tree has no element.
 */
brbtree_node* brbtree_last( brbtree *tree );

/**
 * Get the successive element for the specified node.
 * The successive element is an element with greater key value.
 * @param tree the tree.
 * @param node the node.
 * @return the successive node, or NULL if the node has no successor.
 */
brbtree_node* brbtree_next( brbtree *tree, 
					 brbtree_node *node );

/**
 * The the previous node for the specified node.
 * The previous node is an element with less key value.
 * @param tree the tree.
 * @param node the node.
 * @return the previous node, or NULL if the node has no previous node.
 */
brbtree_node* brbtree_prev( brbtree *tree, 
					 brbtree_node *node );

/**
 * Insert a new node. 
 * The node will be inserted at sorted location. The key of the node must 
 * be UNIQUE, i.e. it hasn't existed in the tree.
 * @param tree the tree.
 * @param node the node to be inserted.
 * @return zero on success, or -1 if the key already exist.
 */
int brbtree_insert( brbtree *tree, 
			       brbtree_node *node );

/**
 * Find a node which has the specified key.
 * @param tree the tree.
 * @param key the key to search.
 * @return the tree node with the specified key, or NULL if the key can not
 *         be found.
 */
brbtree_node* brbtree_find( brbtree *tree,
					 const void *key );

/**
 * Erase a node from the tree.
 * @param tree the tree.
 * @param node the node to be erased.
 * @return the tree node itself.
 */
brbtree_node* brbtree_erase( brbtree *tree,
					  brbtree_node *node );

/**
 * Get the maximum tree height from the specified node.
 * @param tree the tree.
 * @param node the node, or NULL to get the root of the tree.
 * @return the maximum height, which should be at most lg(N)
 */
unsigned brbtree_max_height( brbtree *tree,
					brbtree_node *node );

/**
 * Get the minumum tree height from the specified node.
 * @param tree the tree.
 * @param node the node, or NULL to get the root of the tree.
 * @return the height
 */
unsigned brbtree_min_height( brbtree *tree, brbtree_node *node );


BASE_END_DECL

#endif

