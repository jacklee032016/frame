/* 
 *
 */
#include <baseFileIo.h>
#include <baseUnicode.h>
#include <baseErrno.h>
#include <baseAssert.h>
#include <baseString.h>

#include <windows.h>

#ifndef INVALID_SET_FILE_POINTER
#   define INVALID_SET_FILE_POINTER     ((DWORD)-1)
#endif

static bstatus_t set_file_pointer(bOsHandle_t fd,
    boff_t offset,
    boff_t* newPos,
    DWORD dwMoveMethod)
{
#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    LARGE_INTEGER liDistance, liNewPos;

    liDistance.QuadPart = offset;
    if (!SetFilePointerEx(fd, liDistance, &liNewPos, dwMoveMethod)) {
	return BASE_RETURN_OS_ERROR(GetLastError());
    }
    *newPos = liNewPos.QuadPart;
#else
    DWORD dwNewPos;
    LONG  hi32;

    hi32 = (LONG)(offset >> 32);

    dwNewPos = SetFilePointer(fd, (long)offset, &hi32, dwMoveMethod);
    if (dwNewPos == (DWORD)INVALID_SET_FILE_POINTER) {
	DWORD dwStatus = GetLastError();
	if (dwStatus != 0)
	    return BASE_RETURN_OS_ERROR(dwStatus);
	/* dwNewPos actually is not an error. */
    }
    *newPos = hi32;
    *newPos = (*newPos << 32) + dwNewPos;
#endif

    return BASE_SUCCESS;
}

/**
 * Check for end-of-file condition on the specified descriptor.
 *
 * @param fd            The file descriptor.
 * @param access        The desired access.
 *
 * @return              Non-zero if file is EOF.
 */
bbool_t bfile_eof(bOsHandle_t fd, 
                               enum bfile_access access);


bstatus_t bfile_open( bpool_t *pool,
                                  const char *pathname, 
                                  unsigned flags,
                                  bOsHandle_t *fd)
{
    BASE_DECL_UNICODE_TEMP_BUF(wpathname, 256)
    HANDLE hFile;
    DWORD dwDesiredAccess = 0;
    DWORD dwShareMode = 0;
    DWORD dwCreationDisposition = 0;
    DWORD dwFlagsAndAttributes = 0;

    BASE_UNUSED_ARG(pool);

    BASE_ASSERT_RETURN(pathname!=NULL, BASE_EINVAL);

    if ((flags & BASE_O_WRONLY) == BASE_O_WRONLY) {
        dwDesiredAccess |= GENERIC_WRITE;
        if ((flags & BASE_O_APPEND) == BASE_O_APPEND) {
#if !defined(BASE_WIN32_WINCE) || !BASE_WIN32_WINCE
	    /* FILE_APPEND_DATA is invalid on WM2003 and WM5, but it seems
	     * to be working on WM6. All are tested on emulator though.
	     * Removing this also seem to work (i.e. data is appended), so
	     * I guess this flag is "optional".
	     * See http://trac.extsip.org/repos/ticket/825
	     */
            dwDesiredAccess |= FILE_APPEND_DATA;
#endif
	    dwCreationDisposition |= OPEN_ALWAYS;
        } else {
            dwDesiredAccess &= ~(FILE_APPEND_DATA);
            dwCreationDisposition |= CREATE_ALWAYS;
        }
    }
    if ((flags & BASE_O_RDONLY) == BASE_O_RDONLY) {
        dwDesiredAccess |= GENERIC_READ;
        if (flags == BASE_O_RDONLY)
            dwCreationDisposition |= OPEN_EXISTING;
    }

    if (dwDesiredAccess == 0) {
        bassert(!"Invalid file open flags");
        return BASE_EINVAL;
    }

    dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
    
    dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8  
    hFile = CreateFile2(BASE_STRING_TO_NATIVE(pathname,
			wpathname, sizeof(wpathname)),
			dwDesiredAccess, dwShareMode, dwCreationDisposition,
			NULL);
#else
    hFile = CreateFile(BASE_STRING_TO_NATIVE(pathname,
		       wpathname, sizeof(wpathname)),
		       dwDesiredAccess, dwShareMode, NULL,
		       dwCreationDisposition, dwFlagsAndAttributes, NULL);
#endif

    if (hFile == INVALID_HANDLE_VALUE) {
	DWORD lastErr = GetLastError();	
        *fd = 0;
        return BASE_RETURN_OS_ERROR(lastErr);
    }

    if ((flags & BASE_O_APPEND) == BASE_O_APPEND) {
	bstatus_t status;

	status = bfile_setpos(hFile, 0, BASE_SEEK_END);
	if (status != BASE_SUCCESS) {
	    bfile_close(hFile);
	    return status;
	}
    }

    *fd = hFile;
    return BASE_SUCCESS;
}

bstatus_t bfile_close(bOsHandle_t fd)
{
    if (CloseHandle(fd)==0)
        return BASE_RETURN_OS_ERROR(GetLastError());
    return BASE_SUCCESS;
}

bstatus_t bfile_write( bOsHandle_t fd,
                                   const void *data,
                                   bssize_t *size)
{
    BOOL rc;
    DWORD bytesWritten;

    rc = WriteFile(fd, data, (DWORD)*size, &bytesWritten, NULL);
    if (!rc) {
        *size = -1;
        return BASE_RETURN_OS_ERROR(GetLastError());
    }

    *size = bytesWritten;
    return BASE_SUCCESS;
}

bstatus_t bfile_read( bOsHandle_t fd,
                                  void *data,
                                  bssize_t *size)
{
    BOOL rc;
    DWORD bytesRead;

    rc = ReadFile(fd, data, (DWORD)*size, &bytesRead, NULL);
    if (!rc) {
        *size = -1;
        return BASE_RETURN_OS_ERROR(GetLastError());
    }

    *size = bytesRead;
    return BASE_SUCCESS;
}

/*
bbool_t bfile_eof(bOsHandle_t fd, enum bfile_access access)
{
    BOOL rc;
    DWORD dummy = 0, bytes;
    DWORD dwStatus;

    if ((access & BASE_O_RDONLY) == BASE_O_RDONLY) {
        rc = ReadFile(fd, &dummy, 0, &bytes, NULL);
    } else if ((access & BASE_O_WRONLY) == BASE_O_WRONLY) {
        rc = WriteFile(fd, &dummy, 0, &bytes, NULL);
    } else {
        bassert(!"Invalid access");
        return BASE_TRUE;
    }

    dwStatus = GetLastError();
    if (dwStatus==ERROR_HANDLE_EOF)
        return BASE_TRUE;

    return 0;
}
*/

bstatus_t bfile_setpos( bOsHandle_t fd,
                                    boff_t offset,
                                    enum bfile_seek_type whence)
{
    DWORD dwMoveMethod;
    boff_t newPos;

    if (whence == BASE_SEEK_SET)
        dwMoveMethod = FILE_BEGIN;
    else if (whence == BASE_SEEK_CUR)
        dwMoveMethod = FILE_CURRENT;
    else if (whence == BASE_SEEK_END)
        dwMoveMethod = FILE_END;
    else {
        bassert(!"Invalid whence in file_setpos");
        return BASE_EINVAL;
    }

    if (set_file_pointer(fd, offset, &newPos, dwMoveMethod) != BASE_SUCCESS) {
	return BASE_RETURN_OS_ERROR(GetLastError());
    }

    return BASE_SUCCESS;
}

bstatus_t bfile_getpos( bOsHandle_t fd,
                                    boff_t *pos)
{
    if (set_file_pointer(fd, 0, pos, FILE_CURRENT) != BASE_SUCCESS) {
	return BASE_RETURN_OS_ERROR(GetLastError());
    }

    return BASE_SUCCESS;
}

bstatus_t bfile_flush(bOsHandle_t fd)
{
    BOOL rc;

    rc = FlushFileBuffers(fd);

    if (!rc) {
	DWORD dwStatus = GetLastError();
        if (dwStatus != 0)
            return BASE_RETURN_OS_ERROR(dwStatus);
    }

    return BASE_SUCCESS;
}
