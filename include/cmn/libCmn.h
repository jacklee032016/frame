/*
 * $Id$
*/

#ifndef __LIB_COMMON_H__
#define __LIB_COMMON_H__

#ifdef __cplusplus
	extern "C"
	{
#endif

#ifdef ARM
//#warning		"build for ARM platform"
#elif  X86
//#warning		"build for X86 platform"
#else
//#error	"unknowm platform, build stopped"
#endif


/* build options for all modules */
#define	CMN_SHARED_DEBUG						1

#define	CMN_TIMER_DEBUG						0

/* timer thread start up after daemonized */
#define	CMN_TIMER_START_DELAY				1

#define	CMN_THREAD_DEBUG						0

#define	DEBUG_CONFIG_FILE						0


#define	MUX_OPTIONS_DEBUG_IP_COMMAND		1

#define	MUX_OPTIONS_DEBUG_IPC_CLIENT		0



/* playlist which is added is only saved in testing. In release version, no any playlist is saved in configuration file */
#define	MUX_SAVE_PLAYLIST						0

/* Macros for constants */


#define	MUX_INVALIDATE_CHAR					0xFF
#define	MUX_INVALIDATE_SHORT					0xFFFF
#define	MUX_INVALIDATE_INTEGER				0xFFFFFFFF


/* board type, so libMux and libCmnSys can be customized */
#define	EXT_BOARD_TYPE			MUX_BOARD_TYPE_774

#if 1
#define	MUX_ATMEL_XPLAINED		1
#define	MUX_BOARD_768				2
#define	MUX_BOARD_774				3
#define	MUX_BOARD_767				4

//#define	MUX_BOARD				MUX_BOARD_774
//#define	MUX_BOARD				MUX_ATMEL_XPLAINED
#endif

#include "compact.h"

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#ifndef	OS_WINDOWS
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#else
//typedef	unsigned int		size_t;
#endif
#include <errno.h>

//#include "pthread.h"


#define	DEBUG_RAW_RW						1




/* define board, libMedia uses it, so defined here */
typedef	enum _MUX_BOARD_TYPE
{
	MUX_BOARD_TYPE_RX769 = 0,
	MUX_BOARD_TYPE_RX762,
	MUX_BOARD_TYPE_RX,

	/* */	
	MUX_BOARD_TYPE_774,
	MUX_BOARD_TYPE_767,
	MUX_BOARD_TYPE_768,
	MUX_BOARD_TYPE_UNKNOWN
}MUX_BOARD_TYPE;


typedef	enum _MEDIA_DEVICE_TYPE
{
	MEDIA_DEVICE_USBDISK = 0, 		/* USB disk is default device */
	MEDIA_DEVICE_SDCARD,
	MEDIA_DEVICE_FTP,
	MEDIA_DEVICE_UNKNOWN
}MEDIA_DEVICE_T;

/* also used as the stream index of capture media and capture feed s*/
typedef	enum _MUX_MEDIA_TYPE
{
	MUX_MEDIA_TYPE_VIDEO = 0,
	MUX_MEDIA_TYPE_AUDIO,
	MUX_MEDIA_TYPE_SUBTITLE,
	MUX_MEDIA_TYPE_ERROR,
}MUX_MEDIA_TYPE;


#define		STR_BOOL_VALUE(bValue)	\
			(((bValue)==0)?"NO":"YES")


#if	ARCH_ARM
#define	RUN_HOME_DIR						""
#define	RUN_HOME_DIR_TEMP				""
#else
//#define	RUN_HOME_DIR						ROOT_DIR"/releases"
#define	RUN_HOME_DIR						"./releases"
#define	RUN_HOME_DIR_TEMP				RUN_HOME_DIR"/x86"	/* this directory will not be packed into release version of ARM */
#endif

#define	CONFIG_FILE_HOME_PROJECT			RUN_HOME_DIR"/etc/mLab/"


#define	MUX_FACTORY_FLAGS_FILE			RUN_HOME_DIR"/var/factory.sh"

#ifndef EXT_CONFIG_FILE_PTPD
#define	EXT_CONFIG_FILE_PTPD				RUN_HOME_DIR"/etc/mLab/muxPtpd.cfg"
#endif

#define	EXT_PTP_OFFSET_FILE				RUN_HOME_DIR"/var/run/ptpOffset.dat"


#define	PTP_EXE								"muxPtpd"

#define ADD_ELEMENT(header, element)	\
	if (header == NULL){					\
		header  = element;				\
		element->next   = NULL;			\
	}									\
	else	{								\
		element->next   = header;		\
		header = element;				\
	}


#define REMOVE_ELEMENT(header, element, type)	\
{type **cp, *c1; cp = &header; \
	while ((*cp) != NULL){  c1 = *cp; \
		if (c1 == element ) \
		{ *cp = element->next;} \
		else	{ cp = &c1->next;} \
	}; }


#define	GET_BIT(value, bit)				(((value)>>(bit))&0x01)
#define	SET_BIT(value, bit)				((value) << (bit))



#define	CMN_SET_BIT(flags, bitPosition)	\
		flags |= SET_BIT(0x01, (bitPosition) ) 

#define	CMN_CLEAR_BIT(flags, bitPosition)	\
		flags &= ~(SET_BIT(0x01, (bitPosition) ) )	

#define	CMN_CHECK_BIT(flags, bitPosition)	\
		( (flags&SET_BIT(0x01,(bitPosition) ) )!=0 )

#define	CMN_SET_FLAGS(flags, value)	\
		flags |= (value) 

#define	CMN_CLEAR_FLAGS(flags, value)	\
		flags &= ~((value) ) )	

#define	CMN_CHECK_FLAGS(flags, value)	\
		( (flags&(value) )!=0 )


#include "cmnLog.h"




/* macros for version */
#define	EXT_VERSION_DOT(a, b, c)				a ##.## b ##.## c

#define	EXT_VERSION(a, b, c)					EXT_VERSION_DOT(a, b, c)

#define	EXT_STRINGIFY(s)         					EXT_TOSTRING(s)
#define	EXT_TOSTRING(s)						#s


#define	EXT_GLUE(a, b)							a ## b
#define	EXT_JOIN(a, b)							EXT_GLUE(a, b)

#define	EXT_VERSION_MAJOR						1
#define	EXT_VERSION_MINOR						1
#define	EXT_VERSION_REVISION					1

#define	BL_VERSION_MAJOR						0
#define	BL_VERSION_MINOR						1
#define	BL_VERSION_REVISION					1


#define	EXT_VERSION_INFO()					((EXT_VERSION_MAJOR<<16)|(EXT_VERSION_MINOR<<8)|(EXT_VERSION_REVISION))


#define	BL_VERSION_INFO()						((BL_VERSION_MAJOR<<16)|(BL_VERSION_MINOR<<8)|(BL_VERSION_REVISION))


#define	EXT_VERSION_INTEGER()					((EXT_VERSION_MAJOR)<<16 | (EXT_VERSION_MINOR)<<8 | (EXT_VERSION_REVISION))


#define	BL_VERSION_TOKEN						EXT_VERSION(BL_VERSION_MAJOR, BL_VERSION_MINOR, BL_VERSION_REVISION)
#define	EXT_VERSION_TOKEN					EXT_VERSION(EXT_VERSION_MAJOR, EXT_VERSION_MINOR, EXT_VERSION_REVISION)


/* only call EXT_STRINGFY, can't call EXT_TOSTRING; otherwise return string of 'EXT_VERSION_TOKEN' */
#define	EXT_VERSION_STRING		\
			EXT_STRINGIFY(EXT_VERSION_TOKEN)	
#define	BL_VERSION_STRING		\
			EXT_STRINGIFY(BL_VERSION_TOKEN)	


#define	BL_SYSTEM_NAME			""
#define	EXT_SYSTEM_NAME			""

//EXT_NEW_LINE EXT_NEW_LINE 

#define	BUILD_DATE_TIME			__DATE__" "__TIME__

#define	EXT_SYSTEM_STRING(sysName, verInfo) 		\
		EXT_NEW_LINE"" sysName" (" \
		"Version: " verInfo "; " \
		"Built: " BUILD_DATE_TIME ")"EXT_NEW_LINE


#define	EXT_OS_NAME		EXT_SYSTEM_STRING(EXT_SYSTEM_NAME, EXT_VERSION_STRING)

#define	EXT_BL_NAME		EXT_SYSTEM_STRING(BL_SYSTEM_NAME, BL_VERSION_STRING)


#define	EXT_TASK_CONSOLE				"console"
#define	EXT_TASK_MAC					"mac"	/* GMAC controller */

#define	EXT_TASK_HTTP					"httpd"
#define	EXT_TASK_HTTP_CLIENT			"hcd"
#define	EXT_TASK_SCHEDULE				"sched"	/* schedule task before HTTP Client */

#define	EXT_TASK_NAME					"poll"

#define	EXT_TASK_UDP_CMD_NAME		"udpd"

#define	EXT_TASK_RS232					"rs232"
#define	EXT_TASK_RESET					"reset"

/* following are not used */
#define	EXT_TASK_TELNET				"telnetd"
#define	EXT_TASK_SYS_CTRL				"sysd"



#define	EXT_DBG_TRACE					0x40U
#define	EXT_DBG_STATE					0x20U
#define	EXT_DBG_FRESH					0x10U
#define	EXT_DBG_HALT					0x08U


/* level used */
#define	EXT_DBG_TYPES_ON				EXT_DBG_ON
#define	EXT_DBG_MIN_LEVEL			EXT_DBG_LEVEL_ALL



#define	EXT_IPCMD_DEBUG						EXT_DBG_OFF

#define	SDP_CLIENT_DEBUG						EXT_DBG_ON





#define CMN_ARRAYSIZE(x)		(sizeof(x)/sizeof((x)[0]))


#define CFG_MAKEU32(a,b,c,d) (((int)((a) & 0xff) << 24) | \
                               ((int)((b) & 0xff) << 16) | \
                               ((int)((c) & 0xff) << 8)  | \
                                (int)((d) & 0xff))

#if IP_ADDRESS_IN_NET_ORDER	
/* IP address in string form of 'a.b.c.d' */
/* network/big byte-order: 
*  sent : a sent first, then b; 
*  storage: a saved in high address; d saved in lowest address
*/
#define CFG_MAKE_IP_ADDRESS(a,b,c,d) (((int)((d) & 0xff) << 24) | \
                               ((int)((c) & 0xff) << 16) | \
                               ((int)((b) & 0xff) << 8)  | \
                                (int)((a) & 0xff))
#else
#define CFG_MAKE_IP_ADDRESS(a,b,c,d) (((int)((a) & 0xff) << 24) | \
                               ((int)((b) & 0xff) << 16) | \
                               ((int)((c) & 0xff) << 8)  | \
                                (int)((d) & 0xff))
#endif


#define	CFG_SET_FLAGS(flags, value)	\
		flags |= (value) 

#define	CFG_CLEAR_FLAGS(flags, value)	\
		flags &= ~((value) ) 


#define	EXT_DHCP_IS_ENABLE(runCfg)		\
				( (runCfg)->netMode != EXT_FALSE )

#define	EXT_CFG_SET_DHCP(runCfg, value)	\
				{(runCfg)->netMode = value;}

/*
				{ if(value==0) {CFG_CLEAR_FLAGS((runCfg)->netMode,(EXT_IP_CFG_DHCP_ENABLE));} \
				else{CFG_SET_FLAGS((runCfg)->netMode, (EXT_IP_CFG_DHCP_ENABLE));} }
*/


/* add at the header of list */
#define ADD_ELEMENT(header, element)	\
	if (header == NULL){					\
		header  = element;				\
		element->next   = NULL;			\
	}									\
	else	{								\
		element->next   = header;		\
		header = element;				\
	}


/* add at the tail of list */
#define APPEND_ELEMENT(header, element, type)	\
	if ((header) == NULL){					\
		(header)  = (element);				\
		(element)->next   = NULL;			\
	}									\
	else	{ type *cur = (header), *_next = (header)->next; \
		while(_next){cur = _next; _next = cur->next;};		\
		cur->next = (element);				\
	}



#define REMOVE_ELEMENT(header, element, type)	\
{type **cp, *c1; cp = &header; \
	while ((*cp) != NULL){  c1 = *cp; \
		if (c1 == element ) \
		{ *cp = element->next;} \
		else	{ cp = &c1->next;} \
	}; }



#define		STR_BOOL_VALUE(bValue)	\
			(((bValue)==0)?"NO":"YES")


#define	IS_STRING_NULL( str)	\
	 ((str)==NULL) 


#define	IS_STRING_NULL_OR_ZERO( str)	\
	(((str)==NULL)||(!strlen((str))) )


#define	IS_STRING_EQUAL( str1, str2)	\
	( !strcasecmp((str1), (str2)))


int safe_open (const char *pathname,int flags);
void safe_rewind (int fd, int offset, const char *filename);
void safe_read (int fd,const char *filename,void *buf, size_t count, int verbose);


/* time for warm-reset, when both set to 0, it set to maxium: about 64 seconds */
int cmn_watchdog_enable(int seconds, int millseconds);
int cmn_watchdog_disable();
int cmn_watchdog_update();
void cmn_beep_long(int times);
void cmn_beep_short(int times);
int cmn_get_button_states(void);
int cmn_ver_log( char *modulename, char *info);
int cmn_ver_opt (int argc, char **argv, char *info);
int cmn_ver_debug( char *verInfo);

int cmn_write_serial_no (unsigned char *serNO);
int cmn_read_serial_no(unsigned char *read_serial_no, int length);

#define FLASH_SERIAL_OFFSET 			131072L		/* offset is 128 K bytes */
#define FLASH_SERIAL_DEVICE			"/dev/mtd0"	

/* cmd-line flags */
#define FLAG_NONE			0x00
#define FLAG_VERBOSE		0x01
#define FLAG_HELP			0x02
#define FLAG_FILENAME		0x04
#define FLAG_DEVICE			0x08

/* 6 blocks and 32K per block, total 192 K for bootloader partition in FLASH mtd 
* First 3 blocks are occupied by bootloader
*/
#define LOCK_START 			0
#define LOCK_SECTORS 		3


/* size of read/write buffer */
#define BUFSIZE 				(10 * 1024)
#define SERIAL_BUFSIZE 		20


#define	CMN_FLASH_MAGIC_START			0xAB
#define	CMN_FLASH_MAGIC_END			0xCD
#define	CMN_FLASH_MAGIC_LENGTH		2

#define	CMN_FLASH_ITEM_LENGTH			32
typedef	struct
{
	char		magicStart[CMN_FLASH_MAGIC_LENGTH];
	
	char		serialNo[CMN_FLASH_ITEM_LENGTH];
	char		userName[CMN_FLASH_ITEM_LENGTH];
	char		passwd[CMN_FLASH_ITEM_LENGTH];

	char		wanMac[CMN_FLASH_ITEM_LENGTH];
	char		wanIp[CMN_FLASH_ITEM_LENGTH];
	char		wanMask[CMN_FLASH_ITEM_LENGTH];

	char		lanMac[CMN_FLASH_ITEM_LENGTH];
	char		lanIp[CMN_FLASH_ITEM_LENGTH];
	char		lanMask[CMN_FLASH_ITEM_LENGTH];

	char		magicEnd[CMN_FLASH_MAGIC_LENGTH];
}CMN_FLASH_INFO_T;

extern	CMN_FLASH_INFO_T		factoryDefault;

int	cmn_default_write(CMN_FLASH_INFO_T *defaultInfo);
int	cmn_default_read(CMN_FLASH_INFO_T *defaultInfo);
void cmn_default_info_debug(CMN_FLASH_INFO_T *info);


#include "md5.h"
#include "base64.h"

#include <cmnOsPort.h>


/* return : 0 : success */
int	cmnLibInit(int	options);
void cmnLibDestroy(void);


#ifdef   __CMN_RELEASE__
#define	CMN_SHARED_INIT()	\
{ int options = 0;	\
	cmnLibInit(options);}
#else
#define	CMN_SHARED_INIT()	\
{ int options = 0;	\
	options |= CMNLIB_OPS_DEBUG;	\
	cmnLibInit(options);}
#endif	

uint64_t cmnGetTimeFromStartup(void);
uint64_t cmnGetTimeUs(void);

long cmnGetTimeMs(void);
char *cmnGetCtime1(char *buf2, size_t buf_size);

FILE *fopen_safe(const char *path, const char *mode);

void cmnDumpBuffer(const char *buff, size_t count, FILE* fp, int indent);

//void cmnHexDump(const uint8_t *buf, int size);
#define	cmnHexDump(buf, size)			cmnDumpBuffer(buf, size, stderr, 0)

#define	CMN_HEX_DUMP(buf, size, msg)		\
	do{CMN_INFO((msg)); cmnHexDump((buf), (size)); }while(0)
		

int	cmnParseGetHexIntValue(char *hexString);

unsigned int cmnMuxCRC32b(void *message, int len);

#include "cmnList.h"


#include "cJSON.h"

int cmnJsonGetIpAddress(cJSON* json, const char * key, uint32_t *ipAddress);
int cmnJsonGetStrIntoString(cJSON* json, const char * key, char *value, int size);
char* cmnGetStrFromJsonObject(cJSON* json, const char * key);
int cmnGetIntegerFromJsonObject(cJSON* json, const char * key);


void cmnForkCommand(const char *cmd);
int cmnKillProcess(char * processName);

/* parse configuration file */
void cmnParseGetArg(char *buf, int buf_size, const char **pp);
int cmnParseGetIntValue( const char **pp);
int cmnParseGetBoolValue( const char **pp);
int cmnParseGetIpAddress(struct in_addr *ipAddress, const char *p);



extern volatile int recvSigalTerminal;


#define	SYSTEM_SIGNAL_EXIT() \
		{ recvSigalTerminal = TRUE; \
		}

#define	SYSTEM_IS_EXITING() \
		(recvSigalTerminal == TRUE)


#if MUX_OPTIONS_DEBUG_IP_COMMAND			
#define	MUX_DEBUG_JSON_OBJ(obj)	\
	{ char *printedStr =cJSON_Print((obj));  CMN_DEBUG("JSON object %s :\n'%s'", #obj, printedStr);	\
		free(printedStr);}

#else
#define	MUX_DEBUG_JSON_OBJ(obj)					{}

#endif


char *cmn_read_file(const char *filename, uint32_t *size);
int cmn_write_file(const char *filename, void *data, uint32_t size);
int cmn_count_file(const char *filename);


//EXT_API 
char *cmnTimestampStr(void);


/* Definitions from include/linux/timerfd.h */
//#define TFD_TIMER_ABSTIME	(1 << 0)

#define	EXT_CLOCK_ID						CLOCK_REALTIME

#define	EXT_CLOCK_ID_MONO				CLOCK_MONOTONIC

int timerfd_create(int clockid, int flags);
int timerfd_set_time(int fd, long mstime);


char *strnstr(const char *haystack, const char *needle, size_t len);

#define	CMN_THREAD_NAME_MAIN				"muxMain"
#define	CMN_THREAD_NAME_TIMER				"muxTimer"
#define	CMN_THREAD_NAME_COMM				"muxBroker"
#define	CMN_THREAD_NAME_MANAGER			"muxManager"
#define	CMN_THREAD_NAME_SDP_MANAGER		"muxSdpMngr"
#define	CMN_THREAD_NAME_SDP_RECEIVER		"muxSdpRecv"
#define	CMN_THREAD_NAME_BUTTON				"muxButton"
#define	CMN_THREAD_NAME_LED					"muxLed"
#define	CMN_THREAD_NAME_POLL				"muxPoll"
#define	CMN_THREAD_NAME_TEST_PIN			"muxPinCtrl"
#define	CMN_THREAD_NAME_TX_FREQ				"muxTxFreq"


#define	WITH_ANCILLIARY_STREAM			0




const char *cmnSafeStrError(int errnum);



#ifdef __cplusplus
};
#endif

#endif

