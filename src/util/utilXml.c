/* 
 *
 */
#include <utilXml.h>
#include <utilScanner.h>
#include <baseExcept.h>
#include <basePool.h>
#include <baseString.h>
#include <baseLog.h>
#include <baseOs.h>

#define EX_SYNTAX_ERROR	12

static void on_syntax_error(struct bscanner *scanner)
{
    BASE_UNUSED_ARG(scanner);
    BASE_THROW(EX_SYNTAX_ERROR);
}

static bxml_node *alloc_node( bpool_t *pool )
{
    bxml_node *node;

    node = BASE_POOL_ZALLOC_T(pool, bxml_node);
    blist_init( &node->attr_head );
    blist_init( &node->node_head );

    return node;
}

static bxml_attr *alloc_attr( bpool_t *pool )
{
    return BASE_POOL_ZALLOC_T(pool, bxml_attr);
}

/* This is a recursive function! */
static bxml_node *xml_parse_node( bpool_t *pool, bscanner *scanner)
{
    bxml_node *node;
    bstr_t end_name;

    BASE_CHECK_STACK();

    if (*scanner->curptr != '<')
	on_syntax_error(scanner);

    /* Handle Processing Instructino (PI) construct (i.e. "<?") */
    if (*scanner->curptr == '<' && *(scanner->curptr+1) == '?') {
	bscan_advance_n(scanner, 2, BASE_FALSE);
	for (;;) {
	    bstr_t dummy;
	    bscan_get_until_ch(scanner, '?', &dummy);
	    if (*scanner->curptr=='?' && *(scanner->curptr+1)=='>') {
		bscan_advance_n(scanner, 2, BASE_TRUE);
		break;
	    } else {
		bscan_advance_n(scanner, 1, BASE_FALSE);
	    }
	}
	return xml_parse_node(pool, scanner);
    }

    /* Handle comments construct (i.e. "<!") */
    if (bscan_strcmp(scanner, "<!", 2) == 0) {
	bscan_advance_n(scanner, 2, BASE_FALSE);
	for (;;) {
	    bstr_t dummy;
	    bscan_get_until_ch(scanner, '>', &dummy);
	    if (bscan_strcmp(scanner, ">", 1) == 0) {
		bscan_advance_n(scanner, 1, BASE_TRUE);
		break;
	    } else {
		bscan_advance_n(scanner, 1, BASE_FALSE);
	    }
	}
	return xml_parse_node(pool, scanner);
    }

    /* Alloc node. */
    node = alloc_node(pool);

    /* Get '<' */
    bscan_get_char(scanner);

    /* Get node name. */
    bscan_get_until_chr( scanner, " />\t\r\n", &node->name);

    /* Get attributes. */
    while (*scanner->curptr != '>' && *scanner->curptr != '/') {
	bxml_attr *attr = alloc_attr(pool);
	
	bscan_get_until_chr( scanner, "=> \t\r\n", &attr->name);
	if (*scanner->curptr == '=') {
	    bscan_get_char( scanner );
            bscan_get_quotes(scanner, "\"'", "\"'", 2, &attr->value);
	    /* remove quote characters */
	    ++attr->value.ptr;
	    attr->value.slen -= 2;
	}
	
	blist_push_back( &node->attr_head, attr );
    }

    if (*scanner->curptr == '/') {
	bscan_get_char(scanner);
	if (bscan_get_char(scanner) != '>')
	    on_syntax_error(scanner);
	return node;
    }

    /* Enclosing bracket. */
    if (bscan_get_char(scanner) != '>')
	on_syntax_error(scanner);

    /* Sub nodes. */
    while (*scanner->curptr == '<' && *(scanner->curptr+1) != '/'
				   && *(scanner->curptr+1) != '!')
    {
	bxml_node *sub_node = xml_parse_node(pool, scanner);
	blist_push_back( &node->node_head, sub_node );
    }

    /* Content. */
    if (!bscan_is_eof(scanner) && *scanner->curptr != '<') {
	bscan_get_until_ch(scanner, '<', &node->content);
    }

    /* CDATA content. */
    if (*scanner->curptr == '<' && *(scanner->curptr+1) == '!' &&
	bscan_strcmp(scanner, "<![CDATA[", 9) == 0)
    {
	bscan_advance_n(scanner, 9, BASE_FALSE);
	bscan_get_until_ch(scanner, ']', &node->content);
	while (bscan_strcmp(scanner, "]]>", 3)) {
	    bstr_t dummy;
	    bscan_get_until_ch(scanner, ']', &dummy);
	}
	node->content.slen = scanner->curptr - node->content.ptr;
	bscan_advance_n(scanner, 3, BASE_TRUE);
    }

    /* Enclosing node. */
    if (bscan_get_char(scanner) != '<' || bscan_get_char(scanner) != '/')
	on_syntax_error(scanner);

    bscan_get_until_chr(scanner, " \t>", &end_name);

    /* Compare name. */
    if (bstricmp(&node->name, &end_name) != 0)
	on_syntax_error(scanner);

    /* Enclosing '>' */
    if (bscan_get_char(scanner) != '>')
	on_syntax_error(scanner);

    return node;
}

bxml_node* bxml_parse( bpool_t *pool, char *msg, bsize_t len)
{
    bxml_node *node = NULL;
    bscanner scanner;
    BASE_USE_EXCEPTION;

    if (!msg || !len || !pool)
	return NULL;

    bscan_init( &scanner, msg, len, 
		  BASE_SCAN_AUTOSKIP_WS|BASE_SCAN_AUTOSKIP_NEWLINE, 
		  &on_syntax_error);
    BASE_TRY {
	node =  xml_parse_node(pool, &scanner);
    }
    BASE_CATCH_ANY {
	BASE_ERROR("Syntax error parsing XML in line %d column %d", scanner.line, bscan_get_col(&scanner));
    }
    BASE_END;
    bscan_fini( &scanner );
    return node;
}

/* This is a recursive function. */
static int xml_print_node( const bxml_node *node, int indent, 
			   char *buf, bsize_t len )
{
    int i;
    char *p = buf;
    bxml_attr *attr;
    bxml_node *sub_node;

#define SIZE_LEFT()	((int)(len - (p-buf)))

    BASE_CHECK_STACK();

    /* Print name. */
    if (SIZE_LEFT() < node->name.slen + indent + 5)
	return -1;
    for (i=0; i<indent; ++i)
	*p++ = ' ';
    *p++ = '<';
    bmemcpy(p, node->name.ptr, node->name.slen);
    p += node->name.slen;

    /* Print attributes. */
    attr = node->attr_head.next;
    while (attr != &node->attr_head) {

	if (SIZE_LEFT() < attr->name.slen + attr->value.slen + 4)
	    return -1;

	*p++ = ' ';

	/* Attribute name. */
	bmemcpy(p, attr->name.ptr, attr->name.slen);
	p += attr->name.slen;

	/* Attribute value. */
	if (attr->value.slen) {
	    *p++ = '=';
	    *p++ = '"';
	    bmemcpy(p, attr->value.ptr, attr->value.slen);
	    p += attr->value.slen;
	    *p++ = '"';
	}

	attr = attr->next;
    }

    /* Check for empty node. */
    if (node->content.slen==0 &&
	node->node_head.next==(bxml_node*)&node->node_head)
    {
        if (SIZE_LEFT() < 3) return -1;
	*p++ = ' ';
	*p++ = '/';
	*p++ = '>';
	return (int)(p-buf);
    }

    /* Enclosing '>' */
    if (SIZE_LEFT() < 1) return -1;
    *p++ = '>';

    /* Print sub nodes. */
    sub_node = node->node_head.next;
    while (sub_node != (bxml_node*)&node->node_head) {
	int printed;

	if (SIZE_LEFT() < indent + 3)
	    return -1;
	//*p++ = '\r';
	*p++ = '\n';

	printed = xml_print_node(sub_node, indent + 1, p, SIZE_LEFT());
	if (printed < 0)
	    return -1;

	p += printed;
	sub_node = sub_node->next;
    }

    /* Content. */
    if (node->content.slen) {
	if (SIZE_LEFT() < node->content.slen) return -1;
	bmemcpy(p, node->content.ptr, node->content.slen);
	p += node->content.slen;
    }

    /* Enclosing node. */
    if (node->node_head.next != (bxml_node*)&node->node_head) {
	if (SIZE_LEFT() < node->name.slen + 5 + indent)
	    return -1;
	//*p++ = '\r';
	*p++ = '\n';
	for (i=0; i<indent; ++i)
	    *p++ = ' ';
    } else {
	if (SIZE_LEFT() < node->name.slen + 3)
	    return -1;
    }
    *p++ = '<';
    *p++ = '/';
    bmemcpy(p, node->name.ptr, node->name.slen);
    p += node->name.slen;
    *p++ = '>';

#undef SIZE_LEFT

    return (int)(p-buf);
}

int bxml_print(const bxml_node *node, char *buf, bsize_t len,
			 bbool_t include_prolog)
{
    int prolog_len = 0;
    int printed;

    if (!node || !buf || !len)
	return 0;

    if (include_prolog) {
	bstr_t prolog = {"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", 39};
	if ((int)len < prolog.slen)
	    return -1;
	bmemcpy(buf, prolog.ptr, prolog.slen);
	prolog_len = (int)prolog.slen;
    }

    printed = xml_print_node(node, 0, buf+prolog_len, len-prolog_len) + prolog_len;
    if (printed > 0 && len-printed >= 1) {
	buf[printed++] = '\n';
    }
    return printed;
}

bxml_node* bxml_node_new(bpool_t *pool, const bstr_t *name)
{
    bxml_node *node = alloc_node(pool);
    bstrdup(pool, &node->name, name);
    return node;
}

bxml_attr* bxml_attr_new( bpool_t *pool, const bstr_t *name,
				      const bstr_t *value)
{
    bxml_attr *attr = alloc_attr(pool);
    bstrdup( pool, &attr->name, name);
    bstrdup( pool, &attr->value, value);
    return attr;
}

void bxml_add_node( bxml_node *parent, bxml_node *node )
{
    blist_push_back(&parent->node_head, node);
}

void bxml_add_attr( bxml_node *node, bxml_attr *attr )
{
    blist_push_back(&node->attr_head, attr);
}

bxml_node* bxml_find_node(const bxml_node *parent, 
				      const bstr_t *name)
{
    const bxml_node *node = parent->node_head.next;

    BASE_CHECK_STACK();

    while (node != (void*)&parent->node_head) {
	if (bstricmp(&node->name, name) == 0)
	    return (bxml_node*)node;
	node = node->next;
    }
    return NULL;
}

bxml_node* bxml_find_node_rec(const bxml_node *parent, 
					  const bstr_t *name)
{
    const bxml_node *node = parent->node_head.next;

    BASE_CHECK_STACK();

    while (node != (void*)&parent->node_head) {
	bxml_node *found;
	if (bstricmp(&node->name, name) == 0)
	    return (bxml_node*)node;
	found = bxml_find_node_rec(node, name);
	if (found)
	    return (bxml_node*)found;
	node = node->next;
    }
    return NULL;
}

bxml_node* bxml_find_nbnode( const bxml_node *parent, 
					    const bxml_node *node,
					    const bstr_t *name)
{
    BASE_CHECK_STACK();

    node = node->next;
    while (node != (void*)&parent->node_head) {
	if (bstricmp(&node->name, name) == 0)
	    return (bxml_node*)node;
	node = node->next;
    }
    return NULL;
}


bxml_attr* bxml_find_attr( const bxml_node *node, 
				       const bstr_t *name,
				       const bstr_t *value)
{
    const bxml_attr *attr = node->attr_head.next;
    while (attr != (void*)&node->attr_head) {
	if (bstricmp(&attr->name, name)==0) {
	    if (value) {
		if (bstricmp(&attr->value, value)==0)
		    return (bxml_attr*)attr;
	    } else {
		return (bxml_attr*)attr;
	    }
	}
	attr = attr->next;
    }
    return NULL;
}



bxml_node* bxml_find( const bxml_node *parent, 
				  const bstr_t *name,
				  const void *data, 
				  bbool_t (*match)(const bxml_node *, 
						     const void*))
{
    const bxml_node *node = (const bxml_node *)parent->node_head.next;

    if (!name && !match)
	return NULL;

    while (node != (const bxml_node*) &parent->node_head) {
	if (name) {
	    if (bstricmp(&node->name, name)!=0) {
		node = node->next;
		continue;
	    }
	}
	if (match) {
	    if (match(node, data))
		return (bxml_node*)node;
	} else {
	    return (bxml_node*)node;
	}

	node = node->next;
    }
    return NULL;
}

bxml_node* bxml_find_rec( const bxml_node *parent, 
				      const bstr_t *name,
				      const void *data, 
				      bbool_t (*match)(const bxml_node*, 
							 const void*))
{
    const bxml_node *node = (const bxml_node *)parent->node_head.next;

    if (!name && !match)
	return NULL;

    while (node != (const bxml_node*) &parent->node_head) {
	bxml_node *found;

	if (name) {
	    if (bstricmp(&node->name, name)==0) {
		if (match) {
		    if (match(node, data))
			return (bxml_node*)node;
		} else {
		    return (bxml_node*)node;
		}
	    }

	} else if (match) {
	    if (match(node, data))
		return (bxml_node*)node;
	}

	found = bxml_find_rec(node, name, data, match);
	if (found)
	    return found;

	node = node->next;
    }
    return NULL;
}

bxml_node* bxml_clone( bpool_t *pool, const bxml_node *rhs)
{
    bxml_node *node;
    const bxml_attr *r_attr;
    const bxml_node *child;

    node = alloc_node(pool);

    bstrdup(pool, &node->name, &rhs->name);
    bstrdup(pool, &node->content, &rhs->content);

    /* Clone all attributes */
    r_attr = rhs->attr_head.next;
    while (r_attr != &rhs->attr_head) {

	bxml_attr *attr;

	attr = alloc_attr(pool);
	bstrdup(pool, &attr->name, &r_attr->name);
	bstrdup(pool, &attr->value, &r_attr->value);

	blist_push_back(&node->attr_head, attr);

	r_attr = r_attr->next;
    }

    /* Clone all child nodes. */
    child = rhs->node_head.next;
    while (child != (bxml_node*) &rhs->node_head) {
	bxml_node *new_child;

	new_child = bxml_clone(pool, child);
	blist_push_back(&node->node_head, new_child);

	child = child->next;
    }

    return node;
}
