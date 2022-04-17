/* 
 *
 */
#ifndef __BASE_XML_H__
#define __BASE_XML_H__

/**
 * @brief  XML Parser/Helper.
 */

#include <_baseTypes.h>
#include <baseList.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_TINY_XML Mini/Tiny XML Parser/Helper
 * @ingroup BASE_FILE_FMT
 * @{
 */

/** Typedef for XML attribute. */
typedef struct bxml_attr bxml_attr;

/** Typedef for XML nodes. */
typedef struct bxml_node bxml_node;

/** This structure declares XML attribute. */
struct bxml_attr
{
    BASE_DECL_LIST_MEMBER(bxml_attr);   /**< Standard list elements.    */
    bstr_t	name;	                /**< Attribute name.            */
    bstr_t	value;	                /**< Attribute value.           */
};

/** This structure describes XML node head inside XML node structure.
 */
typedef struct bxml_node_head
{
    BASE_DECL_LIST_MEMBER(bxml_node);   /**< Standard list elements.    */
} bxml_node_head;

/** This structure describes XML node. */
struct bxml_node
{
    BASE_DECL_LIST_MEMBER(bxml_node);   /**< List @a prev and @a next member */
    bstr_t		name;		/**< Node name.                      */
    bxml_attr		attr_head;      /**< Attribute list.                 */
    bxml_node_head	node_head;      /**< Node list.                      */
    bstr_t		content;	/**< Node content.                   */
};

/**
 * Parse XML message into XML document with a single root node. The parser
 * is capable of parsing XML processing instruction construct ("<?") and 
 * XML comments ("<!--"), however such constructs will be ignored and will not 
 * be included in the resulted XML node tree.
 *
 * Note that the XML message input buffer MUST be NULL terminated and have
 * length at least len+1 (len MUST NOT include the NULL terminator).
 *
 * @param pool	    Pool to allocate memory from.
 * @param msg	    The XML message to parse, MUST be NULL terminated.
 * @param len	    The length of the message, not including NULL terminator.
 *
 * @return	    XML root node, or NULL if the XML document can not be parsed.
 */
bxml_node* bxml_parse( bpool_t *pool, char *msg, bsize_t len);


/**
 * Print XML into XML message. Note that the function WILL NOT NULL terminate
 * the output.
 *
 * @param node	    The XML node to print.
 * @param buf	    Buffer to hold the output message.
 * @param len	    The length of the buffer.
 * @param prolog    If set to nonzero, will print XML prolog ("<?xml..")
 *
 * @return	    The size of the printed message, or -1 if there is not 
 *		    sufficient space in the buffer to print the message.
 */
int bxml_print( const bxml_node *node, char *buf, bsize_t len,
			   bbool_t prolog);

/**
 * Clone XML node and all subnodes.
 *
 * @param pool	    Pool to allocate memory for new nodes.
 * @param rhs	    The node to clone.
 *
 * @return	    Cloned XML node, or NULL on fail.
 */
bxml_node* bxml_clone( bpool_t *pool, const bxml_node *rhs);


/**
 * Create an empty node.
 *
 * @param pool	    Pool.
 * @param name	    Node name.
 *
 * @return	    The new node.
 */
bxml_node* bxml_node_new(bpool_t *pool, const bstr_t *name);


/**
 * Create new XML attribute.
 *
 * @param pool	    Pool.
 * @param name	    Attribute name.
 * @param value	    Attribute value.
 *
 * @return	    The new XML attribute.
 */
bxml_attr* bxml_attr_new(bpool_t *pool, const bstr_t *name,
				      const bstr_t *value);

/**
 * Add node to another node.
 *
 * @param parent    Parent node.
 * @param node	    Node to be added to parent.
 */
void bxml_add_node( bxml_node *parent, bxml_node *node );


/**
 * Add attribute to a node.
 *
 * @param node	    Node.
 * @param attr	    Attribute to add to node.
 */
void bxml_add_attr( bxml_node *node, bxml_attr *attr );

/**
 * Find first direct child node with the specified name.
 *
 * @param parent    Parent node.
 * @param name	    Node name to find.
 *
 * @return	    XML node found or NULL.
 */
bxml_node* bxml_find_node(const bxml_node *parent, 
				       const bstr_t *name);

/**
 * Find next direct child node with the specified name.
 *
 * @param parent    Parent node.
 * @param node	    node->next is the starting point.
 * @param name	    Node name to find.
 *
 * @return	    XML node found or NULL.
 */
bxml_node* bxml_find_nbnode(const bxml_node *parent, 
					    const bxml_node *node,
					    const bstr_t *name);

/**
 * Recursively find the first node with the specified name in the child nodes
 * and their children.
 *
 * @param parent    Parent node.
 * @param name	    Node name to find.
 *
 * @return	    XML node found or NULL.
 */
bxml_node* bxml_find_node_rec(const bxml_node *parent, 
					   const bstr_t *name);


/**
 * Find first attribute within a node with the specified name and optional 
 * value.
 *
 * @param node	    XML Node.
 * @param name	    Attribute name to find.
 * @param value	    Optional value to match.
 *
 * @return	    XML attribute found, or NULL.
 */
bxml_attr* bxml_find_attr(const bxml_node *node, 
				       const bstr_t *name,
				       const bstr_t *value);


/**
 * Find a direct child node with the specified name and match the function.
 *
 * @param parent    Parent node.
 * @param name	    Optional name. If this is NULL, the name will not be
 *		    matched.
 * @param data	    Data to be passed to matching function.
 * @param match	    Optional matching function.
 *
 * @return	    The first matched node, or NULL.
 */
bxml_node* bxml_find( const bxml_node *parent, 
				   const bstr_t *name,
				   const void *data, 
				   bbool_t (*match)(const bxml_node *, 
						      const void*));


/**
 * Recursively find a child node with the specified name and match the 
 * function.
 *
 * @param parent    Parent node.
 * @param name	    Optional name. If this is NULL, the name will not be
 *		    matched.
 * @param data	    Data to be passed to matching function.
 * @param match	    Optional matching function.
 *
 * @return	    The first matched node, or NULL.
 */
bxml_node* bxml_find_rec(const bxml_node *parent, 
				      const bstr_t *name,
				      const void *data, 
				      bbool_t (*match)(const bxml_node*, 
							 const void*));

BASE_END_DECL

#endif

