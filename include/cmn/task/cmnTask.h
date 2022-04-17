/*
 * task: psedoule thread
 */

#ifndef __CMN_TASK_H__
#define __CMN_Task_H__

/* system includes */
#include <sys/types.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/timerfd.h>
#ifdef _WITH_SNMP_
#include <sys/select.h>
#endif

#include "utils/cmnTime.h"
#include "utils/cmnSysList.h"
#include "utils/cmnListHead.h"
#include "utils/cmnRbTree.h"

/* Thread types. */
typedef enum
{
	TASK_READ,		/* thread_master.read rb tree */
	TASK_WRITE,		/* thread_master.write rb tree */
	TASK_TIMER,		/* thread_master.timer rb tree */
	TASK_TIMER_SHUTDOWN,	/* thread_master.timer rb tree */
	TASK_CHILD,		/* thread_master.child rb tree */
	
	TASK_UNUSED,		/* thread_master.unuse list_head */

	/* The following are all on the thread_master.next list_head */
	TASK_READY,
	TASK_EVENT,
	TASK_WRITE_TIMEOUT,
	TASK_READ_TIMEOUT,
	TASK_CHILD_TIMEOUT,
	TASK_CHILD_TERMINATED,
	TASK_TERMINATE_START,
	TASK_TERMINATE,
	TASK_READY_FD,
	TASK_READ_ERROR,
	TASK_WRITE_ERROR,
#ifdef USE_SIGNAL_THREADS
	TASK_SIGNAL,
#endif
} TaskType;

#define TASK_MAX_WAITING			TASK_CHILD

/* Event flags */
enum TaskEventFlags
{
	TASK_EVENT_FLAG_READ,
	TASK_EVENT_FLAG_WRITE,
	TASK_EVENT_FLAG_EPOLL,
	TASK_EVENT_FLAG_EPOLL_READ,
	TASK_EVENT_FLAG_EPOLL_WRITE,
};

/* epoll def */
#define TASK_EPOLL_REALLOC_THRESH	64

typedef struct _task 		CmnTask;
typedef const CmnTask 	*CmnTaskCRef;

typedef int (*TaskCallback)(CmnTaskCRef);

/* Thread itself. */
struct _task
{
	unsigned long				id;
	TaskType				type;		/* thread type */
	struct _thread_master		*master;	/* pointer to the struct thread_master. */
	
	TaskCallback				func;		/* event function */
	void						*arg;		/* event argument */
	
	timeval_t					sands;		/* rest of time sands value. */

	union
	{
		int val;		/* second argument of the event. */
		struct
		{
			int fd;		/* file descriptor in case of read/write. */
			bool close_on_reload;
		} f;
		
		struct
		{
			pid_t pid;	/* process id a child thread is wanting. */
			int status;	/* return status of the process */
		}
		c;
	} u;
	
	struct _TaskEvent 	*event;	/* Thread Event back-pointer */

	union
	{
		rb_node_t		n;
		list_head_t		next;
	};

	rb_node_t			rb_data;		/* PID or fd/vrid */
};

/* Thread Event */
typedef struct _TaskEvent
{
	CmnTask			*read;
	CmnTask			*write;
	unsigned long		flags;
	int				fd;

	rb_node_t		n;
} TaskEvent;

/* Master of the threads. */
typedef struct _thread_master
{
	rb_root_cached_t		read;
	rb_root_cached_t		write;
	rb_root_cached_t		timer;
	rb_root_cached_t		child;
	
	list_head_t			event;
#ifdef USE_SIGNAL_THREADS
	list_head_t 			signal;
#endif
	list_head_t			ready;
	list_head_t			unuse;

	/* child process related */
	rb_root_t				child_pid;

	/* epoll related */
	rb_root_t				io_events;

	struct epoll_event		*epoll_events;
	TaskEvent		*current_event;
	unsigned int			epoll_size;
	unsigned int			epoll_count;
	int					epoll_fd;

	/* timer related */
	int					timer_fd;
	CmnTaskCRef			timer_thread;

	/* signal related */
	int					signal_fd;

#ifdef _WITH_SNMP_
	/* snmp related */
	CmnTaskCRef		snmp_timer_thread;
	int				snmp_fdsetsize;
	fd_set			snmp_fdset;
#endif

	/* Local data */
	unsigned long			alloc;
	unsigned long			id;
	bool					shutdown_timer_running;
} TaskMaster;

#ifndef _DEBUG_
typedef enum {
	PROG_TYPE_PARENT,
#ifdef _WITH_VRRP_
	PROG_TYPE_VRRP,
#endif
#ifdef _WITH_LVS_
	PROG_TYPE_CHECKER,
#endif
#ifdef _WITH_BFD_
	PROG_TYPE_BFD,
#endif
} prog_type_t;
#endif

/* MICRO SEC def */
#define BOOTSTRAP_DELAY			TIMER_HZ

/* Macros. */
#define TASK_ARG(X) ((X)->arg)
#define TASK_VAL(X) ((X)->u.val)
#define TASK_CHILD_PID(X) ((X)->u.c.pid)
#define TASK_CHILD_STATUS(X) ((X)->u.c.status)

/* Exit codes */
#define KEEPALIVED_EXIT_OK			EXIT_SUCCESS
#define KEEPALIVED_EXIT_NO_MEMORY		(EXIT_FAILURE  )
#define KEEPALIVED_EXIT_FATAL			(EXIT_FAILURE+1)
#define KEEPALIVED_EXIT_CONFIG			(EXIT_FAILURE+2)
#define KEEPALIVED_EXIT_CONFIG_TEST		(EXIT_FAILURE+3)
#define KEEPALIVED_EXIT_CONFIG_TEST_SECURITY	(EXIT_FAILURE+4)
#define KEEPALIVED_EXIT_NO_CONFIG		(EXIT_FAILURE+5)

#define DEFAULT_CHILD_FINDER ((void *)1)

/* global vars exported */
extern TaskMaster *master;
#ifndef _DEBUG_
extern prog_type_t prog_type;		/* Parent/VRRP/Checker process */
#endif
#ifdef _WITH_SNMP_
extern bool snmp_running;
#endif
#ifdef _EPOLL_DEBUG_
extern bool do_epoll_debug;
#endif
#ifdef _EPOLL_TASK_DUMP_
extern bool do_epoll_thread_dump;
#endif

/* Prototypes. */
extern void set_child_finder_name(char const * (*)(pid_t));
extern void save_cmd_line_options(int, char * const *);
extern void log_command_line(unsigned);
#ifndef _DEBUG_
extern bool report_child_status(int, pid_t, const char *);
#endif
extern TaskMaster *sysThreadMasterCreate(void);
extern CmnTaskCRef thread_add_terminate_event(TaskMaster *);
extern CmnTaskCRef thread_add_start_terminate_event(TaskMaster *, TaskCallback);
#ifdef TASK_DUMP
extern void dump_thread_data(const TaskMaster *, FILE *);
#endif
extern void sysThreadMasterCleanup(TaskMaster *);
extern void sysThreadMasterDestroy(TaskMaster *);
extern CmnTaskCRef sysThreadAddReadSands(TaskMaster *, TaskCallback, void *, int, const timeval_t *, bool);
extern CmnTaskCRef sysThreadAddRead(TaskMaster *, TaskCallback, void *, int, unsigned long, bool);
extern int sysThreadDeleteRead(CmnTaskCRef);
extern void thread_requeue_read(TaskMaster *, int, const timeval_t *);
extern CmnTaskCRef sysThreadAddWrite(TaskMaster *, TaskCallback, void *, int, unsigned long, bool);
extern int sysThreadDeleteWrite(CmnTaskCRef);
extern void thread_close_fd(CmnTaskCRef);
extern CmnTaskCRef sysThreadAddTimer(TaskMaster *, TaskCallback, void *, unsigned long);
extern void timer_thread_update_timeout(CmnTaskCRef, unsigned long);
extern CmnTaskCRef sysThreadAddTimerShutdown(TaskMaster *, TaskCallback, void *, unsigned long);
extern CmnTaskCRef sysThreadAddChild(TaskMaster *, TaskCallback, void *, pid_t, unsigned long);
extern void sysThreadChildrenReschedule(TaskMaster *, TaskCallback, unsigned long);
extern CmnTaskCRef sysThreadAddEvent(TaskMaster *, TaskCallback, void *, int);
extern void sysThreadCancel(CmnTaskCRef);
extern void sysThreadCancel_read(TaskMaster *, int);
#ifdef _WITH_SNMP_
extern int sysThreadSnmpTimeoutThread(CmnTaskCRef);
void sysThreadSnmpEpollReset(TaskMaster *);
#endif

void sysThreadProcessThreads(TaskMaster *);
extern void thread_child_handler(void *, int);
extern void thread_add_base_threads(TaskMaster *, bool);
extern void launch_thread_scheduler(TaskMaster *);
#ifdef TASK_DUMP
extern const char *sysThreadGetSignalFunctionName(void (*)(void *, int));
extern void register_signal_handler_address(const char *, void (*)(void *, int));
extern void register_thread_address(const char *, TaskCallback);
extern void deregister_thread_addresses(void);
extern void register_scheduler_addresses(void);
#endif
#ifdef _VRRP_FD_DEBUG_
extern void set_extra_threads_debug(void (*)(void));
#endif

#endif

