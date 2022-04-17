/*
 *
 */
#include <baseFileIo.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <stdio.h>
#include <errno.h>

bstatus_t bfile_open( bpool_t *pool,
                                  const char *pathname, 
                                  unsigned flags,
                                  bOsHandle_t *fd)
{
    char mode[8];
    char *p = mode;

    BASE_ASSERT_RETURN(pathname && fd, BASE_EINVAL);
    BASE_UNUSED_ARG(pool);

    if ((flags & BASE_O_APPEND) == BASE_O_APPEND) {
        if ((flags & BASE_O_WRONLY) == BASE_O_WRONLY) {
            *p++ = 'a';
            if ((flags & BASE_O_RDONLY) == BASE_O_RDONLY)
                *p++ = '+';
        } else {
            /* This is invalid.
             * Can not specify BASE_O_RDONLY with BASE_O_APPEND! 
             */
        }
    } else {
        if ((flags & BASE_O_RDONLY) == BASE_O_RDONLY) {
            *p++ = 'r';
            if ((flags & BASE_O_WRONLY) == BASE_O_WRONLY)
                *p++ = '+';
        } else {
            *p++ = 'w';
        }
    }

    if (p==mode)
        return BASE_EINVAL;

    *p++ = 'b';
    *p++ = '\0';

    *fd = fopen(pathname, mode);
    if (*fd == NULL)
        return BASE_RETURN_OS_ERROR(errno);
    
    return BASE_SUCCESS;
}

bstatus_t bfile_close(bOsHandle_t fd)
{
    BASE_ASSERT_RETURN(fd, BASE_EINVAL);
    if (fclose((FILE*)fd) != 0)
        return BASE_RETURN_OS_ERROR(errno);

    return BASE_SUCCESS;
}

bstatus_t bfile_write( bOsHandle_t fd,
                                   const void *data,
                                   bssize_t *size)
{
    size_t written;

    clearerr((FILE*)fd);
    written = fwrite(data, 1, *size, (FILE*)fd);
    if (ferror((FILE*)fd)) {
        *size = -1;
        return BASE_RETURN_OS_ERROR(errno);
    }

    *size = written;
    return BASE_SUCCESS;
}

bstatus_t bfile_read( bOsHandle_t fd,
                                  void *data,
                                  bssize_t *size)
{
    size_t bytes;

    clearerr((FILE*)fd);
    bytes = fread(data, 1, *size, (FILE*)fd);
    if (ferror((FILE*)fd)) {
        *size = -1;
        return BASE_RETURN_OS_ERROR(errno);
    }

    *size = bytes;
    return BASE_SUCCESS;
}

/*
bbool_t bfile_eof(bOsHandle_t fd, enum bfile_access access)
{
    BASE_UNUSED_ARG(access);
    return feof((FILE*)fd) ? BASE_TRUE : 0;
}
*/

bstatus_t bfile_setpos( bOsHandle_t fd,
                                    boff_t offset,
                                    enum bfile_seek_type whence)
{
    int mode;

    switch (whence) {
    case BASE_SEEK_SET:
        mode = SEEK_SET; break;
    case BASE_SEEK_CUR:
        mode = SEEK_CUR; break;
    case BASE_SEEK_END:
        mode = SEEK_END; break;
    default:
        bassert(!"Invalid whence in file_setpos");
        return BASE_EINVAL;
    }

    if (fseek((FILE*)fd, (long)offset, mode) != 0)
        return BASE_RETURN_OS_ERROR(errno);

    return BASE_SUCCESS;
}

bstatus_t bfile_getpos( bOsHandle_t fd,
                                    boff_t *pos)
{
    long offset;

    offset = ftell((FILE*)fd);
    if (offset == -1) {
        *pos = -1;
        return BASE_RETURN_OS_ERROR(errno);
    }

    *pos = offset;
    return BASE_SUCCESS;
}

bstatus_t bfile_flush(bOsHandle_t fd)
{
    int rc;

    rc = fflush((FILE*)fd);
    if (rc == EOF) {
	return BASE_RETURN_OS_ERROR(errno);
    }

    return BASE_SUCCESS;
}
