/* 
 *
 */
#include <baseFileAccess.h>
#include <baseAssert.h>
#include <baseErrno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>	/* rename() */
#include <errno.h>

/*
 * bfile_exists()
 */
bbool_t bfile_exists(const char *filename)
{
    struct stat buf;

    BASE_ASSERT_RETURN(filename, 0);

    if (stat(filename, &buf) != 0)
	return 0;

    return BASE_TRUE;
}


/*
 * bfile_size()
 */
boff_t bfile_size(const char *filename)
{
    struct stat buf;

    BASE_ASSERT_RETURN(filename, -1);

    if (stat(filename, &buf) != 0)
	return -1;

    return buf.st_size;
}


/*
 * bfile_delete()
 */
bstatus_t bfile_delete(const char *filename)
{
    BASE_ASSERT_RETURN(filename, BASE_EINVAL);

    if (unlink(filename)!=0) {
	return BASE_RETURN_OS_ERROR(errno);
    }
    return BASE_SUCCESS;
}


/*
 * bfile_move()
 */
bstatus_t bfile_move( const char *oldname, const char *newname)
{
    BASE_ASSERT_RETURN(oldname && newname, BASE_EINVAL);

    if (rename(oldname, newname) != 0) {
	return BASE_RETURN_OS_ERROR(errno);
    }
    return BASE_SUCCESS;
}


/*
 * bfile_getstat()
 */
bstatus_t bfile_getstat(const char *filename, 
				    bfile_stat *statbuf)
{
    struct stat buf;

    BASE_ASSERT_RETURN(filename && statbuf, BASE_EINVAL);

    if (stat(filename, &buf) != 0) {
	return BASE_RETURN_OS_ERROR(errno);
    }

    statbuf->size = buf.st_size;
    statbuf->ctime.sec = buf.st_ctime;
    statbuf->ctime.msec = 0;
    statbuf->mtime.sec = buf.st_mtime;
    statbuf->mtime.msec = 0;
    statbuf->atime.sec = buf.st_atime;
    statbuf->atime.msec = 0;

    return BASE_SUCCESS;
}

