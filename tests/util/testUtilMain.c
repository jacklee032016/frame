/*
 *
 */
#include "testUtilTest.h"
#include <baseString.h>

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

#define boost()

int main(int argc, char *argv[])
{
    int rc;

    BASE_UNUSED_ARG(argc);
    BASE_UNUSED_ARG(argv);

    boost();
    init_signals();

    rc = test_main();

    if (argc==2 && bansi_strcmp(argv[1], "-i")==0) {
	char s[10];

	puts("Press ENTER to quit");
	if (fgets(s, sizeof(s), stdin) == NULL)
	    return rc;
    }

    return rc;
}

