/* 
 *
 */
#ifndef __BASE_FIFOBUF_H__
#define __BASE_FIFOBUF_H__

#include <_baseTypes.h>

BASE_BEGIN_DECL

typedef struct _bfifobuf_t bfifobuf_t;
struct _bfifobuf_t
{
	char		*first, *last;
	char		*ubegin, *uend;
	int		full;
};

void	     bfifobuf_init (bfifobuf_t *fb, void *buffer, unsigned size);
unsigned    bfifobuf_max_size (bfifobuf_t *fb);
void*	     bfifobuf_alloc (bfifobuf_t *fb, unsigned size);
bstatus_t bfifobuf_unalloc (bfifobuf_t *fb, void *buf);
bstatus_t bfifobuf_free (bfifobuf_t *fb, void *buf);

BASE_END_DECL

#endif

