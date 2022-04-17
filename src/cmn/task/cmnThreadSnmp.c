
static void __sysThreadDelReadFd(TaskMaster *m, int fd)
{
	const TaskEvent *event;

	event = _sysThreadEventGet(m, fd);
	if (!event || !event->read)
		return;

	sysThreadCancel(event->read);
}

static int _sysThreadSnmpReadThread(CmnTaskCRef thread)
{
	fd_set snmp_fdset;

	FD_ZERO(&snmp_fdset);
	FD_SET(thread->u.f.fd, &snmp_fdset);

	snmp_read(&snmp_fdset);
	netsnmp_check_outstanding_agent_requests();

	sysThreadAddRead(thread->master, _sysThreadSnmpReadThread, thread->arg, thread->u.f.fd, TIMER_NEVER, false);

	return 0;
}

int sysThreadSnmpTimeoutThread(CmnTaskCRef thread)
{
	snmp_timeout();
	run_alarms();
	netsnmp_check_outstanding_agent_requests();

	thread->master->snmp_timer_thread = sysThreadAddTimer(thread->master, sysThreadSnmpTimeoutThread, thread->arg, TIMER_NEVER);

	return 0;
}

// See https://vincent.bernat.im/en/blog/2012-snmp-event-loop
static void _sysThreadSnmpEpollInfo(TaskMaster *m)
{
	fd_set snmp_fdset;
	int fdsetsize = 0;
	int max_fdsetsize;
	int set_words;
	struct timeval snmp_timer_wait = { .tv_sec = TIMER_DISABLED };
	int snmpblock = true;
	unsigned long *old_set, *new_set;	// Must be unsigned for ffsl() to work for us
	unsigned long diff;
	int i;
	int fd;
	int bit;

#if 0
// TODO
#if sizeof fd_mask  != sizeof diff
#error "snmp_epoll_info sizeof(fd_mask) does not match old_set/new_set/diff"
#endif
#endif

	FD_ZERO(&snmp_fdset);

	/* When SNMP is enabled, we may have to select() on additional
	 * FD. snmp_select_info() will add them to `readfd'. The trick
	 * with this function is its last argument. We need to set it
	 * true to set its own timer that we then compare against ours. */
	snmp_select_info(&fdsetsize, &snmp_fdset, &snmp_timer_wait, &snmpblock);

	if (snmpblock)
		snmp_timer_wait.tv_sec = TIMER_DISABLED;
	timer_thread_update_timeout(m->snmp_timer_thread, cmnTimeLong(snmp_timer_wait));

	max_fdsetsize = m->snmp_fdsetsize > fdsetsize ? m->snmp_fdsetsize : fdsetsize;
	if (!max_fdsetsize)
		return;
	set_words = (max_fdsetsize - 1) / (sizeof(*old_set) * CHAR_BIT) + 1;

	for (i = 0, old_set = (unsigned long *)&m->snmp_fdset, new_set = (unsigned long *)&snmp_fdset; i < set_words; i++, old_set++, new_set++) {
		if (*old_set == *new_set)
			continue;

		diff = *old_set ^ *new_set;
		fd = i * sizeof(*old_set) * CHAR_BIT - 1;
		do {
			bit = ffsl(diff);
			diff >>= bit;
			fd += bit;
			if (FD_ISSET(fd, &snmp_fdset)) {
				/* Add the fd */
				sysThreadAddRead(m, _sysThreadSnmpReadThread, 0, fd, TIMER_NEVER, false);
				FD_SET(fd, &m->snmp_fdset);
			} else {
				/* Remove the fd */
				__sysThreadDelReadFd(m, fd);
				FD_CLR(fd, &m->snmp_fdset);
			}
		} while (diff);
	}
	m->snmp_fdsetsize = fdsetsize;
}

/* If a file descriptor was registered with epoll, but it has been closed, the registration
 * will have been lost, even though the new fd value is the same. We therefore need to
 * unregister all the fds we had registered, and reregister them. */
void sysThreadSnmpEpollReset(TaskMaster *m)
{
	fd_set snmp_fdset;
	int fdsetsize = 0;
	int set_words;
	int max_fdsetsize;
	struct timeval snmp_timer_wait = { .tv_sec = TIMER_DISABLED };
	int snmpblock = true;
	unsigned long *old_set, *new_set;	// Must be unsigned for ffsl() to work for us
	unsigned long bits;
	int i;
	int fd;
	int bit;

	FD_ZERO(&snmp_fdset);

	snmp_select_info(&fdsetsize, &snmp_fdset, &snmp_timer_wait, &snmpblock);

	max_fdsetsize = m->snmp_fdsetsize > fdsetsize ? m->snmp_fdsetsize : fdsetsize;
	if (!max_fdsetsize)
		return;

	set_words = (max_fdsetsize - 1) / (sizeof(*old_set) * CHAR_BIT) + 1;

	/* Clear all the fds that were registered with epoll */
	for (i = 0, old_set = (unsigned long *)&m->snmp_fdset; i < set_words; i++, old_set++) {
		bits = *old_set;
		fd = i * sizeof(*old_set) * CHAR_BIT - 1;
		do {
			bit = ffsl(bits);
			bits >>= bit;
			fd += bit;

			/* Remove the fd */
			__sysThreadDelReadFd(m, fd);
			FD_CLR(fd, &m->snmp_fdset);
		} while (bits);
	}

	/* Add the file descriptors that are now in use */
	for (i = 0, new_set = (unsigned long *)&snmp_fdset; i < set_words; i++, new_set++) {
		bits = *new_set;
		fd = i * sizeof(*new_set) * CHAR_BIT - 1;
		do {
			bit = ffsl(bits);
			bits >>= bit;
			fd += bit;

			/* Add the fd */
			sysThreadAddRead(m, _sysThreadSnmpReadThread, 0, fd, TIMER_NEVER, false);
			FD_SET(fd, &m->snmp_fdset);
		} while (bits);
	}
	m->snmp_fdsetsize = fdsetsize;
}

