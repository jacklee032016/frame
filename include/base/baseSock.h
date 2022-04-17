/* 
 *
 */
#ifndef __BASE_SOCK_H__
#define __BASE_SOCK_H__


#include <_baseTypes.h>

BASE_BEGIN_DECL 

/**
 * @defgroup BASE_SOCK Socket Abstraction
 * @ingroup BASE_IO
 * @{
 *
 * The  socket abstraction layer is a thin and very portable abstraction
 * for socket API. It provides API similar to BSD socket API. The abstraction
 * is needed because BSD socket API is not always available on all platforms,
 * therefore it wouldn't be possible to create a trully portable network
 * programs unless we provide such abstraction.
 *
 * Applications can use this API directly in their application, just
 * as they would when using traditional BSD socket API, provided they
 * call #libBaseInit() first.
 *
 * \section bsock_examples_sec Examples
 *
 * For some examples on how to use the socket API, please see:
 *
 *  - \ref page_baselib_sock_test
 *  - \ref page_baselib_select_test
 *  - \ref page_baselib_sock_perf_test
 */


/**
 * Supported address families. 
 * APPLICATION MUST USE THESE VALUES INSTEAD OF NORMAL AF_*, BECAUSE
 * THE LIBRARY WILL DO TRANSLATION TO THE NATIVE VALUE.
 */

/** Address family is unspecified. @see bAF_UNSPEC() */
extern const buint16_t						BASE_AF_UNSPEC;

/** Unix domain socket.	@see bAF_UNIX() */
extern const buint16_t				BASE_AF_UNIX;

/** POSIX name for AF_UNIX	*/
#define BASE_AF_LOCAL	 			BASE_AF_UNIX;

/** Internet IP protocol. @see bAF_INET() */
extern const buint16_t				BASE_AF_INET;

/** IP version 6. @see bAF_INET6() */
extern const buint16_t						BASE_AF_INET6;

/** Packet family. @see bAF_PACKET() */
extern const buint16_t BASE_AF_PACKET;

/** IRDA sockets. @see bAF_IRDA() */
extern const buint16_t BASE_AF_IRDA;

/*
 * Accessor functions for various address family constants. These
 * functions are provided because Symbian doesn't allow exporting
 * global variables from a DLL.
 */

#if defined(BASE_DLL)
    /** Get #BASE_AF_UNSPEC value */
    buint16_t bAF_UNSPEC(void);
    /** Get #BASE_AF_UNIX value. */
    buint16_t bAF_UNIX(void);
    /** Get #BASE_AF_INET value. */
    buint16_t bAF_INET(void);
    /** Get #BASE_AF_INET6 value. */
    buint16_t bAF_INET6(void);
    /** Get #BASE_AF_PACKET value. */
    buint16_t bAF_PACKET(void);
    /** Get #BASE_AF_IRDA value. */
    buint16_t bAF_IRDA(void);
#else
    /* When libBase is not built as DLL, these accessor functions are
     * simply a macro to get their constants
     */
    /** Get #BASE_AF_UNSPEC value */
#   define bAF_UNSPEC()   BASE_AF_UNSPEC
    /** Get #BASE_AF_UNIX value. */
#   define bAF_UNIX()	    BASE_AF_UNIX
    /** Get #BASE_AF_INET value. */
#   define bAF_INET()	    BASE_AF_INET
    /** Get #BASE_AF_INET6 value. */
#   define bAF_INET6()    BASE_AF_INET6
    /** Get #BASE_AF_PACKET value. */
#   define bAF_PACKET()   BASE_AF_PACKET
    /** Get #BASE_AF_IRDA value. */
#   define bAF_IRDA()	    BASE_AF_IRDA
#endif


/**
 * Supported types of sockets.
 * APPLICATION MUST USE THESE VALUES INSTEAD OF NORMAL SOCK_*, BECAUSE
 * THE LIBRARY WILL TRANSLATE THE VALUE TO THE NATIVE VALUE.
 */

/** Sequenced, reliable, connection-based byte streams.
 *  @see bSOCK_STREAM() */
extern const buint16_t BASE_SOCK_STREAM;

/** Connectionless, unreliable datagrams of fixed maximum lengths.
 *  @see bSOCK_DGRAM() */
extern const buint16_t BASE_SOCK_DGRAM;

/** Raw protocol interface. @see bSOCK_RAW() */
extern const buint16_t BASE_SOCK_RAW;

/** Reliably-delivered messages.  @see bSOCK_RDM() */
extern const buint16_t BASE_SOCK_RDM;


/*
 * Accessor functions for various constants. These functions are provided
 * because Symbian doesn't allow exporting global variables from a DLL.
 */

#if defined(BASE_DLL)
    /** Get #BASE_SOCK_STREAM constant */
    int bSOCK_STREAM(void);
    /** Get #BASE_SOCK_DGRAM constant */
    int bSOCK_DGRAM(void);
    /** Get #BASE_SOCK_RAW constant */
    int bSOCK_RAW(void);
    /** Get #BASE_SOCK_RDM constant */
    int bSOCK_RDM(void);
#else
    /** Get #BASE_SOCK_STREAM constant */
#   define bSOCK_STREAM() BASE_SOCK_STREAM
    /** Get #BASE_SOCK_DGRAM constant */
#   define bSOCK_DGRAM()  BASE_SOCK_DGRAM
    /** Get #BASE_SOCK_RAW constant */
#   define bSOCK_RAW()    BASE_SOCK_RAW
    /** Get #BASE_SOCK_RDM constant */
#   define bSOCK_RDM()    BASE_SOCK_RDM
#endif


/**
 * Socket level specified in #bsock_setsockopt() or #bsock_getsockopt().
 * APPLICATION MUST USE THESE VALUES INSTEAD OF NORMAL SOL_*, BECAUSE
 * THE LIBRARY WILL TRANSLATE THE VALUE TO THE NATIVE VALUE.
 */
/** Socket level. @see bSOL_SOCKET() */
extern const buint16_t						BASE_SOL_SOCKET;
/** IP level. @see bSOL_IP() */
extern	const buint16_t						BASE_SOL_IP;
/** TCP level. @see bSOL_TCP() */
extern	const buint16_t						BASE_SOL_TCP;
/** UDP level. @see bSOL_UDP() */
extern	const buint16_t						BASE_SOL_UDP;
/** IP version 6. @see bSOL_IPV6() */
extern	const buint16_t						BASE_SOL_IPV6;

/*
 * Accessor functions for various constants. These functions are provided
 * because Symbian doesn't allow exporting global variables from a DLL.
 */

#if defined(BASE_DLL)
    /** Get #BASE_SOL_SOCKET constant */
    buint16_t bSOL_SOCKET(void);
    /** Get #BASE_SOL_IP constant */
    buint16_t bSOL_IP(void);
    /** Get #BASE_SOL_TCP constant */
    buint16_t bSOL_TCP(void);
    /** Get #BASE_SOL_UDP constant */
    buint16_t bSOL_UDP(void);
    /** Get #BASE_SOL_IPV6 constant */
    buint16_t bSOL_IPV6(void);
#else
    /** Get #BASE_SOL_SOCKET constant */
#   define bSOL_SOCKET()  BASE_SOL_SOCKET
    /** Get #BASE_SOL_IP constant */
#   define bSOL_IP()	    BASE_SOL_IP
    /** Get #BASE_SOL_TCP constant */
#   define bSOL_TCP()	    BASE_SOL_TCP
    /** Get #BASE_SOL_UDP constant */
#   define bSOL_UDP()	    BASE_SOL_UDP
    /** Get #BASE_SOL_IPV6 constant */
#   define bSOL_IPV6()    BASE_SOL_IPV6
#endif


/* IP_TOS 
 *
 * Note:
 *  TOS CURRENTLY DOES NOT WORK IN Windows 2000 and above!
 *  See http://support.microsoft.com/kb/248611
 */
/** IP_TOS optname in setsockopt(). @see bIP_TOS() */
extern const buint16_t BASE_IP_TOS;

/*
 * IP TOS related constats.
 *
 * Note:
 *  TOS CURRENTLY DOES NOT WORK IN Windows 2000 and above!
 *  See http://support.microsoft.com/kb/248611
 */
/** Minimize delays. @see bIPTOS_LOWDELAY() */
extern const buint16_t BASE_IPTOS_LOWDELAY;

/** Optimize throughput. @see bIPTOS_THROUGHPUT() */
extern const buint16_t BASE_IPTOS_THROUGHPUT;

/** Optimize for reliability. @see bIPTOS_RELIABILITY() */
extern const buint16_t BASE_IPTOS_RELIABILITY;

/** "filler data" where slow transmission does't matter.
 *  @see bIPTOS_MINCOST() */
extern const buint16_t BASE_IPTOS_MINCOST;


#if defined(BASE_DLL)
    /** Get #BASE_IP_TOS constant */
    int bIP_TOS(void);

    /** Get #BASE_IPTOS_LOWDELAY constant */
    int bIPTOS_LOWDELAY(void);

    /** Get #BASE_IPTOS_THROUGHPUT constant */
    int bIPTOS_THROUGHPUT(void);

    /** Get #BASE_IPTOS_RELIABILITY constant */
    int bIPTOS_RELIABILITY(void);

    /** Get #BASE_IPTOS_MINCOST constant */
    int bIPTOS_MINCOST(void);
#else
    /** Get #BASE_IP_TOS constant */
#   define bIP_TOS()		BASE_IP_TOS

    /** Get #BASE_IPTOS_LOWDELAY constant */
#   define bIPTOS_LOWDELAY()	BASE_IP_TOS_LOWDELAY

    /** Get #BASE_IPTOS_THROUGHPUT constant */
#   define bIPTOS_THROUGHPUT() BASE_IP_TOS_THROUGHPUT

    /** Get #BASE_IPTOS_RELIABILITY constant */
#   define bIPTOS_RELIABILITY() BASE_IP_TOS_RELIABILITY

    /** Get #BASE_IPTOS_MINCOST constant */
#   define bIPTOS_MINCOST()	BASE_IP_TOS_MINCOST
#endif


/** IPV6_TCLASS optname in setsockopt(). @see bIPV6_TCLASS() */
extern const buint16_t BASE_IPV6_TCLASS;


#if defined(BASE_DLL)
    /** Get #BASE_IPV6_TCLASS constant */
    int bIPV6_TCLASS(void);
#else
    /** Get #BASE_IPV6_TCLASS constant */
#   define bIPV6_TCLASS()	BASE_IPV6_TCLASS
#endif


/**
 * Values to be specified as \c optname when calling #bsock_setsockopt() 
 * or #bsock_getsockopt().
 */

/** Socket type. @see bSO_TYPE() */
extern const buint16_t BASE_SO_TYPE;

/** Buffer size for receive. @see bSO_RCVBUF() */
extern const buint16_t BASE_SO_RCVBUF;

/** Buffer size for send. @see bSO_SNDBUF() */
extern const buint16_t BASE_SO_SNDBUF;

/** Disables the Nagle algorithm for send coalescing. @see bTCP_NODELAY */
extern const buint16_t BASE_TCP_NODELAY;

/** Allows the socket to be bound to an address that is already in use.
 *  @see bSO_REUSEADDR */
extern const buint16_t BASE_SO_REUSEADDR;

/** Do not generate SIGPIPE. @see bSO_NOSIGPIPE */
extern const buint16_t BASE_SO_NOSIGPIPE;

/** Set the protocol-defined priority for all packets to be sent on socket.
 */
extern const buint16_t BASE_SO_PRIORITY;

extern	const buint16_t	BASE_SO_RCVTIMEO;	/* 04.02, 2020. JL */
extern	const buint16_t	BASE_SO_SNDTIMEO;
extern	const buint16_t	BASE_SO_BOARDCAST;	/* 03.28, 2020. JL */

/** IP multicast interface. @see bIP_MULTICAST_IF() */
extern const buint16_t BASE_IP_MULTICAST_IF;
 
/** IP multicast ttl. @see bIP_MULTICAST_TTL() */
extern const buint16_t BASE_IP_MULTICAST_TTL;

/** IP multicast loopback. @see bIP_MULTICAST_LOOP() */
extern const buint16_t BASE_IP_MULTICAST_LOOP;

/** Add an IP group membership. @see bIP_ADD_MEMBERSHIP() */
extern const buint16_t BASE_IP_ADD_MEMBERSHIP;

/** Drop an IP group membership. @see bIP_DROP_MEMBERSHIP() */
extern const buint16_t BASE_IP_DROP_MEMBERSHIP;


#if defined(BASE_DLL)
    /** Get #BASE_SO_TYPE constant */
    buint16_t bSO_TYPE(void);

    /** Get #BASE_SO_RCVBUF constant */
    buint16_t bSO_RCVBUF(void);

    /** Get #BASE_SO_SNDBUF constant */
    buint16_t bSO_SNDBUF(void);

    /** Get #BASE_TCP_NODELAY constant */
    buint16_t bTCP_NODELAY(void);

    /** Get #BASE_SO_REUSEADDR constant */
    buint16_t bSO_REUSEADDR(void);

    /** Get #BASE_SO_NOSIGPIPE constant */
    buint16_t bSO_NOSIGPIPE(void);

    /** Get #BASE_SO_PRIORITY constant */
    buint16_t bSO_PRIORITY(void);

    /** Get #BASE_IP_MULTICAST_IF constant */
    buint16_t bIP_MULTICAST_IF(void);

    /** Get #BASE_IP_MULTICAST_TTL constant */
    buint16_t bIP_MULTICAST_TTL(void);

    /** Get #BASE_IP_MULTICAST_LOOP constant */
    buint16_t bIP_MULTICAST_LOOP(void);

    /** Get #BASE_IP_ADD_MEMBERSHIP constant */
    buint16_t bIP_ADD_MEMBERSHIP(void);

    /** Get #BASE_IP_DROP_MEMBERSHIP constant */
    buint16_t bIP_DROP_MEMBERSHIP(void);
#else
    /** Get #BASE_SO_TYPE constant */
#   define bSO_TYPE()	    BASE_SO_TYPE

    /** Get #BASE_SO_RCVBUF constant */
#   define bSO_RCVBUF()   BASE_SO_RCVBUF

    /** Get #BASE_SO_SNDBUF constant */
#   define bSO_SNDBUF()   BASE_SO_SNDBUF

    /** Get #BASE_TCP_NODELAY constant */
#   define bTCP_NODELAY() BASE_TCP_NODELAY

    /** Get #BASE_SO_REUSEADDR constant */
#   define bSO_REUSEADDR() BASE_SO_REUSEADDR

    /** Get #BASE_SO_NOSIGPIPE constant */
#   define bSO_NOSIGPIPE() BASE_SO_NOSIGPIPE

    /** Get #BASE_SO_PRIORITY constant */
#   define bSO_PRIORITY() BASE_SO_PRIORITY

    /** Get #BASE_IP_MULTICAST_IF constant */
#   define bIP_MULTICAST_IF()    BASE_IP_MULTICAST_IF

    /** Get #BASE_IP_MULTICAST_TTL constant */
#   define bIP_MULTICAST_TTL()   BASE_IP_MULTICAST_TTL

    /** Get #BASE_IP_MULTICAST_LOOP constant */
#   define bIP_MULTICAST_LOOP()  BASE_IP_MULTICAST_LOOP

    /** Get #BASE_IP_ADD_MEMBERSHIP constant */
#   define bIP_ADD_MEMBERSHIP()  BASE_IP_ADD_MEMBERSHIP

    /** Get #BASE_IP_DROP_MEMBERSHIP constant */
#   define bIP_DROP_MEMBERSHIP() BASE_IP_DROP_MEMBERSHIP
#endif


/*
 * Flags to be specified in #bsock_recv, #bsock_send, etc.
 */

/** Out-of-band messages. @see bMSG_OOB() */
extern const int BASE_MSG_OOB;

/** Peek, don't remove from buffer. @see bMSG_PEEK() */
extern const int BASE_MSG_PEEK;

/** Don't route. @see bMSG_DONTROUTE() */
extern const int BASE_MSG_DONTROUTE;


#if defined(BASE_DLL)
    /** Get #BASE_MSG_OOB constant */
    int bMSG_OOB(void);

    /** Get #BASE_MSG_PEEK constant */
    int bMSG_PEEK(void);

    /** Get #BASE_MSG_DONTROUTE constant */
    int bMSG_DONTROUTE(void);
#else
    /** Get #BASE_MSG_OOB constant */
#   define bMSG_OOB()		BASE_MSG_OOB

    /** Get #BASE_MSG_PEEK constant */
#   define bMSG_PEEK()	BASE_MSG_PEEK

    /** Get #BASE_MSG_DONTROUTE constant */
#   define bMSG_DONTROUTE()	BASE_MSG_DONTROUTE
#endif


/**
 * Flag to be specified in #bsock_shutdown().
 */
typedef enum bsocket_sd_type
{
    BASE_SD_RECEIVE   = 0,    /**< No more receive.	    */
    BASE_SHUT_RD	    = 0,    /**< Alias for SD_RECEIVE.	    */
    BASE_SD_SEND	    = 1,    /**< No more sending.	    */
    BASE_SHUT_WR	    = 1,    /**< Alias for SD_SEND.	    */
    BASE_SD_BOTH	    = 2,    /**< No more send and receive.  */
    BASE_SHUT_RDWR    = 2     /**< Alias for SD_BOTH.	    */
} bsocket_sd_type;



/** Address to accept any incoming messages. */
#define BASE_INADDR_ANY	    ((buint32_t)0)

/** Address indicating an error return */
#define BASE_INADDR_NONE	    ((buint32_t)0xffffffff)

/** Address to send to all hosts. */
#define BASE_INADDR_BROADCAST ((buint32_t)0xffffffff)


/** 
 * Maximum length specifiable by #bsock_listen().
 * If the build system doesn't override this value, then the lowest 
 * denominator (five, in Win32 systems) will be used.
 */
#if !defined(BASE_SOMAXCONN)
#  define BASE_SOMAXCONN	5
#endif


/**
 * Constant for invalid socket returned by #bsock_socket() and
 * #bsock_accept().
 */
#define BASE_INVALID_SOCKET   (-1)

/* Must undefine s_addr because of bin_addr below */
#undef s_addr

/**
 * This structure describes Internet address.
 */
typedef struct _bin_addr
{
	buint32_t	s_addr;		/**< The 32bit IP address.	    */
}bin_addr;


/**
 * Maximum length of text representation of an IPv4 address.
 */
#define BASE_INET_ADDRSTRLEN	16

/**
 * Maximum length of text representation of an IPv6 address.
 */
#define BASE_INET6_ADDRSTRLEN	46

/**
 * The size of sin_zero field in bsockaddr_in structure. Most OSes
 * use 8, but others such as the BSD TCP/IP stack in eCos uses 24.
 */
#ifndef BASE_SOCKADDR_IN_SIN_ZERO_LEN
#   define BASE_SOCKADDR_IN_SIN_ZERO_LEN	8
#endif

/**
 * This structure describes Internet socket address.
 * If BASE_SOCKADDR_HAS_LEN is not zero, then sin_zero_len member is added
 * to this struct. As far the application is concerned, the value of
 * this member will always be zero. Internally,  may modify the value
 * before calling OS socket API, and reset the value back to zero before
 * returning the struct to application.
 */
struct _bsockaddr_in
{
#if defined(BASE_SOCKADDR_HAS_LEN) && BASE_SOCKADDR_HAS_LEN!=0
    buint8_t  sin_zero_len;	/**< Just ignore this.		    */
    buint8_t  sin_family;	/**< Address family.		    */
#else
    buint16_t	sin_family;	/**< Address family.		    */
#endif
    buint16_t	sin_port;	/**< Transport layer port number.   */
    bin_addr	sin_addr;	/**< IP address.		    */
    char	sin_zero[BASE_SOCKADDR_IN_SIN_ZERO_LEN]; /**< Padding.*/
};


#undef s6_addr

/**
 * This structure describes IPv6 address.
 */
typedef union bin6_addr
{
    /* This is the main entry */
    buint8_t  s6_addr[16];   /**< 8-bit array */

    /* While these are used for proper alignment */
    buint32_t	u6_addr32[4];

    /* Do not use this with Winsock2, as this will align bsockaddr_in6
     * to 64-bit boundary and Winsock2 doesn't like it!
     * Update 26/04/2010:
     *  This is now disabled, see http://trac.extsip.org/repos/ticket/1058
     */
#if 0 && defined(BASE_HAS_INT64) && BASE_HAS_INT64!=0 && \
    (!defined(BASE_WIN32) || BASE_WIN32==0)
    bint64_t	u6_addr64[2];
#endif

} bin6_addr;


/** Initializer value for bin6_addr. */
#define BASE_IN6ADDR_ANY_INIT { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } } }

/** Initializer value for bin6_addr. */
#define BASE_IN6ADDR_LOOPBACK_INIT { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } }

/**
 * This structure describes IPv6 socket address.
 * If BASE_SOCKADDR_HAS_LEN is not zero, then sin_zero_len member is added
 * to this struct. As far the application is concerned, the value of
 * this member will always be zero. Internally,  may modify the value
 * before calling OS socket API, and reset the value back to zero before
 * returning the struct to application.
 */
typedef struct bsockaddr_in6
{
#if defined(BASE_SOCKADDR_HAS_LEN) && BASE_SOCKADDR_HAS_LEN!=0
    buint8_t  sin6_zero_len;	    /**< Just ignore this.	   */
    buint8_t  sin6_family;	    /**< Address family.	   */
#else
    buint16_t	sin6_family;	    /**< Address family		    */
#endif
    buint16_t	sin6_port;	    /**< Transport layer port number. */
    buint32_t	sin6_flowinfo;	    /**< IPv6 flow information	    */
    bin6_addr sin6_addr;	    /**< IPv6 address.		    */
    buint32_t sin6_scope_id;	    /**< Set of interfaces for a scope	*/
} bsockaddr_in6;


/**
 * This structure describes common attributes found in transport addresses.
 * If BASE_SOCKADDR_HAS_LEN is not zero, then sa_zero_len member is added
 * to this struct. As far the application is concerned, the value of
 * this member will always be zero. Internally,  may modify the value
 * before calling OS socket API, and reset the value back to zero before
 * returning the struct to application.
 */
typedef struct baddr_hdr
{
#if defined(BASE_SOCKADDR_HAS_LEN) && BASE_SOCKADDR_HAS_LEN!=0
    buint8_t  sa_zero_len;
    buint8_t  sa_family;
#else
    buint16_t	sa_family;	/**< Common data: address family.   */
#endif
} baddr_hdr;


/**
 * This union describes a generic socket address.
 */
typedef union _bsockaddr
{
    baddr_hdr	    addr;	/**< Generic transport address.	    */
    bsockaddr_in  ipv4;	/**< IPv4 transport address.	    */
    bsockaddr_in6 ipv6;	/**< IPv6 transport address.	    */
} bsockaddr;


/**
 * This structure provides multicast group information for IPv4 addresses.
 */
typedef struct bip_mreq {
    bin_addr imr_multiaddr;	/**< IP multicast address of group. */
    bin_addr imr_interface;	/**< local IP address of interface. */
} bip_mreq;


/**
 * Options to be set for the socket. 
 */
typedef struct bsockopt_params
{
    /* The number of options to be applied. */
    unsigned cnt;

    /* Array of options to be applied. */
    struct {
	/* The level at which the option is defined. */
	int level;

	/* Option name. */
	int optname;

	/* Pointer to the buffer in which the option is specified. */
	void *optval;

	/* Buffer size of the buffer pointed by optval. */
	int optlen;
    } options[BASE_MAX_SOCKOPT_PARAMS];
} bsockopt_params;

/*****************************************************************************
 *
 * SOCKET ADDRESS MANIPULATION.
 *
 *****************************************************************************
 */

/**
 * Convert 16-bit value from network byte order to host byte order.
 *
 * @param netshort  16-bit network value.
 * @return	    16-bit host value.
 */
buint16_t bntohs(buint16_t netshort);

/**
 * Convert 16-bit value from host byte order to network byte order.
 *
 * @param hostshort 16-bit host value.
 * @return	    16-bit network value.
 */
buint16_t bhtons(buint16_t hostshort);

/**
 * Convert 32-bit value from network byte order to host byte order.
 *
 * @param netlong   32-bit network value.
 * @return	    32-bit host value.
 */
buint32_t bntohl(buint32_t netlong);

/**
 * Convert 32-bit value from host byte order to network byte order.
 *
 * @param hostlong  32-bit host value.
 * @return	    32-bit network value.
 */
buint32_t bhtonl(buint32_t hostlong);

/**
 * Convert an Internet host address given in network byte order
 * to string in standard numbers and dots notation.
 *
 * @param inaddr    The host address.
 * @return	    The string address.
 */
char* binet_ntoa(bin_addr inaddr);

/**
 * This function converts the Internet host address cp from the standard
 * numbers-and-dots notation into binary data and stores it in the structure
 * that inp points to. 
 *
 * @param cp	IP address in standard numbers-and-dots notation.
 * @param inp	Structure that holds the output of the conversion.
 *
 * @return	nonzero if the address is valid, zero if not.
 */
int binet_aton(const bstr_t *cp, bin_addr *inp);

/**
 * This function converts an address in its standard text presentation form
 * into its numeric binary form. It supports both IPv4 and IPv6 address
 * conversion.
 *
 * @param af	Specify the family of the address.  The BASE_AF_INET and 
 *		BASE_AF_INET6 address families shall be supported.  
 * @param src	Points to the string being passed in. 
 * @param dst	Points to a buffer into which the function stores the 
 *		numeric address; this shall be large enough to hold the
 *		numeric address (32 bits for BASE_AF_INET, 128 bits for
 *		BASE_AF_INET6).  
 *
 * @return	BASE_SUCCESS if conversion was successful.
 */
bstatus_t binet_pton(int af, const bstr_t *src, void *dst);

/**
 * This function converts a numeric address into a text string suitable
 * for presentation. It supports both IPv4 and IPv6 address
 * conversion. 
 * @see bsockaddr_print()
 *
 * @param af	Specify the family of the address. This can be BASE_AF_INET
 *		or BASE_AF_INET6.
 * @param src	Points to a buffer holding an IPv4 address if the af argument
 *		is BASE_AF_INET, or an IPv6 address if the af argument is
 *		BASE_AF_INET6; the address must be in network byte order.  
 * @param dst	Points to a buffer where the function stores the resulting
 *		text string; it shall not be NULL.  
 * @param size	Specifies the size of this buffer, which shall be large 
 *		enough to hold the text string (BASE_INET_ADDRSTRLEN characters
 *		for IPv4, BASE_INET6_ADDRSTRLEN characters for IPv6).
 *
 * @return	BASE_SUCCESS if conversion was successful.
 */
bstatus_t binet_ntop(int af, const void *src,
				  char *dst, int size);

/**
 * Converts numeric address into its text string representation.
 * @see bsockaddr_print()
 *
 * @param af	Specify the family of the address. This can be BASE_AF_INET
 *		or BASE_AF_INET6.
 * @param src	Points to a buffer holding an IPv4 address if the af argument
 *		is BASE_AF_INET, or an IPv6 address if the af argument is
 *		BASE_AF_INET6; the address must be in network byte order.  
 * @param dst	Points to a buffer where the function stores the resulting
 *		text string; it shall not be NULL.  
 * @param size	Specifies the size of this buffer, which shall be large 
 *		enough to hold the text string (BASE_INET_ADDRSTRLEN characters
 *		for IPv4, BASE_INET6_ADDRSTRLEN characters for IPv6).
 *
 * @return	The address string or NULL if failed.
 */
char* binet_ntop2(int af, const void *src,
			     char *dst, int size);

/**
 * Print socket address.
 *
 * @param addr	The socket address.
 * @param buf	Text buffer.
 * @param size	Size of buffer.
 * @param flags	Bitmask combination of these value:
 *		  - 1: port number is included.
 *		  - 2: square bracket is included for IPv6 address.
 *
 * @return	The address string.
 */
char* bsockaddr_print(const bsockaddr_t *addr,
				 char *buf, int size,
				 unsigned flags);

/**
 * Convert address string with numbers and dots to binary IP address.
 * 
 * @param cp	    The IP address in numbers and dots notation.
 * @return	    If success, the IP address is returned in network
 *		    byte order. If failed, BASE_INADDR_NONE will be
 *		    returned.
 * @remark
 * This is an obsolete interface to #binet_aton(); it is obsolete
 * because -1 is a valid address (255.255.255.255), and #binet_aton()
 * provides a cleaner way to indicate error return.
 */
bin_addr binet_addr(const bstr_t *cp);

/**
 * Convert address string with numbers and dots to binary IP address.
 * 
 * @param cp	    The IP address in numbers and dots notation.
 * @return	    If success, the IP address is returned in network
 *		    byte order. If failed, BASE_INADDR_NONE will be
 *		    returned.
 * @remark
 * This is an obsolete interface to #binet_aton(); it is obsolete
 * because -1 is a valid address (255.255.255.255), and #binet_aton()
 * provides a cleaner way to indicate error return.
 */
bin_addr binet_addr2(const char *cp);

/**
 * Initialize IPv4 socket address based on the address and port info.
 * The string address may be in a standard numbers and dots notation or 
 * may be a hostname. If hostname is specified, then the function will 
 * resolve the host into the IP address.
 *
 * @see bsockaddr_init()
 *
 * @param addr	    The IP socket address to be set.
 * @param cp	    The address string, which can be in a standard 
 *		    dotted numbers or a hostname to be resolved.
 * @param port	    The port number, in host byte order.
 *
 * @return	    Zero on success.
 */
bstatus_t bsockaddr_in_init( bsockaddr_in *addr,
				          const bstr_t *cp,
					  buint16_t port);

/**
 * Initialize IP socket address based on the address and port info.
 * The string address may be in a standard numbers and dots notation or 
 * may be a hostname. If hostname is specified, then the function will 
 * resolve the host into the IP address.
 *
 * @see bsockaddr_in_init()
 *
 * @param af	    Internet address family.
 * @param addr	    The IP socket address to be set.
 * @param cp	    The address string, which can be in a standard 
 *		    dotted numbers or a hostname to be resolved.
 * @param port	    The port number, in host byte order.
 *
 * @return	    Zero on success.
 */
bstatus_t bsockaddr_init(int af, 
				      bsockaddr *addr,
				      const bstr_t *cp,
				      buint16_t port);

/**
 * Compare two socket addresses.
 *
 * @param addr1	    First address.
 * @param addr2	    Second address.
 *
 * @return	    Zero on equal, -1 if addr1 is less than addr2,
 *		    and +1 if addr1 is more than addr2.
 */
int bsockaddr_cmp(const bsockaddr_t *addr1,
			     const bsockaddr_t *addr2);

/**
 * Get pointer to the address part of a socket address.
 * 
 * @param addr	    Socket address.
 *
 * @return	    Pointer to address part (sin_addr or sin6_addr,
 *		    depending on address family)
 */
void* bsockaddr_get_addr(const bsockaddr_t *addr);

/**
 * Check that a socket address contains a non-zero address part.
 *
 * @param addr	    Socket address.
 *
 * @return	    Non-zero if address is set to non-zero.
 */
bbool_t bsockaddr_has_addr(const bsockaddr_t *addr);

/**
 * Get the address part length of a socket address, based on its address
 * family. For BASE_AF_INET, the length will be sizeof(bin_addr), and
 * for BASE_AF_INET6, the length will be sizeof(bin6_addr).
 * 
 * @param addr	    Socket address.
 *
 * @return	    Length in bytes.
 */
unsigned bsockaddr_get_addr_len(const bsockaddr_t *addr);

/**
 * Get the socket address length, based on its address
 * family. For BASE_AF_INET, the length will be sizeof(bsockaddr_in), and
 * for BASE_AF_INET6, the length will be sizeof(bsockaddr_in6).
 * 
 * @param addr	    Socket address.
 *
 * @return	    Length in bytes.
 */
unsigned bsockaddr_get_len(const bsockaddr_t *addr);

/** 
 * Copy only the address part (sin_addr/sin6_addr) of a socket address.
 *
 * @param dst	    Destination socket address.
 * @param src	    Source socket address.
 *
 * @see @bsockaddr_cp()
 */
void bsockaddr_copy_addr(bsockaddr *dst,
				    const bsockaddr *src);
/**
 * Copy socket address. This will copy the whole structure depending
 * on the address family of the source socket address.
 *
 * @param dst	    Destination socket address.
 * @param src	    Source socket address.
 *
 * @see @bsockaddr_copy_addr()
 */
void bsockaddr_cp(bsockaddr_t *dst, const bsockaddr_t *src);

/*
 * If the source's and desired address family matches, copy the address,
 * otherwise synthesize a new address with the desired address family,
 * from the source address. This can be useful to generate an IPv4-mapped
 * IPv6 address.
 *
 * @param dst_af    Desired address family.
 * @param dst	    Destination socket address, invalid if synthesis is
 *		    required and failed.
 * @param src	    Source socket address.
 *
 * @return	    BASE_SUCCESS on success, or the error status
 *		    if synthesis is required and failed.
 */
bstatus_t bsockaddr_synthesize(int dst_af,
				            bsockaddr_t *dst,
				            const bsockaddr_t *src);

/**
 * Get the IP address of an IPv4 socket address.
 * The address is returned as 32bit value in host byte order.
 *
 * @param addr	    The IP socket address.
 * @return	    32bit address, in host byte order.
 */
bin_addr bsockaddr_in_get_addr(const bsockaddr_in *addr);

/**
 * Set the IP address of an IPv4 socket address.
 *
 * @param addr	    The IP socket address.
 * @param hostaddr  The host address, in host byte order.
 */
void bsockaddr_in_set_addr(bsockaddr_in *addr,
				      buint32_t hostaddr);

/**
 * Set the IP address of an IP socket address from string address, 
 * with resolving the host if necessary. The string address may be in a
 * standard numbers and dots notation or may be a hostname. If hostname
 * is specified, then the function will resolve the host into the IP
 * address.
 *
 * @see bsockaddr_set_str_addr()
 *
 * @param addr	    The IP socket address to be set.
 * @param cp	    The address string, which can be in a standard 
 *		    dotted numbers or a hostname to be resolved.
 *
 * @return	    BASE_SUCCESS on success.
 */
bstatus_t bsockaddr_in_set_str_addr( bsockaddr_in *addr,
					          const bstr_t *cp);

/**
 * Set the IP address of an IPv4 or IPv6 socket address from string address,
 * with resolving the host if necessary. The string address may be in a
 * standard IPv6 or IPv6 address or may be a hostname. If hostname
 * is specified, then the function will resolve the host into the IP
 * address according to the address family.
 *
 * @param af	    Address family.
 * @param addr	    The IP socket address to be set.
 * @param cp	    The address string, which can be in a standard 
 *		    IP numbers (IPv4 or IPv6) or a hostname to be resolved.
 *
 * @return	    BASE_SUCCESS on success.
 */
bstatus_t bsockaddr_set_str_addr(int af,
					      bsockaddr *addr,
					      const bstr_t *cp);

/**
 * Get the port number of a socket address, in host byte order. 
 * This function can be used for both IPv4 and IPv6 socket address.
 * 
 * @param addr	    Socket address.
 *
 * @return	    Port number, in host byte order.
 */
buint16_t bsockaddr_get_port(const bsockaddr_t *addr);

/**
 * Get the transport layer port number of an Internet socket address.
 * The port is returned in host byte order.
 *
 * @param addr	    The IP socket address.
 * @return	    Port number, in host byte order.
 */
buint16_t bsockaddr_in_get_port(const bsockaddr_in *addr);

/**
 * Set the port number of an Internet socket address.
 *
 * @param addr	    The socket address.
 * @param hostport  The port number, in host byte order.
 */
bstatus_t bsockaddr_set_port(bsockaddr *addr, 
					  buint16_t hostport);

/**
 * Set the port number of an IPv4 socket address.
 *
 * @see bsockaddr_set_port()
 *
 * @param addr	    The IP socket address.
 * @param hostport  The port number, in host byte order.
 */
void bsockaddr_in_set_port(bsockaddr_in *addr, 
				      buint16_t hostport);

/**
 * Parse string containing IP address and optional port into socket address,
 * possibly also with address family detection. This function supports both
 * IPv4 and IPv6 parsing, however IPv6 parsing may only be done if IPv6 is
 * enabled during compilation.
 *
 * This function supports parsing several formats. Sample IPv4 inputs and
 * their default results::
 *  - "10.0.0.1:80": address 10.0.0.1 and port 80.
 *  - "10.0.0.1": address 10.0.0.1 and port zero.
 *  - "10.0.0.1:": address 10.0.0.1 and port zero.
 *  - "10.0.0.1:0": address 10.0.0.1 and port zero.
 *  - ":80": address 0.0.0.0 and port 80.
 *  - ":": address 0.0.0.0 and port 0.
 *  - "localhost": address 127.0.0.1 and port 0.
 *  - "localhost:": address 127.0.0.1 and port 0.
 *  - "localhost:80": address 127.0.0.1 and port 80.
 *
 * Sample IPv6 inputs and their default results:
 *  - "[fec0::01]:80": address fec0::01 and port 80
 *  - "[fec0::01]": address fec0::01 and port 0
 *  - "[fec0::01]:": address fec0::01 and port 0
 *  - "[fec0::01]:0": address fec0::01 and port 0
 *  - "fec0::01": address fec0::01 and port 0
 *  - "fec0::01:80": address fec0::01:80 and port 0
 *  - "::": address zero (::) and port 0
 *  - "[::]": address zero (::) and port 0
 *  - "[::]:": address zero (::) and port 0
 *  - ":::": address zero (::) and port 0
 *  - "[::]:80": address zero (::) and port 0
 *  - ":::80": address zero (::) and port 80
 *
 * Note: when the IPv6 socket address contains port number, the IP 
 * part of the socket address should be enclosed with square brackets, 
 * otherwise the port number will be included as part of the IP address
 * (see "fec0::01:80" example above).
 *
 * @param af	    Optionally specify the address family to be used. If the
 *		    address family is to be deducted from the input, specify
 *		    bAF_UNSPEC() here. Other supported values are
 *		    #bAF_INET() and #bAF_INET6()
 * @param options   Additional options to assist the parsing, must be zero
 *		    for now.
 * @param str	    The input string to be parsed.
 * @param addr	    Pointer to store the result.
 *
 * @return	    BASE_SUCCESS if the parsing is successful.
 *
 * @see bsockaddr_parse2()
 */
bstatus_t bsockaddr_parse(int af, unsigned options,
				       const bstr_t *str,
				       bsockaddr *addr);

/**
 * This function is similar to #bsockaddr_parse(), except that it will not
 * convert the hostpart into IP address (thus possibly resolving the hostname
 * into a #bsockaddr. 
 *
 * Unlike #bsockaddr_parse(), this function has a limitation that if port 
 * number is specified in an IPv6 input string, the IP part of the IPv6 socket
 * address MUST be enclosed in square brackets, otherwise the port number will
 * be considered as part of the IPv6 IP address.
 *
 * @param af	    Optionally specify the address family to be used. If the
 *		    address family is to be deducted from the input, specify
 *		    #bAF_UNSPEC() here. Other supported values are
 *		    #bAF_INET() and #bAF_INET6()
 * @param options   Additional options to assist the parsing, must be zero
 *		    for now.
 * @param str	    The input string to be parsed.
 * @param hostpart  Optional pointer to store the host part of the socket 
 *		    address, with any brackets removed.
 * @param port	    Optional pointer to store the port number. If port number
 *		    is not found, this will be set to zero upon return.
 * @param raf	    Optional pointer to store the detected address family of
 *		    the input address.
 *
 * @return	    BASE_SUCCESS if the parsing is successful.
 *
 * @see bsockaddr_parse()
 */
bstatus_t bsockaddr_parse2(int af, unsigned options,
				        const bstr_t *str,
				        bstr_t *hostpart,
				        buint16_t *port,
					int *raf);

/*****************************************************************************
 *
 * HOST NAME AND ADDRESS.
 *
 *****************************************************************************
 */

/**
 * Get system's host name.
 *
 * @return	    The hostname, or empty string if the hostname can not
 *		    be identified.
 */
const bstr_t* bgethostname(void);

/**
 * Get host's IP address, which the the first IP address that is resolved
 * from the hostname.
 *
 * @return	    The host's IP address, BASE_INADDR_NONE if the host
 *		    IP address can not be identified.
 */
bin_addr bgethostaddr(void);


/*****************************************************************************
 *
 * SOCKET API.
 *
 *****************************************************************************
 */

/**
 * Create new socket/endpoint for communication.
 *
 * @param family    Specifies a communication domain; this selects the
 *		    protocol family which will be used for communication.
 * @param type	    The socket has the indicated type, which specifies the 
 *		    communication semantics.
 * @param protocol  Specifies  a  particular  protocol  to  be used with the
 *		    socket.  Normally only a single protocol exists to support 
 *		    a particular socket  type  within  a given protocol family, 
 *		    in which a case protocol can be specified as 0.
 * @param sock	    New socket descriptor, or BASE_INVALID_SOCKET on error.
 *
 * @return	    Zero on success.
 */
bstatus_t bsock_socket(int family, 
				    int type, 
				    int protocol,
				    bsock_t *sock);

/**
 * Close the socket descriptor.
 *
 * @param sockfd    The socket descriptor.
 *
 * @return	    Zero on success.
 */
bstatus_t bsock_close(bsock_t sockfd);


/**
 * This function gives the socket sockfd the local address my_addr. my_addr is
 * addrlen bytes long.  Traditionally, this is called assigning a name to
 * a socket. When a socket is created with #bsock_socket(), it exists in a
 * name space (address family) but has no name assigned.
 *
 * @param sockfd    The socket desriptor.
 * @param my_addr   The local address to bind the socket to.
 * @param addrlen   The length of the address.
 *
 * @return	    Zero on success.
 */
bstatus_t bsock_bind( bsock_t sockfd, 
				   const bsockaddr_t *my_addr,
				   int addrlen);

/**
 * Bind the IP socket sockfd to the given address and port.
 *
 * @param sockfd    The socket descriptor.
 * @param addr	    Local address to bind the socket to, in host byte order.
 * @param port	    The local port to bind the socket to, in host byte order.
 *
 * @return	    Zero on success.
 */
bstatus_t bsock_bind_in( bsock_t sockfd, 
				      buint32_t addr,
				      buint16_t port);

/**
 * Bind the IP socket sockfd to the given address and a random port in the
 * specified range.
 *
 * @param sockfd    	The socket desriptor.
 * @param addr      	The local address and port to bind the socket to.
 * @param port_range	The port range, relative the to start port number
 * 			specified in port field in #addr. Note that if the
 * 			port is zero, this param will be ignored.
 * @param max_try   	Maximum retries.
 *
 * @return	    	Zero on success.
 */
bstatus_t bsock_bind_random( bsock_t sockfd,
				          const bsockaddr_t *addr,
				          buint16_t port_range,
				          buint16_t max_try);

#if BASE_HAS_TCP
/**
 * Listen for incoming connection. This function only applies to connection
 * oriented sockets (such as BASE_SOCK_STREAM or BASE_SOCK_SEQPACKET), and it
 * indicates the willingness to accept incoming connections.
 *
 * @param sockfd	The socket descriptor.
 * @param backlog	Defines the maximum length the queue of pending
 *			connections may grow to.
 *
 * @return		Zero on success.
 */
bstatus_t bsock_listen( bsock_t sockfd, 
				     int backlog );

/**
 * Accept new connection on the specified connection oriented server socket.
 *
 * @param serverfd  The server socket.
 * @param newsock   New socket on success, of BASE_INVALID_SOCKET if failed.
 * @param addr	    A pointer to sockaddr type. If the argument is not NULL,
 *		    it will be filled by the address of connecting entity.
 * @param addrlen   Initially specifies the length of the address, and upon
 *		    return will be filled with the exact address length.
 *
 * @return	    Zero on success, or the error number.
 */
bstatus_t bsock_accept( bsock_t serverfd,
				     bsock_t *newsock,
				     bsockaddr_t *addr,
				     int *addrlen);
#endif

/**
 * The file descriptor sockfd must refer to a socket.  If the socket is of
 * type BASE_SOCK_DGRAM  then the serv_addr address is the address to which
 * datagrams are sent by default, and the only address from which datagrams
 * are received. If the socket is of type BASE_SOCK_STREAM or BASE_SOCK_SEQPACKET,
 * this call attempts to make a connection to another socket.  The
 * other socket is specified by serv_addr, which is an address (of length
 * addrlen) in the communications space of the  socket.  Each  communications
 * space interprets the serv_addr parameter in its own way.
 *
 * @param sockfd	The socket descriptor.
 * @param serv_addr	Server address to connect to.
 * @param addrlen	The length of server address.
 *
 * @return		Zero on success.
 */
bstatus_t bsock_connect( bsock_t sockfd,
				      const bsockaddr_t *serv_addr,
				      int addrlen);

/**
 * Return the address of peer which is connected to socket sockfd.
 *
 * @param sockfd	The socket descriptor.
 * @param addr		Pointer to sockaddr structure to which the address
 *			will be returned.
 * @param namelen	Initially the length of the addr. Upon return the value
 *			will be set to the actual length of the address.
 *
 * @return		Zero on success.
 */
bstatus_t bsock_getpeername(bsock_t sockfd,
					  bsockaddr_t *addr,
					  int *namelen);

/**
 * Return the current name of the specified socket.
 *
 * @param sockfd	The socket descriptor.
 * @param addr		Pointer to sockaddr structure to which the address
 *			will be returned.
 * @param namelen	Initially the length of the addr. Upon return the value
 *			will be set to the actual length of the address.
 *
 * @return		Zero on success.
 */
bstatus_t bsock_getsockname( bsock_t sockfd,
					  bsockaddr_t *addr,
					  int *namelen);

/**
 * Get socket option associated with a socket. Options may exist at multiple
 * protocol levels; they are always present at the uppermost socket level.
 *
 * @param sockfd	The socket descriptor.
 * @param level		The level which to get the option from.
 * @param optname	The option name.
 * @param optval	Identifies the buffer which the value will be
 *			returned.
 * @param optlen	Initially contains the length of the buffer, upon
 *			return will be set to the actual size of the value.
 *
 * @return		Zero on success.
 */
bstatus_t bsock_getsockopt( bsock_t sockfd,
					 buint16_t level,
					 buint16_t optname,
					 void *optval,
					 int *optlen);
/**
 * Manipulate the options associated with a socket. Options may exist at 
 * multiple protocol levels; they are always present at the uppermost socket 
 * level.
 *
 * @param sockfd	The socket descriptor.
 * @param level		The level which to get the option from.
 * @param optname	The option name.
 * @param optval	Identifies the buffer which contain the value.
 * @param optlen	The length of the value.
 *
 * @return		BASE_SUCCESS or the status code.
 */
bstatus_t bsock_setsockopt( bsock_t sockfd,
					 buint16_t level,
					 buint16_t optname,
					 const void *optval,
					 int optlen);

/**
 * Set socket options associated with a socket. This method will apply all the 
 * options specified, and ignore any errors that might be raised.
 *
 * @param sockfd	The socket descriptor.
 * @param params	The socket options.
 *
 * @return		BASE_SUCCESS or the last error code. 
 */
bstatus_t bsock_setsockopt_params( bsock_t sockfd,
					       const bsockopt_params *params);					       

/**
 * Helper function to set socket buffer size using #bsock_setsockopt()
 * with capability to auto retry with lower buffer setting value until
 * the highest possible value is successfully set.
 *
 * @param sockfd	The socket descriptor.
 * @param optname	The option name, valid values are bSO_RCVBUF()
 *			and bSO_SNDBUF().
 * @param auto_retry	Option whether auto retry with lower value is
 *			enabled.
 * @param buf_size	On input, specify the prefered buffer size setting,
 *			on output, the buffer size setting applied.
 *
 * @return		BASE_SUCCESS or the status code.
 */
bstatus_t bsock_setsockopt_sobuf( bsock_t sockfd,
					       buint16_t optname,
					       bbool_t auto_retry,
					       unsigned *buf_size);


/**
 * Receives data stream or message coming to the specified socket.
 *
 * @param sockfd	The socket descriptor.
 * @param buf		The buffer to receive the data or message.
 * @param len		On input, the length of the buffer. On return,
 *			contains the length of data received.
 * @param flags		Flags (such as bMSG_PEEK()).
 *
 * @return		BASE_SUCCESS or the error code.
 */
bstatus_t bsock_recv(bsock_t sockfd,
				  void *buf,
				  bssize_t *len,
				  unsigned flags);

/**
 * Receives data stream or message coming to the specified socket.
 *
 * @param sockfd	The socket descriptor.
 * @param buf		The buffer to receive the data or message.
 * @param len		On input, the length of the buffer. On return,
 *			contains the length of data received.
 * @param flags		Flags (such as bMSG_PEEK()).
 * @param from		If not NULL, it will be filled with the source
 *			address of the connection.
 * @param fromlen	Initially contains the length of from address,
 *			and upon return will be filled with the actual
 *			length of the address.
 *
 * @return		BASE_SUCCESS or the error code.
 */
bstatus_t bsock_recvfrom( bsock_t sockfd,
				      void *buf,
				      bssize_t *len,
				      unsigned flags,
				      bsockaddr_t *from,
				      int *fromlen);

/**
 * Transmit data to the socket.
 *
 * @param sockfd	Socket descriptor.
 * @param buf		Buffer containing data to be sent.
 * @param len		On input, the length of the data in the buffer.
 *			Upon return, it will be filled with the length
 *			of data sent.
 * @param flags		Flags (such as bMSG_DONTROUTE()).
 *
 * @return		BASE_SUCCESS or the status code.
 */
bstatus_t bsock_send(bsock_t sockfd,
				  const void *buf,
				  bssize_t *len,
				  unsigned flags);

/**
 * Transmit data to the socket to the specified address.
 *
 * @param sockfd	Socket descriptor.
 * @param buf		Buffer containing data to be sent.
 * @param len		On input, the length of the data in the buffer.
 *			Upon return, it will be filled with the length
 *			of data sent.
 * @param flags		Flags (such as bMSG_DONTROUTE()).
 * @param to		The address to send.
 * @param tolen		The length of the address in bytes.
 *
 * @return		BASE_SUCCESS or the status code.
 */
bstatus_t bsock_sendto(bsock_t sockfd,
				    const void *buf,
				    bssize_t *len,
				    unsigned flags,
				    const bsockaddr_t *to,
				    int tolen);

#if BASE_HAS_TCP
/**
 * The shutdown call causes all or part of a full-duplex connection on the
 * socket associated with sockfd to be shut down.
 *
 * @param sockfd	The socket descriptor.
 * @param how		If how is BASE_SHUT_RD, further receptions will be 
 *			disallowed. If how is BASE_SHUT_WR, further transmissions
 *			will be disallowed. If how is BASE_SHUT_RDWR, further 
 *			receptions andtransmissions will be disallowed.
 *
 * @return		Zero on success.
 */
bstatus_t bsock_shutdown( bsock_t sockfd,
				       int how);
#endif

/*****************************************************************************
 *
 * Utilities.
 *
 *****************************************************************************
 */

/**
 * Print socket address string. This method will enclose the address string 
 * with square bracket if it's IPv6 address.
 *
 * @param host_str  The host address string.
 * @param port	    The port address.
 * @param buf	    Text buffer.
 * @param size	    Size of buffer.
 * @param flags	    Bitmask combination of these value:
 *		    - 1: port number is included. 
 *
 * @return	The address string.
 */
char * baddr_str_print( const bstr_t *host_str, int port, 
				   char *buf, int size, unsigned flag);

BASE_END_DECL

#endif

