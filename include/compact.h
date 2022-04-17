
#ifndef	__COMPACT_H__
#define	__COMPACT_H__

#ifdef __cplusplus
	extern "C"
	{
#endif

#ifdef EXT_EXPORTS
	#ifdef _WIN32
		#define EXT_API __declspec(dllexport)
	#else
		#define EXT_API
	#endif
#else
	#ifdef _WIN32
		#define EXT_API __declspec(dllimport)
	#else
		#define EXT_API
	#endif
		
#endif

#if 0
#ifdef __cplusplus
#define EXT_API extern "C"
#else
#define EXT_API
#endif
#endif

/* Macro for calling convention, if required */
#ifdef _WIN32
	//#define CAL_CONV
	#define CAL_CONV	__stdcall
#else
	#define CAL_CONV
#endif


// Check windows
#if _WIN32 || _WIN64
#if _WIN64
#define	ENV_64
#else
#define	ENV_32
#endif
#define	OS_WINDOWS
#else
#define	OS_LINUX		
#endif

// Check GCC
#if __GNUC__
#if __x86_64__ || __ppc64__
#define	ENV_64
#define	POINTRT_TO_INTEGER	long int
#else
#define	ENV_32
#define	POINTRT_TO_INTEGER	int
#endif
#endif


//#if(CPU == x86)
#if 1
	typedef unsigned char   uint8;
	typedef unsigned short  uint16;
	typedef unsigned long long uint64;

	typedef signed char   int8;
	typedef signed short  int16;
	typedef signed long long int64;

#ifndef __cplusplus
//	typedef unsigned char	bool;
#endif

	#ifdef __x86_64__
/*20111025: 64bit linux doesn't work is uint32 is unsigned long*/
		typedef unsigned int   uint32;
		typedef signed int   int32;
	#else
		typedef unsigned long   uint32;
		typedef signed long   int32;
	#endif
#endif

#ifndef	u_int32_t
typedef unsigned int    u_int32_t;
#endif

#ifndef	uint32_t
typedef unsigned int    uint32_t;
#endif

#ifndef	u_int16_t
typedef unsigned short  u_int16_t;
#endif

#ifndef	uint16_t
typedef unsigned short  uint16_t;
#endif

#ifndef	u_short
typedef unsigned short  u_short;
#endif

#ifndef	ushort
typedef unsigned short  ushort;
#endif

#ifndef	u_int8_t
typedef unsigned char   u_int8_t;
#endif

#ifndef	uint8_t
typedef unsigned char   uint8_t;
#endif

#ifndef	u_char
typedef unsigned char   u_char;
#endif


#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    (!FALSE)
#endif

#ifndef	false
#define	false								0
#endif

#ifndef	_MSC_VER
#ifndef	true
#define	true									(!false)
#endif
#endif

#ifndef	OK
#define	OK									0
#endif

#ifndef	FAIL
#define	FAIL								-1
#endif


/* defined in stdlib.h */
#ifndef	EXIT_FAILURE
#define	EXIT_FAILURE 			1
#endif

#ifndef	EXIT_SUCCESS
#define	EXIT_SUCCESS 			0
#endif

#ifndef EXT_FALSE
#define EXT_FALSE				0
#endif

#ifndef EXT_TRUE
#define EXT_TRUE				(!EXT_FALSE)
#endif

#define	CMN_NAME_LENGTH						256


#define KB(x)						((x) / 1024)
#define PERCENTAGE(x,total)		(((x) * 100) / (total))

#define	UNIT_OF_KILO					1024
#define	UNIT_OF_MEGA					(UNIT_OF_KILO*UNIT_OF_KILO)
#define	UNIT_OF_GIGA					(UNIT_OF_KILO*UNIT_OF_MEGA)

#define	UNIT_K_HZ						1000
#define	UNIT_M_HZ						((UNIT_K_HZ)*(UNIT_K_HZ))

#define	UNIT_B_HZ						((UNIT_K_HZ)*(UNIT_M_HZ))


#define	CMNLIB_OPS_DEBUG						0x01
#define	CMNLIB_OPS_OTHER						0x02



#ifdef __MINGW32__
#define	PRI_32				"l"
#else
#define	PRI_32				
#endif

#define	INVALIDATE_VALUE_U8						0xFF
#define	INVALIDATE_VALUE_U16						0xFFFF
#define	INVALIDATE_VALUE_U32						0xFFFFFFFF

#ifndef	__FILENAME__
#define	__FILENAME__ 	__FILE__
#endif

//#define __FILENAME__ (strrchr(__FILENAME__, '/') ? strrchr(__FILENAME__, '/') + 1 : __FILENAME__)
#define	EXT_NEW_LINE							"\r\n"


#if	defined(WIN32) || defined(__QNX__)
#ifndef strcasecmp
#define strcasecmp(a,b) _stricmp(a,b)
#endif
#ifndef strncasecmp
#define strncasecmp(a,b,c) _strnicmp(a,b,c)
#endif
#else
#endif

#ifdef WIN32
//#include "sharedLib.h"
#else
#define		_fileno				fileno
#define		_strnicmp			strncasecmp
#define		_stricmp			strcasecmp
#endif


/* Macros to provide branch prediction information to compiler */
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

/* Macro to check null expression and exit if true */


#ifdef __linux__
	#define MILLI_SECOND_SLEEP(exp)			usleep((exp)*1000);
#else
	#define MILLI_SECOND_SLEEP(exp)			Sleep(exp);
#endif


/* MAX / MIN are not commonly defined, but useful */
#ifndef MAX
#define MAX(a, b) \
	({ typeof (a) _a = (a); \
	   typeof (b) _b = (b); \
	   _a > _b ? _a : _b; })
#endif
#ifndef MIN
#define MIN(a, b) \
	({ typeof (a) _a = (a); \
	   typeof (b) _b = (b); \
	   _a < _b ? _a : _b; })
#endif



#ifdef __cplusplus
};
#endif


#endif

