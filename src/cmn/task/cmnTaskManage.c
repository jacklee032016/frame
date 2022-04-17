/* Make unique thread id for non pthread version of thread manager. */
#if 0
static inline unsigned long thread_get_id(TaskMaster *m)
{
	return m->id++;
}
#else
#define	thread_get_id(m)	(m)->id++
#endif

/* Make new thread. */
static CmnTask *_sysThreadNew(TaskMaster *m)
{
	CmnTask *_new;

	/* If one thread is already allocated return it */
	_new = _sysThreadTrimHead(&m->unuse);
	if (!_new)
	{
		_new = (CmnTask *)MALLOC(sizeof(CmnTask));
		m->alloc++;
	}

	INIT_LIST_HEAD(&_new->next);
	_new->id = thread_get_id(m);
	return new;
}

/* Add new read thread. */
CmnTaskCRef sysThreadAddReadSands(TaskMaster *m, TaskCallback func, void *arg, int fd, const timeval_t *sands, bool close_on_reload)
{
	TaskEvent *event;
	CmnTask *thread;

	assert(m != NULL);

	/* I feel lucky ! :D */
	if (m->current_event && m->current_event->fd == fd)
		event = m->current_event;
	else
		event = _sysThreadEventGet(m, fd);

	if (!event) {
		if (!(event = _sysThreadEventNew(m, fd))) {
			log_message(LOG_INFO, "scheduler: Cant allocate read event for fd [%d](%m)", fd);
			return NULL;
		}
	}
	else if (__test_bit(TASK_EVENT_FLAG_READ, &event->flags) && event->read) {
		log_message(LOG_INFO, "scheduler: There is already read event %p (read %p) registered on fd [%d]", event, event->read, fd);
		return NULL;
	}

	thread = _sysThreadNew(m);
	thread->type = TASK_READ;
	thread->master = m;
	thread->func = func;
	thread->arg = arg;
	thread->u.f.fd = fd;
	thread->u.f.close_on_reload = close_on_reload;
	thread->event = event;

	/* Set & flag event */
	__set_bit(TASK_EVENT_FLAG_READ, &event->flags);
	event->read = thread;
	if (!__test_bit(TASK_EVENT_FLAG_EPOLL_READ, &event->flags)) {
		if (_sysThreadEventSet(thread) < 0) {
			log_message(LOG_INFO, "scheduler: Cant register read event for fd [%d](%m)", fd);
			_threadAddUnuse(m, thread);
			return NULL;
		}
		__set_bit(TASK_EVENT_FLAG_EPOLL_READ, &event->flags);
	}

	thread->sands = *sands;

	/* Sort the thread. */
	rb_insert_sort_cached(&m->read, thread, n, thread_timer_cmp);

	return thread;
}

CmnTaskCRef sysThreadAddRead(TaskMaster *m, TaskCallback func, void *arg, int fd, unsigned long timer, bool close_on_reload)
{
	timeval_t sands;

	/* Compute read timeout value */
	if (timer == TIMER_NEVER) {
		sands.tv_sec = TIMER_DISABLED;
		sands.tv_usec = 0;
	} else {
		cmnTimeSetNow();
		sands = cmnTimeAddLong(time_now, timer);
	}

	return sysThreadAddReadSands(m, func, arg, fd, &sands, close_on_reload);
}

/* Add new write thread. */
CmnTaskCRef sysThreadAddWrite(TaskMaster *m, TaskCallback func, void *arg, int fd, unsigned long timer, bool close_on_reload)
{
	TaskEvent *event;
	CmnTask *thread;

	assert(m != NULL);

	/* I feel lucky ! :D */
	if (m->current_event && m->current_event->fd == fd)
		event = m->current_event;
	else
		event = _sysThreadEventGet(m, fd);

	if (!event)
	{
		if (!(event = _sysThreadEventNew(m, fd)))
		{
			log_message(LOG_INFO, "scheduler: Cant allocate write event for fd [%d](%m)", fd);
			return NULL;
		}
	}
	else if (__test_bit(TASK_EVENT_FLAG_WRITE, &event->flags) && event->write)
	{
		log_message(LOG_INFO, "scheduler: There is already write event registered on fd [%d]", fd);
		return NULL;
	}

	thread = _sysThreadNew(m);
	thread->type = TASK_WRITE;
	thread->master = m;
	thread->func = func;
	thread->arg = arg;
	thread->u.f.fd = fd;
	thread->u.f.close_on_reload = close_on_reload;
	thread->event = event;

	/* Set & flag event */
	__set_bit(TASK_EVENT_FLAG_WRITE, &event->flags);
	event->write = thread;
	if (!__test_bit(TASK_EVENT_FLAG_EPOLL_WRITE, &event->flags))
	{
		if (_sysThreadEventSet(thread) < 0)
		{
			log_message(LOG_INFO, "scheduler: Cant register write event for fd [%d](%m)" , fd);
			_threadAddUnuse(m, thread);
			return NULL;
		}
		__set_bit(TASK_EVENT_FLAG_EPOLL_WRITE, &event->flags);
	}

	/* Compute write timeout value */
	if (timer == TIMER_NEVER)
		thread->sands.tv_sec = TIMER_DISABLED;
	else {
		cmnTimeSetNow();
		thread->sands = cmnTimeAddLong(time_now, timer);
	}

	/* Sort the thread. */
	rb_insert_sort_cached(&m->write, thread, n, thread_timer_cmp);

	return thread;
}

CmnTaskCRef sysThreadAddTimerShutdown(TaskMaster *m, TaskCallback func, void *arg, unsigned long timer)
{
	union {
		CmnTask *p;
		const CmnTask *cp;
	} thread;
       
	thread.cp = sysThreadAddTimer(m, func, arg, timer);

	thread.p->type = TASK_TIMER_SHUTDOWN;

	return thread.cp;
}

/* Add timer event thread. */
CmnTaskCRef sysThreadAddTimer(TaskMaster *m, TaskCallback func, void *arg, unsigned long timer)
{
	CmnTask *thread;

	assert(m != NULL);

	thread = _sysThreadNew(m);
	thread->type = TASK_TIMER;
	thread->master = m;
	thread->func = func;
	thread->arg = arg;
	thread->u.val = 0;

	/* Do we need jitter here? */
	if (timer == TIMER_NEVER)
		thread->sands.tv_sec = TIMER_DISABLED;
	else {
		cmnTimeSetNow();
		thread->sands = cmnTimeAddLong(time_now, timer);
	}

	/* Sort by timeval. */
	rb_insert_sort_cached(&m->timer, thread, n, thread_timer_cmp);

	return thread;
}

/* Add a child thread. */
CmnTaskCRef sysThreadAddChild(TaskMaster * m, TaskCallback func, void * arg, pid_t pid, unsigned long timer)
{
	CmnTask *thread;

	assert(m != NULL);

	thread = _sysThreadNew(m);
	thread->type = TASK_CHILD;
	thread->master = m;
	thread->func = func;
	thread->arg = arg;
	thread->u.c.pid = pid;
	thread->u.c.status = 0;

	/* Compute child timeout value */
	if (timer == TIMER_NEVER)
	{
		thread->sands.tv_sec = TIMER_DISABLED;
	}
	else
	{
		cmnTimeSetNow();
		thread->sands = cmnTimeAddLong(time_now, timer);
	}

	/* Sort by timeval. */
	rb_insert_sort_cached(&m->child, thread, n, thread_timer_cmp);

	/* Sort by PID */
	rb_insert_sort(&m->child_pid, thread, rb_data, _sysThreadChildPidCmp);

	return thread;
}

/* Add simple event thread. */
CmnTaskCRef sysThreadAddEvent(TaskMaster * m, TaskCallback func, void *arg, int val)
{
	CmnTask *thread;

	assert(m != NULL);

	thread = _sysThreadNew(m);
	thread->type = TASK_EVENT;
	thread->master = m;
	thread->func = func;
	thread->arg = arg;
	thread->u.val = val;
	
	INIT_LIST_HEAD(&thread->next);
	list_add_tail(&thread->next, &m->event);

	return thread;
}

int sysThreadDeleteRead(CmnTaskCRef thread)
{
	if (!thread || !thread->event)
		return -1;

	if (_sysThreadEventDel(thread, TASK_EVENT_FLAG_EPOLL_READ) < 0)
		return -1;

	return 0;
}

int sysThreadDeleteWrite(CmnTaskCRef thread)
{
	if (!thread || !thread->event)
		return -1;

	if (_sysThreadEventDel(thread, TASK_EVENT_FLAG_EPOLL_WRITE) < 0)
		return -1;

	return 0;
}


static void _threadReadRequeue(TaskMaster *m, int fd, const timeval_t *new_sands)
{
	CmnTask *thread;
	TaskEvent *event;

	event = _sysThreadEventGet(m, fd);
	if (!event || !event->read)
		return;

	thread = event->read;

	if (thread->type != TASK_READ)
	{/* If the thread is not on the read list, don't touch it */
		return;
	}

	thread->sands = *new_sands;

	rb_move_cached(&thread->master->read, thread, n, thread_timer_cmp);
}

void thread_requeue_read(TaskMaster *m, int fd, const timeval_t *sands)
{
	_threadReadRequeue(m, fd, sands);
}

void thread_close_fd(CmnTaskCRef thread_cp)
{
	CmnTask *thread = no_const(CmnTask, thread_cp);

	if (thread->u.f.fd == -1)
		return;

	if (thread->event)
		_sysThreadEventCancel(thread);

	close(thread->u.f.fd);
	thread->u.f.fd = -1;
}

void timer_thread_update_timeout(CmnTaskCRef thread_cp, unsigned long timer)
{
	CmnTask *thread = no_const(CmnTask, thread_cp);
	timeval_t sands;

	if (thread->type > TASK_MAX_WAITING) {
		/* It is probably on the ready list, so we'd better just let it run */
		return;
	}

	cmnTimeSetNow();
	sands = cmnTimeAddLong(time_now, timer);

	if (timercmp(&thread->sands, &sands, ==))
		return;

	thread->sands = sands;

	rb_move_cached(&thread->master->timer, thread, n, thread_timer_cmp);
}

void sysThreadChildrenReschedule(TaskMaster *m, TaskCallback func, unsigned long timer)
{
	CmnTask *thread;

// What is this used for ??
	cmnTimeSetNow();
	rb_for_each_entry_cached(thread, &m->child, n)
	{
		thread->func = func;
		thread->sands = cmnTimeAddLong(time_now, timer);
	}
}

/* Add terminate event thread. */
static CmnTaskCRef _threadAddGenericTerminateEvent(TaskMaster * m, TaskType type, TaskCallback func)
{
	CmnTask *thread;

	assert(m != NULL);

	thread = _sysThreadNew(m);
	thread->type = type;
	thread->master = m;
	thread->func = func;
	thread->arg = NULL;
	thread->u.val = 0;
	INIT_LIST_HEAD(&thread->next);
	list_add_tail(&thread->next, &m->event);

	return thread;
}

CmnTaskCRef thread_add_terminate_event(TaskMaster *m)
{
	return _threadAddGenericTerminateEvent(m, TASK_TERMINATE, NULL);
}

CmnTaskCRef thread_add_start_terminate_event(TaskMaster *m, TaskCallback func)
{
	return _threadAddGenericTerminateEvent(m, TASK_TERMINATE_START, func);
}

#ifdef USE_SIGNAL_THREADS
/* Add signal thread. */
CmnTaskCRef thread_add_signal(TaskMaster *m, TaskCallback func, void *arg, int signum)
{
	CmnTask *thread;

	assert(m != NULL);

	thread = _sysThreadNew(m);
	thread->type = TASK_SIGNAL;
	thread->master = m;
	thread->func = func;
	thread->arg = arg;
	thread->u.val = signum;
	INIT_LIST_HEAD(&thread->next);
	list_add_tail(&thread->next, &m->signal);

	/* Update signalfd accordingly */
	if (sigismember(&m->signal_mask, signum))
		return thread;
	sigaddset(&m->signal_mask, signum);
	signalfd(m->signal_fd, &m->signal_mask, 0);

	return thread;
}
#endif


