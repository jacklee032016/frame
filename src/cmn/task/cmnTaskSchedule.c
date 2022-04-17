
/* Cancel thread from scheduler. */
void sysThreadCancel(CmnTaskCRef thread_cp)
{
	CmnTask *thread = no_const(CmnTask, thread_cp);
	TaskMaster *m;

	if (!thread || thread->type == TASK_UNUSED)
		return;

	m = thread->master;

	switch (thread->type)
	{
		case TASK_READ:
			_sysThreadEventDel(thread, TASK_EVENT_FLAG_EPOLL_READ);
			rb_erase_cached(&thread->n, &m->read);
			break;
			
		case TASK_WRITE:
			_sysThreadEventDel(thread, TASK_EVENT_FLAG_EPOLL_WRITE);
			rb_erase_cached(&thread->n, &m->write);
			break;
			
		case TASK_TIMER:
			rb_erase_cached(&thread->n, &m->timer);
			break;
			
		case TASK_CHILD:
			/* Does this need to kill the child, or is that the
			 * caller's job?
			 * This function is currently unused, so leave it for now.
			 */
			rb_erase_cached(&thread->n, &m->child);
			rb_erase(&thread->rb_data, &m->child_pid);
			break;
			
		case TASK_READY_FD:
		case TASK_READ_TIMEOUT:
		case TASK_WRITE_TIMEOUT:
			if (thread->event) {
				rb_erase(&thread->event->n, &m->io_events);
				FREE(thread->event);
			}
			/* ... falls through ... */
		case TASK_EVENT:
		case TASK_READY:
#ifdef USE_SIGNAL_THREADS
		case TASK_SIGNAL:
#endif
		case TASK_CHILD_TIMEOUT:
		case TASK_CHILD_TERMINATED:
			list_head_del(&thread->next);
			break;
			
		default:
			break;
	}

	_threadAddUnuse(m, thread);
}

void sysThreadCancelRead(TaskMaster *m, int fd)
{
	CmnTask *thread, *thread_tmp;

	rb_for_each_entry_safe_cached(thread, thread_tmp, &m->read, n)
	{
		if (thread->u.f.fd == fd)
		{
			if (thread->event->write)
			{
				sysThreadCancel(thread->event->write);
				thread->event->write = NULL;
			}
			
			sysThreadCancel(thread);
			break;
		}
	}
}

#ifdef _INCLUDE_UNUSED_CODE_
/* Delete all events which has argument value arg. */
void thread_cancel_event(TaskMaster *m, void *arg)
{
	CmnTask *thread, *thread_tmp;
	list_head_t *l = &m->event;

// Why doesn't this use sysThreadCancel() above
	list_for_each_entry_safe(thread, thread_tmp, l, next) {
		if (thread->arg == arg) {
			list_head_del(&thread->next);
			_threadAddUnuse(m, thread);
		}
	}
}
#endif

#ifdef _WITH_SNMP_
#include "_sysThreadSnmp.c"
#endif

/* Update timer value */
static void __threadUpdateTimer(rb_root_cached_t *root, timeval_t *timer_min)
{
	const CmnTask *first;

	if (!root->rb_root.rb_node)
		return;

	first = rb_entry(rb_first_cached(root), CmnTask, n);

	if (first->sands.tv_sec == TIMER_DISABLED)
		return;

	if (!timerisset(timer_min) || timercmp(&first->sands, timer_min, <=))
	{
		*timer_min = first->sands;
	}
}

/* Compute the wait timer. Take care of timeouted fd */
static void __threadSetTimer(TaskMaster *m)
{
	timeval_t timer_wait;
	struct itimerspec its;

	/* Prepare timer */
	timerclear(&timer_wait);
	__threadUpdateTimer(&m->timer, &timer_wait);
	__threadUpdateTimer(&m->write, &timer_wait);
	__threadUpdateTimer(&m->read, &timer_wait);
	__threadUpdateTimer(&m->child, &timer_wait);

	if (timerisset(&timer_wait))
	{
		/* Re-read the current time to get the maximum accuracy */
		cmnTimeSetNow();

		/* Take care about monotonic clock */
		timersub(&timer_wait, &time_now, &timer_wait);

		if (timer_wait.tv_sec < 0)
		{
			/* This will disable the timerfd */
			timerclear(&timer_wait);
		}
	}
	else
	{
		/* set timer to a VERY long time */
		timer_wait.tv_sec = LONG_MAX;
		timer_wait.tv_usec = 0;
	}

	its.it_value.tv_sec = timer_wait.tv_sec;
	if (!timerisset(&timer_wait))
	{
		/* We could try to avoid doing the epoll_wait since
		 * testing shows it takes about 4 microseconds
		 * for the timer to expire. */
		its.it_value.tv_nsec = 1;
	}
	else
		its.it_value.tv_nsec = timer_wait.tv_usec * 1000;

	/* We don't want periodic timer expiry */
	its.it_interval.tv_sec = its.it_interval.tv_nsec = 0;

	timerfd_settime(m->timer_fd, 0, &its, NULL);

#ifdef _EPOLL_DEBUG_
	if (do_epoll_debug)
		log_message(LOG_INFO, "Setting timer_fd %ld.%9.9ld", its.it_value.tv_sec, its.it_value.tv_nsec);
#endif
}

/* Fetch next ready thread. */
static list_head_t *_sysThreadFetchNextQueue(TaskMaster *m)
{
	int last_epoll_errno = 0;
	int ret;
	int i;

	assert(m != NULL);

	/* If there is event process it first. */
	if (m->event.next != &m->event)
		return &m->event;

	/* If there are ready threads process them */
	if (m->ready.next != &m->ready)
		return &m->ready;

	do {
#ifdef _WITH_SNMP_
		if (snmp_running)
		{
			_sysThreadSnmpEpollInfo(m);
		}
#endif

		/* Calculate and set wait timer. Take care of timeouted fd.  */
		__threadSetTimer(m);

#ifdef _VRRP_FD_DEBUG_
		if (extra_threads_debug)
			extra_threads_debug();
#endif

#ifdef _EPOLL_TASK_DUMP_
		if (do_epoll_thread_dump)
			dump_thread_data(m, NULL);
#endif

#ifdef _EPOLL_DEBUG_
		if (do_epoll_debug)
			log_message(LOG_INFO, "calling epoll_wait");
#endif

		/* Call epoll function. */
		ret = epoll_wait(m->epoll_fd, m->epoll_events, m->epoll_count, -1);

#ifdef _EPOLL_DEBUG_
		if (do_epoll_debug) {
			int sav_errno = errno;

			if (ret == -1)
				log_message(LOG_INFO, "epoll_wait returned %d, errno %d", ret, sav_errno);
			else
				log_message(LOG_INFO, "epoll_wait returned %d fds", ret);

			errno = sav_errno;
		}
#endif

		if (ret < 0) {
			if (check_EINTR(errno))
				continue;

			/* Real error. */
			if (errno != last_epoll_errno) {
				last_epoll_errno = errno;

				/* Log the error first time only */
				log_message(LOG_INFO, "scheduler: epoll_wait error: %s", strerror(errno));
			}

			/* Make sure we don't sit it a tight loop */
			if (last_epoll_errno == EBADF || last_epoll_errno == EFAULT || last_epoll_errno == EINVAL)
				sleep(1);

			continue;
		}

		/* Handle epoll events */
		for (i = 0; i < ret; i++) {
			struct epoll_event *ep_ev;
			TaskEvent *ev;

			ep_ev = &m->epoll_events[i];
			ev = ep_ev->data.ptr;

			/* Error */
// TODO - no thread processing function handles TASK_READ_ERROR/TASK_WRITE_ERROR yet
			if (ep_ev->events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
				if (ev->read) {
					_threadMoveReady(m, &m->read, ev->read, TASK_READ_ERROR);
					ev->read = NULL;
				}

				if (ev->write) {
					_threadMoveReady(m, &m->write, ev->write, TASK_WRITE_ERROR);
					ev->write = NULL;
				}

				if (__test_bit(LOG_DETAIL_BIT, &debug)) {
					if (ep_ev->events & EPOLLRDHUP)
						log_message(LOG_INFO, "Received EPOLLRDHUP for fd %d", ev->fd);
				}

				continue;
			}

			/* READ */
			if (ep_ev->events & EPOLLIN) {
				if (!ev->read) {
					log_message(LOG_INFO, "scheduler: No read thread bound on fd:%d (fl:0x%.4X)"
						      , ev->fd, ep_ev->events);
					continue;
				}
				_threadMoveReady(m, &m->read, ev->read, TASK_READY_FD);
				ev->read = NULL;
			}

			/* WRITE */
			if (ep_ev->events & EPOLLOUT) {
				if (!ev->write) {
					log_message(LOG_INFO, "scheduler: No write thread bound on fd:%d (fl:0x%.4X)"
						      , ev->fd, ep_ev->events);
					continue;
				}
				_threadMoveReady(m, &m->write, ev->write, TASK_READY_FD);
				ev->write = NULL;
			}
		}

		/* Update current time */
		cmnTimeSetNow();

		/* If there is a ready thread, return it. */
		if (m->ready.next != &m->ready)
			return &m->ready;
	} while (true);
}


void sysThreadProcessThreads(TaskMaster *m)
{
	CmnTask* thread;
	list_head_t *thread_list;
	int thread_type;

	/*
	 * Processing the master thread queues,
	 * return and execute one ready thread.
	 */
	while ((thread_list = _sysThreadFetchNextQueue(m)))
	{
		/* Run until error, used for debuging only */
#if defined _DEBUG_ && defined _MEM_CHECK_
		if (__test_bit(MEM_ERR_DETECT_BIT, &debug)
#ifdef _WITH_VRRP_
		    && __test_bit(DONT_RELEASE_VRRP_BIT, &debug)
#endif
								)
		{
			__clear_bit(MEM_ERR_DETECT_BIT, &debug);
#ifdef _WITH_VRRP_
			__clear_bit(DONT_RELEASE_VRRP_BIT, &debug);
#endif
			thread_add_terminate_event(master);
		}
#endif

		/* If we are shutting down, only process relevant thread types.
		 * We only want timer and signal fd, and don't want inotify, vrrp socket,
		 * snmp_read, bfd_receiver, bfd pipe in vrrp/check, dbus pipe or netlink fds. */
		thread = _sysThreadTrimHead(thread_list);
		if (!shutting_down ||
		    (thread->type == TASK_READY_FD &&
		     (thread->u.f.fd == m->timer_fd ||
		      thread->u.f.fd == m->signal_fd
#ifdef _WITH_SNMP_
		      || FD_ISSET(thread->u.f.fd, &m->snmp_fdset)
#endif
							       )) ||
		    thread->type == TASK_CHILD ||
		    thread->type == TASK_CHILD_TIMEOUT ||
		    thread->type == TASK_CHILD_TERMINATED ||
		    thread->type == TASK_TIMER_SHUTDOWN ||
		    thread->type == TASK_TERMINATE)
		{
			if (thread->func)
			{
//				thread_call(thread);
#ifdef _EPOLL_DEBUG_
				if (do_epoll_debug)
					log_message(LOG_INFO, "Calling thread function %s(), type %s, val/fd/pid %d, status %d id %lu", _getFunctionName(thread->func), _getThreadTypeName(thread->type), thread->u.val, thread->u.c.status, thread->id);
#endif

				(*thread->func) (thread);
			}

			if (thread->type == TASK_TERMINATE_START)
				shutting_down = true;
		}

		m->current_event = (thread->type == TASK_READY_FD) ? thread->event : NULL;
		thread_type = thread->type;
		_threadAddUnuse(master, thread);

		/* If we are shutting down, and the shutdown timer is not running and
		 * all children have terminated, then we can terminate */
		if (shutting_down && !m->shutdown_timer_running && !m->child.rb_root.rb_node)
			break;

		/* If daemon hanging event is received stop processing */
		if (thread_type == TASK_TERMINATE)
			break;
	}
}

static void _sysThreadChildTermination(pid_t pid, int status)
{
	TaskMaster * m = master;
	CmnTask th = { .u.c.pid = pid };
	CmnTask *thread;
	bool permanent_vrrp_checker_error = false;

#ifndef _DEBUG_
	if (prog_type == PROG_TYPE_PARENT)
		permanent_vrrp_checker_error = report_child_status(status, pid, NULL);
#endif

	thread = rb_search(&master->child_pid, &th, rb_data, _sysThreadChildPidCmp);

#ifdef _EPOLL_DEBUG_
	if (do_epoll_debug)
		log_message(LOG_INFO, "Child %d terminated with status 0x%x, thread_id %lu", pid, (unsigned)status, thread ? thread->id : 0);
#endif

	if (!thread)
		return;

	rb_erase(&thread->rb_data, &master->child_pid);

	thread->u.c.status = status;

	if (permanent_vrrp_checker_error)
	{
		/* The child had a permanant error, so no point in respawning */
		rb_erase_cached(&thread->n, &m->child);
		_threadAddUnuse(m, thread);

		thread_add_terminate_event(m);
	}
	else
		_threadMoveReady(m, &m->child, thread, TASK_CHILD_TERMINATED);
}

/* Synchronous signal handler to reap child processes */
void thread_child_handler(__attribute__((unused)) void *v, __attribute__((unused)) int unused)
{
	pid_t pid;
	int status;

	while ((pid = waitpid(-1, &status, WNOHANG)))
	{
		if (pid == -1)
		{
			if (errno == ECHILD)
			{
				return;
			}
			log_message(LOG_DEBUG, "waitpid error %d: %s", errno, strerror(errno));
			assert(0);

			return;
		}
		
		_sysThreadChildTermination(pid, status);
	}
}

void thread_add_base_threads(TaskMaster *m,
#ifndef _WITH_SNMP_
					    __attribute__ ((unused))
#endif
								     bool with_snmp)
{
	m->timer_thread = sysThreadAddRead(m, thread_timerfd_handler, NULL, m->timer_fd, TIMER_NEVER, false);
	
	sysSignalReadThreadAdd(m);
	
#ifdef _WITH_SNMP_
	if (with_snmp)
		m->snmp_timer_thread = sysThreadAddTimer(m, sysThreadSnmpTimeoutThread, 0, TIMER_NEVER);
#endif
}

/* Our infinite scheduling loop */
void launch_thread_scheduler(TaskMaster *m)
{
// TODO - do this somewhere better
	sysSignalSetHandler(SIGCHLD, thread_child_handler, m);

	sysThreadProcessThreads(m);
}


