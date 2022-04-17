/*
 *
 */

#include "config.h"

/* SNMP should be included first: it redefines "FREE" */
#ifdef _WITH_SNMP_
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#undef FREE
#endif

#include <errno.h>
#include <sys/wait.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <unistd.h>
#ifdef HAVE_SIGNALFD
#include <sys/signalfd.h>
#endif
#include <sys/utsname.h>
#include <linux/version.h>

#include "bitops.h"
#include "warnings.h"

#include "lib/cmnSysThread.h"
#include "sysMemory.h"
#include "sysRbtree.h"
#include "sysUtils.h"
#include "sysSignals.h"
#include "sysLogger.h"
//#include "git-commit.h"
#include "sysTimer.h"
#if !HAVE_EPOLL_CREATE1 || !defined TFD_NONBLOCK
#include "old_socket.h"
#endif
#include "assert_debug.h"


#ifdef TASK_DUMP
typedef struct _func_det
{
	const char		*name;
	TaskCallback		func;
	
	rb_node_t		n;
} func_det_t;
#endif

/* global vars */
TaskMaster *master = NULL;
#ifndef _DEBUG_
prog_type_t prog_type;		/* Parent/VRRP/Checker process */
#endif
#ifdef _WITH_SNMP_
bool snmp_running;		/* True if this process is running SNMP */
#endif

struct	_TASK_CTRL
{
/* local variables */
	bool				shuttingDown;
	int				savArgc;
	char * const 		*savArgv;
#ifdef _EPOLL_DEBUG_
	bool				doEpollDebug;
#endif
#ifdef _EPOLL_TASK_DUMP_
	bool				doEpollThreadDump;
#endif
#ifdef TASK_DUMP
	rb_root_t			funcs = RB_ROOT;
#endif
#ifdef _VRRP_FD_DEBUG_
	void				(*extraThreadsDebug)(void);
#endif
};

struct	_TASK_CTRL		_threadCtrl;


/* local variables */
static bool shutting_down;
static int sav_argc;
static char * const *sav_argv;
#ifdef _EPOLL_DEBUG_
bool do_epoll_debug;
#endif
#ifdef _EPOLL_TASK_DUMP_
bool do_epoll_thread_dump;
#endif
#ifdef TASK_DUMP
static rb_root_t funcs = RB_ROOT;
#endif
#ifdef _VRRP_FD_DEBUG_
static void (*extra_threads_debug)(void);
#endif


/* Function that returns prog_name if pid is a known child */
static char const * (*child_finder_name)(pid_t);


#ifdef _VRRP_FD_DEBUG_
void set_extra_threads_debug(void (*func)(void))
{
	extra_threads_debug = func;
}
#endif

/* Move ready thread into ready queue */
static int _threadMoveReady(TaskMaster *m, rb_root_cached_t *root, CmnTask *thread, int type)
{
	rb_erase_cached(&thread->n, root);
	if (type == TASK_CHILD_TIMEOUT)
		rb_erase(&thread->rb_data, &master->child_pid);
	INIT_LIST_HEAD(&thread->next);
	list_add_tail(&thread->next, &m->ready);
	if (thread->type != TASK_TIMER_SHUTDOWN)
		thread->type = type;
	return 0;
}

/* Move ready thread into ready queue */
static void thread_rb_move_ready(TaskMaster *m, rb_root_cached_t *root, int type)
{
	CmnTask *thread, *thread_tmp;

	rb_for_each_entry_safe_cached(thread, thread_tmp, root, n) {
		if (thread->sands.tv_sec == TIMER_DISABLED || timercmp(&time_now, &thread->sands, <))
			break;

		if (type == TASK_READ_TIMEOUT)
			thread->event->read = NULL;
		else if (type == TASK_WRITE_TIMEOUT)
			thread->event->write = NULL;
		_threadMoveReady(m, root, thread, type);
	}
}


static int thread_timerfd_handler(CmnTaskCRef thread)
{
	TaskMaster *m = thread->master;
	uint64_t expired;
	ssize_t len;

	len = read(m->timer_fd, &expired, sizeof(expired));
	if (len < 0)
		log_message(LOG_ERR, "scheduler: Error reading on timerfd fd:%d (%m)", m->timer_fd);

	/* Read, Write, Timer, Child thread. */
	thread_rb_move_ready(m, &m->read, TASK_READ_TIMEOUT);
	thread_rb_move_ready(m, &m->write, TASK_WRITE_TIMEOUT);
	thread_rb_move_ready(m, &m->timer, TASK_READY);
	thread_rb_move_ready(m, &m->child, TASK_CHILD_TIMEOUT);

	/* Register next timerfd thread */
	m->timer_thread = sysThreadAddRead(m, thread_timerfd_handler, NULL, m->timer_fd, TIMER_NEVER, false);

	return 0;
}

/* Child PID cmp helper */
static inline int _sysThreadChildPidCmp(const CmnTask *t1, const CmnTask *t2)
{
	return t1->u.c.pid - t2->u.c.pid;
}

void set_child_finder_name(char const * (*func)(pid_t))
{
	child_finder_name = func;
}

void save_cmd_line_options(int argc, char * const *argv)
{
	sav_argc = argc;
	sav_argv = argv;
}

#ifndef _DEBUG_
static const char *get_end(const char *str, size_t max_len)
{
	size_t len = strlen(str);
	const char *end;

	if (len <= max_len)
		return str + len;

	end = str + max_len;
	if (*end == ' ')
		return end;

	while (end > str && *--end != ' ');
	if (end > str)
		return end;

	return str + max_len;
}

static void log_options(const char *option, const char *option_str, unsigned indent)
{
	const char *p = option_str;
	size_t opt_len = strlen(option);
	const char *end;
	bool first_line = true;

	while (*p) {
		/* Skip leading spaces */
		while (*p == ' ')
			p++;

		end = get_end(p, 100 - opt_len);
		if (first_line) {
			log_message(LOG_INFO, "%*s%s: %.*s", (int)indent, "", option, (int)(end - p), p);
			first_line = false;
		}
		else
			log_message(LOG_INFO, "%*s%.*s", (int)(indent + opt_len + 2), "", (int)(end - p), p);
		p = end;
	}
}

void
log_command_line(unsigned indent)
{
	size_t len = 0;
	char *log_str;
	char *p;
	int i;

	if (!sav_argv)
		return;

	for (i = 0; i < sav_argc; i++)
		len += strlen(sav_argv[i]) + 3;	/* Add opening and closing 's, and following space or '\0' */

	log_str = MALLOC(len);

RELAX_STRICT_OVERFLOW_START
	for (i = 0, p = log_str; i < sav_argc; i++) {
RELAX_STRICT_OVERFLOW_END
		p += sprintf(p, "%s'%s'", i ? " " : "", sav_argv[i]);
	}

	log_options("Command line", log_str, indent);

	FREE(log_str);
}

/* report_child_status returns true if the exit is a hard error, so unable to continue */
bool report_child_status(int status, pid_t pid, char const *prog_name)
{
	char const *prog_id = NULL;
	char pid_buf[12];	/* "pid 4194303" + '\0' - see definition of PID_MAX_LIMIT in include/linux/threads.h */
	int exit_status ;

	if (prog_name)
		prog_id = prog_name;
	else if (child_finder_name)
		prog_id = child_finder_name(pid);

	if (!prog_id) {
		snprintf(pid_buf, sizeof(pid_buf), "pid %hd", pid);
		prog_id = pid_buf;
	}

	if (WIFEXITED(status)) {
		exit_status = WEXITSTATUS(status);

		/* Handle exit codes of vrrp or checker child */
		if (exit_status == KEEPALIVED_EXIT_FATAL ||
		    exit_status == KEEPALIVED_EXIT_CONFIG) {
			log_message(LOG_INFO, "%s exited with permanent error %s. Terminating", prog_id, exit_status == KEEPALIVED_EXIT_CONFIG ? "CONFIG" : "FATAL" );
			return true;
		}

		if (exit_status != EXIT_SUCCESS)
			log_message(LOG_INFO, "%s exited with status %d", prog_id, exit_status);
	} else if (WIFSIGNALED(status)) {
		if (WTERMSIG(status) == SIGSEGV) {
			struct utsname uname_buf;

			log_message(LOG_INFO, "%s exited due to segmentation fault (SIGSEGV).", prog_id);
			log_message(LOG_INFO, "  %s", "Please report a bug at https://github.com/acassen/keepalived/issues");
			log_message(LOG_INFO, "  %s", "and include this log from when keepalived started, a description");
			log_message(LOG_INFO, "  %s", "of what happened before the crash, your configuration file and the details below.");
			log_message(LOG_INFO, "  %s", "Also provide the output of keepalived -v, what Linux distro and version");
			log_message(LOG_INFO, "  %s", "you are running on, and whether keepalived is being run in a container or VM.");
			log_message(LOG_INFO, "  %s", "A failure to provide all this information may mean the crash cannot be investigated.");
			log_message(LOG_INFO, "  %s", "If you are able to provide a stack backtrace with gdb that would really help.");
			log_message(LOG_INFO, "  Source version %s %s%s", PACKAGE_VERSION,
#ifdef GIT_COMMIT
									   ", git commit ", GIT_COMMIT
#else
									   "", ""
#endif
				   );
			log_message(LOG_INFO, "  Built with kernel headers for Linux %d.%d.%d",
						(LINUX_VERSION_CODE >> 16) & 0xff,
						(LINUX_VERSION_CODE >>  8) & 0xff,
						(LINUX_VERSION_CODE      ) & 0xff);
			uname(&uname_buf);
			log_message(LOG_INFO, "  Running on %s %s %s", uname_buf.sysname, uname_buf.release, uname_buf.version);
			log_command_line(2);
			log_options("configure options", KEEPALIVED_CONFIGURE_OPTIONS, 2);
			log_options("Config options", CONFIGURATION_OPTIONS, 2);
			log_options("System options", SYSTEM_OPTIONS, 2);

//			if (__test_bit(DONT_RESPAWN_BIT, &debug))
//				segv_termination = true;
		}
		else
			log_message(LOG_INFO, "%s exited due to signal %d", prog_id, WTERMSIG(status));
	}

	return false;
}
#endif


/* declare thread_timer_cmp() for rbtree compares */
RB_TIMER_CMP(thread);

/* Free all unused thread. */
static void _threadCleanUnuse(TaskMaster * m)
{
	CmnTask *thread, *thread_tmp;
	list_head_t *l = &m->unuse;

	list_for_each_entry_safe(thread, thread_tmp, l, next)
	{
		list_head_del(&thread->next);

		/* free the thread */
		FREE(thread);
		m->alloc--;
	}

	INIT_LIST_HEAD(l);
}

/* Move thread to unuse list. */
static void _threadAddUnuse(TaskMaster *m, CmnTask *thread)
{
	assert(m != NULL);

	thread->type = TASK_UNUSED;
	thread->event = NULL;
	INIT_LIST_HEAD(&thread->next);
	list_add_tail(&thread->next, &m->unuse);
}

/* Move list element to unuse queue */
static void _threadDestroyList(TaskMaster *m, list_head_t *l)
{
	CmnTask *thread, *thread_tmp;

	list_for_each_entry_safe(thread, thread_tmp, l, next)
	{
		if (thread->event)
		{
			sysThreadDeleteRead(thread);
			sysThreadDeleteWrite(thread);
		}
		
		list_head_del(&thread->next);
		_threadAddUnuse(m, thread);
	}
}

static void _threadDestroyRb(TaskMaster *m, rb_root_cached_t *root)
{
	CmnTask *thread, *thread_tmp;

	rb_for_each_entry_safe_cached(thread, thread_tmp, root, n)
	{
		rb_erase_cached(&thread->n, root);

		if (thread->type == TASK_READ ||
			thread->type == TASK_WRITE ||
			thread->type == TASK_READY_FD ||
			thread->type == TASK_READ_TIMEOUT ||
			thread->type == TASK_WRITE_TIMEOUT ||
			thread->type == TASK_READ_ERROR ||
			thread->type == TASK_WRITE_ERROR)
		{
			/* Do we have a thread_event, and does it need deleting? */
			if (thread->type == TASK_READ)
				sysThreadDeleteRead(thread);
			else if (thread->type == TASK_WRITE)
				sysThreadDeleteWrite(thread);

			/* Do we have a file descriptor that needs closing ? */
			if (thread->u.f.close_on_reload)
				thread_close_fd(thread);
		}

		_threadAddUnuse(m, thread);
	}
}


/* Make thread master. */
TaskMaster *sysThreadMasterCreate(void)
{
	TaskMaster *_new;

	_new = (TaskMaster *) MALLOC(sizeof (TaskMaster));

#if HAVE_EPOLL_CREATE1
	_new->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
#else
	_new->epoll_fd = epoll_create(1);
#endif
	if (_new->epoll_fd < 0) {
		log_message(LOG_INFO, "scheduler: Error creating EPOLL instance (%m)");
		FREE(_new);
		return NULL;
	}

#if !HAVE_EPOLL_CREATE1
	if (set_sock_flags(_new->epoll_fd, F_SETFD, FD_CLOEXEC))
		log_message(LOG_INFO, "Unable to set CLOEXEC on epoll_fd - %s (%d)", strerror(errno), errno);
#endif

	_new->read = RB_ROOT_CACHED;
	_new->write = RB_ROOT_CACHED;
	_new->timer = RB_ROOT_CACHED;
	_new->child = RB_ROOT_CACHED;
	
	_new->io_events = RB_ROOT;
	_new->child_pid = RB_ROOT;
	
	INIT_LIST_HEAD(&_new->event);
#ifdef USE_SIGNAL_THREADS
	INIT_LIST_HEAD(&_new->signal);
#endif
	INIT_LIST_HEAD(&_new->ready);
	INIT_LIST_HEAD(&_new->unuse);


	/* Register timerfd thread */
	_new->timer_fd = timerfd_create(CLOCK_MONOTONIC,
#ifdef TFD_NONBLOCK				/* Since Linux 2.6.27 */
						        TFD_NONBLOCK | TFD_CLOEXEC
#else
							0
#endif
										  );
	if (_new->timer_fd < 0)
	{
		log_message(LOG_ERR, "scheduler: Cant create timerfd (%m)");
		FREE(_new);
		return NULL;
	}

#ifndef TFD_NONBLOCK
	if (set_sock_flags(_new->timer_fd, F_SETFL, O_NONBLOCK))
		log_message(LOG_INFO, "Unable to set NONBLOCK on timer_fd - %s (%d)", strerror(errno), errno);

	if (set_sock_flags(_new->timer_fd, F_SETFD, FD_CLOEXEC))
		log_message(LOG_INFO, "Unable to set CLOEXEC on timer_fd - %s (%d)", strerror(errno), errno);
#endif

	_new->signal_fd = sysSignalHandlerInit();

	_new->timer_thread = sysThreadAddRead(_new, thread_timerfd_handler, NULL, _new->timer_fd, TIMER_NEVER, false);

	sysSignalReadThreadAdd(_new);

	return _new;
}

/* Cleanup master */
void sysThreadMasterCleanup(TaskMaster * m)
{
	/* Unuse current thread lists */
	m->current_event = NULL;
	_threadDestroyRb(m, &m->read);
	_threadDestroyRb(m, &m->write);
	_threadDestroyRb(m, &m->timer);
	_threadDestroyRb(m, &m->child);
	
	_threadDestroyList(m, &m->event);
#ifdef USE_SIGNAL_THREADS
	_threadDestroyList(m, &m->signal);
#endif
	_threadDestroyList(m, &m->ready);
	m->child_pid = RB_ROOT;

	/* Clean garbage */
	_threadCleanUnuse(m);

	FREE(m->epoll_events);
	m->epoll_size = 0;
	m->epoll_count = 0;

	m->timer_thread = NULL;

#ifdef _WITH_SNMP_
	m->snmp_timer_thread = NULL;
	FD_ZERO(&m->snmp_fdset);
	m->snmp_fdsetsize = 0;
#endif
}

/* Stop thread scheduler. */
void sysThreadMasterDestroy(TaskMaster * m)
{
	if (m->epoll_fd != -1)
	{
		close(m->epoll_fd);
		m->epoll_fd = -1;
	}

	if (m->timer_fd != -1)
	{
		close(m->timer_fd);
	}

	if (m->signal_fd != -1)
	{
		sysSignalHandlerDestroy();
	}

	sysThreadMasterCleanup(m);

	FREE(m);
}

/* Delete top of the list and return it. */
static CmnTask *_sysThreadTrimHead(list_head_t *l)
{
	CmnTask *thread;

	if (list_empty(l))
		return NULL;

	thread = list_first_entry(l, CmnTask, next);
	list_del_init(&thread->next);
	return thread;
}

