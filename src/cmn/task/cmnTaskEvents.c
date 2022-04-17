

/* epoll related */
static int __sysThreadEventsResize(TaskMaster *m, int delta)
{
	unsigned int new_size;

	m->epoll_count += delta;
	if (m->epoll_count < m->epoll_size)
		return 0;

	new_size = ((m->epoll_count / TASK_EPOLL_REALLOC_THRESH) + 1);
	new_size *= TASK_EPOLL_REALLOC_THRESH;

	if (m->epoll_events)
		FREE(m->epoll_events);
	m->epoll_events = MALLOC(new_size * sizeof(struct epoll_event));
	if (!m->epoll_events)
	{
		m->epoll_size = 0;
		return -1;
	}

	m->epoll_size = new_size;
	return 0;
}

static inline int _sysThreadEventCompare(const TaskEvent *event1, const TaskEvent *event2)
{
	if (event1->fd < event2->fd)
		return -1;
	if (event1->fd > event2->fd)
		return 1;
	return 0;
}

static TaskEvent *_sysThreadEventNew(TaskMaster *m, int fd)
{
	TaskEvent *event;

	event = (TaskEvent *) MALLOC(sizeof(TaskEvent));
	if (!event)
		return NULL;

	if (__sysThreadEventsResize(m, 1) < 0)
	{
		FREE(event);
		return NULL;
	}

	event->fd = fd;

	rb_insert_sort(&m->io_events, event, n, _sysThreadEventCompare);

	return event;
}

//static TaskEvent * __attribute__ ((pure))
static TaskEvent * _sysThreadEventGet(TaskMaster *m, int fd)
{
	TaskEvent event = { .fd = fd };

	return rb_search(&m->io_events, &event, n, _sysThreadEventCompare);
}

static int _sysThreadEventSet(const CmnTask *thread)
{
	TaskEvent *event = thread->event;
	TaskMaster *m = thread->master;
	struct epoll_event ev = { .events = 0 };
	int op;

	ev.data.ptr = event;
	if (__test_bit(TASK_EVENT_FLAG_READ, &event->flags))
		ev.events |= EPOLLIN;

	if (__test_bit(TASK_EVENT_FLAG_WRITE, &event->flags))
		ev.events |= EPOLLOUT;

	if (__test_bit(TASK_EVENT_FLAG_EPOLL, &event->flags))
		op = EPOLL_CTL_MOD;
	else
		op = EPOLL_CTL_ADD;

	if (epoll_ctl(m->epoll_fd, op, event->fd, &ev) < 0) {
		log_message(LOG_INFO, "scheduler: Error performing control on EPOLL instance (%m)");
		return -1;
	}

	__set_bit(TASK_EVENT_FLAG_EPOLL, &event->flags);
	return 0;
}

static int _sysThreadEventCancel(const CmnTask *thread_cp)
{
	CmnTask *thread = no_const(CmnTask, thread_cp);
	TaskEvent *event = thread->event;
	TaskMaster *m = thread->master;

	if (!event) {
		log_message(LOG_INFO, "scheduler: Error performing epoll_ctl DEL op no event linked?!");
		return -1;
	}

	/* Ignore error if it was an SNMP fd, since we don't know
	 * if they have been closed */
	if (m->epoll_fd != -1 &&
	    epoll_ctl(m->epoll_fd, EPOLL_CTL_DEL, event->fd, NULL) < 0 &&
#ifdef _WITH_SNMP_
	    !FD_ISSET(event->fd, &m->snmp_fdset) &&
#endif
	    errno != EBADF)
		log_message(LOG_INFO, "scheduler: Error performing epoll_ctl DEL op for fd:%d (%m)", event->fd);

	rb_erase(&event->n, &m->io_events);
	if (event == m->current_event)
		m->current_event = NULL;
	FREE(thread->event);
	return 0;
}

static int _sysThreadEventDel(const CmnTask *thread_cp, unsigned flag)
{
	CmnTask *thread = no_const(CmnTask, thread_cp);
	TaskEvent *event = thread->event;

	if (!__test_bit(flag, &event->flags))
		return 0;

	if (flag == TASK_EVENT_FLAG_EPOLL_READ) {
		__clear_bit(TASK_EVENT_FLAG_READ, &event->flags);
		if (!__test_bit(TASK_EVENT_FLAG_EPOLL_WRITE, &event->flags))
			return _sysThreadEventCancel(thread);

		event->read = NULL;
	}
	else if (flag == TASK_EVENT_FLAG_EPOLL_WRITE) {
		__clear_bit(TASK_EVENT_FLAG_WRITE, &event->flags);
		if (!__test_bit(TASK_EVENT_FLAG_EPOLL_READ, &event->flags))
			return _sysThreadEventCancel(thread);

		event->write = NULL;
	}

	if (_sysThreadEventSet(thread) < 0)
		return -1;

	__clear_bit(flag, &event->flags);
	return 0;
}


