
#ifndef	__CMN_SYS_UTILS_H__
#define	__CMN_SYS_UTILS_H__

/* If signalfd() is used, we will have no signal handlers, and
 * so we cannot get EINTR. If we cannot get EINTR, there is no
 * point checking for it.
 * If check_EINTR is defined as false, gcc will optimise out the
 * test, and remove any surrounding while loop such as:
 * while (recvmsg(...) == -1 && check_EINTR(errno)); */
#if defined DEBUG_EINTR
static inline bool check_EINTR(int xx)
{
	if ((xx) == EINTR)
	{
		log_message(LOG_INFO, "%s:%s(%d) - EINTR returned", (__FILE__), (__func__), (__LINE__));
		return true;
	}

	return false;
}
#elif defined CHECK_EINTR
#define check_EINTR(xx)	((xx) == EINTR)
#else
#define check_EINTR(xx)	(false)
#endif

/* Functions that can return EAGAIN also document that they can return
 * EWOULDBLOCK, and that both should be checked. If they are the same
 * value, that is unnecessary. */
#if EAGAIN == EWOULDBLOCK
#define check_EAGAIN(xx)	((xx) == EAGAIN)
#else
#define check_EAGAIN(xx)	((xx) == EAGAIN || (xx) == EWOULDBLOCK)
#endif

/* The argv parameter to execve etc is declared as char *const [], whereas
 * it should be char const *const [], so we use the following union to cast
 * away the const that we have, but execve etc doesn't. */
union non_const_args
{
	const char *const		*args;
	char *const			*execve_args;
};


bool cmnSysModProbe(const char *drvModule);

void cmnSysSetStdFds(bool);
void cmnSysCloseStdFds(void);

int cmnSysForkExec(const char * const []);
int cmnSysOpenPipe(int [2]);

const char *cmnSysGetLocalName(void);// __attribute__((malloc));

void cmnSysSetProcessName(const char *);

#ifdef _WITH_STACKTRACE_
void cmnSysWriteStackTrace(const char *, const char *);
#endif

#endif

