
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>


void *xmalloc(size_t size)
{
	void *r;

	r = malloc(size);
	if (!r)
	{
		BASE_ERRORF("failed to allocate memory");
		exit(1);
	}

	return r;
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *r;

	r = calloc(nmemb, size);
	if (!r)
	{
		BASE_ERRORF("failed to allocate memory");
		exit(1);
	}

	return r;
}

void *xrealloc(void *ptr, size_t size)
{
	void *r;

	r = realloc(ptr, size);
	if (!r)
	{
		BASE_ERRORF("failed to allocate memory");
		exit(1);
	}

	return r;
}

char *xstrdup(const char *s)
{
	void *r;

	r = strdup(s);
	if (!r)
	{
		BASE_ERRORF("failed to allocate memory");
		exit(1);
	}

	return r;
}


char *string_newf(const char *format, ...)
{
	va_list ap;
	char *s;

	va_start(ap, format);
	if (vasprintf(&s, format, ap) < 0)
	{
		BASE_ERRORF("failed to allocate memory: %m");
		exit(1);
	}
	va_end(ap);

	return s;
}

void string_append(char **s, const char *str)
{
	size_t len1, len2;

	len1 = strlen(*s);
	len2 = strlen(str);
	*s = xrealloc(*s, len1 + len2 + 1);
	memcpy((*s) + len1, str, len2 + 1);
}

void string_appendf(char **s, const char *format, ...)
{
	va_list ap;
	size_t len1;
	int len2;
	char *s2;

	len1 = strlen(*s);

	va_start(ap, format);
	len2 = vasprintf(&s2, format, ap);
	va_end(ap);

	if (len2 < 0)
	{
		*s = NULL;
		return;
	}

	*s = xrealloc(*s, len1 + len2 + 1);
	memcpy((*s) + len1, s2, len2 + 1);
	free(s2);
}

void **parray_new(void)
{
	void **a;

	a = xmalloc(sizeof(*a));
	*a = NULL;

	return a;
}

void parray_append(void ***a, void *p)
{
	parray_extend(a, p, NULL);
}

void parray_extend(void ***a, ...)
{
	va_list ap;
	int ilen, len, alloced;
	void *p;

	for (len = 0; (*a)[len]; len++)
		;
	len++;

	va_start(ap, a);
	for (ilen = 0; va_arg(ap, void *); ilen++)
		;
	va_end(ap);

	/* Reallocate in exponentially increasing sizes. */
	for (alloced = 1; alloced < len; alloced <<= 1)
		;
	if (alloced < len + ilen) {
		while (alloced < len + ilen)
			alloced *= 2;
		*a = xrealloc(*a, alloced * sizeof **a);
	}

	va_start(ap, a);
	while ((p = va_arg(ap, void *)))
		(*a)[len++ - 1] = p;
	va_end(ap);
	(*a)[len - 1] = NULL;
}

