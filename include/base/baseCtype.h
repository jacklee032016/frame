/* 
 *
 */
#ifndef __BASE_CTYPE_H__
#define __BASE_CTYPE_H__

/**
 * @brief C type helper macros.
 */

#include <_baseTypes.h>
#include <compat/ctype.h>

BASE_BEGIN_DECL

/**
 * @defgroup bctype ctype - Character Type
 * @ingroup BASE_MISC
 * @{
 *
 * This module contains several inline functions/macros for testing or
 * manipulating character types. It is provided in  because 
 * must not depend to LIBC.
 */

/** 
 * Returns a non-zero value if either isalpha or isdigit is true for c.
 * @param c     The integer character to test.
 * @return      Non-zero value if either isalpha or isdigit is true for c.
 */
BASE_INLINE_SPECIFIER int bisalnum(unsigned char c) { return isalnum(c); }

/** 
 * Returns a non-zero value if c is a particular representation of an 
 * alphabetic character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of an 
 *              alphabetic character.
 */
BASE_INLINE_SPECIFIER int bisalpha(unsigned char c) { return isalpha(c); }

/** 
 * Returns a non-zero value if c is a particular representation of an 
 * ASCII character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              an ASCII character.
 */
BASE_INLINE_SPECIFIER int bisascii(unsigned char c) { return c<128; }

/** 
 * Returns a non-zero value if c is a particular representation of 
 * a decimal-digit character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a decimal-digit character.
 */
BASE_INLINE_SPECIFIER int bisdigit(unsigned char c) { return isdigit(c); }

/** 
 * Returns a non-zero value if c is a particular representation of 
 * a space character (0x09 - 0x0D or 0x20).
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a space character (0x09 - 0x0D or 0x20).
 */
BASE_INLINE_SPECIFIER int bisspace(unsigned char c) { return isspace(c); }

/** 
 * Returns a non-zero value if c is a particular representation of 
 * a lowercase character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a lowercase character.
 */
BASE_INLINE_SPECIFIER int bislower(unsigned char c) { return islower(c); }


/** 
 * Returns a non-zero value if c is a particular representation of 
 * a uppercase character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              a uppercase character.
 */
BASE_INLINE_SPECIFIER int bisupper(unsigned char c) { return isupper(c); }

/**
 * Returns a non-zero value if c is a either a space (' ') or horizontal
 * tab ('\\t') character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a either a space (' ') or horizontal
 *              tab ('\\t') character.
 */
BASE_INLINE_SPECIFIER int bisblank(unsigned char c) { return (c==' ' || c=='\t'); }

/**
 * Converts character to lowercase.
 * @param c     The integer character to convert.
 * @return      Lowercase character of c.
 */
BASE_INLINE_SPECIFIER int btolower(unsigned char c) { return tolower(c); }

/**
 * Converts character to uppercase.
 * @param c     The integer character to convert.
 * @return      Uppercase character of c.
 */
BASE_INLINE_SPECIFIER int btoupper(unsigned char c) { return toupper(c); }

/**
 * Returns a non-zero value if c is a particular representation of 
 * an hexadecimal digit character.
 * @param c     The integer character to test.
 * @return      Non-zero value if c is a particular representation of 
 *              an hexadecimal digit character.
 */
BASE_INLINE_SPECIFIER int bisxdigit(unsigned char c){ return isxdigit(c); }

/**
 * Array of hex digits, in lowerspace.
 */
/*extern char bhex_digits[];*/
#define bhex_digits	"0123456789abcdef"

/**
 * Convert a value to hex representation.
 * @param value	    Integral value to convert.
 * @param p	    Buffer to hold the hex representation, which must be
 *		    at least two bytes length.
 */
BASE_INLINE_SPECIFIER void bval_to_hex_digit(unsigned value, char *p)
{
    *p++ = bhex_digits[ (value & 0xF0) >> 4 ];
    *p   = bhex_digits[ (value & 0x0F) ];
}

/**
 * Convert hex digit c to integral value.
 * @param c	The hex digit character.
 * @return	The integral value between 0 and 15.
 */
BASE_INLINE_SPECIFIER unsigned bhex_digit_to_val(unsigned char c)
{
    if (c <= '9')
	return (c-'0') & 0x0F;
    else if (c <= 'F')
	return  (c-'A'+10) & 0x0F;
    else
	return (c-'a'+10) & 0x0F;
}

BASE_END_DECL

#endif

