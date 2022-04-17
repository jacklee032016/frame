/* 
*
*/
#include "testBaseTest.h"

#include <baseString.h>
#include <baseSock.h>
#include <baseLog.h>
#include <stdio.h>

extern int param_echo_sock_type;
extern const char *param_echo_server;
extern int param_echo_port;


//#if defined(BASE_WIN32) && BASE_WIN32!=0
#if 0
#include <windows.h>
static void boost(void)
{
    SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
}
#else
#define boost()
#endif

#if defined(BASE_SUNOS) && BASE_SUNOS!=0
#include <signal.h>
static void init_signals()
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;

    sigaction(SIGALRM, &act, NULL);
}

#else
#define init_signals()
#endif

int main(int argc, char *argv[])
{
	int rc;
	int interractive = 0;

	boost();
	init_signals();

	while (argc > 1)
	{
		char *arg = argv[--argc];

		if (*arg=='-' && *(arg+1)=='i')
		{
			interractive = 1;
		}
		else if (*arg=='-' && *(arg+1)=='p')
		{
			bstr_t port = bstr(argv[--argc]);

			param_echo_port = bstrtoul(&port);
		}
		else if (*arg=='-' && *(arg+1)=='s')
		{
			param_echo_server = argv[--argc];
		}
		else if (*arg=='-' && *(arg+1)=='t')
		{
			bstr_t type = bstr(argv[--argc]);

			if (bstricmp2(&type, "tcp")==0)
				param_echo_sock_type = bSOCK_STREAM();
			else if (bstricmp2(&type, "udp")==0)
				param_echo_sock_type = bSOCK_DGRAM();
			else
			{
				BASE_ERROR("error: unknown socket type %s", type.ptr);
				return 1;
			}
		}
	}

	rc = testBaseMain();

	if (interractive)
	{
		char s[10];
		puts("");
		puts("Press <ENTER> to exit");
		if (!fgets(s, sizeof(s), stdin))
			return rc;
	}

	return rc;
}

