/*
 * Memory management framework. This framework is used to find any memory leak.
 */

#include "config.h"

#ifdef _MEM_CHECK_
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <sys/stat.h>
#endif

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "_cmn.h"
#include "bitops.h"
#include "utils/cmnMem.h"
#include "cmnLog.h"

#ifdef _MEM_CHECK_
static
#endif
//void * __attribute__ ((malloc))
void * zalloc(unsigned long size);


#ifdef _MEM_CHECK_

#include "utils/cmnRbTree.h"
#include "utils/cmnListHead.h"

/* memory management. in debug mode,
 * help finding eventual memory leak.
 * Allocation memory types manipulated are :
 *
 * +-type------------------+-meaning------------------+
 * ! FREE_SLOT             ! Free slot                !
 * ! OVERRUN               ! Overrun                  !
 * ! FREE_NULL             ! free null                !
 * ! REALLOC_NULL          ! realloc null             !
 * ! DOUBLE_FREE           ! double free              !
 * ! REALLOC_DOUBLE_FREE   ! realloc freed block      !
 * ! FREE_NOT_ALLOC        ! Not previously allocated !
 * ! REALLOC_NOT_ALLOC     ! Not previously allocated !
 * ! MALLOC_ZERO_SIZE      ! malloc with size 0       !
 * ! REALLOC_ZERO_SIZE     ! realloc with size 0      !
 * ! LAST_FREE             ! Last free list           !
 * ! ALLOCATED             ! Allocated                !
 * +-----------------------+--------------------------+
 *
 * global variable debug bit MEM_ERR_DETECT_BIT used to
 * flag some memory error.
 *
 */


enum slot_type
{
	FREE_SLOT = 0,
	OVERRUN,
	FREE_NULL,
	REALLOC_NULL,
	DOUBLE_FREE,
	REALLOC_DOUBLE_FREE,
	FREE_NOT_ALLOC,
	REALLOC_NOT_ALLOC,
	MALLOC_ZERO_SIZE,
	REALLOC_ZERO_SIZE,
	LAST_FREE,
	ALLOCATED,
} ;

#define TIME_STR_LEN	9

#if ULONG_MAX == 0xffffffffffffffffUL
#define CHECK_VAL	0xa5a55a5aa5a55a5aUL
#elif ULONG_MAX == 0xffffffffUL
#define CHECK_VAL	0xa5a55a5aUL
#else
#define CHECK_VAL	0xa5a5
#endif

typedef struct
{
	enum slot_type	type;
	
	int				line;
	const char		*func;
	const char		*file;
	
	void				*ptr;
	size_t			size;

	union
	{
		list_head_t	l;	/* When on free list */
		rb_node_t	t;
	};
	
	unsigned			seqNum;
} MEMCHECK;

struct	_SYS_MEM_CTRL
{
//	LH_LIST_HEAD(freeList);
	struct list_head				freeList;

	unsigned						freeListSize;

/* alloc_list entries used for 1000 VRRP instance each with VMAC interfaces is 33589 */
	rb_root_t 					allocList;
//	LH_LIST_HEAD(badList);
	struct list_head				badList;

	unsigned						numberAllocList;	/* number of alloc_list allocation entries */
	unsigned						maxAllocList;
	unsigned						numMallocs;
	unsigned						numReallocs;
	
	unsigned						seqNum;

	FILE							*logFp;


	size_t						memAllocated;			/* Total memory used in Bytes */
	size_t						maxMemAllocated;	/* Maximum memory used in Bytes */

	const char					*terminateBanner;	/* banner string for report file */

	bool							skipMemCheckFinal;

};

struct	_SYS_MEM_CTRL		_sysMemCtrl;


static inline int __memcheckPtrCmp(const MEMCHECK *m1, const MEMCHECK *m2)
{
	return (char *)m1->ptr - (char *)m2->ptr;
}

static inline int __memcheckSeqCmp(const MEMCHECK *m1, const MEMCHECK *m2)
{
	return m1->seqNum - m2->seqNum;
}


static MEMCHECK *_getFreeAllocEntry(struct	_SYS_MEM_CTRL *memCtrl)
{
	MEMCHECK *entry;

	/* If number on free list < 256, allocate new entry, otherwise take head */
	if (memCtrl->freeListSize < 256)
	{
		entry = malloc(sizeof *entry);
	}
	else
	{
		entry = list_first_entry(&memCtrl->freeList, MEMCHECK, l);
		list_head_del(&entry->l);
		memCtrl->freeListSize--;
	}

	entry->seqNum = memCtrl->seqNum++;

	return entry;
}

static void *__mallocCommon(size_t size, const char *file, const char *function, int line, const char *name)
{
	struct	_SYS_MEM_CTRL *memCtrl = &_sysMemCtrl;
	void *buf;
	MEMCHECK *entry, *entry2;

	buf = zalloc(size + sizeof (unsigned long));

#ifndef _NO_UNALIGNED_ACCESS_
	*(unsigned long *) ((char *) buf + size) = size + CHECK_VAL;
#else
	unsigned long check_val = CHECK_VAL;

	memcpy((unsigned char *)buf + size, (unsigned char *)&check_val, sizeof(check_val));
#endif

	entry = _getFreeAllocEntry(memCtrl);

	entry->ptr = buf;
	entry->size = size;
	entry->file = file;
	entry->func = function;
	entry->line = line;
	entry->type = ALLOCATED;

	rb_insert_sort(&memCtrl->allocList, entry, t, __memcheckPtrCmp);
	if (++memCtrl->numberAllocList > memCtrl->maxAllocList)
	{
		memCtrl->maxAllocList = memCtrl->numberAllocList;
	}

	fprintf(memCtrl->logFp, "%s %s [%3u:%3u], %9p, %4zu at %s, %3d, %s%s\n",
	       cmnTimestampStr(), name, entry->seqNum, memCtrl->numberAllocList, buf, size, file, line, function, !size ? " - size is 0" : "");
	
#ifdef _MEM_CHECK_LOG_
	if (SYSTEM_DEBUG_CHECK(MEM_CHECK_LOG_BIT) )
	{
		CMN_INFO("%s[%3u:%3u], %9p, %4zu at %s, %3d, %s", 
		       name, entry->seqNum, memCtrl->numberAllocList, buf, size, file, line, function);
	}
#endif

	memCtrl->numMallocs++;

	if (!size)
	{
		/* Record malloc with 0 size */
		entry2 = _getFreeAllocEntry(memCtrl);
		*entry2 = *entry;
		entry2->type = MALLOC_ZERO_SIZE;
		
		list_add_tail(&entry2->l, &memCtrl->badList);
	}

	/* coverity[leaked_storage] */
	return buf;
}

static void *__freeReallocCommon(void *buffer, size_t size, const char *file, const char *function, int line, bool is_realloc)
{
	struct	_SYS_MEM_CTRL *memCtrl = &_sysMemCtrl;
	unsigned long check;
	MEMCHECK *entry, *entry2, *le;
	MEMCHECK search = {.ptr = buffer};
#ifdef _NO_UNALIGNED_ACCESS_
	unsigned long check_val = CHECK_VAL;
#endif

	/* If nullpointer remember */
	if (buffer == NULL)
	{
		entry = _getFreeAllocEntry(memCtrl);

		entry->ptr = NULL;
		entry->size = size;
		entry->file = file;
		entry->func = function;
		entry->line = line;
		entry->type = !size ? FREE_NULL : REALLOC_NULL;

		if (!is_realloc)
		{
			fprintf(memCtrl->logFp, "%s%-7s%9s, %9s, %4s at %s, %3d, %s\n", cmnTimestampStr(),
				"free", "ERROR", "NULL", "",
				file, line, function);
		}
		else
		{
			fprintf(memCtrl->logFp, "%s%-7s%9s, %9s, %4zu at %s, %3d, %s%s\n", cmnTimestampStr(),
				"realloc", "ERROR", "NULL",
				size, file, line, function, size ? " *** converted to malloc" : "");
		}

		SYSTEM_DEBUG_SET(MEM_ERR_DETECT_BIT);

		list_add_tail(&entry->l, &memCtrl->badList);

		return !size ? NULL : cmnMemMalloc(size, file, function, line);
	}

	entry = rb_search(&memCtrl->allocList, &search, t, __memcheckPtrCmp);

	/* Not found */
	if (!entry) {
		entry = _getFreeAllocEntry(memCtrl);

		entry->ptr = buffer;
		entry->size = size;
		entry->file = file;
		entry->func = function;
		entry->line = line;
		entry->type = !size ? FREE_NOT_ALLOC : REALLOC_NOT_ALLOC;
		entry->seqNum = memCtrl->seqNum++;

		if (!is_realloc)
		{
			fprintf(memCtrl->logFp, "%s%-7s%9s, %9p,      at %s, %3d, %s - not found\n", cmnTimestampStr(),
				"free", "ERROR", buffer, file, line, function);
		}
		else
		{
			fprintf(memCtrl->logFp, "%s%-7s%9s, %9p, %4zu at %s, %3d, %s - not found\n", cmnTimestampStr(),
				"realloc", "ERROR", buffer, size, file, line, function);
		}

		SYSTEM_DEBUG_SET(MEM_ERR_DETECT_BIT);

		list_for_each_entry_reverse(le, &memCtrl->freeList, l)
		{
			if (le->ptr == buffer && le->type == LAST_FREE)
			{
				fprintf(memCtrl->logFp, "%11s-> pointer last released at [%3u:%3u], at %s, %3d, %s\n",
				     "", le->seqNum, memCtrl->numberAllocList, le->file, le->line, le->func);

				entry->type = !size ? DOUBLE_FREE : REALLOC_DOUBLE_FREE;
				break;
			}
		};

		list_add_tail(&entry->l, &memCtrl->badList);

		/* coverity[leaked_storage] */
		return NULL;
	}

	check = entry->size + CHECK_VAL;
#ifndef _NO_UNALIGNED_ACCESS_
	if (*(unsigned long *)((char *)buffer + entry->size) != check)
#else
	if (memcmp((unsigned char *)buffer + entry->size, (unsigned char *)&check_val, sizeof(check_val)))
#endif
	{
		entry2 = _getFreeAllocEntry(memCtrl);

		*entry2 = *entry;
		entry2->type = OVERRUN;
		list_add_tail(&entry2->l, &memCtrl->badList);

		fprintf(memCtrl->logFp, "%s%s corrupt, buffer overrun [%3u:%3u], %9p, %4zu at %s, %3d, %s\n",
		       cmnTimestampStr(), !is_realloc ? "free" : "realloc",
		       entry->seqNum, memCtrl->numberAllocList, buffer,
		       entry->size, file,
		       line, function);
		cmnDumpBuffer(entry->ptr, entry->size + sizeof (check), memCtrl->logFp, TIME_STR_LEN);
		fprintf(memCtrl->logFp, "%*sCheck_sum\n", TIME_STR_LEN, "");
		cmnDumpBuffer((char *) &check,  sizeof(check), memCtrl->logFp, TIME_STR_LEN);

		SYSTEM_DEBUG_SET(MEM_ERR_DETECT_BIT);
	}

	memCtrl->memAllocated -= entry->size;

	if (!size)
	{
		free(buffer);

		if (is_realloc)
		{
			fprintf(memCtrl->logFp, "%s%-7s[%3u:%3u], %9p, %4zu at %s, %3d, %s -> %9s, %4s at %s, %3d, %s\n",
				cmnTimestampStr(), "realloc", entry->seqNum,
				memCtrl->numberAllocList, entry->ptr,
				entry->size, entry->file,
				entry->line, entry->func,
				"made free", "", file, line, function);

			/* Record bad realloc */
			entry2 = _getFreeAllocEntry(memCtrl);
			*entry2 = *entry;
			entry2->type = REALLOC_ZERO_SIZE;
			entry2->file = file;
			entry2->line = line;
			entry2->func = function;
			list_add_tail(&entry2->l, &memCtrl->badList);
		}
		else
		{
			fprintf(memCtrl->logFp, "%s%-7s[%3u:%3u], %9p, %4zu at %s, %3d, %s -> %9s, %4s at %s, %3d, %s\n",
				cmnTimestampStr(), "free", entry->seqNum,
				memCtrl->numberAllocList, entry->ptr,
				entry->size, entry->file,
				entry->line, entry->func,
				"NULL", "", file, line, function);
		}
#ifdef _MEM_CHECK_LOG_
		if (SYSTEM_DEBUG_CHECK(MEM_CHECK_LOG_BIT) )
		{
			CMN_INFO("%-7s[%3u:%3u], %9p, %4zu at %s, %3d, %s",
			       is_realloc ? "realloc" : "free",
			       entry->seqNum, memCtrl->numberAllocList, buffer,
			       entry->size, file, line, function);
		}
#endif

		entry->file = file;
		entry->line = line;
		entry->func = function;
		entry->type = LAST_FREE;

		rb_erase(&entry->t, &memCtrl->allocList);
		list_add_tail(&entry->l, &memCtrl->freeList);

		memCtrl->freeListSize++;
		memCtrl->numberAllocList--;

		return NULL;
	}

	buffer = realloc(buffer, size + sizeof (unsigned long));
	memCtrl->memAllocated += size;

	if (memCtrl->memAllocated > memCtrl->maxMemAllocated)
		memCtrl->maxMemAllocated = memCtrl->memAllocated;

	fprintf(memCtrl->logFp, "%srealloc[%3u:%3u], %9p, %4zu at %s, %3d, %s -> %9p, %4zu at %s, %3d, %s\n",
		cmnTimestampStr(), entry->seqNum,
		memCtrl->numberAllocList, entry->ptr,
		entry->size, entry->file,
		entry->line, entry->func,
		buffer, size, file, line, function);
#ifdef _MEM_CHECK_LOG_
	if (SYSTEM_DEBUG_CHECK(MEM_CHECK_LOG_BIT) )
	{
		CMN_INFO("realloc[%3u:%3u], %9p, %4zu at %s, %3d, %s -> %9p, %4zu at %s, %3d, %s",
		       entry->seqNum, memCtrl->numberAllocList, entry->ptr,
		       entry->size, entry->file,
		       entry->line, entry->func,
		       buffer, size, file, line, function);
	}
#endif

#ifndef _NO_UNALIGNED_ACCESS_
	*(unsigned long *) ((char *) buffer + size) = size + CHECK_VAL;
#else
	memcpy((unsigned char *)buffer + size, (unsigned char *)&check_val, sizeof(check_val));
#endif

	if (entry->ptr != buffer)
	{
		rb_erase(&entry->t, &memCtrl->allocList);
		entry->ptr = buffer;
		rb_insert_sort(&memCtrl->allocList, entry, t, __memcheckPtrCmp);
	}
	else
	{
		entry->ptr = buffer;
	}
	
	entry->size = size;
	entry->file = file;
	entry->line = line;
	entry->func = function;

	memCtrl->numReallocs++;

	/* coverity[leaked_storage] */
	return buffer;
}

static void _allocLog(bool final)
{
	struct	_SYS_MEM_CTRL *memCtrl = &_sysMemCtrl;

	unsigned int overrun = 0, badptr = 0, zero_size = 0;
	size_t sum = 0;
	MEMCHECK *entry;

	if (final)
	{
		/* If this is a forked child, we don't want the dump */
		if (memCtrl->skipMemCheckFinal )
			return;

		fprintf(memCtrl->logFp, "\n---[ memory dump for (%s) ]---\n\n", memCtrl->terminateBanner);
	}
	else
	{
		fprintf(memCtrl->logFp, "\n---[ memory dump for (%s) at %s ]---\n\n", memCtrl->terminateBanner, cmnTimestampStr());
	}

	/* List the blocks currently allocated */
	if (!RB_EMPTY_ROOT(&memCtrl->allocList) )
	{
		fprintf(memCtrl->logFp, "Entries %s\n\n", final ? "not released" : "currently allocated");
		rb_for_each_entry(entry, &memCtrl->allocList, t)
		{
			sum += entry->size;
			fprintf(memCtrl->logFp, "%9p [%3u:%3u], %4zu at %s, %3d, %s",
			       entry->ptr, entry->seqNum, memCtrl->numberAllocList,
			       entry->size, entry->file, entry->line, entry->func);
			if (entry->type != ALLOCATED)
			{
				fprintf(memCtrl->logFp, " type = %u", entry->type);
			}
			fprintf(memCtrl->logFp, "\n");
		}
	}

	if (!list_empty(&memCtrl->badList))
	{
		if (!RB_EMPTY_ROOT(&memCtrl->allocList) )
		{
			fprintf(memCtrl->logFp, "\n");
		}
		fprintf(memCtrl->logFp, "Bad entry list\n\n");

		list_for_each_entry(entry, &memCtrl->badList, l)
		{
			switch (entry->type)
			{
				case FREE_NULL:
					badptr++;
					fprintf(memCtrl->logFp, "%9s %9s, %4s at %s, %3d, %s - null pointer to free\n",
					       "NULL", "", "", entry->file, entry->line, entry->func);
					break;
				case REALLOC_NULL:
					badptr++;
					fprintf(memCtrl->logFp, "%9s %9s, %4zu at %s, %3d, %s - null pointer to realloc (converted to malloc)\n",
					     "NULL", "", entry->size, entry->file, entry->line, entry->func);
					break;
				case FREE_NOT_ALLOC:
					badptr++;
					fprintf(memCtrl->logFp, "%9p %9s, %4s at %s, %3d, %s - pointer not found for free\n",
					     entry->ptr, "", "", entry->file, entry->line, entry->func);
					break;
				case REALLOC_NOT_ALLOC:
					badptr++;
					fprintf(memCtrl->logFp, "%9p %9s, %4zu at %s, %3d, %s - pointer not found for realloc\n",
					     entry->ptr, "", entry->size, entry->file, entry->line, entry->func);
					break;
				case DOUBLE_FREE:
					badptr++;
					fprintf(memCtrl->logFp, "%9p %9s, %4s at %s, %3d, %s - double free of pointer\n",
					     entry->ptr, "", "", entry->file, entry->line, entry->func);
					break;
				case REALLOC_DOUBLE_FREE:
					badptr++;
					fprintf(memCtrl->logFp, "%9p %9s, %4zu at %s, %3d, %s - realloc 0 size already freed\n",
					     entry->ptr, "", entry->size, entry->file, entry->line, entry->func);
					break;
				case OVERRUN:
					overrun++;
					fprintf(memCtrl->logFp, "%9p [%3u:%3u], %4zu at %s, %3d, %s - buffer overrun\n",
					       entry->ptr, entry->seqNum, memCtrl->numberAllocList,
					       entry->size, entry->file, entry->line, entry->func);
					break;
				case MALLOC_ZERO_SIZE:
					zero_size++;
					fprintf(memCtrl->logFp, "%9p [%3u:%3u], %4zu at %s, %3d, %s - malloc zero size\n",
					       entry->ptr, entry->seqNum, memCtrl->numberAllocList,
					       entry->size, entry->file, entry->line, entry->func);
					break;
				case REALLOC_ZERO_SIZE:
					zero_size++;
					fprintf(memCtrl->logFp, "%9p [%3u:%3u], %4zu at %s, %3d, %s - realloc zero size (handled as free)\n",
					       entry->ptr, entry->seqNum, memCtrl->numberAllocList,
					       entry->size, entry->file, entry->line, entry->func);
					break;
				case ALLOCATED:	/* not used - avoid compiler warning */
				case FREE_SLOT:
				case LAST_FREE:
					break;
			}
		}
	}

	fprintf(memCtrl->logFp, "\n\n---[ memory dump summary for (%s) ]---\n", memCtrl->terminateBanner);
	fprintf(memCtrl->logFp, "Total number of bytes %s...: %zu\n", final ? "not freed" : "allocated", sum);
	fprintf(memCtrl->logFp, "Number of entries %s.......: %u\n", final ? "not freed" : "allocated", memCtrl->numberAllocList);
	fprintf(memCtrl->logFp, "Maximum allocated entries.........: %u\n", memCtrl->maxAllocList);
	fprintf(memCtrl->logFp, "Maximum memory allocated..........: %zu\n", memCtrl->maxMemAllocated);
	fprintf(memCtrl->logFp, "Number of mallocs.................: %u\n", memCtrl->numMallocs);
	fprintf(memCtrl->logFp, "Number of reallocs................: %u\n", memCtrl->numReallocs);
	fprintf(memCtrl->logFp, "Number of bad entries.............: %u\n", badptr);
	fprintf(memCtrl->logFp, "Number of buffer overrun..........: %u\n", overrun);
	fprintf(memCtrl->logFp, "Number of 0 size allocations......: %u\n\n", zero_size);
	if (sum != memCtrl->memAllocated)
		fprintf(memCtrl->logFp, "ERROR - sum of allocated %zu != mem_allocated %zu\n", sum, memCtrl->memAllocated);

	if (final)
	{
		if (sum || memCtrl->numberAllocList || badptr || overrun)
			fprintf(memCtrl->logFp, "=> Program seems to have some memory problem !!!\n\n");
		else
			fprintf(memCtrl->logFp, "=> Program seems to be memory allocation safe...\n\n");
	}
}

static void _freeFinal(void)
{
	_allocLog(true);
}


void *cmnMemMalloc(size_t size, const char *file, const char *function, int line)
{
	return __mallocCommon(size, file, function, line, "zalloc");
}

void cmnMemFree(void *buffer, const char *file, const char *function, int line)
{
	__freeReallocCommon(buffer, 0, file, function, line, false);
}

void *cmnMemRealloc(void *buffer, size_t size, const char *file, const char *function, int line)
{
	return __freeReallocCommon(buffer, size, file, function, line, true);
}


char *cmnMemStrdup(const char *str, const char *file, const char *function, int line)
{
	char *str_p;

	str_p = __mallocCommon(strlen(str) + 1, file, function, line, "strdup");
	return strcpy(str_p, str);
}

char *cmnMemStrndup(const char *str, size_t size, const char *file, const char *function, int line)
{
	char *str_p;

	str_p = __mallocCommon(size + 1, file, function, line, "strndup");
	return strncpy(str_p, str, size);
}


void cmnMemDumpAlloc(void)
{
	_allocLog(false);
}

void cmnMemInit(const char* prog_name, const char *banner)
{
	struct	_SYS_MEM_CTRL *memCtrl = &_sysMemCtrl;
	size_t log_name_len;
	char *log_name;
	char *prog;

	prog = prog_name;
	if(strrchr(prog_name, '/') )
	{
		prog = strrchr(prog_name, '/');
	}
	
	memset(memCtrl, 0, sizeof(struct _SYS_MEM_CTRL));
	memCtrl->allocList = RB_ROOT;	/* not nessary */
	memCtrl->freeList.prev= &memCtrl->freeList;
	memCtrl->freeList.next = &memCtrl->freeList;
	memCtrl->badList.prev = &memCtrl->badList;
	memCtrl->badList.next = &memCtrl->badList;
	
	if (SYSTEM_DEBUG_CHECK(LOG_CONSOLE_BIT) )
	{
		memCtrl->logFp = stderr;
		return;
	}

	if (memCtrl->logFp)
		fclose(memCtrl->logFp);

	log_name_len = 5 + strlen(prog) + 5 + 7 + 4 + 1;	/* "/tmp/" + prog_name + "_mem." + PID + ".log" + '\0" */
	log_name = malloc(log_name_len);
	if (!log_name)
	{
		CMN_INFO( "Unable to malloc log file name");
		memCtrl->logFp = stderr;
		return;
	}

	snprintf(log_name, log_name_len, "/tmp/%s_mem.%d.log", prog, getpid());
	memCtrl->logFp = fopen_safe(log_name, "w");
	if (memCtrl->logFp == NULL)
	{
		CMN_INFO( "Unable to open %s for appending", log_name);
		memCtrl->logFp = stderr;
	}
	else
	{
		int fd = fileno(memCtrl->logFp);

		/* We don't want any children to inherit the log file */
		if (fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC))
		{
			CMN_INFO( "Warning - failed to set CLOEXEC on log file %s", log_name);
		}

		/* Make the log output line buffered. This was to ensure that
		 * children didn't inherit the buffer, but the CLOEXEC above
		 * should resolve that. */
		setlinebuf(memCtrl->logFp);

		fprintf(memCtrl->logFp, "\n");
	}

	free(log_name);

	memCtrl->terminateBanner = banner;
}

void cmnMemSkipDump(void)
{
	struct	_SYS_MEM_CTRL *memCtrl = &_sysMemCtrl;
	
	memCtrl->skipMemCheckFinal = true;
}

void enable_mem_log_termination(void)
{
	atexit(_freeFinal);
}

void update_mem_check_log_perms(mode_t umask_bits)
{
	struct	_SYS_MEM_CTRL *memCtrl = &_sysMemCtrl;
	
	if (memCtrl->logFp)
	{
		fchmod(fileno(memCtrl->logFp), (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) & ~umask_bits);
	}
}



void memcheck_log(const char *called_func, const char *param, const char *file, const char *function, int line)
{
	struct	_SYS_MEM_CTRL *memCtrl = &_sysMemCtrl;
	int len = strlen(called_func) + (param ? strlen(param) : 0);

	if ((len = 36 - len) < 0)
		len = 0;

	fprintf(memCtrl->logFp, "%s%*s%s(%s) at %s, %d, %s\n", cmnTimestampStr(), len, "", called_func, param ? param : "", file, line, function);
}


#endif


//static void * __attribute__ ((malloc))
static void *__xalloc(unsigned long size)
{
	void *mem = malloc(size);

	if (mem == NULL)
	{
		if( SYSTEM_DEBUG_CHECK(DONT_FORK_BIT) )
		{
			perror("__xalloc");
		}
		else
		{
			CMN_INFO("__xalloc() error - %s", strerror(errno));
		}
		exit(1/*KEEPALIVED_EXIT_NO_MEMORY*/);
	}

#ifdef _MEM_CHECK_
	struct	_SYS_MEM_CTRL *memCtrl = &_sysMemCtrl;

	memCtrl->memAllocated += size - sizeof(long);
	if (memCtrl->memAllocated > memCtrl->maxMemAllocated)
	{
		memCtrl->maxMemAllocated = memCtrl->memAllocated;
	}
#endif

	return mem;
}

#ifdef _MEM_CHECK_
static
#endif
//void * __attribute__ ((malloc))
void * zalloc(unsigned long size)
{
	void *mem = __xalloc(size);
	if (mem)
	{		
		memset(mem, 0, size);
	}
	else
	{
		fprintf(stderr, "cmn_malloc failed\n");
		exit(1);
	}

	return mem;
}


