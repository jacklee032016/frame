/* 
 *
 */
#ifndef __UTIL_JSON_H__
#define __UTIL_JSON_H__


/**
 * @brief  JSON Implementation
 */

#include <_baseTypes.h>
#include <baseList.h>
#include <basePool.h>

BASE_BEGIN_DECL

/**
 * @defgroup BASE_JSON JSON Writer and Loader
 * @ingroup BASE_FILE_FMT
 * @{
 * This API implements JSON file format according to RFC 4627. It can be used
 * to parse, write, and manipulate JSON documents.
 */

/**
 * Type of JSON value.
 */
typedef enum bjson_val_type
{
    BASE_JSON_VAL_NULL,		/**< Null value (null)			*/
    BASE_JSON_VAL_BOOL,		/**< Boolean value (true, false)	*/
    BASE_JSON_VAL_NUMBER,		/**< Numeric (float or fixed point)	*/
    BASE_JSON_VAL_STRING,		/**< Literal string value.		*/
    BASE_JSON_VAL_ARRAY,		/**< Array				*/
    BASE_JSON_VAL_OBJ		/**< Object.				*/
} bjson_val_type;

/* Forward declaration for JSON element */
typedef struct bjson_elem bjson_elem;

/**
 * JSON list to store child elements.
 */
typedef struct bjson_list
{
    BASE_DECL_LIST_MEMBER(bjson_elem);
} bjson_list;

/**
 * This represents JSON element. A JSON element is basically a name/value
 * pair, where the name is a string and the value can be one of null, boolean
 * (true and false constants), number, string, array (containing zero or more
 * elements), or object. An object can be viewed as C struct, that is a
 * compound element containing other elements, each having name/value pair.
 */
struct bjson_elem
{
    BASE_DECL_LIST_MEMBER(bjson_elem);
    bstr_t		name;		/**< ELement name.		*/
    bjson_val_type	type;		/**< Element type.		*/
    union
    {
	bbool_t	is_true;	/**< Boolean value.		*/
	float		num;		/**< Number value.		*/
	bstr_t	str;		/**< String value.		*/
	bjson_list	children;	/**< Object and array children	*/
    } value;				/**< Element value.		*/
};

/**
 * Structure to be specified to bjson_parse() to be filled with additional
 * info when parsing failed.
 */
typedef struct bjson_err_info
{
    unsigned	line;		/**< Line location of the error		*/
    unsigned	col;		/**< Column location of the error	*/
    int		err_char;	/**< The offending character.		*/
} bjson_err_info;

/**
 * Type of function callback to write JSON document in bjson_writef().
 *
 * @param s		The string to be written to the document.
 * @param size		The length of the string
 * @param user_data	User data that was specified to bjson_writef()
 *
 * @return		If the callback returns non-BASE_SUCCESS, it will
 * 			stop the bjson_writef() function and this error
 * 			will be returned to caller.
 */
typedef bstatus_t (*bjson_writer)(const char *s,
				      unsigned size,
				      void *user_data);

/**
 * Initialize null element.
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 */
void bjson_elem_null(bjson_elem *el, bstr_t *name);

/**
 * Initialize boolean element with the specified value.
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 * @param val		The value.
 */
void bjson_elem_bool(bjson_elem *el, bstr_t *name,
                                bbool_t val);

/**
 * Initialize number element with the specified value.
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 * @param val		The value.
 */
void bjson_elem_number(bjson_elem *el, bstr_t *name,
                                  float val);

/**
 * Initialize string element with the specified value.
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 * @param val		The value.
 */
void bjson_elem_string(bjson_elem *el, bstr_t *name,
                                  bstr_t *val);

/**
 * Initialize element as an empty array
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 */
void bjson_elem_array(bjson_elem *el, bstr_t *name);

/**
 * Initialize element as an empty object
 *
 * @param el		The element.
 * @param name		Name to be given to the element, or NULL.
 */
void bjson_elem_obj(bjson_elem *el, bstr_t *name);

/**
 * Add an element to an object or array.
 *
 * @param el		The object or array element.
 * @param child		Element to be added to the object or array.
 */
void bjson_elem_add(bjson_elem *el, bjson_elem *child);

/**
 * Parse a JSON document in the buffer. The buffer MUST be NULL terminated,
 * or if not then it must have enough size to put the NULL character.
 *
 * @param pool		The pool to allocate memory for creating elements.
 * @param buffer	String buffer containing JSON document.
 * @param size		Size of the document.
 * @param err_info	Optional structure to be filled with info when
 * 			parsing failed.
 *
 * @return		The root element from the document.
 */
bjson_elem* bjson_parse(bpool_t *pool,
                                     char *buffer,
                                     unsigned *size,
                                     bjson_err_info *err_info);

/**
 * Write the specified element to the string buffer.
 *
 * @param elem		The element to be written.
 * @param buffer	Output buffer.
 * @param size		On input, it must be set to the size of the buffer.
 * 			Upon successful return, this will be set to
 * 			the length of the written string.
 *
 * @return		BASE_SUCCESS on success or the appropriate error.
 */
bstatus_t   bjson_write(const bjson_elem *elem,
                                     char *buffer, unsigned *size);

/**
 * Incrementally write the element to arbitrary medium using the specified
 * callback to write the document chunks.
 *
 * @param elem		The element to be written.
 * @param writer	Callback function which will be called to write
 * 			text chunks.
 * @param user_data	Arbitrary user data which will be given back when
 * 			calling the callback.
 *
 * @return		BASE_SUCCESS on success or the appropriate error.
 */
bstatus_t   bjson_writef(const bjson_elem *elem,
                                      bjson_writer writer,
                                      void *user_data);

BASE_END_DECL

#endif

