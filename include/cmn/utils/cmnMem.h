/*
 *
 */

#ifndef __CMN_MEM_H__
#define __CMN_MEM_H__

#include "config.h"

/* system includes */
#include <stddef.h>

#ifdef _MEM_CHECK_
#include <sys/types.h>
#include <sys/stat.h>
#else
#include <stdlib.h>
#endif

/* Local defines */
#ifdef _MEM_CHECK_

#define MALLOC(n)		( cmnMemMalloc((n), (__FILE__), (__func__), (__LINE__)) )
		      
#define FREE(b)			( cmnMemFree((b), (__FILE__), (__func__), (__LINE__)), (b) = NULL )

#define FREE_ONLY(b)		( cmnMemFree((b), (__FILE__), (__func__), (__LINE__)))

#define REALLOC(b,n)		( cmnMemRealloc((b), (n), (__FILE__), (__func__), (__LINE__)) )

#define STRDUP(p)		(cmnMemStrdup((p), (__FILE__), (__func__), (__LINE__)) )

#define STRNDUP(p,n)		(cmnMemStrndup((p),(n), (__FILE__), (__func__), (__LINE__)) )



/* Memory debug prototypes defs */
void memcheck_log(const char *, const char *, const char *, const char *, int);
void *cmnMemMalloc(size_t, const char *, const char *, int);
//		__attribute__((alloc_size(1))) __attribute__((malloc));
char *cmnMemStrdup(const char *, const char *, const char *, int);
//		__attribute__((malloc)) __attribute__((nonnull (1)));
char *cmnMemStrndup(const char *, size_t, const char *, const char *, int);
//		__attribute__((malloc)) __attribute__((nonnull (1)));
void cmnMemFree(void *, const char *, const char *, int);
void *cmnMemRealloc(void *, size_t, const char *, const char *, int);
//		__attribute__((alloc_size(2)));

void cmnMemDumpAlloc(void);
void cmnMemInit(const char *, const char *);
void cmnMemSkipDump(void);
void enable_mem_log_termination(void);

void update_mem_check_log_perms(mode_t);
#else

extern void *zalloc(unsigned long size);

#define MALLOC(n)    (zalloc(n))
#define FREE(p)      	do{if(p){free(p); (p) = NULL;}}while(0)
#define FREE_ONLY(p) (free(p))
#define REALLOC(p,n) (realloc((p),(n)))
#define STRDUP(p)    (strdup(p))
#define STRNDUP(p,n) (strndup((p),(n)))

#endif

/* Common defines */
typedef union _ptr_hack
{
	void				*p;
	const void		*cp;
} ptr_hack_t;

#define FREE_CONST(ptr)				{ ptr_hack_t ptr_hack = { .cp = ptr }; FREE(ptr_hack.p); }
#define FREE_CONST_ONLY(ptr)		{ ptr_hack_t ptr_hack = { .cp = ptr }; FREE_ONLY(ptr_hack.p); }
#define REALLOC_CONST(ptr, n)		({ ptr_hack_t ptr_hack = { .cp = ptr }; REALLOC(ptr_hack.p, n); })

#define PMALLOC(p)					{ p = MALLOC(sizeof(*p)); }
#define FREE_PTR(p)					{ if (p) { FREE(p);} }
#define FREE_CONST_PTR(p)			{ if (p) { FREE_CONST(p);} }
#endif

