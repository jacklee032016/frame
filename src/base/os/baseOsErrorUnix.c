/* 
 *
 */
#include <baseErrno.h>
#include <baseString.h>
#include <errno.h>

bstatus_t bget_os_error(void)
{
	return BASE_STATUS_FROM_OS(errno);
}

void bset_os_error(bstatus_t code)
{
	errno = BASE_STATUS_TO_OS(code);
}

bstatus_t bget_netos_error(void)
{
	return BASE_STATUS_FROM_OS(errno);
}

void bset_netos_error(bstatus_t code)
{
	errno = BASE_STATUS_TO_OS(code);
}

/* 
 * Platform specific error message. This file is called by extStrError() in errno.c 
 */
int platform_strerror( bos_err_type os_errcode, char *buf, bsize_t bufsize)
{
	const char *syserr = strerror(os_errcode);
	bsize_t len = syserr ? strlen(syserr) : 0;

	if (len >= bufsize)
		len = bufsize - 1;
	
	if (len > 0)
		bmemcpy(buf, syserr, len);
	
	buf[len] = '\0';
	return len;
}

