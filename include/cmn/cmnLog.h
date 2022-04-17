/*
* $Id: cmn_utils.h,v 1.8 2007/03/25 08:32:47 lizhijie Exp $
*/

#ifndef  __CMN_LOG_H__
#define  __CMN_LOG_H__

#include <stdio.h>
#include "compact.h"

#define CMN_LOG_0      (16<<3) /* facility not used */
#define CMN_LOG_1      (17<<3) /* facility not used */
#define CMN_LOG_2      (18<<3) /* facility not used*/
#define CMN_LOG_3      (19<<3) /* facility not used */
#define CMN_LOG_4      (20<<3) /* facility for cdr */
#define CMN_LOG_5      (21<<3) /* facility for sipserver */
#define CMN_LOG_6      (22<<3) /* facility for cgi */
#define CMN_LOG_7      (23<<3) /* facility for pbx */

#define CMN_LOG_UNKNOW   0   /* facility for unknow */

typedef unsigned int log_facility_t;

typedef enum
{
	USE_SYSLOG = 0,		/* log messages to syslog */
	USE_CONSOLE ,		/* log messages to console */
	USE_FILE			/* log messages into a normal disk file */

}log_style_t;

//#ifdef LOG_EMERG
#if 1
	typedef enum
	{
		CMN_LOG_EMERG = 0,		/* system is unusable */
		CMN_LOG_ALERT,			/* action must be taken immediately */
		CMN_LOG_CRIT ,			/* critical conditions */
		CMN_LOG_ERR , 			/* error conditions */
		CMN_LOG_WARNING ,		/* warning conditions */
		CMN_LOG_NOTICE ,		/* normal but significant condition */
		CMN_LOG_INFO,			/* informational */
		CMN_LOG_DEBUG			/* 7 : debug-level messages */
	}cmn_log_level_t;
#else
	typedef enum
	{
		LOG_EMERG = 0,       /* system is unusable */
		LOG_ALERT,       	     /* action must be taken immediately */
		LOG_CRIT ,              /* critical conditions */
		LOG_ERR ,       /* error conditions */
		LOG_WARNING , /* warning conditions */
		LOG_NOTICE ,     /* normal but significant condition */
		LOG_INFO ,   /* informational */
		LOG_DEBUG      /* debug-level messages */
		
	}cmn_log_level_t;

#endif

#define	CMN_PROGRAM_NAME_LENGTH			64
struct log_object
{
	char			name[CMN_PROGRAM_NAME_LENGTH];
	log_style_t 	lstyle;
	cmn_log_level_t 	llevel;
	log_facility_t 	lfacility;
	int			isDaemonized;		/* 1 : daemonized; 0 : front-end */

	/* following 2 fields are added by lizhijie, 2008.08.07 */
	int			maxSize;			/* in byte */
	int			offset;				/* current offset of write */
	
	char			logFileName[256];

	FILE			*fp;
};

typedef struct log_object log_stru_t;

#ifndef   __CMN_RELEASE__
void log_information(int pri, const char* file, int line, const char* frmt,...);
#else
void log_information(int pri, const char* frmt,...);
#endif

cmn_log_level_t get_current_level();


int cmn_log_init(log_stru_t *lobj);


#if CMN_SHARED_DEBUG
	#ifndef	TRACE
	#define	TRACE()	CMN_MSG_LOG(CMN_LOG_DEBUG,"" )
	#endif
#else
	#ifndef	TRACE
	#define	TRACE()	do{}while(0)
	#endif
#endif



/*
* ESC (27, 0x1b) charactor is '\e' or '\x1b'
*/
#define ANSI_COLOR_RED				"\x1b[31m"	/* ESC[31m : red */
#define ANSI_COLOR_GREEN			"\x1b[32m"
#define ANSI_COLOR_YELLOW			"\x1b[33m"
#define ANSI_COLOR_BLUE			"\x1b[34m"
#define ANSI_COLOR_MAGENTA		"\x1b[35m"
#define ANSI_COLOR_CYAN			"\x1b[36m"
#define ANSI_COLOR_RESET			"\x1b[0m"

#if 1//def	MSC_VER
#define	ERROR_TEXT_BEGIN			""ANSI_COLOR_RED""
#define	ERROR_TEXT_END				ANSI_COLOR_RESET
#else
#define	ERROR_TEXT_BEGIN			"\t\e[31m"
#define	ERROR_TEXT_END				"\e[0m"
#endif

#define	WARN_TEXT_BEGIN			""ANSI_COLOR_MAGENTA""

#define	INFO_TEXT_BEGIN			""ANSI_COLOR_YELLOW""


#define	FILE_TO_STDOUT			0

#ifndef   __CMN_RELEASE__
#if FILE_TO_STDOUT
#define	CMN_MSG_LOG(level, format ,...)   \
	do{ char buf[4096]; snprintf(buf, sizeof(buf), ERROR_TEXT_BEGIN  "%s.[ERR, %s]:[%s-%u.%s()]: %s" ERROR_TEXT_END "\n", sysTimestamp(), cmnThreadGetName(), __FILENAME__, __LINE__, __FUNCTION__, format ) ;\
		printf(buf, ##__VA_ARGS__);			\
		}while(0)
#else
#define	CMN_MSG_LOG(level, format ,...)   \
	do{ 					\
		if ( level <= CMN_LOG_DEBUG && get_current_level() >= level) \
			log_information(level, __FILENAME__, __LINE__, format, ##__VA_ARGS__);	\
	}while(0)
#endif
#else
#define	CMN_MSG_LOG(level, format ,...) \
	do{ 					\
		if ( level <= CMN_LOG_DEBUG && get_current_level() >= level) \
			log_information(level, format, ##__VA_ARGS__);	\
	}while(0)
#endif

#ifndef   __CMN_RELEASE__
#if FILE_TO_STDOUT
#define	CMN_MSG_INFO(level, format ,...)   \
	do{ char buf[4096]; snprintf(buf, sizeof(buf), INFO_TEXT_BEGIN"%s.[INFO,%s]:[%s-%u.%s]: %s"ANSI_COLOR_RESET "\n", sysTimestamp(), cmnThreadGetName(),  __FILENAME__, __LINE__, __FUNCTION__, format ) ;\
		printf(buf, ##__VA_ARGS__);			\
		}while(0)
#else
#define	CMN_MSG_INFO(level, format ,...)   \
	do{ 					\
		if ( level <= CMN_LOG_DEBUG && get_current_level() >= level) \
			log_information(level, __FILENAME__, __LINE__, format, ##__VA_ARGS__);	\
	}while(0)
#endif
#else
#define	CMN_MSG_INFO(level, format ,...) \
	do{ 					\
		if ( level <= CMN_LOG_DEBUG && get_current_level() >= level) \
			log_information(level, format, ##__VA_ARGS__);	\
	}while(0)
#endif


#define	CMN_ABORT(format ,...)		\
			{CMN_MSG_LOG(CMN_LOG_ERR, format, ##__VA_ARGS__); CMN_MSG_LOG(CMN_LOG_ERR,"Program exit now!!!");exit(1);/*abort();*/}

#ifndef   __CMN_RELEASE__
#if FILE_TO_STDOUT
#define	CMN_MSG_DEBUG(level, format ,...) 	 \
	do{ char buf[4096]; snprintf(buf, sizeof(buf), "%s.[DBUG,%s]:[%s-%u.%s]: %s\n", sysTimestamp(), cmnThreadGetName(),  __FILENAME__, __LINE__, __FUNCTION__, format ) ;\
		printf(buf, ##__VA_ARGS__);			\
		}while(0)

	//printf(format, ##__VA_ARGS__)
#else
#define	CMN_MSG_DEBUG(level, format ,...) 	\
	do{ 					\
		if ( level <= CMN_LOG_DEBUG && get_current_level() >= level) \
			log_information(level, __FILENAME__, __LINE__, format, ##__VA_ARGS__);	\
	}while(0)
#endif

#else
#define	CMN_MSG_DEBUG(level, format ,...) 	{}
#endif


#define  CMN_DEBUG(...)		{CMN_MSG_DEBUG(CMN_LOG_DEBUG, __VA_ARGS__);}

#define  CMN_INFO(...)		{CMN_MSG_INFO(CMN_LOG_INFO, __VA_ARGS__);}

#define  CMN_WARN(...)		{CMN_MSG_INFO(CMN_LOG_WARNING, __VA_ARGS__);}

#define  CMN_ERROR(...)		{CMN_MSG_LOG(CMN_LOG_ERR, __VA_ARGS__);}


#include "debug.h"

void cmnThreadMask(char *threadName);


#endif

