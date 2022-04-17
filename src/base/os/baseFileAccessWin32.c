/* 
 *
 */
#include <baseFileAccess.h>
#include <baseUnicode.h>
#include <baseAssert.h>
#include <baseErrno.h>
#include <baseString.h>
#include <baseOs.h>
#include <windows.h>
#include <time.h>

#if defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE!=0
    /* WinCE lacks READ_CONTROL so we must use GENERIC_READ */
#   define CONTROL_ACCESS   GENERIC_READ
#else
#   define CONTROL_ACCESS   READ_CONTROL
#endif

static bstatus_t get_file_size(HANDLE hFile, boff_t *size)
{
#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    FILE_COMPRESSION_INFO fileInfo;

    if (GetFileInformationByHandleEx(hFile, FileCompressionInfo, &fileInfo,
	sizeof(FILE_COMPRESSION_INFO)))
    {
	*size = fileInfo.CompressedFileSize.QuadPart;
    }
    else {
	*size = -1;
	return BASE_RETURN_OS_ERROR(GetLastError());
    }
#else
    DWORD sizeLo, sizeHi;

    sizeLo = GetFileSize(hFile, &sizeHi);
    if (sizeLo == INVALID_FILE_SIZE) {
	DWORD dwStatus = GetLastError();
	if (dwStatus != NO_ERROR) {
	    *size = -1;
	    return BASE_RETURN_OS_ERROR(dwStatus);
	}
    }
    *size = sizeHi;
    *size = ((*size) << 32) + sizeLo;
#endif
    return BASE_SUCCESS;
}

static HANDLE WINAPI create_file(LPCTSTR filename, DWORD desired_access,
    DWORD share_mode,
    LPSECURITY_ATTRIBUTES security_attributes,
    DWORD creation_disposition,
    DWORD flags_and_attributes,
    HANDLE template_file)
{
#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    BASE_UNUSED_ARG(security_attributes);
    BASE_UNUSED_ARG(flags_and_attributes);
    BASE_UNUSED_ARG(template_file);

    return CreateFile2(filename, desired_access, share_mode,
	creation_disposition, NULL);
#else
    return CreateFile(filename, desired_access, share_mode,
		      security_attributes, creation_disposition,
		      flags_and_attributes, template_file);
#endif
}

/*
 * bfile_exists()
 */
bbool_t bfile_exists(const char *filename)
{
    BASE_DECL_UNICODE_TEMP_BUF(wfilename,256)
    HANDLE hFile;

    BASE_ASSERT_RETURN(filename != NULL, 0);

    hFile = create_file(BASE_STRING_TO_NATIVE(filename,
					    wfilename, sizeof(wfilename)),
		        CONTROL_ACCESS, 
		        FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return 0;

    CloseHandle(hFile);
    return BASE_TRUE;
}


/*
 * bfile_size()
 */
boff_t bfile_size(const char *filename)
{
    BASE_DECL_UNICODE_TEMP_BUF(wfilename,256)
    HANDLE hFile;    
    boff_t size;

    BASE_ASSERT_RETURN(filename != NULL, -1);

    hFile = create_file(BASE_STRING_TO_NATIVE(filename, 
					    wfilename, sizeof(wfilename)),
		        CONTROL_ACCESS, 
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return -1;

    get_file_size(hFile, &size);

    CloseHandle(hFile);
    return size;
}


/*
 * bfile_delete()
 */
bstatus_t bfile_delete(const char *filename)
{
    BASE_DECL_UNICODE_TEMP_BUF(wfilename,256)

    BASE_ASSERT_RETURN(filename != NULL, BASE_EINVAL);

    if (DeleteFile(BASE_STRING_TO_NATIVE(filename,wfilename,sizeof(wfilename))) == FALSE)
        return BASE_RETURN_OS_ERROR(GetLastError());

    return BASE_SUCCESS;
}


/*
 * bfile_move()
 */
bstatus_t bfile_move( const char *oldname, const char *newname)
{
    BASE_DECL_UNICODE_TEMP_BUF(woldname,256)
    BASE_DECL_UNICODE_TEMP_BUF(wnewname,256)
    BOOL rc;

    BASE_ASSERT_RETURN(oldname!=NULL && newname!=NULL, BASE_EINVAL);

#if BASE_WIN32_WINNT >= 0x0400
    rc = MoveFileEx(BASE_STRING_TO_NATIVE(oldname,woldname,sizeof(woldname)), 
		    BASE_STRING_TO_NATIVE(newname,wnewname,sizeof(wnewname)), 
                    MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING);
#else
    rc = MoveFile(BASE_STRING_TO_NATIVE(oldname,woldname,sizeof(woldname)), 
		  BASE_STRING_TO_NATIVE(newname,wnewname,sizeof(wnewname)));
#endif

    if (!rc)
        return BASE_RETURN_OS_ERROR(GetLastError());

    return BASE_SUCCESS;
}


static bstatus_t file_time_to_time_val(const FILETIME *file_time,
                                         btime_val *time_val)
{
#if !defined(BASE_WIN32_WINPHONE8) || !BASE_WIN32_WINPHONE8
    FILETIME local_file_time;
#endif

    SYSTEMTIME localTime;
    bparsed_time pt;

#if !defined(BASE_WIN32_WINPHONE8) || !BASE_WIN32_WINPHONE8
    if (!FileTimeToLocalFileTime(file_time, &local_file_time))
	return BASE_RETURN_OS_ERROR(GetLastError());
#endif

    if (!FileTimeToSystemTime(file_time, &localTime))
        return BASE_RETURN_OS_ERROR(GetLastError());

    //if (!SystemTimeToTzSpecificLocalTime(NULL, &systemTime, &localTime))
    //    return BASE_RETURN_OS_ERROR(GetLastError());

    bbzero(&pt, sizeof(pt));
    pt.year = localTime.wYear;
    pt.mon = localTime.wMonth-1;
    pt.day = localTime.wDay;
    pt.wday = localTime.wDayOfWeek;

    pt.hour = localTime.wHour;
    pt.min = localTime.wMinute;
    pt.sec = localTime.wSecond;
    pt.msec = localTime.wMilliseconds;

    return btime_encode(&pt, time_val);
}

/*
 * bfile_getstat()
 */
bstatus_t bfile_getstat(const char *filename, bfile_stat *stat)
{
    BASE_DECL_UNICODE_TEMP_BUF(wfilename,256)
    HANDLE hFile;
    FILETIME creationTime, accessTime, writeTime;
#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    FILE_BASIC_INFO fileInfo;
#endif

    BASE_ASSERT_RETURN(filename!=NULL && stat!=NULL, BASE_EINVAL);

    hFile = create_file(BASE_STRING_TO_NATIVE(filename,
					    wfilename, sizeof(wfilename)), 
		        CONTROL_ACCESS, 
		        FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return BASE_RETURN_OS_ERROR(GetLastError());

    if (get_file_size(hFile, &stat->size) != BASE_SUCCESS) {
	CloseHandle(hFile);
	return BASE_RETURN_OS_ERROR(GetLastError());
    }

#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
    if (GetFileInformationByHandleEx(hFile, FileBasicInfo, &fileInfo,
	sizeof(FILE_BASIC_INFO)))
    {
	creationTime.dwLowDateTime = fileInfo.CreationTime.LowPart;
	creationTime.dwHighDateTime = fileInfo.CreationTime.HighPart;
	accessTime.dwLowDateTime = fileInfo.LastAccessTime.LowPart;
	accessTime.dwHighDateTime = fileInfo.LastAccessTime.HighPart;
	writeTime.dwLowDateTime = fileInfo.LastWriteTime.LowPart;
	writeTime.dwHighDateTime = fileInfo.LastWriteTime.HighPart;
    }
    else {
	CloseHandle(hFile);
	return BASE_RETURN_OS_ERROR(GetLastError());
    }
#else
    if (GetFileTime(hFile, &creationTime, &accessTime, &writeTime) == FALSE) {
	DWORD dwStatus = GetLastError();
	CloseHandle(hFile);
	return BASE_RETURN_OS_ERROR(dwStatus);
    }
#endif

    CloseHandle(hFile);

    if (file_time_to_time_val(&creationTime, &stat->ctime) != BASE_SUCCESS)
        return BASE_RETURN_OS_ERROR(GetLastError());

    file_time_to_time_val(&accessTime, &stat->atime);
    file_time_to_time_val(&writeTime, &stat->mtime);

    return BASE_SUCCESS;
}

