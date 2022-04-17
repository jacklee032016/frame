
/*
* common debug header, cross-platform
*/

#ifndef	__DEBUG_H__
#define	__DEBUG_H__

/** Debug level: ALL messages*/
#define	EXT_DBG_LEVEL_ALL				0x00
#define	EXT_DBG_LEVEL_INFO			0x01
#define	EXT_DBG_LEVEL_WARN			0x02
#define	EXT_DBG_LEVEL_ERROR			0x03
#define	EXT_DBG_LEVEL_FATAL			0x04


#define	EXT_DBG_MASK_LEVEL			0x04
#define	EXT_DBG_LEVEL_OFF				EXT_DBG_LEVEL_ALL


#define	EXT_DBG_ON					0x80U
#define	EXT_DBG_OFF					0x00U

/*
* ESC (27, 0x1b) charactor is '\e' or '\x1b'
*/
#ifdef	_MSC_VER

#define	EXT_COLOR_RED				""
#define	EXT_COLOR_GREEN			""
#define	EXT_COLOR_YELLOW			""
#define	EXT_COLOR_BLUE			""
#define	EXT_COLOR_MAGENTA		""
#define	EXT_COLOR_CYAN			""
#define	EXT_COLOR_RESET			""

#define	EXT_ERROR_TEXT_BEGIN			""
#define	EXT_ERROR_TEXT_END			""
#else
#define	EXT_COLOR_RED				"\x1b[31m"	/* ESC[31m : red */
#define	EXT_COLOR_GREEN			"\x1b[32m"
#define	EXT_COLOR_YELLOW			"\x1b[33m"
#define	EXT_COLOR_BLUE			"\x1b[34m"
#define	EXT_COLOR_MAGENTA		"\x1b[35m"
#define	EXT_COLOR_CYAN			"\x1b[36m"
#define	EXT_COLOR_RESET			"\x1b[0m"	/* for all colors, other than red, this must be used. April,15,2018. JL*/


#define	EXT_ERROR_TEXT_BEGIN			"\t\e[31m"
#define	EXT_ERROR_TEXT_END			"\e[0m"

#endif

#define	EXT_WARN_TEXT_BEGIN			""EXT_COLOR_MAGENTA"WARN:"

#define	EXT_INFO_TEXT_BEGIN			""EXT_COLOR_BLUE"INFO:"

#define	SYS_PRINT							printf

//#define	sysTaskName()		cmnThreadGetName()
//#define	sysTimestamp()		cmnTimestampStr()

#define	sysTaskName()			""
#define	sysTimestamp()			""

#if 0//ndef __EXT_RELEASE__
//	#define	EXT_PRINTF(x)						{printf x ;}
	
//	#define	EXT_DEBUGF(fmt,...)	{printf("[%s-%u] DEBUG: " fmt EXT_NEW_LINE, __FILENAME__, __LINE__, ## args);}
	#define	EXT_DEBUGF(debug,...)		do { \
                               if ( \
                                   ((debug) & EXT_DBG_ON) && \
                                   ((debug) & EXT_DBG_TYPES_ON) && \
                                   ((short)((debug) & EXT_DBG_MASK_LEVEL) >= EXT_DBG_MIN_LEVEL)) { \
                                 SYS_PRINT("%s [DBUG,%s]: [%s-%u.%s()]: " format EXT_NEW_LINE, sysTimestamp(),  sysTaskName(), __FILENAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
                                 if ((debug) & EXT_DBG_HALT) { \
                                   while(1); \
                                 } \
                               } \
                             } while(0)

                             
	#define	EXT_INFOF(format, ...)		{SYS_PRINT(EXT_COLOR_CYAN "%s [INFO,%s]: [%s-%u]:" format EXT_COLOR_RESET EXT_NEW_LINE, sysTimestamp(), sysTaskName(), __FILENAME__, __LINE__, ##__VA_ARGS__);}
	#define	EXT_WARNF(format, ...)		{SYS_PRINT(EXT_COLOR_BLUE "%s [WARN%s]: [%s-%u]:" format EXT_COLOR_RESET EXT_NEW_LINE, sysTimestamp(), sysTaskName(), __FILENAME__, __LINE__, ##__VA_ARGS__);}
	#define	EXT_ERRORF(format, ...)		{SYS_PRINT(EXT_ERROR_TEXT_BEGIN "%s [ERR, %s]: [%s-%u]:" format EXT_ERROR_TEXT_END  EXT_NEW_LINE, sysTimestamp(), sysTaskName(), __FILENAME__, __LINE__, ##__VA_ARGS__);}

//	#define	EXT_ASSERT(x)						{printf("Assertion \"%s\" failed at line %d in %s\n", x, __LINE__, __FILENAME__); while(1);}
	#define	EXT_ASSERT(x, format, ...)		{if((x)==0) {SYS_PRINT(EXT_ERROR_TEXT_BEGIN"%s %s: ASSERT: [%s-%u]:" format EXT_ERROR_TEXT_END EXT_NEW_LINE, sysTimestamp(),  sysTaskName(), __FILENAME__, __LINE__ , ##__VA_ARGS__);while(0){};}}
	#define	EXT_ABORT(fmt,... )				SYS_PRINT("%s %s: ABORT in [" __FILENAME__ "-%u]:" fmt EXT_NEW_LINE, sysTimestamp(), sysTaskName(), __LINE__, ##args );while(1){}

//	#define	TRACE()								_TRACE_OUT(EXT_NEW_LINE )
#else
//	#define	EXT_PRINTF(x)						{;}

	#define	EXT_DEBUGF(debug, format, ...)		{}
	#define	EXT_INFOF(format, ...)				{SYS_PRINT(EXT_COLOR_CYAN "%s [INFO, %s] "format EXT_COLOR_RESET EXT_NEW_LINE, sysTimestamp(), sysTaskName(), ##__VA_ARGS__ );}
	#define	EXT_WARNF(format, ...)				{SYS_PRINT(EXT_COLOR_CYAN "%s [WARN%s] "format EXT_COLOR_RESET EXT_NEW_LINE, sysTimestamp(), sysTaskName(), ##__VA_ARGS__ );}
	#define	EXT_ERRORF(format, ...)				{SYS_PRINT(EXT_ERROR_TEXT_BEGIN "%s [ERR, %s] "format EXT_ERROR_TEXT_END EXT_NEW_LINE, sysTimestamp(), sysTaskName(), ##__VA_ARGS__);}
	
//	#define	EXT_ASSERT(x)				{while (1);}
	#define	EXT_ASSERT(x, format, ...)				{}
	#define	EXT_ABORT(fmt,... )						{}

//	#define	TRACE()										{}
#endif

#define	_TRACE_OUT(format, ...)	\
			{SYS_PRINT("%s: [%s-%u.%s()]: "format,  sysTaskName(), __FILENAME__, __LINE__, __FUNCTION__, ##__VA_ARGS__); }


#define		MUX_DEBUG_HW_CTRL					EXT_DBG_OFF

#define		MUX_DEBUG_FPGA					EXT_DBG_ON


/* when RELEASE build, only omit the output  */
#define	EXT_DBG_ERRORF(message, expression, handler) do { if (!(expression)) { \
		_TRACE_OUT(message); handler;}} while(0)

#endif

