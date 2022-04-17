/* 
 *
 */
#include <baseOs.h>
#include <baseCtype.h>
#include <baseErrno.h>
#include <baseString.h>

/*
 * FYI these links contain useful infos about predefined macros across
 * platforms:
 *  - http://predef.sourceforge.net/preos.html
 */

#if defined(BASE_HAS_SYS_UTSNAME_H) && BASE_HAS_SYS_UTSNAME_H != 0
/* For uname() */
#   include <sys/utsname.h>
#   include <stdlib.h>
#   define BASE_HAS_UNAME		1
#endif

#if defined(BASE_HAS_LIMITS_H) && BASE_HAS_LIMITS_H != 0
/* Include <limits.h> to get <features.h> to get various glibc macros.
 * See http://predef.sourceforge.net/prelib.html
 */
#   include <limits.h>
#endif

#if defined(_MSC_VER)
/* For all Windows including mobile */
#   include <windows.h>
#endif

#if defined(BASE_DARWINOS) && BASE_DARWINOS != 0
#   include "TargetConditionals.h"
#endif

#ifndef BASE_SYS_INFO_BUFFER_SIZE
#   define BASE_SYS_INFO_BUFFER_SIZE	64
#endif


#if defined(BASE_DARWINOS) && BASE_DARWINOS != 0 && TARGET_OS_IPHONE
#   include <sys/types.h>
#   include <sys/sysctl.h>
    void biphone_os_get_sys_info(bsys_info *si, bstr_t *si_buffer);
#endif
    
#if defined(BASE_SYMBIAN) && BASE_SYMBIAN != 0
    BASE_BEGIN_DECL
    unsigned bsymbianos_get_model_info(char *buf, unsigned buf_size);
    unsigned bsymbianos_get_platform_info(char *buf, unsigned buf_size);
    void bsymbianos_get_sdk_info(bstr_t *name, buint32_t *ver);
    BASE_END_DECL
#endif


static char *_verInfo(buint32_t ver, char *buf)
{
    bsize_t len;

    if (ver == 0) {
	*buf = '\0';
	return buf;
    }

    sprintf(buf, "-%u.%u",
	    (ver & 0xFF000000) >> 24,
	    (ver & 0x00FF0000) >> 16);
    len = strlen(buf);

    if (ver & 0xFFFF) {
	sprintf(buf+len, ".%u", (ver & 0xFF00) >> 8);
	len = strlen(buf);

	if (ver & 0x00FF) {
	    sprintf(buf+len, ".%u", (ver & 0xFF));
	}
    }

    return buf;
}

static buint32_t _parseVersion(char *str)
{    
    int i, maxtok;
    bssize_t found_idx;
    buint32_t version = 0;
    bstr_t in_str = bstr(str);
    bstr_t token, delim;
    
    while (*str && !bisdigit(*str))
	str++;

    maxtok = 4;
    delim = bstr(".-");
    for (found_idx = bstrtok(&in_str, &delim, &token, 0), i=0; 
	 found_idx != in_str.slen && i < maxtok;
	 ++i, found_idx = bstrtok(&in_str, &delim, &token, 
	                            found_idx + token.slen))
    {
	int n;

	if (!bisdigit(*token.ptr))
	    break;
	
	n = atoi(token.ptr);
	version |= (n << ((3-i)*8));
    }
    
    return version;
}

const bsys_info* bget_sys_info(void)
{
	static char si_buffer[BASE_SYS_INFO_BUFFER_SIZE];
	static bsys_info si;
	static bbool_t si_initialized;
	bsize_t left = BASE_SYS_INFO_BUFFER_SIZE, len;

	if (si_initialized)
		return &si;

	si.machine.ptr = si.os_name.ptr = si.sdk_name.ptr = si.info.ptr = "";

#define ALLOC_CP_STR(str,field)	\
	do { \
	    len = bansi_strlen(str); \
	    if (len && left >= len+1) { \
		si.field.ptr = si_buffer + BASE_SYS_INFO_BUFFER_SIZE - left; \
		si.field.slen = len; \
		bmemcpy(si.field.ptr, str, len+1); \
		left -= (len+1); \
	    } \
	} while (0)

    /*
     * Machine and OS info.
     */
#if defined(BASE_HAS_UNAME) && BASE_HAS_UNAME
	#if defined(BASE_DARWINOS) && BASE_DARWINOS != 0 && TARGET_OS_IPHONE && \
	(!defined TARGET_IPHONE_SIMULATOR || TARGET_IPHONE_SIMULATOR == 0)
	{
		bstr_t buf = {si_buffer + BASE_SYS_INFO_BUFFER_SIZE - left, left};
		bstr_t machine = {"arm-", 4};
		bstr_t sdk_name = {"iOS-SDK", 7};
		size_t size = BASE_SYS_INFO_BUFFER_SIZE - machine.slen;
		char tmp[BASE_SYS_INFO_BUFFER_SIZE];
		int name[] = {CTL_HW,HW_MACHINE};

		biphone_os_get_sys_info(&si, &buf);
		left -= si.os_name.slen + 1;

		si.os_ver = _parseVersion(si.machine.ptr);

		bmemcpy(tmp, machine.ptr, machine.slen);
		sysctl(name, 2, tmp+machine.slen, &size, NULL, 0);
		ALLOC_CP_STR(tmp, machine);
		si.sdk_name = sdk_name;

		#ifdef BASE_SDK_NAME
		bmemcpy(tmp, BASE_SDK_NAME, bansi_strlen(BASE_SDK_NAME) + 1);
		si.sdk_ver = _parseVersion(tmp);
		#endif
	}
	#else    
	{
		struct utsname u;

		/* Successful uname() returns zero on Linux and positive value on OpenSolaris. */
		if (uname(&u) == -1)
			goto get_sdk_info;

		ALLOC_CP_STR(u.machine, machine);
		ALLOC_CP_STR(u.sysname, os_name);

		si.os_ver = _parseVersion(u.release);
	}
	#endif
#elif defined(_MSC_VER)
	{
	#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
		si.os_name = bstr("winphone");
    	#else
		OSVERSIONINFO ovi;
		ovi.dwOSVersionInfoSize = sizeof(ovi);

		if (GetVersionEx(&ovi) == FALSE)
			goto get_sdk_info;

		si.os_ver = (ovi.dwMajorVersion << 24) |(ovi.dwMinorVersion << 16);
		#if defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE
			si.os_name = bstr("wince");
		#else 
			#if defined(BASE_WIN64)
				si.os_name = bstr("win64");
			#else
				si.os_name = bstr("win32");
			#endif
		#endif
	#endif
	}

	{
		SYSTEM_INFO wsi;
	#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8
		GetNativeSystemInfo(&wsi);
	#else
		GetSystemInfo(&wsi);
	#endif
		switch (wsi.wProcessorArchitecture)
		{
		#if (defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE) || \
			(defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8)
			case PROCESSOR_ARCHITECTURE_ARM:
				si.machine = bstr("arm");
				break;

			case PROCESSOR_ARCHITECTURE_SHX:
				si.machine = bstr("shx");
				break;
		#else
			case PROCESSOR_ARCHITECTURE_AMD64:
				si.machine = bstr("x86_64");
				break;
			case PROCESSOR_ARCHITECTURE_IA64:
				si.machine = bstr("ia64");
				break;
			case PROCESSOR_ARCHITECTURE_INTEL:
				si.machine = bstr("i386");
				break;
		#endif	/* BASE_WIN32_WINCE */
		}

		#if defined(BASE_WIN32_WINPHONE8) && BASE_WIN32_WINPHONE8

		/* Avoid compile warning. */
		goto get_sdk_info;
		#endif
	}
#elif defined(BASE_SYMBIAN) && BASE_SYMBIAN != 0
	{
		bsymbianos_get_model_info(si_buffer, sizeof(si_buffer));
		ALLOC_CP_STR(si_buffer, machine);

		char *p = si_buffer + sizeof(si_buffer) - left;
		unsigned plen;
		plen = bsymbianos_get_platform_info(p, left);
		if (plen) {
			/* Output format will be "Series60vX.X" */
			si.os_name = bstr("S60");
			si.os_ver  = _parseVersion(p+9);
		}
		else
		{
			si.os_name = bstr("Unknown");
		}

		/* Avoid compile warning on Symbian. */
		goto get_sdk_info;
	}
#endif


get_sdk_info:

#if defined(__GLIBC__)
	si.sdk_ver = (__GLIBC__ << 24) |(__GLIBC_MINOR__ << 16);
	si.sdk_name = bstr("glibc");
#elif defined(__GNU_LIBRARY__)
	si.sdk_ver = (__GNU_LIBRARY__ << 24) | (__GNU_LIBRARY_MINOR__ << 16);
	si.sdk_name = bstr("libc");
#elif defined(__UCLIBC__)
	si.sdk_ver = (__UCLIBC_MAJOR__ << 24) |(__UCLIBC_MINOR__ << 16);
	si.sdk_name = bstr("uclibc");
#elif defined(_WIN32_WCE) && _WIN32_WCE
	/* Old window mobile declares _WIN32_WCE as decimal (e.g. 300, 420, etc.),
	* but then it was changed to use hex, e.g. 0x420, etc. See
	* http://social.msdn.microsoft.com/forums/en-US/vssmartdevicesnative/thread/8a97c59f-5a1c-4bc6-99e6-427f065ff439/
	*/
#if _WIN32_WCE <= 500
	si.sdk_ver = ( (_WIN32_WCE / 100) << 24) |
		( ((_WIN32_WCE % 100) / 10) << 16) |
		( (_WIN32_WCE % 10) << 8);
#else
	si.sdk_ver = ( ((_WIN32_WCE & 0xFF00) >> 8) << 24) |
		( ((_WIN32_WCE & 0x00F0) >> 4) << 16) |
		( ((_WIN32_WCE & 0x000F) >> 0) << 8);
#endif
	si.sdk_name = bstr("cesdk");
#elif defined(_MSC_VER)
	/* No SDK info is easily obtainable for Visual C, so lets just use
	* _MSC_VER. The _MSC_VER macro reports the major and minor versions
	* of the compiler. For example, 1310 for Microsoft Visual C++ .NET 2003.
	* 1310 represents version 13 and a 1.0 point release.
	* The Visual C++ 2005 compiler version is 1400.
	*/
	si.sdk_ver = ((_MSC_VER / 100) << 24) |
		(((_MSC_VER % 100) / 10) << 16) |
		((_MSC_VER % 10) << 8);
	si.sdk_name = bstr("msvc");
#elif defined(BASE_SYMBIAN) && BASE_SYMBIAN != 0
	bsymbianos_get_sdk_info(&si.sdk_name, &si.sdk_ver);
#endif

    /*
     * Build the info string.
     */
	{
	char tmp[BASE_SYS_INFO_BUFFER_SIZE];
		char os_ver[20], sdk_ver[20];
		int cnt;

		cnt = bansi_snprintf(tmp, sizeof(tmp),"%s%s%s%s%s%s%s",
			si.os_name.ptr, _verInfo(si.os_ver, os_ver), (si.machine.slen ? "/" : ""), si.machine.ptr, (si.sdk_name.slen ? "/" : ""),
			si.sdk_name.ptr,
			_verInfo(si.sdk_ver, sdk_ver));
		
		if (cnt > 0 && cnt < (int)sizeof(tmp))
		{
			ALLOC_CP_STR(tmp, info);
		}
	}

	si_initialized = BASE_TRUE;
	return &si;
}

