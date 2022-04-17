/*
* local heade, internal in libCmb
*/
#ifndef	___CMN_H__
#define	___CMN_H__

/* local functions can be exported with it */
#define	BASE_EXPORTS

#include "config.h"
#include "compact.h"


typedef struct 
{
	int		type;
	char		*name;

	char		*value;
}TYPE_NAME_T;


extern	unsigned long		cmnSystemDebug;

#define	SYSTEM_DEBUG_GET()			(cmnSystemDebug)
#define	SYSTEM_DEBUG_SET(option)		(__test_bit((option), 	&cmnSystemDebug))
#define	SYSTEM_DEBUG_CHECK(bit)		(__test_bit((bit), 	&cmnSystemDebug))


#endif

