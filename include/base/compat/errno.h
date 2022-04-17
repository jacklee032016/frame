/* 
 *
 */
#ifndef __BASE_COMPAT_ERRNO_H__
#define __BASE_COMPAT_ERRNO_H__

#if defined(BASE_WIN32) && BASE_WIN32 != 0 || \
    defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE != 0 || \
    defined(BASE_WIN64) && BASE_WIN64 != 0

typedef unsigned long bos_err_type;
#define bget_native_os_error()	    GetLastError()
#define bget_native_netos_error()	    WSAGetLastError()

#elif defined(BASE_HAS_ERRNO_VAR) && BASE_HAS_ERRNO_VAR!= 0

typedef int bos_err_type;
#define bget_native_os_error()	    (errno)
#define bget_native_netos_error()	    (errno)

#else

#error "Please define how to get errno for this platform here!"

#endif

#endif

