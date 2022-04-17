/* 
 *
 */
#ifndef __BASE_CONFIG_H__
#define __BASE_CONFIG_H__

/*
 * Include compiler specific configuration.
 */
#if defined(_MSC_VER)
#  include <compat/cc_msvc.h>
#elif defined(__GNUC__)
#  include <compat/cc_gcc.h>
#elif defined(__CW32__)
#  include <compat/cc_mwcc.h>
#elif defined(__MWERKS__)
#  include <compat/cc_codew.h>
#elif defined(__GCCE__)
#  include <compat/cc_gcce.h>
#elif defined(__ARMCC__)
#  include <compat/cc_armcc.h>
#else
#  error "Unknown compiler."
#endif

/* BASE_ALIGN_DATA is compiler specific directive to align data address */
#ifndef BASE_ALIGN_DATA
#  error "BASE_ALIGN_DATA is not defined!"
#endif

/*
 * Include target OS specific configuration.
 */
#if defined(BASE_AUTOCONF)
    /*
     * Autoconf
     */
#   include <compat/os_auto.h>

#elif defined(BASE_SYMBIAN) && BASE_SYMBIAN!=0
    /*
     * SymbianOS
     */
#  include <compat/os_symbian.h>

#elif defined(BASE_WIN32_WINCE) || defined(_WIN32_WCE) || defined(UNDER_CE)
    /*
     * Windows CE
     */
#   undef BASE_WIN32_WINCE
#   define BASE_WIN32_WINCE   1
#   include <compat/os_win32_wince.h>

    /* Also define Win32 */
#   define BASE_WIN32 1

#elif defined(BASE_WIN32_WINPHONE8) || defined(_WIN32_WINPHONE8)
    /*
     * Windows Phone 8
     */
#   undef BASE_WIN32_WINPHONE8
#   define BASE_WIN32_WINPHONE8   1
#   include <compat/os_winphone8.h>

    /* Also define Win32 */
#   define BASE_WIN32 1

#elif defined(BASE_WIN32_UWP) || defined(_WIN32_UWP)
    /*
     * Windows UWP
     */
#   undef BASE_WIN32_UWP
#   define BASE_WIN32_UWP   1
#   include <compat/os_winuwp.h>

    /* Define Windows phone */
#   define BASE_WIN32_WINPHONE8 1

    /* Also define Win32 */
#   define BASE_WIN32 1

#elif defined(BASE_WIN32) || defined(_WIN32) || defined(__WIN32__) || \
	defined(WIN32) || defined(BASE_WIN64) || defined(_WIN64) || \
	defined(WIN64) || defined(__TOS_WIN__) 
#   if defined(BASE_WIN64) || defined(_WIN64) || defined(WIN64)
	/*
	 * Win64
	 */
#	undef BASE_WIN64
#	define BASE_WIN64 1
#   endif
#   undef BASE_WIN32
#   define BASE_WIN32 1
#   include <compat/os_win32.h>

#elif defined(BASE_LINUX) || defined(linux) || defined(__linux)
    /*
     * Linux
     */
#   undef BASE_LINUX
#   define BASE_LINUX	    1
#   include <compat/os_linux.h>

#elif defined(BASE_PALMOS) && BASE_PALMOS!=0
    /*
     * Palm
     */
#  include <compat/os_palmos.h>

#elif defined(BASE_SUNOS) || defined(sun) || defined(__sun)
    /*
     * SunOS
     */
#   undef BASE_SUNOS
#   define BASE_SUNOS	    1
#   include <compat/os_sunos.h>

#elif defined(BASE_DARWINOS) || defined(__MACOSX__) || \
      defined (__APPLE__) || defined (__MACH__)
    /*
     * MacOS X
     */
#   undef BASE_DARWINOS
#   define BASE_DARWINOS	    1
#   include <compat/os_darwinos.h>

#elif defined(BASE_RTEMS) && BASE_RTEMS!=0
    /*
     * RTEMS
     */
#  include <compat/os_rtems.h>
#else
#   error "Please specify target os."
#endif


/********************************************************************
 * Target machine specific configuration.
 */
#if defined(BASE_AUTOCONF)
    /*
     * Autoconf configured
     */
#include <compat/m_auto.h>

#elif defined (BASE_M_I386) || defined(_i386_) || defined(i_386_) || \
	defined(_X86_) || defined(x86) || defined(__i386__) || \
	defined(__i386) || defined(_M_IX86) || defined(__I86__)
    /*
     * Generic i386 processor family, little-endian
     */
#   undef BASE_M_I386
#   define BASE_M_I386		1
#   define BASE_M_NAME		"i386"
#   define BASE_HAS_PENTIUM	1
#   define BASE_IS_LITTLE_ENDIAN	1
#   define BASE_IS_BIG_ENDIAN	0


#elif defined (BASE_M_X86_64) || defined(__amd64__) || defined(__amd64) || \
	defined(__x86_64__) || defined(__x86_64) || \
	defined(_M_X64) || defined(_M_AMD64)
    /*
     * AMD 64bit processor, little endian
     */
#   undef BASE_M_X86_64
#   define BASE_M_X86_64		1
#   define BASE_M_NAME		"x86_64"
#   define BASE_HAS_PENTIUM	1
#   define BASE_IS_LITTLE_ENDIAN	1
#   define BASE_IS_BIG_ENDIAN	0

#elif defined(BASE_M_IA64) || defined(__ia64__) || defined(_IA64) || \
	defined(__IA64__) || defined( 	_M_IA64)
    /*
     * Intel IA64 processor, default to little endian
     */
#   undef BASE_M_IA64
#   define BASE_M_IA64		1
#   define BASE_M_NAME		"ia64"
#   define BASE_HAS_PENTIUM	1
#   define BASE_IS_LITTLE_ENDIAN	1
#   define BASE_IS_BIG_ENDIAN	0

#elif defined (BASE_M_M68K) && BASE_M_M68K != 0

    /*
     * Motorola m68k processor, big endian
     */
#   undef BASE_M_M68K
#   define BASE_M_M68K		1
#   define BASE_M_NAME		"m68k"
#   define BASE_HAS_PENTIUM	0
#   define BASE_IS_LITTLE_ENDIAN	0
#   define BASE_IS_BIG_ENDIAN	1


#elif defined (BASE_M_ALPHA) || defined (__alpha__) || defined (__alpha) || \
	defined (_M_ALPHA)
    /*
     * DEC Alpha processor, little endian
     */
#   undef BASE_M_ALPHA
#   define BASE_M_ALPHA		1
#   define BASE_M_NAME		"alpha"
#   define BASE_HAS_PENTIUM	0
#   define BASE_IS_LITTLE_ENDIAN	1
#   define BASE_IS_BIG_ENDIAN	0


#elif defined(BASE_M_MIPS) || defined(__mips__) || defined(__mips) || \
	defined(__MIPS__) || defined(MIPS) || defined(_MIPS_)
    /*
     * MIPS, bi-endian, so raise error if endianness is not configured
     */
#   undef BASE_M_MIPS
#   define BASE_M_MIPS		1
#   define BASE_M_NAME		"mips"
#   define BASE_HAS_PENTIUM	0
#   if !BASE_IS_LITTLE_ENDIAN && !BASE_IS_BIG_ENDIAN
#   	error Endianness must be declared for this processor
#   endif


#elif defined (BASE_M_SPARC) || defined( 	__sparc__) || defined(__sparc)
    /*
     * Sun Sparc, big endian
     */
#   undef BASE_M_SPARC
#   define BASE_M_SPARC		1
#   define BASE_M_NAME		"sparc"
#   define BASE_HAS_PENTIUM	0
#   define BASE_IS_LITTLE_ENDIAN	0
#   define BASE_IS_BIG_ENDIAN	1

#elif defined(ARM) || defined(_ARM_) ||  defined(__arm__) || defined(_M_ARM)
#   define BASE_HAS_PENTIUM	0
    /*
     * ARM, bi-endian, so raise error if endianness is not configured
     */
#   if !BASE_IS_LITTLE_ENDIAN && !BASE_IS_BIG_ENDIAN
#   	error Endianness must be declared for this processor
#   endif
#   if defined (BASE_M_ARMV7) || defined(ARMV7)
#	undef BASE_M_ARMV7
#	define BASE_M_ARM7		1
#	define BASE_M_NAME		"armv7"
#   elif defined (BASE_M_ARMV4) || defined(ARMV4)
#	undef BASE_M_ARMV4
#	define BASE_M_ARMV4		1
#	define BASE_M_NAME		"armv4"
#   endif 

#elif defined (BASE_M_POWERPC) || defined(__powerpc) || defined(__powerpc__) || \
	defined(__POWERPC__) || defined(__ppc__) || defined(_M_PPC) || \
	defined(_ARCH_PPC)
    /*
     * PowerPC, bi-endian, so raise error if endianness is not configured
     */
#   undef BASE_M_POWERPC
#   define BASE_M_POWERPC		1
#   define BASE_M_NAME		"powerpc"
#   define BASE_HAS_PENTIUM	0
#   if !BASE_IS_LITTLE_ENDIAN && !BASE_IS_BIG_ENDIAN
#   	error Endianness must be declared for this processor
#   endif

#elif defined (BASE_M_NIOS2) || defined(__nios2) || defined(__nios2__) || \
      defined(__NIOS2__) || defined(__M_NIOS2) || defined(_ARCH_NIOS2)
    /*
     * Nios2, little endian
     */
#   undef BASE_M_NIOS2
#   define BASE_M_NIOS2		1
#   define BASE_M_NAME		"nios2"
#   define BASE_HAS_PENTIUM	0
#   define BASE_IS_LITTLE_ENDIAN	1
#   define BASE_IS_BIG_ENDIAN	0
		
#else
#   error "Please specify target machine."
#endif

/* Include size_t definition. */
#include <compat/size_t.h>

/* Include site/user specific configuration to control  features.
 * YOU MUST CREATE THIS FILE YOURSELF!!
 */
#include <config_site.h>

/********************************************************************
 *  Features.
 */

/* Overrides for DOXYGEN */
#ifdef DOXYGEN
#   undef BASE_FUNCTIONS_ARE_INLINED
#   undef BASE_HAS_FLOATING_POINT
#   undef BASE_LOG_MAX_LEVEL
#   undef BASE_LOG_MAX_SIZE
#   undef BASE_LOG_USE_STACK_BUFFER
#   undef BASE_TERM_HAS_COLOR
#   undef BASE_POOL_DEBUG
#   undef BASE_HAS_TCP
#   undef BASE_MAX_HOSTNAME
#   undef BASE_IOQUEUE_MAX_HANDLES
#   undef FD_SETSIZE
#   undef BASE_HAS_SEMAPHORE
#   undef BASE_HAS_EVENT_OBJ
#   undef BASE_ENABLE_EXTRA_CHECK
#   undef BASE_EXCEPTION_USE_WIN32_SEH
#   undef BASE_HAS_ERROR_STRING

#   define BASE_HAS_IPV6	1
#endif

/**
 * @defgroup bconfig Build Configuration
 * @{
 *
 * This section contains macros that can set during  build process
 * to controll various aspects of the library.
 *
 * <b>Note</b>: the values in this page does NOT necessarily reflect to the
 * macro values during the build process.
 */

/**
 * If this macro is set to 1, it will enable some debugging checking
 * in the library.
 *
 * Default: equal to (NOT NDEBUG).
 */
#ifndef BASE_LIB_DEBUG
#  ifndef NDEBUG
#    define BASE_LIB_DEBUG		    1
#  else
#    define BASE_LIB_DEBUG		    0
#  endif
#endif

/**
 * Enable this macro to activate logging to mutex/semaphore related events.
 * This is useful to troubleshoot concurrency problems such as deadlocks.
 * In addition, you should also add BASE_LOG_HAS_THREAD_ID flag to the
 * log decoration to assist the troubleshooting.
 *
 * Default: 0
 */
#ifndef BASE_DEBUG_MUTEX
#   define BASE_DEBUG_MUTEX	    0
#endif

/**
 * Expand functions in *_i.h header files as inline.
 *
 * Default: 0.
 */
#ifndef BASE_FUNCTIONS_ARE_INLINED
#  define BASE_FUNCTIONS_ARE_INLINED  0
#endif

/**
 * Use floating point computations in the library.
 *
 * Default: 1.
 */
#ifndef BASE_HAS_FLOATING_POINT
#  define BASE_HAS_FLOATING_POINT	    1
#endif

/**
 * Declare maximum logging level/verbosity. Lower number indicates higher
 * importance, with the highest importance has level zero. The least
 * important level is five in this implementation, but this can be extended
 * by supplying the appropriate implementation.
 *
 * The level conventions:
 *  - 0: fatal error
 *  - 1: error
 *  - 2: warning
 *  - 3: info
 *  - 4: debug
 *  - 5: trace
 *  - 6: more detailed trace
 *
 * Default: 4
 */
#ifndef BASE_LOG_MAX_LEVEL
#define BASE_LOG_MAX_LEVEL   6
#endif

/**
 * Maximum message size that can be sent to output device for each call
 * to BASE_LOG(). If the message size is longer than this value, it will be cut.
 * This may affect the stack usage, depending whether BASE_LOG_USE_STACK_BUFFER
 * flag is set.
 *
 * Default: 4000
 */
#ifndef BASE_LOG_MAX_SIZE
#  define BASE_LOG_MAX_SIZE	    4000
#endif

/**
 * Log buffer.
 * Does the log get the buffer from the stack? (default is yes).
 * If the value is set to NO, then the buffer will be taken from static
 * buffer, which in this case will make the log function non-reentrant.
 *
 * Default: 1
 */
#ifndef BASE_LOG_USE_STACK_BUFFER
#  define BASE_LOG_USE_STACK_BUFFER   1
#endif

/**
 * Enable log indentation feature.
 *
 * Default: 1
 */
#ifndef BASE_LOG_ENABLE_INDENT
#   define BASE_LOG_ENABLE_INDENT        1
#endif

/**
 * Number of BASE_LOG_INDENT_CHAR to put every time blog_push_indent()
 * is called.
 *
 * Default: 1
 */
#ifndef BASE_LOG_INDENT_SIZE
#   define BASE_LOG_INDENT_SIZE        1
#endif

/**
 * Log indentation character.
 *
 * Default: space
 */
#ifndef BASE_LOG_INDENT_CHAR
#   define BASE_LOG_INDENT_CHAR	    '.'
#endif

/**
 * Log sender width.
 *
 * Default: 22 (for 64-bit machines), 14 otherwise
 */
#ifndef BASE_LOG_SENDER_WIDTH
#   if BASE_HAS_STDINT_H
#       include <stdint.h>
#       if (UINTPTR_MAX == 0xffffffffffffffff)
		#define BASE_LOG_SENDER_WIDTH 	22
#       else
#           define BASE_LOG_SENDER_WIDTH		32	/* 14*/
#       endif
#   else
#       define BASE_LOG_SENDER_WIDTH		32	/* Windows use 32; 14*/
#   endif
#endif

/**
 * Log thread name width.
 *
 * Default: 12
 */
#ifndef BASE_LOG_THREAD_WIDTH
#   define BASE_LOG_THREAD_WIDTH	    12
#endif

/**
 * Colorfull terminal (for logging etc).
 *
 * Default: 1
 */
#ifndef BASE_TERM_HAS_COLOR
#define BASE_TERM_HAS_COLOR	    1
#endif


/**
 * Set this flag to non-zero to enable various checking for pool
 * operations. When this flag is set, assertion must be enabled
 * in the application.
 *
 * This will slow down pool creation and destruction and will add
 * few bytes of overhead, so application would normally want to 
 * disable this feature on release build.
 *
 * Default: 0
 */
#ifndef BASE_SAFE_POOL
#   define BASE_SAFE_POOL		    0
#endif


/**
 * If pool debugging is used, then each memory allocation from the pool
 * will call malloc(), and pool will release all memory chunks when it
 * is destroyed. This works better when memory verification programs
 * such as Rational Purify is used.
 *
 * Default: 0
 */
#ifndef BASE_POOL_DEBUG
#  define BASE_POOL_DEBUG		    0
#endif


/**
 * Enable timer heap debugging facility. When this is enabled, application
 * can call btimer_heap_dump() to show the contents of the timer heap
 * along with the source location where the timer entries were scheduled.
 * See https://trac.extsip.org/repos/ticket/1527 for more info.
 *
 * Default: 0
 */
#ifndef BASE_TIMER_DEBUG
#  define BASE_TIMER_DEBUG	    0
#endif


/**
 * Set this to 1 to enable debugging on the group lock. Default: 0
 */
#ifndef BASE_GRP_LOCK_DEBUG
#  define BASE_GRP_LOCK_DEBUG	0
#endif


/**
 * Specify this as \a stack_size argument in #bthreadCreate() to specify
 * that thread should use default stack size for the current platform.
 *
 * Default: 8192
 */
#ifndef BASE_THREAD_DEFAULT_STACK_SIZE 
#  define BASE_THREAD_DEFAULT_STACK_SIZE    8192
#endif


/**
 * Specify if BASE_CHECK_STACK() macro is enabled to check the sanity of 
 * the stack. The OS implementation may check that no stack overflow 
 * occurs, and it also may collect statistic about stack usage. Note
 * that this will increase the footprint of the libraries since it
 * tracks the filename and line number of each functions.
 */
#ifndef BASE_OS_HAS_CHECK_STACK
#	define BASE_OS_HAS_CHECK_STACK		0
#endif

/**
 * Do we have alternate pool implementation?
 *
 * Default: 0
 */
#ifndef BASE_HAS_POOL_ALT_API
#define BASE_HAS_POOL_ALT_API	    BASE_POOL_DEBUG
#endif


/**
 * Support TCP in the library.
 * Disabling TCP will reduce the footprint slightly (about 6KB).
 *
 * Default: 1
 */
#ifndef BASE_HAS_TCP
#  define BASE_HAS_TCP		    1
#endif

/**
 * Support IPv6 in the library. If this support is disabled, some IPv6 
 * related functions will return BASE_EIPV6NOTSUP.
 *
 * Default: 0 (disabled, for now)
 */
#ifndef BASE_HAS_IPV6
#  define BASE_HAS_IPV6		    0
#endif

 /**
 * Maximum hostname length.
 * Libraries sometimes needs to make copy of an address to stack buffer;
 * the value here affects the stack usage.
 *
 * Default: 128
 */
#ifndef BASE_MAX_HOSTNAME
#  define BASE_MAX_HOSTNAME	    (128)
#endif

/**
 * Maximum consecutive identical error for accept() operation before
 * activesock stops calling the next ioqueue accept.
 *
 * Default: 50
 */
#ifndef BASE_ACTIVESOCK_MAX_CONSECUTIVE_ACCEPT_ERROR
#   define BASE_ACTIVESOCK_MAX_CONSECUTIVE_ACCEPT_ERROR 50
#endif

/**
 * Constants for declaring the maximum handles that can be supported by
 * a single IOQ framework. This constant might not be relevant to the 
 * underlying I/O queue impelementation, but still, developers should be 
 * aware of this constant, to make sure that the program will not break when
 * the underlying implementation changes.
 */
#ifndef BASE_IOQUEUE_MAX_HANDLES
#   define BASE_IOQUEUE_MAX_HANDLES	(64)
#endif


/**
 * If BASE_IOQUEUE_HAS_SAFE_UNREG macro is defined, then ioqueue will do more
 * things to ensure thread safety of handle unregistration operation by
 * employing reference counter to each handle.
 *
 * In addition, the ioqueue will preallocate memory for the handles, 
 * according to the maximum number of handles that is specified during 
 * ioqueue creation.
 *
 * All applications would normally want this enabled, but you may disable
 * this if:
 *  - there is no dynamic unregistration to all ioqueues.
 *  - there is no threading, or there is no preemptive multitasking.
 *
 * Default: 1
 */
#ifndef BASE_IOQUEUE_HAS_SAFE_UNREG
#   define BASE_IOQUEUE_HAS_SAFE_UNREG	1
#endif


/**
 * Default concurrency setting for sockets/handles registered to ioqueue.
 * This controls whether the ioqueue is allowed to call the key's callback
 * concurrently/in parallel. The default is yes, which means that if there
 * are more than one pending operations complete simultaneously, more
 * than one threads may call the key's callback at the same time. This
 * generally would promote good scalability for application, at the 
 * expense of more complexity to manage the concurrent accesses.
 *
 * Please see the ioqueue documentation for more info.
 */
#ifndef BASE_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY
#   define BASE_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY   1
#endif


/* Sanity check:
 *  if ioqueue concurrency is disallowed, BASE_IOQUEUE_HAS_SAFE_UNREG
 *  must be enabled.
 */
#if (BASE_IOQUEUE_DEFAULT_ALLOW_CONCURRENCY==0) && (BASE_IOQUEUE_HAS_SAFE_UNREG==0)
#   error BASE_IOQUEUE_HAS_SAFE_UNREG must be enabled if ioqueue concurrency \
	  is disabled
#endif


/**
 * When safe unregistration (BASE_IOQUEUE_HAS_SAFE_UNREG) is configured in
 * ioqueue, the BASE_IOQUEUE_KEY_FREE_DELAY macro specifies how long the
 * ioqueue key is kept in closing state before it can be reused.
 *
 * The value is in miliseconds.
 *
 * Default: 500 msec.
 */
#ifndef BASE_IOQUEUE_KEY_FREE_DELAY
#   define BASE_IOQUEUE_KEY_FREE_DELAY	500
#endif


/**
 * Determine if FD_SETSIZE is changeable/set-able. If so, then we will
 * set it to BASE_IOQUEUE_MAX_HANDLES. Currently we detect this by checking
 * for Winsock.
 */
#ifndef BASE_FD_SETSIZE_SETABLE
#   if (defined(BASE_HAS_WINSOCK_H) && BASE_HAS_WINSOCK_H!=0) || \
       (defined(BASE_HAS_WINSOCK2_H) && BASE_HAS_WINSOCK2_H!=0)
#	define BASE_FD_SETSIZE_SETABLE	1
#   else
#	define BASE_FD_SETSIZE_SETABLE	0
#   endif
#endif

/**
 * Overrides FD_SETSIZE so it is consistent throughout the library.
 * We only do this if we detected that FD_SETSIZE is changeable. If
 * FD_SETSIZE is not set-able, then BASE_IOQUEUE_MAX_HANDLES must be
 * set to value lower than FD_SETSIZE.
 */
#if BASE_FD_SETSIZE_SETABLE
    /* Only override FD_SETSIZE if the value has not been set */
#   ifndef FD_SETSIZE
#	define FD_SETSIZE		BASE_IOQUEUE_MAX_HANDLES
#   endif
#else
    /* When FD_SETSIZE is not changeable, check if BASE_IOQUEUE_MAX_HANDLES
     * is lower than FD_SETSIZE value.
     *
     * Update: Not all ioqueue backends require this (such as epoll), so
     * this check will be done on the ioqueue implementation itself, such as
     * ioqueue select.
     */
/*
#   ifdef FD_SETSIZE
#	if BASE_IOQUEUE_MAX_HANDLES > FD_SETSIZE
#	    error "BASE_IOQUEUE_MAX_HANDLES is greater than FD_SETSIZE"
#	endif
#   endif
*/
#endif


/**
 * Specify whether #benum_ip_interface() function should exclude
 * loopback interfaces.
 *
 * Default: 1
 */
#ifndef BASE_IP_HELPER_IGNORE_LOOPBACK_IF
#   define BASE_IP_HELPER_IGNORE_LOOPBACK_IF	1
#endif


/**
 * Has semaphore functionality?
 *
 * Default: 1
 */
#ifndef BASE_HAS_SEMAPHORE
#  define BASE_HAS_SEMAPHORE	    1
#endif


/**
 * Event object (for synchronization, e.g. in Win32)
 *
 * Default: 1
 */
#ifndef BASE_HAS_EVENT_OBJ
#  define BASE_HAS_EVENT_OBJ	    1
#endif


/**
 * Maximum file name length.
 */
#ifndef BASE_MAXPATH
#   define BASE_MAXPATH		    260
#endif


/**
 * Enable library's extra check.
 * If this macro is enabled, #BASE_ASSERT_RETURN macro will expand to
 * run-time checking. If this macro is disabled, #BASE_ASSERT_RETURN
 * will simply evaluate to #bassert().
 *
 * You can disable this macro to reduce size, at the risk of crashes
 * if invalid value (e.g. NULL) is passed to the library.
 *
 * Default: 1
 */
#ifndef BASE_ENABLE_EXTRA_CHECK
#   define BASE_ENABLE_EXTRA_CHECK    1
#endif


/**
 * Enable name registration for exceptions with #bexception_id_alloc().
 * If this feature is enabled, then the library will keep track of
 * names associated with each exception ID requested by application via
 * #bexception_id_alloc().
 *
 * Disabling this macro will reduce the code and .bss size by a tad bit.
 * See also #BASE_MAX_EXCEPTION_ID.
 *
 * Default: 1
 */
#ifndef BASE_HAS_EXCEPTION_NAMES
#   define BASE_HAS_EXCEPTION_NAMES   1
#endif

/**
 * Maximum number of unique exception IDs that can be requested
 * with #bexception_id_alloc(). For each entry, a small record will
 * be allocated in the .bss segment.
 *
 * Default: 16
 */
#ifndef BASE_MAX_EXCEPTION_ID
#   define BASE_MAX_EXCEPTION_ID      16
#endif

/**
 * Should we use Windows Structured Exception Handling (SEH) for the
 *  exceptions.
 *
 * Default: 0
 */
#ifndef BASE_EXCEPTION_USE_WIN32_SEH
#  define BASE_EXCEPTION_USE_WIN32_SEH 0
#endif

/**
 * Should we attempt to use Pentium's rdtsc for high resolution
 * timestamp.
 *
 * Default: 0
 */
#ifndef BASE_TIMESTAMP_USE_RDTSC
#   define BASE_TIMESTAMP_USE_RDTSC   0
#endif

/**
 * Is native platform error positive number?
 * Default: 1 (yes)
 */
#ifndef BASE_NATIVE_ERR_POSITIVE
#   define BASE_NATIVE_ERR_POSITIVE   1
#endif
 
/**
 * Include error message string in the library (extStrError()).
 * This is very much desirable!
 *
 * Default: 1
 */
#ifndef BASE_HAS_ERROR_STRING
#   define BASE_HAS_ERROR_STRING	    1
#endif


/**
 * Include bstricmp_alnum() and bstrnicmp_alnum(), i.e. custom
 * functions to compare alnum strings. On some systems, they're faster
 * then stricmp/strcasecmp, but they can be slower on other systems.
 * When disabled, libBase will fallback to stricmp/strnicmp.
 * 
 * Default: 0
 */
#ifndef BASE_HAS_STRICMP_ALNUM
#   define BASE_HAS_STRICMP_ALNUM	    0
#endif


/*
 * Types of QoS backend implementation.
 */

/** 
 * Dummy QoS backend implementation, will always return error on all
 * the APIs.
 */
#define BASE_QOS_DUMMY	    1

/** QoS backend based on setsockopt(IP_TOS) */
#define BASE_QOS_BSD	    2

/** QoS backend for Windows Mobile 6 */
#define BASE_QOS_WM	    3

/** QoS backend for Symbian */
#define BASE_QOS_SYMBIAN	    4

/** QoS backend for Darwin */
#define BASE_QOS_DARWIN	    5

/**
 * Force the use of some QoS backend API for some platforms.
 */
#ifndef BASE_QOS_IMPLEMENTATION
#   if defined(BASE_WIN32_WINCE) && BASE_WIN32_WINCE && _WIN32_WCE >= 0x502
	/* Windows Mobile 6 or later */
#	define BASE_QOS_IMPLEMENTATION    BASE_QOS_WM
#   elif defined(BASE_DARWINOS)
	/* Darwin OS (e.g: iOS, MacOS, tvOS) */
#	define BASE_QOS_IMPLEMENTATION    BASE_QOS_DARWIN
#   endif
#endif


/**
 * Enable secure socket. For most platforms, this is implemented using
 * OpenSSL or GnuTLS, so this will require one of those libraries to
 * be installed. For Symbian platform, this is implemented natively
 * using CSecureSocket.
 *
 * Default: 0 (for now)
 */
#ifndef BASE_HAS_SSL_SOCK
#  define BASE_HAS_SSL_SOCK	    0
#endif


/*
 * Secure socket implementation.
 * Select one of these implementations in BASE_SSL_SOCK_IMP.
 */
#define BASE_SSL_SOCK_IMP_NONE 	    0	/**< Disable SSL socket.    */
#define BASE_SSL_SOCK_IMP_OPENSSL	    1	/**< Using OpenSSL.	    */
#define BASE_SSL_SOCK_IMP_GNUTLS      2	/**< Using GnuTLS.	    */


/**
 * Select which SSL socket implementation to use. Currently libBase supports
 * BASE_SSL_SOCK_IMP_OPENSSL, which uses OpenSSL, and BASE_SSL_SOCK_IMP_GNUTLS,
 * which uses GnuTLS. Setting this to BASE_SSL_SOCK_IMP_NONE will disable
 * secure socket.
 *
 * Default is BASE_SSL_SOCK_IMP_NONE if BASE_HAS_SSL_SOCK is not set, otherwise
 * it is BASE_SSL_SOCK_IMP_OPENSSL.
 */
#ifndef BASE_SSL_SOCK_IMP
#   if BASE_HAS_SSL_SOCK==0
#	define BASE_SSL_SOCK_IMP		    BASE_SSL_SOCK_IMP_NONE
#   else
#	define BASE_SSL_SOCK_IMP		    BASE_SSL_SOCK_IMP_OPENSSL
#   endif
#endif


/**
 * Define the maximum number of ciphers supported by the secure socket.
 *
 * Default: 256
 */
#ifndef BASE_SSL_SOCK_MAX_CIPHERS
#  define BASE_SSL_SOCK_MAX_CIPHERS   256
#endif


/**
 * Specify what should be set as the available list of SSL_CIPHERs. For
 * example, set this as "DEFAULT" to use the default cipher list (Note:
 * PJSIP release 2.4 and before used this "DEFAULT" setting).
 *
 * Default: "HIGH:-COMPLEMENTOFDEFAULT"
 */
#ifndef BASE_SSL_SOCK_OSSL_CIPHERS
#  define BASE_SSL_SOCK_OSSL_CIPHERS   "HIGH:-COMPLEMENTOFDEFAULT"
#endif


/**
 * Define the maximum number of curves supported by the secure socket.
 *
 * Default: 32
 */
#ifndef BASE_SSL_SOCK_MAX_CURVES
#  define BASE_SSL_SOCK_MAX_CURVES   32
#endif


/**
 * Disable WSAECONNRESET error for UDP sockets on Win32 platforms. See
 * https://trac.extsip.org/repos/ticket/1197.
 *
 * Default: 1
 */
#ifndef BASE_SOCK_DISABLE_WSAECONNRESET
#   define BASE_SOCK_DISABLE_WSAECONNRESET    1
#endif


/**
 * Maximum number of socket options in bsockopt_params.
 *
 * Default: 4
 */
#ifndef BASE_MAX_SOCKOPT_PARAMS
#   define BASE_MAX_SOCKOPT_PARAMS	    4
#endif



/** @} */

/********************************************************************
 * General macros.
 */

/**
 * @defgroup bdll_target Building Dynamic Link Libraries (DLL/DSO)
 * @ingroup bconfig
 * @{
 *
 * The libraries support generation of dynamic link libraries for
 * Symbian ABIv2 target (.dso/Dynamic Shared Object files, in Symbian
 * terms). Similar procedures may be applied for Win32 DLL with some 
 * modification.
 *
 * Depending on the platforms, these steps may be necessary in order to
 * produce the dynamic libraries:
 *  - Create the (Visual Studio) projects to produce DLL output. 
 *    does not provide ready to use project files to produce DLL, so
 *    you need to create these projects yourself. For Symbian, the MMP
 *    files have been setup to produce DSO files for targets that 
 *    require them.
 *  - In the (Visual Studio) projects, some macros need to be declared
 *    so that appropriate modifiers are added to symbol declarations
 *    and definitions. Please see the macro section below for information
 *    regarding these macros. For Symbian, these have been taken care by the
 *    MMP files.
 *  - Some build systems require .DEF file to be specified when creating
 *    the DLL. 
 *
 * Macros related for building DLL/DSO files:
 *  - For platforms that supports dynamic link libraries generation,
 *    it must declare <tt>BASE_EXPORT_SPECIFIER</tt> macro which value contains
 *    the prefix to be added to symbol definition, to export this 
 *    symbol in the DLL/DSO. For example, on Win32/Visual Studio, the
 *    value of this macro is \a __declspec(dllexport), and for ARM 
 *    ABIv2/Symbian, the value is \a EXPORT_C. 
 *  - For platforms that supports linking with dynamic link libraries,
 *    it must declare <tt>BASE_IMPORT_SPECIFIER</tt> macro which value contains
 *    the prefix to be added to symbol declaration, to import this 
 *    symbol from a DLL/DSO. For example, on Win32/Visual Studio, the
 *    value of this macro is \a __declspec(dllimport), and for ARM 
 *    ABIv2/Symbian, the value is \a IMPORT_C. 
 *  - Both <tt>BASE_EXPORT_SPECIFIER</tt> and <tt>BASE_IMPORT_SPECIFIER</tt> 
 *    macros above can be declared in your \a config_site.h if they are not
 *    declared by libBase.
 *  - When  is built as DLL/DSO, both <tt>BASE_DLL</tt> and 
 *    <tt>BASE_EXPORTING</tt> macros must be declared, so that 
 *     <tt>BASE_EXPORT_SPECIFIER</tt> modifier will be added into function
 *    definition.
 *  - When application wants to link dynamically with , then it
 *    must declare <tt>BASE_DLL</tt> macro when using/including  header,
 *    so that <tt>BASE_IMPORT_SPECIFIER</tt> modifier is properly added into 
 *    symbol declarations.
 *
 * When <b>BASE_DLL</b> macro is not declared, static linking is assumed.
 *
 * For example, here are some settings to produce DLLs with Visual Studio
 * on Windows/Win32:
 *  - Create Visual Studio projects to produce DLL. Add the appropriate 
 *    project dependencies to avoid link errors.
 *  - In the projects, declare <tt>BASE_DLL</tt> and <tt>BASE_EXPORTING</tt> 
 *    macros.
 *  - Declare these macros in your <tt>config_site.h</tt>:
 \verbatim
	#define BASE_EXPORT_SPECIFIER  __declspec(dllexport)
	#define BASE_IMPORT_SPECIFIER  __declspec(dllimport)
 \endverbatim
 *  - And in the application (that links with the DLL) project, add 
 *    <tt>BASE_DLL</tt> in the macro declarations.
 */

/** @} */

/**
 * @defgroup bconfig Build Configuration
 * @{
 */

/**
 * @def BASE_INLINE(type)
 * @param type The return type of the function.
 * Expand the function as inline.
 */
#define BASE_INLINE(type)	  BASE_INLINE_SPECIFIER type

/**
 * This macro declares platform/compiler specific specifier prefix
 * to be added to symbol declaration to export the symbol when 
 * is built as dynamic library.
 *
 * This macro should have been added by platform specific headers,
 * if the platform supports building dynamic library target. 
 */
#ifndef BASE_EXPORT_DECL_SPECIFIER
#   define BASE_EXPORT_DECL_SPECIFIER
#endif


/**
 * This macro declares platform/compiler specific specifier prefix
 * to be added to symbol definition to export the symbol when 
 * is built as dynamic library.
 *
 * This macro should have been added by platform specific headers,
 * if the platform supports building dynamic library target. 
 */
#ifndef BASE_EXPORT_DEF_SPECIFIER
#   define BASE_EXPORT_DEF_SPECIFIER
#endif


/**
 * This macro declares platform/compiler specific specifier prefix
 * to be added to symbol declaration to import the symbol.
 *
 * This macro should have been added by platform specific headers,
 * if the platform supports building dynamic library target.
 */
#ifndef BASE_IMPORT_DECL_SPECIFIER
#   define BASE_IMPORT_DECL_SPECIFIER
#endif


/**
 * This macro has been deprecated. It will evaluate to nothing.
 */
#ifndef BASE_EXPORT_SYMBOL
#define BASE_EXPORT_SYMBOL(x)
#endif


/**
 * @def BASE_DECL(type)
 * @param type The return type of the function.
 * Declare a function.
 */
#if defined(BASE_DLL)
#   if defined(BASE_EXPORTING)
#	define BASE_DECL(type)	    BASE_EXPORT_DECL_SPECIFIER type
#   else
#	define BASE_DECL(type)	    BASE_IMPORT_DECL_SPECIFIER type
#   endif
#elif !defined(BASE_DECL)
#   if defined(__cplusplus)
#	define BASE_DECL(type)	    type
#   else
#	define BASE_DECL(type)	    extern type
#   endif
#endif


/**
 * @def BASE_DEF(type)
 * @param type The return type of the function.
 * Define a function.
 */
#if defined(BASE_DLL) && defined(BASE_EXPORTING)
#   define BASE_DEF(type)		    BASE_EXPORT_DEF_SPECIFIER type
#elif !defined(BASE_DEF)
#   define BASE_DEF(type)		    type
#endif


/**
 * @def BASE_DECL_NO_RETURN(type)
 * @param type The return type of the function.
 * Declare a function that will not return.
 */
/**
 * @def BASE_IDECL_NO_RETURN(type)
 * @param type The return type of the function.
 * Declare an inline function that will not return.
 */
/**
 * @def BASE_BEGIN_DECL
 * Mark beginning of declaration section in a header file.
 */
/**
 * @def BASE_END_DECL
 * Mark end of declaration section in a header file.
 */
#ifdef __cplusplus
#  define BASE_DECL_NO_RETURN(type)   BASE_DECL(type) BASE_NORETURN
#  define BASE_IDECL_NO_RETURN(type)  BASE_INLINE(type) BASE_NORETURN
#  define BASE_BEGIN_DECL		    extern "C" {
#  define BASE_END_DECL		    }
#else
#  define BASE_DECL_NO_RETURN(type)   BASE_NORETURN BASE_DECL(type)
#  define BASE_IDECL_NO_RETURN(type)  BASE_NORETURN BASE_INLINE(type)
#  define BASE_BEGIN_DECL
#  define BASE_END_DECL
#endif



/**
 * @def BASE_DECL_DATA(type)
 * @param type The data type.
 * Declare a global data.
 */ 
#if defined(BASE_DLL)
#   if defined(BASE_EXPORTING)
#	define BASE_DECL_DATA(type)   BASE_EXPORT_DECL_SPECIFIER extern type
#   else
#	define BASE_DECL_DATA(type)   BASE_IMPORT_DECL_SPECIFIER extern type
#   endif
#elif !defined(BASE_DECL_DATA)
#   define BASE_DECL_DATA(type)	    extern type
#endif


/**
 * @def BASE_DEF_DATA(type)
 * @param type The data type.
 * Define a global data.
 */ 
#if defined(BASE_DLL) && defined(BASE_EXPORTING)
#   define BASE_DEF_DATA(type)	    BASE_EXPORT_DEF_SPECIFIER type
#elif !defined(BASE_DEF_DATA)
#   define BASE_DEF_DATA(type)	    type
#endif


/**
 * @def BASE_IDECL(type)
 * @param type  The function's return type.
 * Declare a function that may be expanded as inline.
 */
/**
 * @def BASE_IDEF(type)
 * @param type  The function's return type.
 * Define a function that may be expanded as inline.
 */

#if BASE_FUNCTIONS_ARE_INLINED
#  define BASE_IDECL(type)  BASE_INLINE(type)
#  define BASE_IDEF(type)   BASE_INLINE(type)
#else
#  define BASE_IDECL(type)  BASE_DECL(type)
#  define BASE_IDEF(type)   BASE_DEF(type)
#endif


/**
 * @def BASE_UNUSED_ARG(arg)
 * @param arg   The argument name.
 * BASE_UNUSED_ARG prevents warning about unused argument in a function.
 */
#define BASE_UNUSED_ARG(arg)  (void)arg

/**
 * @def BASE_TODO(id)
 * @param id    Any identifier that will be printed as TODO message.
 * BASE_TODO macro will display TODO message as warning during compilation.
 * Example: BASE_TODO(CLEAN_UP_ERROR);
 */
#ifndef BASE_TODO
#  define BASE_TODO(id)	    TODO___##id:
#endif

/**
 * Simulate race condition by sleeping the thread in strategic locations.
 * Default: no!
 */
#ifndef BASE_RACE_ME
#  define BASE_RACE_ME(x)
#endif

/**
 * Function attributes to inform that the function may throw exception.
 *
 * @param x     The exception list, enclosed in parenthesis.
 */
#define __bthrow__(x)

/** @} */

/********************************************************************
 * Sanity Checks
 */
#ifndef BASE_HAS_HIGH_RES_TIMER
#  error "BASE_HAS_HIGH_RES_TIMER is not defined!"
#endif

#if !defined(BASE_HAS_PENTIUM)
#  error "BASE_HAS_PENTIUM is not defined!"
#endif

#if !defined(BASE_IS_LITTLE_ENDIAN)
#  error "BASE_IS_LITTLE_ENDIAN is not defined!"
#endif

#if !defined(BASE_IS_BIG_ENDIAN)
#  error "BASE_IS_BIG_ENDIAN is not defined!"
#endif

#if !defined(BASE_EMULATE_RWMUTEX)
#  error "BASE_EMULATE_RWMUTEX should be defined in compat/os_xx.h"
#endif

#if !defined(BASE_THREAD_SET_STACK_SIZE)
#  error "BASE_THREAD_SET_STACK_SIZE should be defined in compat/os_xx.h"
#endif

#if !defined(BASE_THREAD_ALLOCATE_STACK)
#  error "BASE_THREAD_ALLOCATE_STACK should be defined in compat/os_xx.h"
#endif

BASE_BEGIN_DECL

/**  version major number. */
#define BASE_VERSION_NUM_MAJOR	2

/**  version minor number. */
#define BASE_VERSION_NUM_MINOR	9

/**  version revision number. */
#define BASE_VERSION_NUM_REV      0

/**
 * Extra suffix for the version (e.g. "-trunk"), or empty for
 * web release version.
 */
#define BASE_VERSION_NUM_EXTRA	""

/**
 *  version number consists of three bytes with the following format:
 * 0xMMIIRR00, where MM: major number, II: minor number, RR: revision
 * number, 00: always zero for now.
 */
#define BASE_VERSION_NUM	((BASE_VERSION_NUM_MAJOR << 24) |	\
			 (BASE_VERSION_NUM_MINOR << 16) | \
			 (BASE_VERSION_NUM_REV << 8))

/**
 *  version string constant. @see bget_version()
 */
extern	const char*	BASE_VERSION;

/**
 * Get  version string.
 *
 * @return #BASE_VERSION constant.
 */
const char* bget_version(void);

/**
 * Dump configuration to log with verbosity equal to info(3).
 */
void bConfigDump(void);

BASE_END_DECL


#endif

