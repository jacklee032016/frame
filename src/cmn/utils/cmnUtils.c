
#include <string.h>
#include <stdio.h>

#include "_cmn.h"
#include "debug.h"

int	cmnTypeFindType(const TYPE_NAME_T *types, char *name)
{
	const TYPE_NAME_T *tmp = types;

	while(tmp->type != -1 && tmp->name != NULL ) 
	{
		if( !strcasecmp(tmp->name, name) )
		//if( strcasestr(tmp->name, name) )
		{		
			return tmp->type;
		}
		tmp++;
	}

#ifndef    __BASE_RELEASE__
	{
		EXT_ERRORF("Type of name %s is not found, the first item is :%d(%s)", name, types->type, types->name);
	}
#endif
	return INVALIDATE_VALUE_U32;
}


char *cmnTypeFindName(const TYPE_NAME_T *types, int type)
{
	const TYPE_NAME_T *tmp = types;

	if(type == INVALIDATE_VALUE_U8 || type == INVALIDATE_VALUE_U16 || type== INVALIDATE_VALUE_U32)
	{
		return "";
	}

	while(tmp->type != -1 && tmp->name != NULL ) 
	{
		if( tmp->type == type )
		{		
			return tmp->name;
		}
		tmp++;
	}


#ifndef   __BASE_RELEASE__
	{
		EXT_ERRORF("Type %d is not found, the first item is :%d(%s)", type, types->type, types->name);
	}
#endif
	return "Unknown";
}


void cmnDumpBuffer(const char *buff, size_t count, FILE* fp, int indent)
{
	size_t i, j, c;
	unsigned char printnext = EXT_TRUE;

	if (count % 16)
		c = count + (16 - count % 16);
	else
		c = count;

	for (i = 0; i < c; i++)
	{
		if (printnext)
		{
			printnext = EXT_FALSE;
#ifdef	_MSC_VER			
			fprintf(fp, "%*s%.4zu ", indent, "", i & 0xffff);
#else
			fprintf(fp, "%*s%.4u ", indent, "", i & 0xffff);
#endif
		}
		
		if (i < count)
		{
			fprintf(fp, "%3.2x", (unsigned char)buff[i] & 0xff);
		}
		else
		{
			fprintf(fp, "   ");
		}
		
		if (!((i + 1) % 8))
		{
			if ((i + 1) % 16)
			{
				fprintf(fp, " -");
			}
			else
			{
				fprintf(fp, "   ");
				for (j = i - 15; j <= i; j++)
				{
					if (j < count)
					{
						if ((buff[j] & 0xff) >= 0x20 && (buff[j] & 0xff) <= 0x7e)
						{
							fprintf(fp, "%c", buff[j] & 0xff);
						}
						else
						{
							fprintf(fp, ".");
						}
					}
					else
						fprintf(fp, " ");
				}
				
				fprintf(fp, "\n");
				printnext = EXT_TRUE;
			}
		}
	}
}

#if defined (WIN32) || defined (_WIN32_WCE)

#include <time.h>
#include <sys/timeb.h>

#if 0
#include <winsock2.h>
#else
#include <winsock.h>
#endif

int sharedGetTimeofDay (void *ttime, void *tz)
{
#if 1
	struct _timeb timebuffer;
#else
	struct __timeb32 timebuffer;
#endif
	struct timeval *tp = (struct timeval *) ttime;
#if 1	
	_ftime (&timebuffer);
#else
	_ftime32 (&timebuffer);
#endif
	tp->tv_sec = (long)timebuffer.time;
	tp->tv_usec = timebuffer.millitm * 1000;
	return 0;
}
#else
#include <sys/time.h>

int sharedGetTimeofDay (void *ttime, void *tz)
{
	struct timeval *tp = (struct timeval *) ttime;

	gettimeofday(tp, tz);
	return 0;
}
#endif

char *cmnTimestampStr(void)
{
#ifdef	_MSC_VER
	__time64_t long_time;
	struct tm ptm;
	errno_t err;
#else
	struct timeval tv;
	struct tm *ptm;
#endif
	static char timestamp[32] = {0};


#ifdef	_MSC_VER
	_time64( &long_time );
	err = _localtime64_s(&ptm, &long_time);
	if(err)
#else
	sharedGetTimeofDay((void *)&tv, NULL);
	ptm = localtime(&tv.tv_sec);
	if(ptm == NULL)
#endif
	{
#ifdef	_MSC_VER
		EXT_ERRORF("localtime failed:%d", GetLastError());
#else
		EXT_ERRORF("localtime failed: %m");
#endif
	}
	else
	{
#ifdef	_MSC_VER
		strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &ptm);
#else
		strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", ptm);
#endif
	}

	return timestamp;
}



#if 0
uint64_t cmnGetTimeFromStartup(void)
{
	struct timeval 	tv;
	static uint64_t 	_timeOffset;
	static int		_initTimeOffset = 0;

	gettimeofday(&tv,NULL);
	if(_initTimeOffset == 0)
	{
		_timeOffset = (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
		_initTimeOffset = 1;
		return (uint64_t) 0;
	}

	return ((uint64_t)tv.tv_sec * 1000000 + tv.tv_usec)-_timeOffset;
}
#endif

