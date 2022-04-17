
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <unistd.h>

#include "libCmn.h"
//#include "config.h"

#ifdef _WITH_STACKTRACE_
#include <sys/stat.h>
#include <execinfo.h>
#include <memory.h>
#endif

#include <sys/prctl.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/wait.h>


#include <limits.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <ctype.h>

#include "bitops.h"
#include "utils/cmnMem.h"
#include "lib/cmnSignals.h"
#include "utils/cmnSysUtils.h"

/*
 */
static char*_getModProbe(void)
{
	int procfile;
	char *ret;
	ssize_t count;
	struct stat buf;

	ret = MALLOC(PATH_MAX);
	if (!ret)
		return NULL;

	procfile = open("/proc/sys/kernel/modprobe", O_RDONLY | O_CLOEXEC);
	if (procfile < 0)
	{
		CMN_ERROR("Open PROC file system failed: %m");
		FREE(ret);
		return NULL;
	}

	count = read(procfile, ret, PATH_MAX - 1);
	ret[PATH_MAX - 1] = '\0';
	close(procfile);

	if (count > 0 && count < PATH_MAX - 1)
	{
		if (ret[count - 1] == '\n')
			ret[count - 1] = '\0';
		else
			ret[count] = '\0';

		/* Check it is a regular file, with a execute bit set */
		if (!stat(ret, &buf) && S_ISREG(buf.st_mode) &&(buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
		{
			return ret;
		}
	}

	CMN_ERROR("%s is invalidate", ret);
	FREE(ret);

	return NULL;
}

bool cmnSysModProbe(const char *mod_name)
{
	const char *argv[] = { "/sbin/modprobe", "-s", "--", mod_name, NULL };
	int child;
	int status;
	int rc;
	char *modprobe = _getModProbe();
	struct sigaction act, old_act;
	union non_const_args args;

	if (modprobe)
		argv[0] = modprobe;

	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	sigaction ( SIGCHLD, &act, &old_act);

#ifdef ENABLE_LOG_TO_FILE
	if (log_file_name)
		flush_log_file();
#endif

	if (!(child = fork())) {
		args.args = argv;
		/* coverity[tainted_string] */
		execv(argv[0], args.execve_args);
		exit(1);
	}

	rc = waitpid(child, &status, 0);

	sigaction ( SIGCHLD, &old_act, NULL);

	if (rc < 0)
	{
		CMN_ERROR(" waitpid error (%s)" , strerror(errno));
	}

	if (modprobe)
	{
		FREE(modprobe);
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status)) {
		return true;
	}

	return false;
}

void cmnSysSetStdFds(bool force)
{
	int fd;

	if (force /*|| __test_bit(DONT_FORK_BIT, &debug) */ )
	{
		fd = open("/dev/null", O_RDWR);
		if (fd != -1)
		{
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			
			if (fd > STDERR_FILENO)
				close(fd);
		}
	}

	sysSignalFdClose(STDERR_FILENO+1);

	/* coverity[leaked_handle] */
}

void cmnSysCloseStdFds(void)
{
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
}

int cmnSysForkExec(const char * const argv[])
{
	pid_t pid;
	int ret_pid;
	int status;
	struct sigaction act, old_act;
	int res = 0;
	union non_const_args args;

	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	sigaction(SIGCHLD, &act, &old_act);


	pid = sysProcessLocalFork();
	if (pid < 0)
	{
		res = -1;
	}
	else if (pid == 0)
	{/* Child */
		cmnSysSetStdFds(false);

		sysSignalHandlerScript();

		args.args = argv;       /* Note: we are casting away constness, since execvp parameter type is wrong */
		execvp(*argv, args.execve_args);
		exit(EXIT_FAILURE);
	}
	else
	{/* Parent */
		while ((ret_pid = waitpid(pid, &status, 0)) == -1 && check_EINTR(errno));

		if (ret_pid != pid || !WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
			res = -1;
	}

	sigaction(SIGCHLD, &old_act, NULL);

	return res;
}


int cmnSysOpenPipe(int pipe_arr[2])
{
	/* Open pipe */
#ifdef HAVE_PIPE2
	if (pipe2(pipe_arr, O_CLOEXEC | O_NONBLOCK) == -1)
#else
	if (pipe(pipe_arr) == -1)
#endif
		return -1;

#ifndef HAVE_PIPE2
	fcntl(pipe_arr[0], F_SETFL, O_NONBLOCK | fcntl(pipe_arr[0], F_GETFL));
	fcntl(pipe_arr[1], F_SETFL, O_NONBLOCK | fcntl(pipe_arr[1], F_GETFL));

	fcntl(pipe_arr[0], F_SETFD, FD_CLOEXEC | fcntl(pipe_arr[0], F_GETFD));
	fcntl(pipe_arr[1], F_SETFD, FD_CLOEXEC | fcntl(pipe_arr[1], F_GETFD));
#endif

	return 0;
}

/* Getting localhost official canonical name */
const char * /*__attribute__((malloc))*/ cmnSysGetLocalName(void)
{
	struct utsname name;
	struct addrinfo hints, *res = NULL;
	char *canonname = NULL;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_CANONNAME;

	if (uname(&name) < 0)
		return NULL;

	if (getaddrinfo(name.nodename, NULL, &hints, &res) != 0)
		return NULL;

	if (res && res->ai_canonname)
		canonname = STRDUP(res->ai_canonname);

	freeaddrinfo(res);

	return canonname;
}

void cmnSysSetProcessName(const char *name)
{
	if (!name)
		name = "main";

	if (prctl(PR_SET_NAME, name))
	{
		CMN_ERROR("Failed to set process name '%s'", name);
	}
}

#ifdef _WITH_STACKTRACE_
void cmnSysWriteStackTrace(const char *fileName, const char *message)
{
	int fd;
	void *buffer[100];
	unsigned int nptrs;
	unsigned int i;
	char **strs;

	nptrs = backtrace(buffer, 100);
	if (fileName)
	{
		fd = open(file_name, O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if (message)
			dprintf(fd, "%s\n", message);
		
		backtrace_symbols_fd(buffer, nptrs, fd);
		if (write(fd, "\n", 1) != 1)
		{/* We don't care, but this stops a warning on Ubuntu */
		}
		
		close(fd);
	}
	else
	{
		if (message)
		{
			CMN_INFO( "%s", message);
		}
		strs = backtrace_symbols(buffer, nptrs);
		if (strs == NULL)
		{
			CMN_INFO( "Unable to get stack backtrace");
			return;
		}

		/* We don't need the call to this function, or the first two entries on the stack */
		nptrs -= 2;
		for (i = 1; i < nptrs; i++)
		{
			CMN_INFO("  %s", strs[i]);
		}
		
		free(strs);
	}
}
#endif

