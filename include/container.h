/*
 *
 * Part:        container.h include file.
 *
 */

#ifndef __CONTAINER_H__
#define __CONTAINER_H__

/* Copy from linux kernel 2.6 source (kernel.h, stddef.h) */

#ifndef offsetof
# define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/*
 * container_of - cast a member of a structure out to the containing structure
 *
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#ifndef container_of
# define container_of(ptr, type, member) ({	\
	 typeof( ((type *)0)->member ) *__mptr = (ptr);  \
	 (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#ifndef container_of_const
# define container_of_const(ptr, type, member) ({	\
	 const typeof( ((type *)0)->member ) *__mptr = (ptr);  \
	 (type *)( (const char *)__mptr - offsetof(type,member) );})
#endif

#endif
