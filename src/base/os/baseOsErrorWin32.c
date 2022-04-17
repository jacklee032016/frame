/* 
 *
 */
#include <baseErrno.h>
#include <baseAssert.h>
#include <compat/stdarg.h>
#include <baseUnicode.h>
#include <baseString.h>


#if defined(BASE_HAS_WINSOCK2_H) && BASE_HAS_WINSOCK2_H != 0
#  include <winsock2.h>
#elif defined(BASE_HAS_WINSOCK_H) && BASE_HAS_WINSOCK_H != 0
#  include <winsock.h>
#endif


/*
 * From Apache's APR:
 */
#if defined(BASE_HAS_ERROR_STRING) && (BASE_HAS_ERROR_STRING!=0)

static const struct {
    bos_err_type code;
    const char *msg;
}
gaErrorList[] =
{
	BASE_BUILD_ERR( WSAEINTR,           "Interrupted system call"),
	BASE_BUILD_ERR( WSAEBADF,           "Bad file number"),
	BASE_BUILD_ERR( WSAEACCES,          "Permission denied"),
	BASE_BUILD_ERR( WSAEFAULT,          "Bad address"),
	BASE_BUILD_ERR( WSAEINVAL,          "Invalid argument"),
	BASE_BUILD_ERR( WSAEMFILE,          "Too many open sockets"),
	BASE_BUILD_ERR( WSAEWOULDBLOCK,     "Operation would block"),
	BASE_BUILD_ERR( WSAEINPROGRESS,     "Operation now in progress"),
	BASE_BUILD_ERR( WSAEALREADY,        "Operation already in progress"),
	BASE_BUILD_ERR( WSAENOTSOCK,        "Socket operation on non-socket"),
	BASE_BUILD_ERR( WSAEDESTADDRREQ,    "Destination address required"),
	BASE_BUILD_ERR( WSAEMSGSIZE,        "Message too long"),
	BASE_BUILD_ERR( WSAEPROTOTYPE,      "Protocol wrong type for socket"),
	BASE_BUILD_ERR( WSAENOPROTOOPT,     "Bad protocol option"),
	BASE_BUILD_ERR( WSAEPROTONOSUPPORT, "Protocol not supported"),
	BASE_BUILD_ERR( WSAESOCKTNOSUPPORT, "Socket type not supported"),
	BASE_BUILD_ERR( WSAEOPNOTSUPP,      "Operation not supported on socket"),
	BASE_BUILD_ERR( WSAEPFNOSUPPORT,    "Protocol family not supported"),
	BASE_BUILD_ERR( WSAEAFNOSUPPORT,    "Address family not supported"),
	BASE_BUILD_ERR( WSAEADDRINUSE,      "Address already in use"),
	BASE_BUILD_ERR( WSAEADDRNOTAVAIL,   "Can't assign requested address"),
	BASE_BUILD_ERR( WSAENETDOWN,        "Network is down"),
	BASE_BUILD_ERR( WSAENETUNREACH,     "Network is unreachable"),
	BASE_BUILD_ERR( WSAENETRESET,       "Net connection reset"),
	BASE_BUILD_ERR( WSAECONNABORTED,    "Software caused connection abort"),
	BASE_BUILD_ERR( WSAECONNRESET,      "Connection reset by peer"),
	BASE_BUILD_ERR( WSAENOBUFS,         "No buffer space available"),
	BASE_BUILD_ERR( WSAEISCONN,         "Socket is already connected"),
	BASE_BUILD_ERR( WSAENOTCONN,        "Socket is not connected"),
	BASE_BUILD_ERR( WSAESHUTDOWN,       "Can't send after socket shutdown"),
	BASE_BUILD_ERR( WSAETOOMANYREFS,    "Too many references, can't splice"),
	BASE_BUILD_ERR( WSAETIMEDOUT,       "Connection timed out"),
	BASE_BUILD_ERR( WSAECONNREFUSED,    "Connection refused"),
	BASE_BUILD_ERR( WSAELOOP,           "Too many levels of symbolic links"),
	BASE_BUILD_ERR( WSAENAMETOOLONG,    "File name too long"),
	BASE_BUILD_ERR( WSAEHOSTDOWN,       "Host is down"),
	BASE_BUILD_ERR( WSAEHOSTUNREACH,    "No route to host"),
	BASE_BUILD_ERR( WSAENOTEMPTY,       "Directory not empty"),
	BASE_BUILD_ERR( WSAEPROCLIM,        "Too many processes"),
	BASE_BUILD_ERR( WSAEUSERS,          "Too many users"),
	BASE_BUILD_ERR( WSAEDQUOT,          "Disc quota exceeded"),
	BASE_BUILD_ERR( WSAESTALE,          "Stale NFS file handle"),
	BASE_BUILD_ERR( WSAEREMOTE,         "Too many levels of remote in path"),
	BASE_BUILD_ERR( WSASYSNOTREADY,     "Network system is unavailable"),
	BASE_BUILD_ERR( WSAVERNOTSUPPORTED, "Winsock version out of range"),
	BASE_BUILD_ERR( WSANOTINITIALISED,  "WSAStartup not yet called"),
	BASE_BUILD_ERR( WSAEDISCON,         "Graceful shutdown in progress"),
/*
#define WSAENOMORE              (WSABASEERR+102)
#define WSAECANCELLED           (WSABASEERR+103)
#define WSAEINVALIDPROCTABLE    (WSABASEERR+104)
#define WSAEINVALIDPROVIDER     (WSABASEERR+105)
#define WSAEPROVIDERFAILEDINIT  (WSABASEERR+106)
#define WSASYSCALLFAILURE       (WSABASEERR+107)
#define WSASERVICE_NOT_FOUND    (WSABASEERR+108)
#define WSATYPE_NOT_FOUND       (WSABASEERR+109)
#define WSA_E_NO_MORE           (WSABASEERR+110)
#define WSA_E_CANCELLED         (WSABASEERR+111)
#define WSAEREFUSED             (WSABASEERR+112)
 */
	BASE_BUILD_ERR( WSAHOST_NOT_FOUND,  "Host not found"),
/*
#define WSATRY_AGAIN            (WSABASEERR+1002)
#define WSANO_RECOVERY          (WSABASEERR+1003)
 */
	BASE_BUILD_ERR( WSANO_DATA,         "No host data of that type was found"),
	{0, NULL}
};

#endif	/* BASE_HAS_ERROR_STRING */



bstatus_t bget_os_error(void)
{
	return BASE_STATUS_FROM_OS(GetLastError());
}

void bset_os_error(bstatus_t code)
{
	SetLastError(BASE_STATUS_TO_OS(code));
}

bstatus_t bget_netos_error(void)
{
	return BASE_STATUS_FROM_OS(WSAGetLastError());
}

void bset_netos_error(bstatus_t code)
{
	WSASetLastError(BASE_STATUS_TO_OS(code));
}

/* 
 * platform_strerror()
 *
 * Platform specific error message. This file is called by extStrError() in errno.c 
 */
int platform_strerror( bos_err_type os_errcode, char *buf, bsize_t bufsize)
{
	bsize_t len = 0;
	BASE_DECL_UNICODE_TEMP_BUF(wbuf,128);

	bassert(buf != NULL);
	bassert(bufsize >= 0);

	/*
	* MUST NOT check stack here.
	* This function might be called from BASE_CHECK_STACK() itself!
	//BASE_CHECK_STACK();
	*/

	if (!len)
	{
#if defined(BASE_HAS_ERROR_STRING) && (BASE_HAS_ERROR_STRING!=0)
		int i;
		for (i = 0; gaErrorList[i].msg; ++i)
		{
			if (gaErrorList[i].code == os_errcode)
			{
				len = strlen(gaErrorList[i].msg);
				if ((bsize_t)len >= bufsize)
				{
					len = bufsize-1;
				}

				bmemcpy(buf, gaErrorList[i].msg, len);
				buf[len] = '\0';
				break;
			}
		}
#endif	/* BASE_HAS_ERROR_STRING */
	}


	if (!len)
	{
#if BASE_NATIVE_STRING_IS_UNICODE
		len = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_IGNORE_INSERTS, NULL, os_errcode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 	wbuf, sizeof(wbuf),	NULL);
		if (len)
		{
			bunicode_to_ansi(wbuf, len, buf, bufsize);
		}
#else
		len = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, 	os_errcode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 	buf,	(int)bufsize, NULL);
		buf[bufsize-1] = '\0';
#endif

		if (len)
		{/* Remove trailing newlines. */
			while (len && (buf[len-1] == '\n' || buf[len-1] == '\r'))
			{
				buf[len-1] = '\0';
				--len;
			}
		}
	}

	if (!len)
	{
		len = bansi_snprintf( buf, bufsize, "Win32 error code %u",  (unsigned)os_errcode);
		if (len < 0 || len >= (int)bufsize)
			len = bufsize-1;
		buf[len] = '\0';
	}

	return (int)len;
}

