/* 
 *
 */
#include <utilJson.h>
#include <utilErrno.h>
#include <utilScanner.h>
#include <baseAssert.h>
#include <baseCtype.h>
#include <baseExcept.h>
#include <baseString.h>

#define EL_INIT(p_el, nm, typ)	do { \
				    if (nm) { \
					p_el->name = *nm; \
				    } else { \
					p_el->name.ptr = (char*)""; \
					p_el->name.slen = 0; \
				    } \
				    p_el->type = typ; \
				} while (0)

struct write_state;
struct parse_state;

#define NO_NAME	1

static bstatus_t elem_write(const bjson_elem *elem,
                              struct write_state *st,
                              unsigned flags);
static bjson_elem* parse_elem_throw(struct parse_state *st,
                                      bjson_elem *elem);


void bjson_elem_null(bjson_elem *el, bstr_t *name)
{
    EL_INIT(el, name, BASE_JSON_VAL_NULL);
}

void bjson_elem_bool(bjson_elem *el, bstr_t *name,
                                bbool_t val)
{
    EL_INIT(el, name, BASE_JSON_VAL_BOOL);
    el->value.is_true = val;
}

void bjson_elem_number(bjson_elem *el, bstr_t *name,
                                  float val)
{
    EL_INIT(el, name, BASE_JSON_VAL_NUMBER);
    el->value.num = val;
}

void bjson_elem_string( bjson_elem *el, bstr_t *name,
                                  bstr_t *value)
{
    EL_INIT(el, name, BASE_JSON_VAL_STRING);
    el->value.str = *value;
}

void bjson_elem_array(bjson_elem *el, bstr_t *name)
{
    EL_INIT(el, name, BASE_JSON_VAL_ARRAY);
    blist_init(&el->value.children);
}

void bjson_elem_obj(bjson_elem *el, bstr_t *name)
{
    EL_INIT(el, name, BASE_JSON_VAL_OBJ);
    blist_init(&el->value.children);
}

void bjson_elem_add(bjson_elem *el, bjson_elem *child)
{
    bassert(el->type == BASE_JSON_VAL_OBJ || el->type == BASE_JSON_VAL_ARRAY);
    blist_push_back(&el->value.children, child);
}

struct parse_state
{
    bpool_t		*pool;
    bscanner		 scanner;
    bjson_err_info 	*err_info;
    bcis_t		 float_spec;	/* numbers with dot! */
};

static bstatus_t parse_children(struct parse_state *st,
                                  bjson_elem *parent)
{
    char end_quote = (parent->type == BASE_JSON_VAL_ARRAY)? ']' : '}';

    bscan_get_char(&st->scanner);

    while (*st->scanner.curptr != end_quote) {
	bjson_elem *child;

	while (*st->scanner.curptr == ',')
	    bscan_get_char(&st->scanner);

	if (*st->scanner.curptr == end_quote)
	    break;

	child = parse_elem_throw(st, NULL);
	if (!child)
	    return UTIL_EINJSON;

	bjson_elem_add(parent, child);
    }

    bscan_get_char(&st->scanner);
    return BASE_SUCCESS;
}

/* Return 0 if success or the index of the invalid char in the string */
static unsigned parse_quoted_string(struct parse_state *st,
                                    bstr_t *output)
{
    bstr_t token;
    char *op, *ip, *iend;

    bscan_get_quote(&st->scanner, '"', '"', &token);

    /* Remove the quote characters */
    token.ptr++;
    token.slen-=2;

    if (bstrchr(&token, '\\') == NULL) {
	*output = token;
	return 0;
    }

    output->ptr = op = bpool_alloc(st->pool, token.slen);

    ip = token.ptr;
    iend = token.ptr + token.slen;

    while (ip != iend) {
	if (*ip == '\\') {
	    ++ip;
	    if (ip==iend) {
		goto on_error;
	    }
	    if (*ip == 'u') {
		ip++;
		if (iend - ip < 4) {
		    ip = iend -1;
		    goto on_error;
		}
		/* Only use the last two hext digits because we're on
		 * ASCII */
		*op++ = (char)(bhex_digit_to_val(ip[2]) * 16 +
			       bhex_digit_to_val(ip[3]));
		ip += 4;
	    } else if (*ip=='"' || *ip=='\\' || *ip=='/') {
		*op++ = *ip++;
	    } else if (*ip=='b') {
		*op++ = '\b';
		ip++;
	    } else if (*ip=='f') {
		*op++ = '\f';
		ip++;
	    } else if (*ip=='n') {
		*op++ = '\n';
		ip++;
	    } else if (*ip=='r') {
		*op++ = '\r';
		ip++;
	    } else if (*ip=='t') {
		*op++ = '\t';
		ip++;
	    } else {
		goto on_error;
	    }
	} else {
	    *op++ = *ip++;
	}
    }

    output->slen = op - output->ptr;
    return 0;

on_error:
    output->slen = op - output->ptr;
    return (unsigned)(ip - token.ptr);
}

static bjson_elem* parse_elem_throw(struct parse_state *st,
                                      bjson_elem *elem)
{
    bstr_t name = {NULL, 0}, value = {NULL, 0};
    bstr_t token;

    if (!elem)
	elem = bpool_alloc(st->pool, sizeof(*elem));

    /* Parse name */
    if (*st->scanner.curptr == '"') {
	bscan_get_char(&st->scanner);
	bscan_get_until_ch(&st->scanner, '"', &token);
	bscan_get_char(&st->scanner);

	if (*st->scanner.curptr == ':') {
	    bscan_get_char(&st->scanner);
	    name = token;
	} else {
	    value = token;
	}
    }

    if (value.slen) {
	/* Element with string value and no name */
	bjson_elem_string(elem, &name, &value);
	return elem;
    }

    /* Parse value */
    if (bcis_match(&st->float_spec, *st->scanner.curptr) ||
	*st->scanner.curptr == '-')
    {
	float val;
	bbool_t neg = BASE_FALSE;

	if (*st->scanner.curptr == '-') {
	    bscan_get_char(&st->scanner);
	    neg = BASE_TRUE;
	}

	bscan_get(&st->scanner, &st->float_spec, &token);
	val = bstrtof(&token);
	if (neg) val = -val;

	bjson_elem_number(elem, &name, val);

    } else if (*st->scanner.curptr == '"') {
	unsigned err;
	char *start = st->scanner.curptr;

	err = parse_quoted_string(st, &token);
	if (err) {
	    st->scanner.curptr = start + err;
	    return NULL;
	}

	bjson_elem_string(elem, &name, &token);

    } else if (bisalpha(*st->scanner.curptr)) {

	if (bscan_strcmp(&st->scanner, "false", 5)==0) {
	    bjson_elem_bool(elem, &name, BASE_FALSE);
	    bscan_advance_n(&st->scanner, 5, BASE_TRUE);
	} else if (bscan_strcmp(&st->scanner, "true", 4)==0) {
	    bjson_elem_bool(elem, &name, BASE_TRUE);
	    bscan_advance_n(&st->scanner, 4, BASE_TRUE);
	} else if (bscan_strcmp(&st->scanner, "null", 4)==0) {
	    bjson_elem_null(elem, &name);
	    bscan_advance_n(&st->scanner, 4, BASE_TRUE);
	} else {
	    return NULL;
	}

    } else if (*st->scanner.curptr == '[') {
	bjson_elem_array(elem, &name);
	if (parse_children(st, elem) != BASE_SUCCESS)
	    return NULL;

    } else if (*st->scanner.curptr == '{') {
	bjson_elem_obj(elem, &name);
	if (parse_children(st, elem) != BASE_SUCCESS)
	    return NULL;

    } else {
	return NULL;
    }

    return elem;
}

static void on_syntax_error(bscanner *scanner)
{
    BASE_UNUSED_ARG(scanner);
    BASE_THROW(11);
}

bjson_elem* bjson_parse(bpool_t *pool,
                                    char *buffer,
                                    unsigned *size,
                                    bjson_err_info *err_info)
{
    bcis_buf_t cis_buf;
    struct parse_state st;
    bjson_elem *root;
    BASE_USE_EXCEPTION;

    BASE_ASSERT_RETURN(pool && buffer && size, NULL);

    if (!*size)
	return NULL;

    bbzero(&st, sizeof(st));
    st.pool = pool;
    st.err_info = err_info;
    bscan_init(&st.scanner, buffer, *size,
                 BASE_SCAN_AUTOSKIP_WS | BASE_SCAN_AUTOSKIP_NEWLINE,
                 &on_syntax_error);
    bcis_buf_init(&cis_buf);
    bcis_init(&cis_buf, &st.float_spec);
    bcis_add_str(&st.float_spec, ".0123456789");

    BASE_TRY {
	root = parse_elem_throw(&st, NULL);
    }
    BASE_CATCH_ANY {
	root = NULL;
    }
    BASE_END

    if (!root && err_info) {
	err_info->line = st.scanner.line;
	err_info->col = bscan_get_col(&st.scanner) + 1;
	err_info->err_char = *st.scanner.curptr;
    }

    *size = (unsigned)((buffer + *size) - st.scanner.curptr);

    bscan_fini(&st.scanner);

    return root;
}

struct buf_writer_data
{
    char	*pos;
    unsigned	 size;
};

static bstatus_t buf_writer(const char *s,
			      unsigned size,
			      void *user_data)
{
    struct buf_writer_data *buf_data = (struct buf_writer_data*)user_data;
    if (size+1 >= buf_data->size)
	return BASE_ETOOBIG;

    bmemcpy(buf_data->pos, s, size);
    buf_data->pos += size;
    buf_data->size -= size;

    return BASE_SUCCESS;
}

bstatus_t bjson_write(const bjson_elem *elem,
                                  char *buffer, unsigned *size)
{
    struct buf_writer_data buf_data;
    bstatus_t status;

    BASE_ASSERT_RETURN(elem && buffer && size, BASE_EINVAL);

    buf_data.pos = buffer;
    buf_data.size = *size;

    status = bjson_writef(elem, &buf_writer, &buf_data);
    if (status != BASE_SUCCESS)
	return status;

    *buf_data.pos = '\0';
    *size = (unsigned)(buf_data.pos - buffer);
    return BASE_SUCCESS;
}

#define MAX_INDENT 		100
#ifndef BASE_JSON_NAME_MIN_LEN
#  define BASE_JSON_NAME_MIN_LEN	20
#endif
#define ESC_BUF_LEN		64
#ifndef BASE_JSON_INDENT_SIZE
#  define BASE_JSON_INDENT_SIZE	3
#endif

struct write_state
{
    bjson_writer	 writer;
    void 		*user_data;
    char		 indent_buf[MAX_INDENT];
    int			 indent;
    char		 space[BASE_JSON_NAME_MIN_LEN];
};

#define CHECK(expr) do { \
			status=expr; if (status!=BASE_SUCCESS) return status; } \
		    while (0)

static bstatus_t write_string_escaped(const bstr_t *value,
                                        struct write_state *st)
{
    const char *ip = value->ptr;
    const char *iend = value->ptr + value->slen;
    char buf[ESC_BUF_LEN];
    char *op = buf;
    char *oend = buf + ESC_BUF_LEN;
    bstatus_t status;

    while (ip != iend) {
	/* Write to buffer to speedup writing instead of calling
	 * the callback one by one for each character.
	 */
	while (ip != iend && op != oend) {
	    if (oend - op < 2)
		break;

	    if (*ip == '"') {
		*op++ = '\\';
		*op++ = '"';
		ip++;
	    } else if (*ip == '\\') {
		*op++ = '\\';
		*op++ = '\\';
		ip++;
	    } else if (*ip == '/') {
		*op++ = '\\';
		*op++ = '/';
		ip++;
	    } else if (*ip == '\b') {
		*op++ = '\\';
		*op++ = 'b';
		ip++;
	    } else if (*ip == '\f') {
		*op++ = '\\';
		*op++ = 'f';
		ip++;
	    } else if (*ip == '\n') {
		*op++ = '\\';
		*op++ = 'n';
		ip++;
	    } else if (*ip == '\r') {
		*op++ = '\\';
		*op++ = 'r';
		ip++;
	    } else if (*ip == '\t') {
		*op++ = '\\';
		*op++ = 't';
		ip++;
	    } else if ((*ip >= 32 && *ip < 127)) {
		/* unescaped */
		*op++ = *ip++;
	    } else {
		/* escaped */
		if (oend - op < 6)
		    break;
		*op++ = '\\';
		*op++ = 'u';
		*op++ = '0';
		*op++ = '0';
		bval_to_hex_digit(*ip, op);
		op+=2;
		ip++;
	    }
	}

	CHECK( st->writer( buf, (unsigned)(op-buf), st->user_data) );
	op = buf;
    }

    return BASE_SUCCESS;
}

static bstatus_t write_children(const bjson_list *list,
                                  const char quotes[2],
                                  struct write_state *st)
{
    unsigned flags = (quotes[0]=='[') ? NO_NAME : 0;
    bstatus_t status;

    //CHECK( st->writer( st->indent_buf, st->indent, st->user_data) );
    CHECK( st->writer( &quotes[0], 1, st->user_data) );
    CHECK( st->writer( " ", 1, st->user_data) );

    if (!blist_empty(list)) {
	bbool_t indent_added = BASE_FALSE;
	bjson_elem *child = list->next;

	if (child->name.slen == 0) {
	    /* Simple list */
	    while (child != (bjson_elem*)list) {
		status = elem_write(child, st, flags);
		if (status != BASE_SUCCESS)
		    return status;

		if (child->next != (bjson_elem*)list)
		    CHECK( st->writer( ", ", 2, st->user_data) );
		child = child->next;
	    }
	} else {
	    if (st->indent < sizeof(st->indent_buf)) {
		st->indent += BASE_JSON_INDENT_SIZE;
		indent_added = BASE_TRUE;
	    }
	    CHECK( st->writer( "\n", 1, st->user_data) );
	    while (child != (bjson_elem*)list) {
		status = elem_write(child, st, flags);
		if (status != BASE_SUCCESS)
		    return status;

		if (child->next != (bjson_elem*)list)
		    CHECK( st->writer( ",\n", 2, st->user_data) );
		else
		    CHECK( st->writer( "\n", 1, st->user_data) );
		child = child->next;
	    }
	    if (indent_added) {
		st->indent -= BASE_JSON_INDENT_SIZE;
	    }
	    CHECK( st->writer( st->indent_buf, st->indent, st->user_data) );
	}
    }
    CHECK( st->writer( &quotes[1], 1, st->user_data) );

    return BASE_SUCCESS;
}

static bstatus_t elem_write(const bjson_elem *elem,
                              struct write_state *st,
                              unsigned flags)
{
    bstatus_t status;

    if (elem->name.slen) {
	CHECK( st->writer( st->indent_buf, st->indent, st->user_data) );
	if ((flags & NO_NAME)==0) {
	    CHECK( st->writer( "\"", 1, st->user_data) );
	    CHECK( write_string_escaped(&elem->name, st) );
	    CHECK( st->writer( "\": ", 3, st->user_data) );
	    if (elem->name.slen < BASE_JSON_NAME_MIN_LEN /*&&
		elem->type != BASE_JSON_VAL_OBJ &&
		elem->type != BASE_JSON_VAL_ARRAY*/)
	    {
		CHECK( st->writer( st->space,
		                   (unsigned)(BASE_JSON_NAME_MIN_LEN -
					      elem->name.slen),
				   st->user_data) );
	    }
	}
    }

    switch (elem->type) {
    case BASE_JSON_VAL_NULL:
	CHECK( st->writer( "null", 4, st->user_data) );
	break;
    case BASE_JSON_VAL_BOOL:
	if (elem->value.is_true)
	    CHECK( st->writer( "true", 4, st->user_data) );
	else
	    CHECK( st->writer( "false", 5, st->user_data) );
	break;
    case BASE_JSON_VAL_NUMBER:
	{
	    char num_buf[65];
	    int len;

	    if (elem->value.num == (int)elem->value.num)
		len = bansi_snprintf(num_buf, sizeof(num_buf), "%d",
		                       (int)elem->value.num);
	    else
		len = bansi_snprintf(num_buf, sizeof(num_buf), "%f",
		                       elem->value.num);

	    if (len < 0 || len >= sizeof(num_buf))
		return BASE_ETOOBIG;
	    CHECK( st->writer( num_buf, len, st->user_data) );
	}
	break;
    case BASE_JSON_VAL_STRING:
	CHECK( st->writer( "\"", 1, st->user_data) );
	CHECK( write_string_escaped( &elem->value.str, st) );
	CHECK( st->writer( "\"", 1, st->user_data) );
	break;
    case BASE_JSON_VAL_ARRAY:
	CHECK( write_children(&elem->value.children, "[]", st) );
	break;
    case BASE_JSON_VAL_OBJ:
	CHECK( write_children(&elem->value.children, "{}", st) );
	break;
    default:
	bassert(!"Unhandled value type");
    }

    return BASE_SUCCESS;
}

#undef CHECK

bstatus_t bjson_writef( const bjson_elem *elem,
                                    bjson_writer writer,
                                    void *user_data)
{
    struct write_state st;

    BASE_ASSERT_RETURN(elem && writer, BASE_EINVAL);

    st.writer 		= writer;
    st.user_data	= user_data;
    st.indent 		= 0;
    bmemset(st.indent_buf, ' ', MAX_INDENT);
    bmemset(st.space, ' ', BASE_JSON_NAME_MIN_LEN);

    return elem_write(elem, &st, 0);
}

