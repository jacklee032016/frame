/* 
 *
 */
#ifndef __BASE_SELECT_H__
#define __BASE_SELECT_H__

/**
 */

#include <_baseTypes.h>

BASE_BEGIN_DECL 

/**
 * @defgroup BASE_SOCK_SELECT Socket select() API.
 * @ingroup BASE_IO
 * @{
 * This module provides portable abstraction for \a select() like API.
 * The abstraction is needed so that it can utilize various event
 * dispatching mechanisms that are available across platforms.
 *
 * The API is very similar to normal \a select() usage. 
 *
 * \section bsock_select_examples_sec Examples
 *
 * For some examples on how to use the select API, please see:
 *
 *  - \ref page_baselib_select_test
 */

/**
 * Portable structure declarations for bfd_set.
 * The implementation of bsock_select() does not use this structure 
 * per-se, but instead it will use the native fd_set structure. However,
 * we must make sure that the size of bfd_set_t can accomodate the
 * native fd_set structure.
 */
typedef struct _bfd_set_t
{
    bsock_t data[BASE_IOQUEUE_MAX_HANDLES+ 4]; /**< Opaque buffer for fd_set */
} bfd_set_t;


/**
 * Initialize the descriptor set pointed to by fdsetp to the null set.
 *
 * @param fdsetp    The descriptor set.
 */
void BASE_FD_ZERO(bfd_set_t *fdsetp);


/**
 * This is an internal function, application shouldn't use this.
 * 
 * Get the number of descriptors in the set. This is defined in sock_select.c
 * This function will only return the number of sockets set from BASE_FD_SET
 * operation. When the set is modified by other means (such as by select()),
 * the count will not be reflected here.
 *
 * @param fdsetp    The descriptor set.
 *
 * @return          Number of descriptors in the set.
 */
bsize_t BASE_FD_COUNT(const bfd_set_t *fdsetp);


/**
 * Add the file descriptor fd to the set pointed to by fdsetp. 
 * If the file descriptor fd is already in this set, there shall be no effect
 * on the set, nor will an error be returned.
 *
 * @param fd	    The socket descriptor.
 * @param fdsetp    The descriptor set.
 */
void BASE_FD_SET(bsock_t fd, bfd_set_t *fdsetp);

/**
 * Remove the file descriptor fd from the set pointed to by fdsetp. 
 * If fd is not a member of this set, there shall be no effect on the set, 
 * nor will an error be returned.
 *
 * @param fd	    The socket descriptor.
 * @param fdsetp    The descriptor set.
 */
void BASE_FD_CLR(bsock_t fd, bfd_set_t *fdsetp);


/**
 * Evaluate to non-zero if the file descriptor fd is a member of the set 
 * pointed to by fdsetp, and shall evaluate to zero otherwise.
 *
 * @param fd	    The socket descriptor.
 * @param fdsetp    The descriptor set.
 *
 * @return	    Nonzero if fd is member of the descriptor set.
 */
bbool_t BASE_FD_ISSET(bsock_t fd, const bfd_set_t *fdsetp);


/**
 * This function wait for a number of file  descriptors to change status.
 * The behaviour is the same as select() function call which appear in
 * standard BSD socket libraries.
 *
 * @param n	    On Unices, this specifies the highest-numbered
 *		    descriptor in any of the three set, plus 1. On Windows,
 *		    the value is ignored.
 * @param readfds   Optional pointer to a set of sockets to be checked for 
 *		    readability.
 * @param writefds  Optional pointer to a set of sockets to be checked for 
 *		    writability.
 * @param exceptfds Optional pointer to a set of sockets to be checked for 
 *		    errors.
 * @param timeout   Maximum time for select to wait, or null for blocking 
 *		    operations.
 *
 * @return	    The total number of socket handles that are ready, or
 *		    zero if the time limit expired, or -1 if an error occurred.
 */
int bsock_select( int n, 
			     bfd_set_t *readfds, 
			     bfd_set_t *writefds,
			     bfd_set_t *exceptfds, 
			     const btime_val *timeout);

BASE_END_DECL

#endif

