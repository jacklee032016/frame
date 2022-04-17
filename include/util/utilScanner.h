/* 
 *
 */
#ifndef __UTIL_SCANNER_H__
#define __UTIL_SCANNER_H__

/**
 * @brief Text Scanning.
 */

#include <utilTypes.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_SCAN Fast Text Scanning
 * @brief Text scanning utility.
 *
 * This module describes a fast text scanning functions.
 *
 * @{
 */
#if defined(BASE_SCANNER_USE_BITWISE) && BASE_SCANNER_USE_BITWISE != 0
#  include <utilScannerCisBitwise.h>
#else
#  include <utilScannerCisUint.h>
#endif

/**
 * Initialize scanner input specification buffer.
 *
 * @param cs_buf    The scanner character specification.
 */
void bcis_buf_init(bcis_buf_t *cs_buf);

/**
 * Create a new input specification.
 *
 * @param cs_buf    Specification buffer.
 * @param cis       Character input specification to be initialized.
 *
 * @return          BASE_SUCCESS if new specification has been successfully
 *                  created, or BASE_ETOOMANY if there are already too many
 *                  specifications in the buffer.
 */
bstatus_t bcis_init(bcis_buf_t *cs_buf, bcis_t *cis);

/**
 * Create a new input specification based on an existing specification.
 *
 * @param new_cis   The new specification to be initialized.
 * @param existing  The existing specification, from which the input
 *                  bitmask will be copied to the new specification.
 *
 * @return          BASE_SUCCESS if new specification has been successfully
 *                  created, or BASE_ETOOMANY if there are already too many
 *                  specifications in the buffer.
 */
bstatus_t bcis_dup(bcis_t *new_cis, bcis_t *existing);

/**
 * Add the characters in the specified range '[cstart, cend)' to the 
 * specification (the last character itself ('cend') is not added).
 *
 * @param cis       The scanner character specification.
 * @param cstart    The first character in the range.
 * @param cend      The next character after the last character in the range.
 */
void bcis_add_range( bcis_t *cis, int cstart, int cend);

/**
 * Add alphabetic characters to the specification.
 *
 * @param cis       The scanner character specification.
 */
void bcis_add_alpha( bcis_t *cis);

/**
 * Add numeric characters to the specification.
 *
 * @param cis       The scanner character specification.
 */
void bcis_add_num( bcis_t *cis);

/**
 * Add the characters in the string to the specification.
 *
 * @param cis       The scanner character specification.
 * @param str       The string.
 */
void bcis_add_str( bcis_t *cis, const char *str);

/**
 * Add specification from another specification.
 *
 * @param cis	    The specification is to be set.
 * @param rhs	    The specification to be copied.
 */
void bcis_add_cis( bcis_t *cis, const bcis_t *rhs);

/**
 * Delete characters in the specified range from the specification.
 *
 * @param cis       The scanner character specification.
 * @param cstart    The first character in the range.
 * @param cend      The next character after the last character in the range.
 */
void bcis_del_range( bcis_t *cis, int cstart, int cend);

/**
 * Delete characters in the specified string from the specification.
 *
 * @param cis       The scanner character specification.
 * @param str       The string.
 */
void bcis_del_str( bcis_t *cis, const char *str);

/**
 * Invert specification.
 *
 * @param cis       The scanner character specification.
 */
void bcis_invert( bcis_t *cis );

/**
 * Check whether the specified character belongs to the specification.
 *
 * @param cis       The scanner character specification.
 * @param c         The character to check for matching.
 *
 * @return	    Non-zero if match (not necessarily one).
 */
BASE_INLINE_SPECIFIER int bcis_match( const bcis_t *cis, buint8_t c )
{
    return BASE_CIS_ISSET(cis, c);
}


/**
 * Flags for scanner.
 */
enum
{
    /** This flags specifies that the scanner should automatically skip
	whitespaces 
     */
    BASE_SCAN_AUTOSKIP_WS = 1,

    /** This flags specifies that the scanner should automatically skip
        SIP header continuation. This flag implies BASE_SCAN_AUTOSKIP_WS.
     */
    BASE_SCAN_AUTOSKIP_WS_HEADER = 3,

    /** Auto-skip new lines.
     */
    BASE_SCAN_AUTOSKIP_NEWLINE = 4
};


/* Forward decl. */
struct bscanner;


/**
 * The callback function type to be called by the scanner when it encounters
 * syntax error.
 *
 * @param scanner       The scanner instance that calls the callback .
 */
typedef void (*bsyn_err_func_ptr)(struct bscanner *scanner);


/**
 * The text scanner structure.
 */
typedef struct bscanner
{
    char *begin;        /**< Start of input buffer.	*/
    char *end;          /**< End of input buffer.	*/
    char *curptr;       /**< Current pointer.		*/
    int   line;         /**< Current line.		*/
    char *start_line;   /**< Where current line starts.	*/
    int   skip_ws;      /**< Skip whitespace flag.	*/
    bsyn_err_func_ptr callback;   /**< Syntax error callback. */
} bscanner;


/**
 * This structure can be used by application to store the state of the parser,
 * so that the scanner state can be rollback to this state when necessary.
 */
typedef struct bscan_state
{
    char *curptr;       /**< Current scanner's pointer. */
    int   line;         /**< Current line.		*/
    char *start_line;   /**< Start of current line.	*/
} bscan_state;


/**
 * Initialize the scanner.
 * Note that the input string buffer MUST be NULL terminated and have
 * length at least buflen+1 (buflen MUST NOT include the NULL terminator).
 *
 * @param scanner   The scanner to be initialized.
 * @param bufstart  The input buffer to scan, which must be NULL terminated.
 * @param buflen    The length of the input buffer, which normally is
 *		    strlen(bufstart), hence not counting the NULL terminator.
 * @param options   Zero, or combination of BASE_SCAN_AUTOSKIP_WS or
 *		    BASE_SCAN_AUTOSKIP_WS_HEADER
 * @param callback  Callback to be called when the scanner encounters syntax
 *		    error condition.
 */
void bscan_init( bscanner *scanner, char *bufstart, 
			    bsize_t buflen, 
			    unsigned options,
			    bsyn_err_func_ptr callback );


/** 
 * Call this function when application has finished using the scanner.
 *
 * @param scanner   The scanner.
 */
void bscan_fini( bscanner *scanner );


/** 
 * Determine whether the EOF condition for the scanner has been met.
 *
 * @param scanner   The scanner.
 *
 * @return Non-zero if scanner is EOF.
 */
BASE_INLINE_SPECIFIER int bscan_is_eof( const bscanner *scanner)
{
    return scanner->curptr >= scanner->end;
}


/** 
 * Peek strings in current position according to parameter spec, and return
 * the strings in parameter out. The current scanner position will not be
 * moved. If the scanner is already in EOF state, syntax error callback will
 * be called thrown.
 *
 * @param scanner   The scanner.
 * @param spec	    The spec to match input string.
 * @param out	    String to store the result.
 *
 * @return the character right after the peek-ed position or zero if there's
 *	   no more characters.
 */
int bscan_peek( bscanner *scanner,
			   const bcis_t *spec, bstr_t *out);


/** 
 * Peek len characters in current position, and return them in out parameter.
 * Note that whitespaces or newlines will be returned as it is, regardless
 * of BASE_SCAN_AUTOSKIP_WS settings. If the character left is less than len, 
 * syntax error callback will be called.
 *
 * @param scanner   The scanner.
 * @param len	    Length to peek.
 * @param out	    String to store the result.
 *
 * @return the character right after the peek-ed position or zero if there's
 *	   no more characters.
 */
int bscan_peek_n( bscanner *scanner,
			     bsize_t len, bstr_t *out);


/** 
 * Peek strings in current position until spec is matched, and return
 * the strings in parameter out. The current scanner position will not be
 * moved. If the scanner is already in EOF state, syntax error callback will
 * be called.
 *
 * @param scanner   The scanner.
 * @param spec	    The peeking will stop when the input match this spec.
 * @param out	    String to store the result.
 *
 * @return the character right after the peek-ed position.
 */
int bscan_peek_until( bscanner *scanner,
				 const bcis_t *spec, 
				 bstr_t *out);


/** 
 * Get characters from the buffer according to the spec, and return them
 * in out parameter. The scanner will attempt to get as many characters as
 * possible as long as the spec matches. If the first character doesn't
 * match the spec, or scanner is already in EOF when this function is called,
 * an exception will be thrown.
 *
 * @param scanner   The scanner.
 * @param spec	    The spec to match input string.
 * @param out	    String to store the result.
 */
void bscan_get( bscanner *scanner,
			   const bcis_t *spec, bstr_t *out);


/** 
 * Just like #bscan_get(), but additionally performs unescaping when
 * escaped ('%') character is found. The input spec MUST NOT contain the
 * specification for '%' characted.
 *
 * @param scanner   The scanner.
 * @param spec	    The spec to match input string.
 * @param out	    String to store the result.
 */
void bscan_get_unescape( bscanner *scanner,
				    const bcis_t *spec, bstr_t *out);


/** 
 * Get characters between quotes. If current input doesn't match begin_quote,
 * syntax error will be thrown. Note that the resulting string will contain
 * the enclosing quote.
 *
 * @param scanner	The scanner.
 * @param begin_quote	The character to begin the quote.
 * @param end_quote	The character to end the quote.
 * @param out		String to store the result.
 */
void bscan_get_quote( bscanner *scanner,
				 int begin_quote, int end_quote, 
				 bstr_t *out);

/** 
 * Get characters between quotes. If current input doesn't match begin_quote,
 * syntax error will be thrown. Note that the resulting string will contain
 * the enclosing quote.
 *
 * @param scanner	The scanner.
 * @param begin_quotes  The character array to begin the quotes. For example,
 *                      the two characters " and '.
 * @param end_quotes    The character array to end the quotes. The position
 *                      found in the begin_quotes array will be used to match
 *                      the end quotes. So if the begin_quotes was the array
 *                      of "'< the end_quotes should be "'>. If begin_array
 *                      matched the ' then the end_quotes will look for ' to
 *                      match at the end.
 * @param qsize         The size of the begin_quotes and end_quotes arrays.
 * @param out           String to store the result.
 */
void bscan_get_quotes(bscanner *scanner,
                                 const char *begin_quotes,
                                 const char *end_quotes, int qsize,
                                 bstr_t *out);


/**
 * Get N characters from the scanner.
 *
 * @param scanner   The scanner.
 * @param N	    Number of characters to get.
 * @param out	    String to store the result.
 */
void bscan_get_n( bscanner *scanner,
			     unsigned N, bstr_t *out);


/** 
 * Get one character from the scanner.
 *
 * @param scanner   The scanner.
 *
 * @return The character.
 */
int bscan_get_char( bscanner *scanner );


/** 
 * Get characters from the scanner and move the scanner position until the
 * current character matches the spec.
 *
 * @param scanner   The scanner.
 * @param spec	    Get until the input match this spec.
 * @param out	    String to store the result.
 */
void bscan_get_until( bscanner *scanner,
				 const bcis_t *spec, bstr_t *out);


/** 
 * Get characters from the scanner and move the scanner position until the
 * current character matches until_char.
 *
 * @param scanner	The scanner.
 * @param until_char    Get until the input match this character.
 * @param out		String to store the result.
 */
void bscan_get_until_ch( bscanner *scanner, 
				    int until_char, bstr_t *out);


/** 
 * Get characters from the scanner and move the scanner position until the
 * current character matches until_char.
 *
 * @param scanner	The scanner.
 * @param until_spec	Get until the input match any of these characters.
 * @param out		String to store the result.
 */
void bscan_get_until_chr( bscanner *scanner,
				     const char *until_spec, bstr_t *out);

/** 
 * Advance the scanner N characters, and skip whitespace
 * if necessary.
 *
 * @param scanner   The scanner.
 * @param N	    Number of characters to skip.
 * @param skip	    Flag to specify whether whitespace should be skipped
 *		    after skipping the characters.
 */
void bscan_advance_n( bscanner *scanner,
				 unsigned N, bbool_t skip);


/** 
 * Compare string in current position with the specified string.
 * 
 * @param scanner   The scanner.
 * @param s	    The string to compare with.
 * @param len	    Length of the string to compare.
 *
 * @return zero, <0, or >0 (just like strcmp()).
 */
int bscan_strcmp( bscanner *scanner, const char *s, int len);


/** 
 * Case-less string comparison of current position with the specified
 * string.
 *
 * @param scanner   The scanner.
 * @param s	    The string to compare with.
 * @param len	    Length of the string to compare with.
 *
 * @return zero, <0, or >0 (just like strcmp()).
 */
int bscan_stricmp( bscanner *scanner, const char *s, int len);

/**
 * Perform case insensitive string comparison of string in current position,
 * knowing that the string to compare only consists of alphanumeric
 * characters.
 *
 * Note that unlike #bscan_stricmp, this function can only return zero or
 * -1.
 *
 * @param scanner   The scanner.
 * @param s	    The string to compare with.
 * @param len	    Length of the string to compare with.
 *
 * @return	    zero if equal or -1.
 *
 * @see strnicmp_alnum, bstricmp_alnum
 */
int bscan_stricmp_alnum( bscanner *scanner, const char *s, 
				    int len);


/** 
 * Get a newline from the scanner. A newline is defined as '\\n', or '\\r', or
 * "\\r\\n". If current input is not newline, syntax error will be thrown.
 *
 * @param scanner   The scanner.
 */
void bscan_get_newline( bscanner *scanner );


/** 
 * Manually skip whitespaces according to flag that was specified when
 * the scanner was initialized.
 *
 * @param scanner   The scanner.
 */
void bscan_skip_whitespace( bscanner *scanner );


/**
 * Skip current line.
 *
 * @param scanner   The scanner.
 */
void bscan_skip_line( bscanner *scanner );

/** 
 * Save the full scanner state.
 *
 * @param scanner   The scanner.
 * @param state	    Variable to store scanner's state.
 */
void bscan_save_state( const bscanner *scanner, 
				  bscan_state *state);


/** 
 * Restore the full scanner state.
 * Note that this would not restore the string if application has modified
 * it. This will only restore the scanner scanning position.
 *
 * @param scanner   The scanner.
 * @param state	    State of the scanner.
 */
void bscan_restore_state( bscanner *scanner, 
				     bscan_state *state);

/**
 * Get current column position.
 *
 * @param scanner   The scanner.
 *
 * @return	    The column position.
 */
BASE_INLINE_SPECIFIER int bscan_get_col( const bscanner *scanner )
{
    return (int)(scanner->curptr - scanner->start_line);
}


BASE_END_DECL

#endif

