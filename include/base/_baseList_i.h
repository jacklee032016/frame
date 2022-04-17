/* 
 *
 */


/* Internal */
BASE_INLINE_SPECIFIER void blink_node(blist_type *prev, blist_type *next)
{
    ((blist*)prev)->next = next;
    ((blist*)next)->prev = prev;
}

void blist_insert_after(blist_type *pos, blist_type *node)
{
    ((blist*)node)->prev = pos;
    ((blist*)node)->next = ((blist*)pos)->next;
    ((blist*) ((blist*)pos)->next) ->prev = node;
    ((blist*)pos)->next = node;
}


BASE_IDEF(void) blist_insert_before(blist_type *pos, blist_type *node)
{
    blist_insert_after(((blist*)pos)->prev, node);
}


BASE_IDEF(void) blist_insert_nodes_after(blist_type *pos, blist_type *lst)
{
    blist *lst_last = (blist *) ((blist*)lst)->prev;
    blist *pos_next = (blist *) ((blist*)pos)->next;

    blink_node(pos, lst);
    blink_node(lst_last, pos_next);
}

BASE_IDEF(void) blist_insert_nodes_before(blist_type *pos, blist_type *lst)
{
    blist_insert_nodes_after(((blist*)pos)->prev, lst);
}

BASE_IDEF(void) blist_merge_last(blist_type *lst1, blist_type *lst2)
{
    if (!blist_empty(lst2)) {
	blink_node(((blist*)lst1)->prev, ((blist*)lst2)->next);
	blink_node(((blist*)lst2)->prev, lst1);
	blist_init(lst2);
    }
}

BASE_IDEF(void) blist_merge_first(blist_type *lst1, blist_type *lst2)
{
    if (!blist_empty(lst2)) {
	blink_node(((blist*)lst2)->prev, ((blist*)lst1)->next);
	blink_node(((blist*)lst1), ((blist*)lst2)->next);
	blist_init(lst2);
    }
}

BASE_IDEF(void) blist_erase(blist_type *node)
{
    blink_node( ((blist*)node)->prev, ((blist*)node)->next);

    /* It'll be safer to init the nprev fields to itself, to
     * prevent multiple erase() from corrupting the list. See
     * ticket #520 for one sample bug.
     */
    blist_init(node);
}


BASE_IDEF(blist_type*) blist_find_node(blist_type *list, blist_type *node)
{
    blist *p = (blist *) ((blist*)list)->next;
    while (p != list && p != node)
	p = (blist *) p->next;

    return p==node ? p : NULL;
}


BASE_IDEF(blist_type*) blist_search(blist_type *list, void *value,
	       		int (*comp)(void *value, const blist_type *node))
{
    blist *p = (blist *) ((blist*)list)->next;
    while (p != list && (*comp)(value, p) != 0)
	p = (blist *) p->next;

    return p==list ? NULL : p;
}


BASE_IDEF(bsize_t) blist_size(const blist_type *list)
{
    const blist *node = (const blist*) ((const blist*)list)->next;
    bsize_t count = 0;

    while (node != list) {
	++count;
	node = (blist*)node->next;
    }

    return count;
}

