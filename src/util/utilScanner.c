/* 
 *
 */
#include <utilScanner.h>
#include <baseCtype.h>
#include <baseString.h>
#include <baseExcept.h>
#include <baseOs.h>
#include <baseLog.h>
#include <baseErrno.h>
#include <baseAssert.h>

#define BASE_SCAN_IS_SPACE(c)		((c)==' ' || (c)=='\t')
#define BASE_SCAN_IS_NEWLINE(c)		((c)=='\r' || (c)=='\n')
#define BASE_SCAN_IS_PROBABLY_SPACE(c)	((c) <= 32)
#define BASE_SCAN_CHECK_EOF(s)		(s != scanner->end)


#if defined(BASE_SCANNER_USE_BITWISE) && BASE_SCANNER_USE_BITWISE != 0
#include "_utilScannerCisBitwise.c"
#else
#include "_utilScannerCisUint.c"
#endif


static void bscan_syntax_err(bscanner *scanner)
{
    (*scanner->callback)(scanner);
}


void bcis_add_range(bcis_t *cis, int cstart, int cend)
{
    /* Can not set zero. This is the requirement of the parser. */
    bassert(cstart > 0);

    while (cstart != cend) {
        BASE_CIS_SET(cis, cstart);
	++cstart;
    }
}

void bcis_add_alpha(bcis_t *cis)
{
    bcis_add_range( cis, 'a', 'z'+1);
    bcis_add_range( cis, 'A', 'Z'+1);
}

void bcis_add_num(bcis_t *cis)
{
    bcis_add_range( cis, '0', '9'+1);
}

void bcis_add_str( bcis_t *cis, const char *str)
{
    while (*str) {
        BASE_CIS_SET(cis, *str);
	++str;
    }
}

void bcis_add_cis( bcis_t *cis, const bcis_t *rhs)
{
    int i;
    for (i=0; i<256; ++i) {
	if (BASE_CIS_ISSET(rhs, i))
	    BASE_CIS_SET(cis, i);
    }
}

void bcis_del_range( bcis_t *cis, int cstart, int cend)
{
    while (cstart != cend) {
        BASE_CIS_CLR(cis, cstart);
        cstart++;
    }
}

void bcis_del_str( bcis_t *cis, const char *str)
{
    while (*str) {
        BASE_CIS_CLR(cis, *str);
	++str;
    }
}

void bcis_invert( bcis_t *cis )
{
    unsigned i;
    /* Can not set zero. This is the requirement of the parser. */
    for (i=1; i<256; ++i) {
	if (BASE_CIS_ISSET(cis,i))
            BASE_CIS_CLR(cis,i);
        else
            BASE_CIS_SET(cis,i);
    }
}

void bscan_init( bscanner *scanner, char *bufstart, 
			   bsize_t buflen, unsigned options, 
			   bsyn_err_func_ptr callback )
{
    BASE_CHECK_STACK();

    scanner->begin = scanner->curptr = bufstart;
    scanner->end = bufstart + buflen;
    scanner->line = 1;
    scanner->start_line = scanner->begin;
    scanner->callback = callback;
    scanner->skip_ws = options;

    if (scanner->skip_ws) 
	bscan_skip_whitespace(scanner);
}


void bscan_fini( bscanner *scanner )
{
    BASE_CHECK_STACK();
    BASE_UNUSED_ARG(scanner);
}

void bscan_skip_whitespace( bscanner *scanner )
{
    register char *s = scanner->curptr;

    while (BASE_SCAN_IS_SPACE(*s)) {
	++s;
    }

    if (BASE_SCAN_IS_NEWLINE(*s) && (scanner->skip_ws & BASE_SCAN_AUTOSKIP_NEWLINE)) {
	for (;;) {
	    if (*s == '\r') {
		++s;
		if (*s == '\n') ++s;
		++scanner->line;
		scanner->curptr = scanner->start_line = s;
	    } else if (*s == '\n') {
		++s;
		++scanner->line;
		scanner->curptr = scanner->start_line = s;
	    } else if (BASE_SCAN_IS_SPACE(*s)) {
		do {
		    ++s;
		} while (BASE_SCAN_IS_SPACE(*s));
	    } else {
		break;
	    }
	}
    }

    if (BASE_SCAN_IS_NEWLINE(*s) && (scanner->skip_ws & BASE_SCAN_AUTOSKIP_WS_HEADER)==BASE_SCAN_AUTOSKIP_WS_HEADER) {
	/* Check for header continuation. */
	scanner->curptr = s;

	if (*s == '\r') {
	    ++s;
	}
	if (*s == '\n') {
	    ++s;
	}
	scanner->start_line = s;

	if (BASE_SCAN_IS_SPACE(*s)) {
	    register char *t = s;
	    do {
		++t;
	    } while (BASE_SCAN_IS_SPACE(*t));

	    ++scanner->line;
	    scanner->curptr = t;
	}
    } else {
	scanner->curptr = s;
    }
}

void bscan_skip_line( bscanner *scanner )
{
    char *s = bansi_strchr(scanner->curptr, '\n');
    if (!s) {
	scanner->curptr = scanner->end;
    } else {
	scanner->curptr = scanner->start_line = s+1;
	scanner->line++;
   }
}

int bscan_peek( bscanner *scanner,
			  const bcis_t *spec, bstr_t *out)
{
    register char *s = scanner->curptr;

    if (s >= scanner->end) {
	bscan_syntax_err(scanner);
	return -1;
    }

    /* Don't need to check EOF with BASE_SCAN_CHECK_EOF(s) */
    while (bcis_match(spec, *s))
	++s;

    bstrset3(out, scanner->curptr, s);
    return *s;
}


int bscan_peek_n( bscanner *scanner,
			     bsize_t len, bstr_t *out)
{
    char *endpos = scanner->curptr + len;

    if (endpos > scanner->end) {
	bscan_syntax_err(scanner);
	return -1;
    }

    bstrset(out, scanner->curptr, len);
    return *endpos;
}


int bscan_peek_until( bscanner *scanner,
				const bcis_t *spec, 
				bstr_t *out)
{
    register char *s = scanner->curptr;

    if (s >= scanner->end) {
	bscan_syntax_err(scanner);
	return -1;
    }

    while (BASE_SCAN_CHECK_EOF(s) && !bcis_match( spec, *s))
	++s;

    bstrset3(out, scanner->curptr, s);
    return *s;
}


void bscan_get( bscanner *scanner,
			  const bcis_t *spec, bstr_t *out)
{
    register char *s = scanner->curptr;

    bassert(bcis_match(spec,0)==0);

    /* EOF is detected implicitly */
    if (!bcis_match(spec, *s)) {
	bscan_syntax_err(scanner);
	return;
    }

    do {
	++s;
    } while (bcis_match(spec, *s));
    /* No need to check EOF here (BASE_SCAN_CHECK_EOF(s)) because
     * buffer is NULL terminated and bcis_match(spec,0) should be
     * false.
     */

    bstrset3(out, scanner->curptr, s);

    scanner->curptr = s;

    if (BASE_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	bscan_skip_whitespace(scanner);    
    }
}


void bscan_get_unescape( bscanner *scanner,
				   const bcis_t *spec, bstr_t *out)
{
    register char *s = scanner->curptr;
    char *dst = s;

    bassert(bcis_match(spec,0)==0);

    /* Must not match character '%' */
    bassert(bcis_match(spec,'%')==0);

    /* EOF is detected implicitly */
    if (!bcis_match(spec, *s) && *s != '%') {
	bscan_syntax_err(scanner);
	return;
    }

    out->ptr = s;
    do {
	if (*s == '%') {
	    if (s+3 <= scanner->end && bisxdigit(*(s+1)) && 
		bisxdigit(*(s+2))) 
	    {
		*dst = (buint8_t) ((bhex_digit_to_val(*(s+1)) << 4) +
				      bhex_digit_to_val(*(s+2)));
		++dst;
		s += 3;
	    } else {
		*dst++ = *s++;
		*dst++ = *s++;
		break;
	    }
	}
	
	if (bcis_match(spec, *s)) {
	    char *start = s;
	    do {
		++s;
	    } while (bcis_match(spec, *s));

	    if (dst != start) bmemmove(dst, start, s-start);
	    dst += (s-start);
	} 
	
    } while (*s == '%');

    scanner->curptr = s;
    out->slen = (dst - out->ptr);

    if (BASE_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	bscan_skip_whitespace(scanner);    
    }
}


void bscan_get_quote( bscanner *scanner,
				int begin_quote, int end_quote, 
				bstr_t *out)
{
    char beg = (char)begin_quote;
    char end = (char)end_quote;
    bscan_get_quotes(scanner, &beg, &end, 1, out);
}

void bscan_get_quotes(bscanner *scanner,
                                const char *begin_quote, const char *end_quote,
                                int qsize, bstr_t *out)
{
    register char *s = scanner->curptr;
    int qpair = -1;
    int i;

    bassert(qsize > 0);

    /* Check and eat the begin_quote. */
    for (i = 0; i < qsize; ++i) {
	if (*s == begin_quote[i]) {
	    qpair = i;
	    break;
	}
    }
    if (qpair == -1) {
	bscan_syntax_err(scanner);
	return;
    }
    ++s;

    /* Loop until end_quote is found. 
     */
    do {
	/* loop until end_quote is found. */
	while (BASE_SCAN_CHECK_EOF(s) && *s != '\n' && *s != end_quote[qpair]) {
	    ++s;
	}

	/* check that no backslash character precedes the end_quote. */
	if (*s == end_quote[qpair]) {
	    if (*(s-1) == '\\') {
		char *q = s-2;
		char *r = s-2;

		while (r != scanner->begin && *r == '\\') {
		    --r;
		}
		/* break from main loop if we have odd number of backslashes */
		if (((unsigned)(q-r) & 0x01) == 1) {
		    break;
		}
		++s;
	    } else {
		/* end_quote is not preceeded by backslash. break now. */
		break;
	    }
	} else {
	    /* loop ended by non-end_quote character. break now. */
	    break;
	}
    } while (1);

    /* Check and eat the end quote. */
    if (*s != end_quote[qpair]) {
	bscan_syntax_err(scanner);
	return;
    }
    ++s;

    bstrset3(out, scanner->curptr, s);

    scanner->curptr = s;

    if (BASE_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	bscan_skip_whitespace(scanner);
    }
}


void bscan_get_n( bscanner *scanner,
			    unsigned N, bstr_t *out)
{
    if (scanner->curptr + N > scanner->end) {
	bscan_syntax_err(scanner);
	return;
    }

    bstrset(out, scanner->curptr, N);
    
    scanner->curptr += N;

    if (BASE_SCAN_IS_PROBABLY_SPACE(*scanner->curptr) && scanner->skip_ws) {
	bscan_skip_whitespace(scanner);
    }
}


int bscan_get_char( bscanner *scanner )
{
    int chr = *scanner->curptr;

    if (!chr) {
	bscan_syntax_err(scanner);
	return 0;
    }

    ++scanner->curptr;

    if (BASE_SCAN_IS_PROBABLY_SPACE(*scanner->curptr) && scanner->skip_ws) {
	bscan_skip_whitespace(scanner);
    }
    return chr;
}


void bscan_get_newline( bscanner *scanner )
{
    if (!BASE_SCAN_IS_NEWLINE(*scanner->curptr)) {
	bscan_syntax_err(scanner);
	return;
    }

    if (*scanner->curptr == '\r') {
	++scanner->curptr;
    }
    if (*scanner->curptr == '\n') {
	++scanner->curptr;
    }

    ++scanner->line;
    scanner->start_line = scanner->curptr;

    /**
     * This probably is a bug, see PROTOS test #2480.
     * This would cause scanner to incorrectly eat two new lines, e.g.
     * when parsing:
     *   
     *	Content-Length: 120\r\n
     *	\r\n
     *	<space><space><space>...
     *
     * When bscan_get_newline() is called to parse the first newline
     * in the Content-Length header, it will eat the second newline
     * too because it thinks that it's a header continuation.
     *
     * if (BASE_SCAN_IS_PROBABLY_SPACE(*scanner->curptr) && scanner->skip_ws) {
     *    bscan_skip_whitespace(scanner);
     * }
     */
}


void bscan_get_until( bscanner *scanner,
				const bcis_t *spec, bstr_t *out)
{
    register char *s = scanner->curptr;

    if (s >= scanner->end) {
	bscan_syntax_err(scanner);
	return;
    }

    while (BASE_SCAN_CHECK_EOF(s) && !bcis_match(spec, *s)) {
	++s;
    }

    bstrset3(out, scanner->curptr, s);

    scanner->curptr = s;

    if (BASE_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	bscan_skip_whitespace(scanner);
    }
}


void bscan_get_until_ch( bscanner *scanner, 
				   int until_char, bstr_t *out)
{
    register char *s = scanner->curptr;

    if (s >= scanner->end) {
	bscan_syntax_err(scanner);
	return;
    }

    while (BASE_SCAN_CHECK_EOF(s) && *s != until_char) {
	++s;
    }

    bstrset3(out, scanner->curptr, s);

    scanner->curptr = s;

    if (BASE_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	bscan_skip_whitespace(scanner);
    }
}


void bscan_get_until_chr( bscanner *scanner,
				     const char *until_spec, bstr_t *out)
{
    register char *s = scanner->curptr;
    bsize_t speclen;

    if (s >= scanner->end) {
	bscan_syntax_err(scanner);
	return;
    }

    speclen = strlen(until_spec);
    while (BASE_SCAN_CHECK_EOF(s) && !memchr(until_spec, *s, speclen)) {
	++s;
    }

    bstrset3(out, scanner->curptr, s);

    scanner->curptr = s;

    if (BASE_SCAN_IS_PROBABLY_SPACE(*s) && scanner->skip_ws) {
	bscan_skip_whitespace(scanner);
    }
}

void bscan_advance_n( bscanner *scanner,
				 unsigned N, bbool_t skip_ws)
{
    if (scanner->curptr + N > scanner->end) {
	bscan_syntax_err(scanner);
	return;
    }

    scanner->curptr += N;

    if (BASE_SCAN_IS_PROBABLY_SPACE(*scanner->curptr) && skip_ws) {
	bscan_skip_whitespace(scanner);
    }
}


int bscan_strcmp( bscanner *scanner, const char *s, int len)
{
    if (scanner->curptr + len > scanner->end) {
	bscan_syntax_err(scanner);
	return -1;
    }
    return strncmp(scanner->curptr, s, len);
}


int bscan_stricmp( bscanner *scanner, const char *s, int len)
{
    if (scanner->curptr + len > scanner->end) {
	bscan_syntax_err(scanner);
	return -1;
    }
    return bansi_strnicmp(scanner->curptr, s, len);
}

int bscan_stricmp_alnum( bscanner *scanner, const char *s, 
				   int len)
{
    if (scanner->curptr + len > scanner->end) {
	bscan_syntax_err(scanner);
	return -1;
    }
    return strnicmp_alnum(scanner->curptr, s, len);
}

void bscan_save_state( const bscanner *scanner, 
				 bscan_state *state)
{
    state->curptr = scanner->curptr;
    state->line = scanner->line;
    state->start_line = scanner->start_line;
}


void bscan_restore_state( bscanner *scanner, 
				    bscan_state *state)
{
    scanner->curptr = state->curptr;
    scanner->line = state->line;
    scanner->start_line = state->start_line;
}


