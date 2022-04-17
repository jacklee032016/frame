/*
 * $Author: lizhijie $
 * $Id: cmn_utils.c,v 1.14 2007/03/25 08:32:47 lizhijie Exp $
*/
#include "_cmn.h"
#include "cmnLog.h"
#include "libCmn.h"

#include <stdio.h> 

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#ifndef	_MSC_VER
#include <unistd.h>
#include <syslog.h>
#include <sys/time.h>
#endif

#include <assert.h>

static const char* priname[] =
{
      "EMRG",
      "ALRT",
      "CRIT",
      "ERR ",
      "WARN",
      "NOTC",
      "INFO",
      "DBUG",
      0
};

static log_stru_t logobj = 
{
	/* in order to prompt by logobj when it is used by standalone program such as test */
#ifndef   __CMN_RELEASE__
	.lstyle		=	USE_CONSOLE,
	.llevel		= 	CMN_LOG_WARNING,
	.lfacility		=	CMN_LOG_DEBUG,
#else
	.lstyle		=	USE_SYSLOG,
	.llevel		= 	CMN_LOG_WARNING,
	.lfacility		=	CMN_LOG_UNKNOW,
#endif
	.isDaemonized	=	0,
	.fp			=	NULL
	
};


unsigned long		cmnSystemDebug = 0;

void cmn_daemon_init()
{
#ifndef	_MSC_VER
	pid_t pid;
	int fd;

	//printf("Daemonizing .....\n\n");
	if((pid = fork()) != 0)
		exit(0);
	
	setsid();	 //become session leader 
//	signal(SIGHUP,SIG_IGN);
	if((pid = fork()) != 0)
		exit(0); 
	
	if(chdir("/") < 0)
	{
		fprintf(stderr, "chdir / failed: %s\n\n", strerror(errno) );
	}
	umask(0);
	
	close(0);
	close(1);
	close(2);
	
	fd=open("/dev/null",O_RDWR);
	if(dup(0) < 0)
	{
		fprintf(stderr, "Dup file descriptor 0 failed: %s\n\n", strerror(errno) );
	}
		
	if(dup(1) < 0 )
	{
		fprintf(stderr, "Dup file descriptor 1 failed: %s\n\n", strerror(errno) );
	}
	if( dup(2) < 0 )
	{
		fprintf(stderr, "Dup file descriptor 2 failed: %s\n\n", strerror(errno) );
	}

	close(fd);
#endif	
}

int sys_log_init(log_facility_t facility)
{   
	log_facility_t temp = facility >> 3;
	if (temp >= 16 && temp <= 23)
		logobj.lfacility = facility;
	else
		return -1; // facility not exist

#ifndef	_MSC_VER
	openlog("LogUtils",  LOG_CONS, logobj.lfacility);
#endif

	return 0; //success
}

cmn_log_level_t get_current_level()
{
	return logobj.llevel;
}

int cmn_log_init(log_stru_t *lobj)
{
	int	isStdOut = 0;
	
	if (lobj->llevel < CMN_LOG_EMERG)
		;
	else if (lobj->llevel > CMN_LOG_DEBUG) 
		logobj.llevel= CMN_LOG_DEBUG;
	else 
		logobj.llevel = lobj->llevel;

	sprintf(logobj.name, "%s", lobj->name);
	logobj.lstyle = lobj->lstyle;
	logobj.isDaemonized = lobj->isDaemonized;

	logobj.offset = 0;
	if(lobj->maxSize < 0)
	{
		logobj.maxSize = UNIT_OF_KILO*256; 	/* default is 256K bytes */
	}
	else
	{
		logobj.maxSize = lobj->maxSize;
	}
	
	if (logobj.lstyle == USE_SYSLOG)
	{
#ifndef   __CMN_RELEASE__
		fprintf(stderr, "Use SysLog\n");
#endif
		return sys_log_init(lobj->lfacility);
	}
	else if(logobj.lstyle == USE_FILE)
	{
#ifndef   __CMN_RELEASE__
		if(! logobj.isDaemonized)
		{
			logobj.fp = stderr;
			isStdOut = 1;
		}
		else
		{
			logobj.fp = fopen( lobj->logFileName, "w+");
		}
#else
		logobj.fp = fopen( lobj->logFileName, "w+");
#endif
		
		if(!logobj.fp)
		{
			fprintf(stderr, "Log File '%s' initialized fail. %s\n\n",lobj->logFileName, strerror(errno) );
			logobj.fp = stderr;
			isStdOut = 1;
		}
#ifndef   __CMN_RELEASE__
		fprintf(stderr, "Use File '%s' as Log\n", lobj->logFileName);
#endif
	}
	else
	{
#ifndef   __CMN_RELEASE__
		fprintf(stderr, "Use stderr as Log\n");
#endif
		logobj.fp = stderr;
		isStdOut = 1;
	}
	
	if(!isStdOut && logobj.isDaemonized )
	{
		cmn_daemon_init();
	}


#ifndef   __CMN_RELEASE__
	fprintf(stderr, "Startup at %s\n", cmnTimestampStr() );
#endif
	//CMN_MSG_LOG(CMN_LOG_NOTICE, "%s startup at %s, Log Level is %s",logobj.name, date, priname[logobj.llevel] );

	return 0;
}

char *colorEscapes[] =
{
	ERROR_TEXT_BEGIN,	/* CMN_LOG_EMERG */
	ERROR_TEXT_BEGIN,	/* CMN_LOG_ALERT */
	ERROR_TEXT_BEGIN,	/* CMN_LOG_CRIT */
	ERROR_TEXT_BEGIN,	/* CMN_LOG_ERR */
	WARN_TEXT_BEGIN,	/* CMN_LOG_WARNING */
	INFO_TEXT_BEGIN,		/* CMN_LOG_NOTICE */
	INFO_TEXT_BEGIN,		/* CMN_LOG_INFO */
	""					/* CMN_LOG_DEBUG */
};


#ifndef   __CMN_RELEASE__
void log_information(int pri, const char* file, int line, const char* frmt,...)
#else
void log_information(int pri, const char* frmt,...)
#endif
{
#ifndef   __CMN_RELEASE__
	char buf[1024*10] = {0};
#else	
	char buf[1024] = {0};
#endif	
	int pri_;
	int ret = 0;
	char *_colorEsc = (pri>= CMN_LOG_EMERG && pri<= CMN_LOG_DEBUG )?colorEscapes[pri]:"";

//	return;

	va_list ap;
	va_start(ap, frmt);
	vsnprintf(buf, sizeof(buf), frmt, ap);
	va_end(ap);

//	if(logobj.fp == NULL)
//		logobj.fp = stderr;
	
	pri_ = (pri <= CMN_LOG_DEBUG) ? pri : CMN_LOG_DEBUG;
	
	if (logobj.lstyle == USE_SYSLOG)
	{
#ifdef	OS_WINDOWS
		printf( "[%s] : %s\n", priname[pri_], buf);
#else
#ifndef   __CMN_RELEASE__
		syslog(pri_, "%s [%s] : %s:%d | %s\n", cmnTimestampStr(), priname[pri_], file, line,buf);
#else
		syslog(pri_, "[%s] : %s\n", priname[pri_], buf);
#endif
#endif
	}
	else
	{
#ifndef   __CMN_RELEASE__
//		fprintf(stderr, "[%s] : %s:%d | %s \n", priname[pri_], file, line, buf);
		if(logobj.fp )
		{
			ret = fprintf(logobj.fp, "%s%s [%s,%s] : %s:%d | %s\n"ERROR_TEXT_END, _colorEsc, cmnTimestampStr(), priname[pri_], cmnThreadGetName(), file, line,buf);
//			assert(0);
		}
		else
		{
		
#if 1	
			/* it makes VID failed in HiPlayer  */
			ret = fprintf(stderr, "%s%s [%s,%s] : %s:%d | %s\n"ERROR_TEXT_END, _colorEsc, cmnTimestampStr(), priname[pri_], cmnThreadGetName(), file, line,buf);
#else
			ret = printf( "%s [%s] : %s:%d | %s\n", date, priname[pri_], file, line,buf);
#endif
		}
#else

		ret = fprintf(logobj.fp, "%s[%s,%s] : %s\n"ERROR_TEXT_END, _colorEsc, priname[pri_], cmnThreadGetName(), buf);
#endif
		if(logobj.fp != NULL)
			fflush(logobj.fp);	/* added this line, lizhijie, 2007.03.16 */
	}

	if(ret > 0 && logobj.lstyle != USE_SYSLOG)
	{
		logobj.offset += ret;
		if(logobj.offset > logobj.maxSize && logobj.fp)
		{
			fseek(logobj.fp, 0L, SEEK_SET);
			logobj.offset = 0;
		}
	}
	
}

int safe_open (const char *pathname,int flags)
{
   	int fd;

   	fd = open (pathname,flags);
   	if (fd < 0)
	{
		CMN_ERROR("While trying to open %s",pathname);
		if (flags & O_RDWR)
		{
			CMN_ERROR(" for read/write access");
		}
		else if (flags & O_RDONLY)
		{
			CMN_ERROR(" for read access");
		}
		else if (flags & O_WRONLY)
		{
			CMN_ERROR(" for write access");
		}
		
		CMN_ERROR(": %m\n");
		exit (EXIT_FAILURE);
	}
	return (fd);
}

void safe_rewind (int fd, int offset, const char *filename)
{
   	if (lseek(fd, offset,SEEK_SET) < 0)
	{
		CMN_ERROR( "While seeking to start of %s: %m",filename);
		exit (EXIT_FAILURE);
	}
}

void safe_read (int fd,const char *filename,void *buf, size_t count, int verbose)
{
   	ssize_t result;

   	result = read (fd,buf,count);
   	if (count != result)
	{
		if (verbose)
		{
			CMN_DEBUG("\n");
		}
		
		if (result < 0)
		{
			CMN_ERROR("While reading data from %s: %m",filename);
			exit (EXIT_FAILURE);
		}
		
		CMN_ERROR( "Short read count returned while reading from %s",filename);
		exit (EXIT_FAILURE);
	}
}


int cmn_count_file(const char *filename)
{
	FILE *file = NULL;
	long length = -1;

	/* open in read binary mode */
	file = fopen(filename, "rb");
	if (file == NULL)
	{
		CMN_ERROR("fopen file '%s' failed : %s",filename, strerror(errno));
		goto cleanup;
	}

	/* get the length */
	if (fseek(file, 0, SEEK_END) != 0)
	{
		CMN_ERROR("fseek file '%s' failed : %s",filename, strerror(errno));
		goto cleanup;
	}
	
	length = ftell(file);
	if (length < 0)
	{
		CMN_ERROR("ftell file '%s' failed : %s",filename, strerror(errno));
		goto cleanup;
	}
	if (fseek(file, 0, SEEK_SET) != 0)
	{
		CMN_ERROR("fseek file '%s' failed : %s",filename, strerror(errno));
		goto cleanup;
	}

cleanup:
	if (file != NULL)
	{
		fclose(file);
	}

	return length;
}

char *cmn_read_file(const char *filename, uint32_t *size)
{
	FILE *file = NULL;
	long length = 0;
	char *content = NULL;
	size_t read_chars = 0;

	/* open in read binary mode */
	file = fopen(filename, "rb");
	if (file == NULL)
	{
		CMN_ERROR("fopen file '%s' failed : %s",filename, strerror(errno));
		goto cleanup;
	}

	/* get the length */
	if (fseek(file, 0, SEEK_END) != 0)
	{
		CMN_ERROR("fseek file '%s' failed : %s",filename, strerror(errno));
		goto cleanup;
	}
	
	length = ftell(file);
	if (length < 0)
	{
		CMN_ERROR("ftell file '%s' failed : %s",filename, strerror(errno));
		goto cleanup;
	}
	if (fseek(file, 0, SEEK_SET) != 0)
	{
		CMN_ERROR("fseek file '%s' failed : %s",filename, strerror(errno));
		goto cleanup;
	}

	/* allocate content buffer */
	content = (char*)malloc((size_t)length + sizeof(""));
	if (content == NULL)
	{
		goto cleanup;
	}

	/* read the file into memory */
	read_chars = fread(content, sizeof(char), (size_t)length, file);
	if ((long)read_chars != length)
	{
		CMN_ERROR("fread file '%s' failed : %s",filename, strerror(errno));
		free(content);
		content = NULL;
		goto cleanup;
	}
	content[read_chars] = '\0';
	
	*size = length;

cleanup:
	if (file != NULL)
	{
		fclose(file);
	}

	return content;
}

int cmn_write_file(const char *filename, void *data, uint32_t size)
{
   	ssize_t result;
	int fd;
#if 1	
	fd = safe_open(filename, O_WRONLY|O_CREAT);
#else
	/* open has 2 forms to call */
   	fd = open (filename, O_WRONLY|O_CREAT, S_IRWXU);
#endif
   	if (fd < 0)
   	{
		CMN_ERROR("While writing data into %s: %m",filename);
		return -EXIT_FAILURE;
   	}

   	result = write (fd, data, size);
   	if (size != result)
	{
		if (result < 0)
		{
			CMN_ERROR("While writing data into %s: %m",filename);
			return -EXIT_FAILURE;
		}
		
		CMN_ERROR( "Short write count returned while writing into %s", filename);
	}
	
	return result;
}


