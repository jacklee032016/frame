/* 
 *
 */

#if	defined(_MSC_VER)
#pragma comment(lib, "Ws2_32.lib")
#endif

#include <baseSock.h>
#include <baseOs.h>
#include <baseAssert.h>
#include <baseString.h>
#include <compat/socket.h>
#include <baseAddrResolv.h>
#include <baseErrno.h>
#include <baseUnicode.h>

/*
 * Address families conversion.
 * The values here are indexed based on baddr_family.
 */
const buint16_t BASE_AF_UNSPEC	= AF_UNSPEC;
const buint16_t BASE_AF_UNIX	= AF_UNIX;
const buint16_t BASE_AF_INET	= AF_INET;
const buint16_t BASE_AF_INET6	= AF_INET6;
#ifdef AF_PACKET
const buint16_t BASE_AF_PACKET	= AF_PACKET;
#else
const buint16_t BASE_AF_PACKET	= 0xFFFF;
#endif
#ifdef AF_IRDA
const buint16_t BASE_AF_IRDA	= AF_IRDA;
#else
const buint16_t BASE_AF_IRDA	= 0xFFFF;
#endif

/*
 * Socket types conversion.
 * The values here are indexed based on bsock_type
 */
const buint16_t BASE_SOCK_STREAM= SOCK_STREAM;
const buint16_t BASE_SOCK_DGRAM	= SOCK_DGRAM;
const buint16_t BASE_SOCK_RAW	= SOCK_RAW;
const buint16_t BASE_SOCK_RDM	= SOCK_RDM;

/*
 * Socket level values.
 */
const buint16_t BASE_SOL_SOCKET	= SOL_SOCKET;
#ifdef SOL_IP
const buint16_t BASE_SOL_IP	= SOL_IP;
#elif (defined(BASE_WIN32) && BASE_WIN32) || (defined(BASE_WIN64) && BASE_WIN64) || \
      (defined (IPPROTO_IP))
const buint16_t BASE_SOL_IP	= IPPROTO_IP;
#else
const buint16_t BASE_SOL_IP	= 0;
#endif /* SOL_IP */

#if defined(SOL_TCP)
const buint16_t BASE_SOL_TCP	= SOL_TCP;
#elif defined(IPPROTO_TCP)
const buint16_t BASE_SOL_TCP	= IPPROTO_TCP;
#elif (defined(BASE_WIN32) && BASE_WIN32) || (defined(BASE_WIN64) && BASE_WIN64)
const buint16_t BASE_SOL_TCP	= IPPROTO_TCP;
#else
const buint16_t BASE_SOL_TCP	= 6;
#endif /* SOL_TCP */

#ifdef SOL_UDP
const buint16_t BASE_SOL_UDP	= SOL_UDP;
#elif defined(IPPROTO_UDP)
const buint16_t BASE_SOL_UDP	= IPPROTO_UDP;
#elif (defined(BASE_WIN32) && BASE_WIN32) || (defined(BASE_WIN64) && BASE_WIN64)
const buint16_t BASE_SOL_UDP	= IPPROTO_UDP;
#else
const buint16_t BASE_SOL_UDP	= 17;
#endif /* SOL_UDP */

#ifdef SOL_IPV6
const buint16_t BASE_SOL_IPV6	= SOL_IPV6;
#elif (defined(BASE_WIN32) && BASE_WIN32) || (defined(BASE_WIN64) && BASE_WIN64)
#   if defined(IPPROTO_IPV6) || (_WIN32_WINNT >= 0x0501)
	const buint16_t BASE_SOL_IPV6	= IPPROTO_IPV6;
#   else
	const buint16_t BASE_SOL_IPV6	= 41;
#   endif
#else
const buint16_t BASE_SOL_IPV6	= 41;
#endif /* SOL_IPV6 */

/* IP_TOS */
#ifdef IP_TOS
const buint16_t BASE_IP_TOS	= IP_TOS;
#else
const buint16_t BASE_IP_TOS	= 1;
#endif


/* TOS settings (declared in netinet/ip.h) */
#ifdef IPTOS_LOWDELAY
const buint16_t BASE_IPTOS_LOWDELAY	= IPTOS_LOWDELAY;
#else
const buint16_t BASE_IPTOS_LOWDELAY	= 0x10;
#endif
#ifdef IPTOS_THROUGHPUT
const buint16_t BASE_IPTOS_THROUGHPUT	= IPTOS_THROUGHPUT;
#else
const buint16_t BASE_IPTOS_THROUGHPUT	= 0x08;
#endif
#ifdef IPTOS_RELIABILITY
const buint16_t BASE_IPTOS_RELIABILITY	= IPTOS_RELIABILITY;
#else
const buint16_t BASE_IPTOS_RELIABILITY	= 0x04;
#endif
#ifdef IPTOS_MINCOST
const buint16_t BASE_IPTOS_MINCOST	= IPTOS_MINCOST;
#else
const buint16_t BASE_IPTOS_MINCOST	= 0x02;
#endif


/* IPV6_TCLASS */
#ifdef IPV6_TCLASS
const buint16_t BASE_IPV6_TCLASS = IPV6_TCLASS;
#else
const buint16_t BASE_IPV6_TCLASS = 0xFFFF;
#endif


/* optname values. */
const buint16_t BASE_SO_TYPE    = SO_TYPE;
const buint16_t BASE_SO_RCVBUF  = SO_RCVBUF;
const buint16_t BASE_SO_SNDBUF  = SO_SNDBUF;
/* J. L. */
const buint16_t BASE_TCP_NODELAY= TCP_NODELAY;
//const buint16_t BASE_TCP_NODELAY= 0;
const buint16_t BASE_SO_REUSEADDR= SO_REUSEADDR;
#ifdef SO_NOSIGPIPE
const buint16_t BASE_SO_NOSIGPIPE = SO_NOSIGPIPE;
#else
const buint16_t BASE_SO_NOSIGPIPE = 0xFFFF;
#endif
#if defined(SO_PRIORITY)
const buint16_t BASE_SO_PRIORITY = SO_PRIORITY;
#else
/* This is from Linux, YMMV */
const buint16_t BASE_SO_PRIORITY = 12;
#endif

const buint16_t	BASE_SO_RCVTIMEO	= SO_RCVTIMEO;
const buint16_t	BASE_SO_SNDTIMEO	= SO_SNDTIMEO;

const buint16_t	BASE_SO_BOARDCAST	= SO_BROADCAST;

/* Multicasting is not supported e.g. in PocketPC 2003 SDK */
#ifdef IP_MULTICAST_IF
const buint16_t BASE_IP_MULTICAST_IF    = IP_MULTICAST_IF;
const buint16_t BASE_IP_MULTICAST_TTL   = IP_MULTICAST_TTL;
const buint16_t BASE_IP_MULTICAST_LOOP  = IP_MULTICAST_LOOP;
const buint16_t BASE_IP_ADD_MEMBERSHIP  = IP_ADD_MEMBERSHIP;
const buint16_t BASE_IP_DROP_MEMBERSHIP = IP_DROP_MEMBERSHIP;
#else
const buint16_t BASE_IP_MULTICAST_IF    = 0xFFFF;
const buint16_t BASE_IP_MULTICAST_TTL   = 0xFFFF;
const buint16_t BASE_IP_MULTICAST_LOOP  = 0xFFFF;
const buint16_t BASE_IP_ADD_MEMBERSHIP  = 0xFFFF;
const buint16_t BASE_IP_DROP_MEMBERSHIP = 0xFFFF;
#endif

/* recv() and send() flags */
const int BASE_MSG_OOB		= MSG_OOB;
const int BASE_MSG_PEEK		= MSG_PEEK;
const int BASE_MSG_DONTROUTE	= MSG_DONTROUTE;


#if 0
static void CHECK_ADDR_LEN(const bsockaddr *addr, int len)
{
    bsockaddr *a = (bsockaddr*)addr;
    bassert((a->addr.sa_family==BASE_AF_INET && len==sizeof(bsockaddr_in)) ||
	      (a->addr.sa_family==BASE_AF_INET6 && len==sizeof(bsockaddr_in6)));

}
#else
#define CHECK_ADDR_LEN(addr,len)
#endif

/*
 * Convert 16-bit value from network byte order to host byte order.
 */
buint16_t bntohs(buint16_t netshort)
{
    return ntohs(netshort);
}

/*
 * Convert 16-bit value from host byte order to network byte order.
 */
buint16_t bhtons(buint16_t hostshort)
{
    return htons(hostshort);
}

/*
 * Convert 32-bit value from network byte order to host byte order.
 */
buint32_t bntohl(buint32_t netlong)
{
    return ntohl(netlong);
}

/*
 * Convert 32-bit value from host byte order to network byte order.
 */
buint32_t bhtonl(buint32_t hostlong)
{
    return htonl(hostlong);
}

/*
 * Convert an Internet host address given in network byte order
 * to string in standard numbers and dots notation.
 */
char* binet_ntoa(bin_addr inaddr)
{
#if 0
	return inet_ntoa(*(struct in_addr*)&inaddr);
#else
	struct in_addr addr;
	//addr.s_addr = inaddr.s_addr;
	bmemcpy(&addr, &inaddr, sizeof(addr));
	return inet_ntoa(addr);
#endif
}

/*
 * This function converts the Internet host address cp from the standard
 * numbers-and-dots notation into binary data and stores it in the structure
 * that inp points to. 
 */
int binet_aton(const bstr_t *cp, bin_addr *inp)
{
    char tempaddr[BASE_INET_ADDRSTRLEN];

    /* Initialize output with BASE_INADDR_NONE.
     * Some apps relies on this instead of the return value
     * (and anyway the return value is quite confusing!)
     */
    inp->s_addr = BASE_INADDR_NONE;

    /* Caution:
     *	this function might be called with cp->slen >= 16
     *  (i.e. when called with hostname to check if it's an IP addr).
     */
    BASE_ASSERT_RETURN(cp && cp->slen && inp, 0);
    if (cp->slen >= BASE_INET_ADDRSTRLEN) {
	return 0;
    }

    bmemcpy(tempaddr, cp->ptr, cp->slen);
    tempaddr[cp->slen] = '\0';

#if defined(BASE_SOCK_HAS_INET_ATON) && BASE_SOCK_HAS_INET_ATON != 0
    return inet_aton(tempaddr, (struct in_addr*)inp);
#else
    inp->s_addr = inet_addr(tempaddr);
    return inp->s_addr == BASE_INADDR_NONE ? 0 : 1;
#endif
}

/*
 * Convert text to IPv4/IPv6 address.
 */
bstatus_t binet_pton(int af, const bstr_t *src, void *dst)
{
    char tempaddr[BASE_INET6_ADDRSTRLEN];

    BASE_ASSERT_RETURN(af==BASE_AF_INET || af==BASE_AF_INET6, BASE_EAFNOTSUP);
    BASE_ASSERT_RETURN(src && src->slen && dst, BASE_EINVAL);

    /* Initialize output with BASE_IN_ADDR_NONE for IPv4 (to be 
     * compatible with binet_aton()
     */
    if (af==BASE_AF_INET) {
	((bin_addr*)dst)->s_addr = BASE_INADDR_NONE;
    }

    /* Caution:
     *	this function might be called with cp->slen >= 46
     *  (i.e. when called with hostname to check if it's an IP addr).
     */
    if (src->slen >= BASE_INET6_ADDRSTRLEN) {
	return BASE_ENAMETOOLONG;
    }

    bmemcpy(tempaddr, src->ptr, src->slen);
    tempaddr[src->slen] = '\0';

#if defined(BASE_SOCK_HAS_INET_PTON) && BASE_SOCK_HAS_INET_PTON != 0
    /*
     * Implementation using inet_pton()
     */
    if (inet_pton(af, tempaddr, dst) != 1) {
	bstatus_t status = bget_netos_error();
	if (status == BASE_SUCCESS)
	    status = BASE_EUNKNOWN;

	return status;
    }

    return BASE_SUCCESS;

#elif defined(BASE_WIN32) || defined(BASE_WIN64) || defined(BASE_WIN32_WINCE)
    /*
     * Implementation on Windows, using WSAStringToAddress().
     * Should also work on Unicode systems.
     */
    {
	BASE_DECL_UNICODE_TEMP_BUF(wtempaddr,BASE_INET6_ADDRSTRLEN)
	bsockaddr sock_addr;
	int addr_len = sizeof(sock_addr);
	int rc;

	sock_addr.addr.sa_family = (buint16_t)af;
	rc = WSAStringToAddress(
		BASE_STRING_TO_NATIVE(tempaddr,wtempaddr,sizeof(wtempaddr)), 
		af, NULL, (LPSOCKADDR)&sock_addr, &addr_len);
	if (rc != 0) {
	    /* If you get rc 130022 Invalid argument (WSAEINVAL) with IPv6,
	     * check that you have IPv6 enabled (install it in the network
	     * adapter).
	     */
	    bstatus_t status = bget_netos_error();
	    if (status == BASE_SUCCESS)
		status = BASE_EUNKNOWN;

	    return status;
	}

	if (sock_addr.addr.sa_family == BASE_AF_INET) {
	    bmemcpy(dst, &sock_addr.ipv4.sin_addr, 4);
	    return BASE_SUCCESS;
	} else if (sock_addr.addr.sa_family == BASE_AF_INET6) {
	    bmemcpy(dst, &sock_addr.ipv6.sin6_addr, 16);
	    return BASE_SUCCESS;
	} else {
	    bassert(!"Shouldn't happen");
	    return BASE_EBUG;
	}
    }
#elif !defined(BASE_HAS_IPV6) || BASE_HAS_IPV6==0
    /* IPv6 support is disabled, just return error without raising assertion */
    return BASE_EIPV6NOTSUP;
#else
    bassert(!"Not supported");
    return BASE_EIPV6NOTSUP;
#endif
}

/*
 * Convert IPv4/IPv6 address to text.
 */
bstatus_t binet_ntop(int af, const void *src,
				 char *dst, int size)

{
    BASE_ASSERT_RETURN(src && dst && size, BASE_EINVAL);

    *dst = '\0';

    BASE_ASSERT_RETURN(af==BASE_AF_INET || af==BASE_AF_INET6, BASE_EAFNOTSUP);

#if defined(BASE_SOCK_HAS_INET_NTOP) && BASE_SOCK_HAS_INET_NTOP != 0
    /*
     * Implementation using inet_ntop()
     */
    if (inet_ntop(af, src, dst, size) == NULL) {
	bstatus_t status = bget_netos_error();
	if (status == BASE_SUCCESS)
	    status = BASE_EUNKNOWN;

	return status;
    }

    return BASE_SUCCESS;

#elif defined(BASE_WIN32) || defined(BASE_WIN64) || defined(BASE_WIN32_WINCE)
    /*
     * Implementation on Windows, using WSAAddressToString().
     * Should also work on Unicode systems.
     */
    {
	BASE_DECL_UNICODE_TEMP_BUF(wtempaddr,BASE_INET6_ADDRSTRLEN)
	bsockaddr sock_addr;
	DWORD addr_len, addr_str_len;
	int rc;

	bbzero(&sock_addr, sizeof(sock_addr));
	sock_addr.addr.sa_family = (buint16_t)af;
	if (af == BASE_AF_INET) {
	    if (size < BASE_INET_ADDRSTRLEN)
		return BASE_ETOOSMALL;
	    bmemcpy(&sock_addr.ipv4.sin_addr, src, 4);
	    addr_len = sizeof(bsockaddr_in);
	    addr_str_len = BASE_INET_ADDRSTRLEN;
	} else if (af == BASE_AF_INET6) {
	    if (size < BASE_INET6_ADDRSTRLEN)
		return BASE_ETOOSMALL;
	    bmemcpy(&sock_addr.ipv6.sin6_addr, src, 16);
	    addr_len = sizeof(bsockaddr_in6);
	    addr_str_len = BASE_INET6_ADDRSTRLEN;
	} else {
	    bassert(!"Unsupported address family");
	    return BASE_EAFNOTSUP;
	}

#if BASE_NATIVE_STRING_IS_UNICODE
	rc = WSAAddressToString((LPSOCKADDR)&sock_addr, addr_len,
				NULL, wtempaddr, &addr_str_len);
	if (rc == 0) {
	    bunicode_to_ansi(wtempaddr, wcslen(wtempaddr), dst, size);
	}
#else
	rc = WSAAddressToString((LPSOCKADDR)&sock_addr, addr_len,
				NULL, dst, &addr_str_len);
#endif

	if (rc != 0) {
	    bstatus_t status = bget_netos_error();
	    if (status == BASE_SUCCESS)
		status = BASE_EUNKNOWN;

	    return status;
	}

	return BASE_SUCCESS;
    }

#elif !defined(BASE_HAS_IPV6) || BASE_HAS_IPV6==0
    /* IPv6 support is disabled, just return error without raising assertion */
    return BASE_EIPV6NOTSUP;
#else
    bassert(!"Not supported");
    return BASE_EIPV6NOTSUP;
#endif
}

/*
 * Get hostname.
 */
const bstr_t* bgethostname(void)
{
    static char buf[BASE_MAX_HOSTNAME];
    static bstr_t hostname;

    BASE_CHECK_STACK();

    if (hostname.ptr == NULL) {
	hostname.ptr = buf;
	if (gethostname(buf, sizeof(buf)) != 0) {
	    hostname.ptr[0] = '\0';
	    hostname.slen = 0;
	} else {
            hostname.slen = strlen(buf);
	}
    }
    return &hostname;
}

#if defined(BASE_WIN32) || defined(BASE_WIN64)
/*
 * Create new socket/endpoint for communication and returns a descriptor.
 */
bstatus_t bsock_socket(int af, 
				   int type, 
				   int proto,
				   bsock_t *sock)
{
    BASE_CHECK_STACK();

    /* Sanity checks. */
    BASE_ASSERT_RETURN(sock!=NULL, BASE_EINVAL);
    BASE_ASSERT_RETURN((SOCKET)BASE_INVALID_SOCKET==INVALID_SOCKET, 
                     (*sock=BASE_INVALID_SOCKET, BASE_EINVAL));

    *sock = WSASocket(af, type, proto, NULL, 0, WSA_FLAG_OVERLAPPED);

    if (*sock == BASE_INVALID_SOCKET) 
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    
#if BASE_SOCK_DISABLE_WSAECONNRESET && \
    (!defined(BASE_WIN32_WINCE) || BASE_WIN32_WINCE==0)

#ifndef SIO_UDP_CONNRESET
    #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
#endif

    /* Disable WSAECONNRESET for UDP.
     * See https://trac.extsip.org/repos/ticket/1197
     */
    if (type==BASE_SOCK_DGRAM) {
	DWORD dwBytesReturned = 0;
	BOOL bNewBehavior = FALSE;
	DWORD rc;

	rc = WSAIoctl(*sock, SIO_UDP_CONNRESET,
		      &bNewBehavior, sizeof(bNewBehavior),
		      NULL, 0, &dwBytesReturned,
		      NULL, NULL);

	if (rc==SOCKET_ERROR) {
	    // Ignored..
	}
    }
#endif

    return BASE_SUCCESS;
}

#else
/*
 * Create new socket/endpoint for communication and returns a descriptor.
 */
bstatus_t bsock_socket(int af, int type, int proto, bsock_t *sock)
{

    BASE_CHECK_STACK();

    /* Sanity checks. */
    BASE_ASSERT_RETURN(sock!=NULL, BASE_EINVAL);
    BASE_ASSERT_RETURN(BASE_INVALID_SOCKET==-1, 
                     (*sock=BASE_INVALID_SOCKET, BASE_EINVAL));
    
    *sock = socket(af, type, proto);
    if (*sock == BASE_INVALID_SOCKET)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else {
	bint32_t val = 1;
	if (type == bSOCK_STREAM()) {
	    bsock_setsockopt(*sock, bSOL_SOCKET(), bSO_NOSIGPIPE(),
			       &val, sizeof(val));
	}
#if defined(BASE_SOCK_HAS_IPV6_V6ONLY) && BASE_SOCK_HAS_IPV6_V6ONLY != 0
	if (af == BASE_AF_INET6) {
	    bsock_setsockopt(*sock, BASE_SOL_IPV6, IPV6_V6ONLY,
			       &val, sizeof(val));
	}
#endif
#if defined(BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT) && \
    BASE_IPHONE_OS_HAS_MULTITASKING_SUPPORT!=0
	if (type == bSOCK_DGRAM()) {
	    bsock_setsockopt(*sock, bSOL_SOCKET(), SO_NOSIGPIPE, 
			       &val, sizeof(val));
	}
#endif
	return BASE_SUCCESS;
    }
}
#endif

/*
 * Bind socket.
 */
bstatus_t bsock_bind( bsock_t sock, 
				  const bsockaddr_t *addr,
				  int len)
{
    BASE_CHECK_STACK();

    BASE_ASSERT_RETURN(addr && len >= (int)sizeof(struct sockaddr_in), BASE_EINVAL);

    CHECK_ADDR_LEN(addr, len);

    if (bind(sock, (struct sockaddr*)addr, len) != 0)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else
	return BASE_SUCCESS;
}


/*
 * Bind socket.
 */
bstatus_t bsock_bind_in( bsock_t sock, 
				     buint32_t addr32,
				     buint16_t port)
{
    bsockaddr_in addr;

    BASE_CHECK_STACK();

    BASE_SOCKADDR_SET_LEN(&addr, sizeof(bsockaddr_in));
    addr.sin_family = BASE_AF_INET;
    bbzero(addr.sin_zero, sizeof(addr.sin_zero));
    addr.sin_addr.s_addr = bhtonl(addr32);
    addr.sin_port = bhtons(port);

    return bsock_bind(sock, &addr, sizeof(bsockaddr_in));
}


/*
 * Close socket.
 */
bstatus_t bsock_close(bsock_t sock)
{
    int rc;

    BASE_CHECK_STACK();
#if defined(BASE_WIN32) && BASE_WIN32!=0 || \
    defined(BASE_WIN64) && BASE_WIN64 != 0 || \
    defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE!=0
    rc = closesocket(sock);
#else
    rc = close(sock);
#endif

    if (rc != 0)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else
	return BASE_SUCCESS;
}

/*
 * Get remote's name.
 */
bstatus_t bsock_getpeername( bsock_t sock,
					 bsockaddr_t *addr,
					 int *namelen)
{
    BASE_CHECK_STACK();
    if (getpeername(sock, (struct sockaddr*)addr, (socklen_t*)namelen) != 0)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else {
	BASE_SOCKADDR_RESET_LEN(addr);
	return BASE_SUCCESS;
    }
}

/*
 * Get socket name.
 */
bstatus_t bsock_getsockname( bsock_t sock,
					 bsockaddr_t *addr,
					 int *namelen)
{
    BASE_CHECK_STACK();
    if (getsockname(sock, (struct sockaddr*)addr, (socklen_t*)namelen) != 0)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else {
	BASE_SOCKADDR_RESET_LEN(addr);
	return BASE_SUCCESS;
    }
}

/*
 * Send data
 */
bstatus_t bsock_send(bsock_t sock,
				 const void *buf,
				 bssize_t *len,
				 unsigned flags)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(len, BASE_EINVAL);

#ifdef MSG_NOSIGNAL
    /* Suppress SIGPIPE. See https://trac.extsip.org/repos/ticket/1538 */
    flags |= MSG_NOSIGNAL;
#endif

    *len = send(sock, (const char*)buf, (int)(*len), flags);

    if (*len < 0)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else
	return BASE_SUCCESS;
}


/*
 * Send data.
 */
bstatus_t bsock_sendto(bsock_t sock,
				   const void *buf,
				   bssize_t *len,
				   unsigned flags,
				   const bsockaddr_t *to,
				   int tolen)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(len, BASE_EINVAL);
    
    CHECK_ADDR_LEN(to, tolen);

    *len = sendto(sock, (const char*)buf, (int)(*len), flags, 
		  (const struct sockaddr*)to, tolen);

    if (*len < 0) 
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else 
	return BASE_SUCCESS;
}

/*
 * Receive data.
 */
bstatus_t bsock_recv(bsock_t sock,
				 void *buf,
				 bssize_t *len,
				 unsigned flags)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(buf && len, BASE_EINVAL);

    *len = recv(sock, (char*)buf, (int)(*len), flags);

    if (*len < 0) 
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else
	return BASE_SUCCESS;
}

/*
 * Receive data.
 */
bstatus_t bsock_recvfrom(bsock_t sock,
				     void *buf,
				     bssize_t *len,
				     unsigned flags,
				     bsockaddr_t *from,
				     int *fromlen)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(buf && len, BASE_EINVAL);

    *len = recvfrom(sock, (char*)buf, (int)(*len), flags, 
		    (struct sockaddr*)from, (socklen_t*)fromlen);

    if (*len < 0) 
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else {
        if (from) {
            BASE_SOCKADDR_RESET_LEN(from);
        }
	return BASE_SUCCESS;
    }
}

/*
 * Get socket option.
 */
bstatus_t bsock_getsockopt( bsock_t sock,
					buint16_t level,
					buint16_t optname,
					void *optval,
					int *optlen)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(optval && optlen, BASE_EINVAL);

    if (getsockopt(sock, level, optname, (char*)optval, (socklen_t*)optlen)!=0)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else
	return BASE_SUCCESS;
}

/*
 * Set socket option.
 */
bstatus_t bsock_setsockopt( bsock_t sock,
					buint16_t level,
					buint16_t optname,
					const void *optval,
					int optlen)
{
    int status;
    BASE_CHECK_STACK();
    
#if (defined(BASE_WIN32) && BASE_WIN32) || (defined(BASE_SUNOS) && BASE_SUNOS)
    /* Some opt may still need int value (e.g:SO_EXCLUSIVEADDRUSE in win32). */
    status = setsockopt(sock, 
		     level, 
		     ((optname&0xff00)==0xff00)?(int)optname|0xffff0000:optname, 		     
		     (const char*)optval, optlen);
#else
    status = setsockopt(sock, level, optname, (const char*)optval, optlen);
#endif

    if (status != 0)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else
	return BASE_SUCCESS;
}

/*
 * Set socket option.
 */
bstatus_t bsock_setsockopt_params( bsock_t sockfd,
					       const bsockopt_params *params)
{
    unsigned int i = 0;
    bstatus_t retval = BASE_SUCCESS;
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(params, BASE_EINVAL);
    
    for (;i<params->cnt && i<BASE_MAX_SOCKOPT_PARAMS;++i) {
	bstatus_t status = bsock_setsockopt(sockfd, 
					(buint16_t)params->options[i].level,
					(buint16_t)params->options[i].optname,
					params->options[i].optval, 
					params->options[i].optlen);
	if (status != BASE_SUCCESS) {
	    retval = status;
	    BASE_PERROR(4,(THIS_FILE, status,
			 "Warning: error applying sock opt %d",
			 params->options[i].optname));
	}
    }

    return retval;
}

/*
 * Connect socket.
 */
bstatus_t bsock_connect( bsock_t sock,
				     const bsockaddr_t *addr,
				     int namelen)
{
    BASE_CHECK_STACK();
    if (connect(sock, (struct sockaddr*)addr, namelen/* sizeof(struct sockaddr) */) != 0)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else
	return BASE_SUCCESS;
}


/*
 * Shutdown socket.
 */
#if BASE_HAS_TCP
bstatus_t bsock_shutdown( bsock_t sock,
				      int how)
{
    BASE_CHECK_STACK();
    if (shutdown(sock, how) != 0)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else
	return BASE_SUCCESS;
}

/*
 * Start listening to incoming connections.
 */
bstatus_t bsock_listen( bsock_t sock,
				    int backlog)
{
    BASE_CHECK_STACK();
    if (listen(sock, backlog) != 0)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else
	return BASE_SUCCESS;
}

/*
 * Accept incoming connections
 */
bstatus_t bsock_accept( bsock_t serverfd,
				    bsock_t *newsock,
				    bsockaddr_t *addr,
				    int *addrlen)
{
    BASE_CHECK_STACK();
    BASE_ASSERT_RETURN(newsock != NULL, BASE_EINVAL);

#if defined(BASE_SOCKADDR_HAS_LEN) && BASE_SOCKADDR_HAS_LEN!=0
    if (addr) {
	BASE_SOCKADDR_SET_LEN(addr, *addrlen);
    }
#endif
    
    *newsock = accept(serverfd, (struct sockaddr*)addr, (socklen_t*)addrlen);
    if (*newsock==BASE_INVALID_SOCKET)
	return BASE_RETURN_OS_ERROR(bget_native_netos_error());
    else {
	
#if defined(BASE_SOCKADDR_HAS_LEN) && BASE_SOCKADDR_HAS_LEN!=0
	if (addr) {
	    BASE_SOCKADDR_RESET_LEN(addr);
	}
#endif
	    
	return BASE_SUCCESS;
    }
}
#endif	/* BASE_HAS_TCP */


