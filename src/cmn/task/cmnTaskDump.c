
#ifdef TASK_DUMP
static const char *_getThreadTypeName(TaskType id)
{
	if (id == TASK_READ) return "READ";
	if (id == TASK_WRITE) return "WRITE";
	if (id == TASK_TIMER) return "TIMER";
	if (id == TASK_TIMER_SHUTDOWN) return "TIMER_SHUTDOWN";
	if (id == TASK_EVENT) return "EVENT";
	if (id == TASK_CHILD) return "CHILD";
	if (id == TASK_READY) return "READY";
	if (id == TASK_UNUSED) return "UNUSED";
	if (id == TASK_WRITE_TIMEOUT) return "WRITE_TIMEOUT";
	if (id == TASK_READ_TIMEOUT) return "READ_TIMEOUT";
	if (id == TASK_CHILD_TIMEOUT) return "CHILD_TIMEOUT";
	if (id == TASK_CHILD_TERMINATED) return "CHILD_TERMINATED";
	if (id == TASK_TERMINATE_START) return "TERMINATE_START";
	if (id == TASK_TERMINATE) return "TERMINATE";
	if (id == TASK_READY_FD) return "READY_FD";
	if (id == TASK_READ_ERROR) return "READ_ERROR";
	if (id == TASK_WRITE_ERROR) return "WRITE_ERROR";
#ifdef USE_SIGNAL_THREADS
	if (id == TASK_SIGNAL) return "SIGNAL";
#endif

	return "unknown";
}

static inline int _functionCmpare(const func_det_t *func1, const func_det_t *func2)
{
	if (func1->func < func2->func)
		return -1;
	if (func1->func > func2->func)
		return 1;
	return 0;
}

static const char *_getFunctionName(TaskCallback func)
{
	func_det_t func_det = { .func = func };
	func_det_t *match;
	static char address[19];

	if (!RB_EMPTY_ROOT(&funcs))
	{
		match = rb_search(&funcs, &func_det, n, _functionCmpare);
		if (match)
		{
			return match->name;
		}
	}

	snprintf(address, sizeof address, "%p", func);
	return address;
}

const char *sysThreadGetSignalFunctionName(void (*func)(void *, int))
{
	/* The cast should really be (int (*)(CmnTask *))func, but gcc 8.1 produces
	 * a warning with -Wcast-function-type, that the cast is to an incompatible
	 * function type. Since we don't actually call the function, but merely use
	 * it to compare function addresses, what we cast it do doesn't really matter */
	return _getFunctionName((void *)func);
}

void register_signal_handler_address(const char *func_name, void (*func)(void *, int))
{
	/* See comment in sysThreadGetSignalFunctionName() above */
	register_thread_address(func_name, (void *)func);
}

void register_thread_address(const char *func_name, TaskCallback func)
{
	func_det_t *func_det;

	func_det = (func_det_t *) MALLOC(sizeof(func_det_t));
	if (!func_det)
		return;

	func_det->name = func_name;
	func_det->func = func;

	rb_insert_sort(&funcs, func_det, n, _functionCmpare);
}


void deregister_thread_addresses(void)
{
	func_det_t *func_det, *func_det_tmp;

	if (RB_EMPTY_ROOT(&funcs))
		return;

	rb_for_each_entry_safe(func_det, func_det_tmp, &funcs, n)
	{
		rb_erase(&func_det->n, &funcs);
		FREE(func_det);
	}
}

static const char *_timerDelayStr(timeval_t sands)
{
	static char str[43];

	if (sands.tv_sec == TIMER_DISABLED)
		return "NEVER";
	if (sands.tv_sec == 0 && sands.tv_usec == 0)
		return "UNSET";

	if (timercmp(&sands, &time_now, >=))
	{
		sands = cmnTimeSubNow(sands);
		snprintf(str, sizeof str, "%ld.%6.6ld", sands.tv_sec, sands.tv_usec);
	}
	else
	{
		timersub(&time_now, &sands, &sands);
		snprintf(str, sizeof str, "-%ld.%6.6ld", sands.tv_sec, sands.tv_usec);
	}

	return str;
}

/* Dump rbtree */
static void _rbDump(const rb_root_cached_t *root, const char *tree, FILE *fp)
{
	CmnTask *thread;
	int i = 1;

	conf_write(fp, "----[ Begin rb_dump %s ]----", tree);

	rb_for_each_entry_cached(thread, root, n)
		conf_write(fp, "#%.2d Thread type %s, event_fd %d, val/fd/pid %d, fd_close %d, timer: %s, func %s(), id %lu", 
			i++, _getThreadTypeName(thread->type), thread->event ? thread->event->fd: -2, thread->u.val, thread->u.f.close_on_reload, _timerDelayStr(thread->sands), _getFunctionName(thread->func), thread->id);

	conf_write(fp, "----[ End rb_dump ]----");
}

static void _listDump(const list_head_t *l, const char *list_type, FILE *fp)
{
	CmnTask *thread;
	int i = 1;

	conf_write(fp, "----[ Begin list_dump %s ]----", list_type);

	list_for_each_entry(thread, l, next)
		conf_write(fp, "#%.2d Thread:%p type %s func %s() id %lu",
				i++, thread, _getThreadTypeName(thread->type), _getFunctionName(thread->func), thread->id);

	conf_write(fp, "----[ End list_dump ]----");
}

static void _eventRbDump(const rb_root_t *root, const char *tree, FILE *fp)
{
	TaskEvent *event;
	int i = 1;

	conf_write(fp, "----[ Begin rb_dump %s ]----", tree);
	rb_for_each_entry(event, root, n)
		conf_write(fp, "#%.2d event %p fd %d, flags: 0x%lx, read %p, write %p", i++, event, event->fd, event->flags, event->read, event->write);
	conf_write(fp, "----[ End rb_dump ]----");
}

void dump_thread_data(const TaskMaster *m, FILE *fp)
{
	_rbDump(&m->read, "read", fp);
	_rbDump(&m->write, "write", fp);
	_rbDump(&m->child, "child", fp);
	_rbDump(&m->timer, "timer", fp);
	
	_listDump(&m->event, "event", fp);
	_listDump(&m->ready, "ready", fp);
	
#ifdef USE_SIGNAL_THREADS
	_listDump(&m->signal, "signal", fp);
#endif
	_listDump(&m->unuse, "unuse", fp);

	_eventRbDump(&m->io_events, "io_events", fp);
}

void register_scheduler_addresses(void)
{
#ifdef _WITH_SNMP_
	register_thread_address("sysThreadSnmpTimeoutThread", sysThreadSnmpTimeoutThread);
	register_thread_address("snmp_read_thread", _sysThreadSnmpReadThread);
#endif
	register_thread_address("thread_timerfd_handler", thread_timerfd_handler);

	register_signal_handler_address("thread_child_handler", thread_child_handler);
}

#endif


