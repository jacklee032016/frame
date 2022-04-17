/*
* data struct of XM interface protocol
*/
#ifndef	__XM_H__
#define	 __XM_H__

#ifdef	__cplusplus
extern	"C" {
#endif

#include "xmMsgType.hpp"
#include "cJSON.h"

#define		XM_HEAD_FLAGS			0xFF
#define		XM_HEAD_VERSION			0x01

enum class XmServicePort
{
	TCP_UNICAST_PORT = 34567,
#if 1
//	UDP_BOARDCAST_PORT = 34570
	UDP_BOARDCAST_PORT = 34569
#else	
	UDP_BOARDCAST_PORT = 3600	/* test with my program in Linux */
#endif	
};

//#define	EXT_PACK_SET(bytes)		#pragma		pack(bytes)
//#define	EXT_PACK_RESET()			#pragma		pack()

#ifdef _MSC_VER
//#  define PACKED_STRUCT(name) \
//    __pragma(pack(push, 1)) struct name __pragma(pack(pop))

#define	PACK_ATTRIBUTE()		
    
#elif defined(__GNUC__)
//#  define PACKED_STRUCT(name) struct __attribute__((packed)) name
#define	PACK_ATTRIBUTE()		__attribute__((__packed__)) 
#endif

#pragma		pack(1)

typedef struct	_XmControlHeader	//PACK_ATTRIBUTE()
{
	unsigned char			flags;
	unsigned char			version;
	unsigned char			reserv0;
	unsigned char			reserv1;

	unsigned int			sessionId;
	unsigned int			sequenceNumber;

	unsigned char			totalPacket;
	unsigned char			currentPacket;
	unsigned short			msgId;

	unsigned int			dataLength;

//	void					*data;
}XmControlHeader;

#pragma		pack()

//void sdkPerror(const char *msg, int rc);


#define sdkPerror(rc, msg,... ) \
do{ char errbuf[BASE_ERR_MSG_SIZE]; extStrError((bstatus_t)rc, errbuf, sizeof(errbuf)); \
	BASE_ERROR( "%s: " msg, errbuf); }while(0)

char* iesGetStrFromJsonObject(cJSON* json, const char * key);
int iesJsonGetStrIntoString(cJSON* json, const char * key, char *value, int size);
int iseGetIntegerFromJsonObject(cJSON* json, const char * key);


#ifdef	__cplusplus
}
#endif

#endif

