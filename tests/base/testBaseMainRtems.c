/* 
 *
 */

/*
 * - Many thanks for Zetron, Inc. and Phil Torre <ptorre@zetron.com> for 
 *   donating this file and the RTEMS port in general!
 */

#include "testBaseTest.h"

#include <baseErrno.h>
#include <baseString.h>
#include <baseSock.h>
#include <baseLog.h>

extern int param_echo_sock_type;
extern const char *param_echo_server;
extern int param_echo_port;

#include <bsp.h>

#define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM
#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS    300
#define CONFIGURE_MAXIMUM_TASKS                     50
#define CONFIGURE_MAXIMUM_MESSAGE_QUEUES            rtems_resource_unlimited(10)
#define CONFIGURE_MAXIMUM_SEMAPHORES                rtems_resource_unlimited(10)
#define CONFIGURE_MAXIMUM_TIMERS                    50
#define CONFIGURE_MAXIMUM_REGIONS                   3
#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_TIMER_DRIVER
#define CONFIGURE_TICKS_PER_TIMESLICE               2
//#define CONFIGURE_RTEMS_INIT_TASKS_TABLE
#define CONFIGURE_POSIX_INIT_THREAD_TABLE


#define CONFIGURE_MAXIMUM_POSIX_MUTEXES	    rtems_resource_unlimited(16)
#define CONFIGURE_MAXIMUM_POSIX_CONDITION_VARIABLES rtems_resource_unlimited(5)
#define CONFIGURE_MAXIMUM_POSIX_SEMAPHORES  rtems_resource_unlimited(16)
#define CONFIGURE_MAXIMUM_POSIX_TIMERS	    rtems_resource_unlimited(5)
#define CONFIGURE_MAXIMUM_POSIX_THREADS	    rtems_resource_unlimited(16)
#define CONFIGURE_MAXIMUM_POSIX_KEYS	    rtems_resource_unlimited(16)

#define CONFIGURE_POSIX_INIT_THREAD_STACK_SIZE	4096

/* Make sure that stack size is at least 4096 */
#define SZ					(4096-RTEMS_MINIMUM_STACK_SIZE)
#define CONFIGURE_EXTRA_TASK_STACKS		((SZ)<0 ? 0 : (SZ))

#define CONFIGURE_INIT
#define STACK_CHECKER_ON

rtems_task Init(rtems_task_argument Argument) ;
void *POSIX_Init(void *argument);

#include <confdefs.h>
#include <rtems.h>

/* Any tests that want to build a linked executable for RTEMS must include
   these headers to get a default config for the network stack. */
#include <rtems/rtems_bsdnet.h>
#include "rtems_network_config.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static void* libUtilTestMain(void* unused);
static void  initialize_network();
static void test_sock(void);

static void my_perror(bstatus_t status, const char *title)
{
    char err[BASE_ERR_MSG_SIZE];

    extStrError(status, err, sizeof(err));
    printf("%s: %s [%d]\n", title, err, status);
}

#define TEST(expr)    { int rc;\
		        /*BASE_INFO("%s", #expr));*/ \
			/*sleep(1);*/ \
		        rc=expr; \
		        if (rc) my_perror(BASE_STATUS_FROM_OS(rc),#expr); }



//rtems_task Init(rtems_task_argument Argument)
void *POSIX_Init(void *argument)
{
    pthread_attr_t	threadAttr;
    pthread_t     	theThread;
    struct sched_param	sched_param;
    size_t		stack_size;
    int           	result;
    char		data[1000];
    

    memset(data, 1, sizeof(data));

    /* Set the TOD clock, so that gettimeofday() will work */
    rtems_time_of_day fakeTime = { 2006, 3, 15, 17, 30, 0, 0 };

    if (RTEMS_SUCCESSFUL != rtems_clock_set(&fakeTime))
    {
	assert(0);
    }	

    /* Bring up the network stack so we can run the socket tests. */
    initialize_network();

    /* Start a POSIX thread for libUtilTestMain(), since that's what it
     * thinks it is running in. 
     */

    /* Initialize attribute */
    TEST( pthread_attr_init(&threadAttr) );

    /* Looks like the rest of the attributes must be fully initialized too,
     * or otherwise pthread_create will return EINVAL.
     */

    /* Specify explicit scheduling request */
    TEST( pthread_attr_setinheritsched(&threadAttr, PTHREAD_EXPLICIT_SCHED));

    /* Timeslicing is needed by thread test, and this is accomplished by
     * SCHED_RR.
     */
    TEST( pthread_attr_setschedpolicy(&threadAttr, SCHED_RR));

    /* Set priority */
    TEST( pthread_attr_getschedparam(&threadAttr, &sched_param));
    sched_param.sched_priority = NETWORK_STACK_PRIORITY - 10;
    TEST( pthread_attr_setschedparam(&threadAttr, &sched_param));

    /* Must have sufficient stack size (large size is needed by
     * logger, because default settings for logger is to use message buffer
     * from the stack).
     */
    TEST( pthread_attr_getstacksize(&threadAttr, &stack_size));
    if (stack_size < 8192)
	TEST( pthread_attr_setstacksize(&threadAttr, 8192));


    /* Create the thread for application */
    result = pthread_create(&theThread, &threadAttr, &libUtilTestMain, NULL);
    if (result != 0) {
	my_perror(BASE_STATUS_FROM_OS(result), 
		  "Error creating libUtilTestMain thread");
	assert(!"Error creating main thread");
    } 

    return NULL;
}



#define boost()
#define init_signals()

static void* libUtilTestMain(void* unused)
{
    int rc;

    /* Drop our priority to below that of the network stack, otherwise
     * select() tests will fail. */
    struct sched_param schedParam;
    int schedPolicy;
  
    printf("libBasetest_main thread started..\n");

    TEST( pthread_getschedparam(pthread_self(), &schedPolicy, &schedParam) );

    schedParam.sched_priority = NETWORK_STACK_PRIORITY - 10;

    TEST( pthread_setschedparam(pthread_self(), schedPolicy, &schedParam) );

    boost();
    init_signals();

    //my_test_thread("from libBase_test_main");
    //test_sock();

    rc = testBaseMain();

    return (void*)rc;
}

#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>

/* 
 * Send UDP packet to some host. We can then use Ethereal to sniff the packet
 * to see if this target really transmits UDP packet.
 */
static void
send_udp(const char *target)
{
    int sock, rc;
    struct sockaddr_in addr;

    BASE_INFO("IP addr=%s/%s, gw=%s", DEFAULT_IP_ADDRESS_STRING, DEFAULT_NETMASK_STRING, DEFAULT_GATEWAY_STRING);

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock > 0);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    rc = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    assert("bind error" && rc==0);

    addr.sin_addr.s_addr = inet_addr(target);
    addr.sin_port = htons(4444);

    while(1) {
	const char *data = "hello";

	rc = sendto(sock, data, 5, 0, (struct sockaddr*)&addr, sizeof(addr));
	BASE_INFO("pinging %s..(rc=%d)", target, rc);
    	sleep(1);
    }
}


static void test_sock(void)
{
    int sock;
    struct sockaddr_in addr;
    int rc;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
	printf("socket() error\n");
	goto end;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(5000);

    rc = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (rc != 0) {
	printf("bind() error %d\n", rc);
	close(sock);
	goto end;
    }

    puts("Bind socket success");

    close(sock);

end:
    while(1) sleep(1);
}

/* 
 * Initialize the network stack and Ethernet driver, using the configuration
 * in rtems-network-config.h
 */
static void
initialize_network()
{
    unsigned32 fd, result;
    char ip_address_string[] = DEFAULT_IP_ADDRESS_STRING;
    char netmask_string[] = DEFAULT_NETMASK_STRING;
    char gateway_string[] = DEFAULT_GATEWAY_STRING;

    // Write the network config files to /etc/hosts and /etc/host.conf
    result = mkdir("/etc", S_IRWXU | S_IRWXG | S_IRWXO);
    fd = open("/etc/host.conf", O_RDWR | O_CREAT, 0744);
    result = write(fd, "hosts,bind\n", 11);
    result = close(fd);
    fd = open("/etc/hosts", O_RDWR | O_CREAT, 0744);
    result = write(fd, "127.0.0.1	localhost\n", 41);
    result = write(fd, ip_address_string, strlen(ip_address_string));
    result = write(fd, "	extsip-test\n", 32); 
    result = close(fd);

    netdriver_config.ip_address = ip_address_string;
    netdriver_config.ip_netmask = netmask_string;
    rtems_bsdnet_config.gateway = gateway_string;

    if (0 != rtems_bsdnet_initialize_network())
	BASE_ERROR("Error: Unable to initialize network stack!");
    else
	BASE_INFO("IP addr=%s/%s, gw=%s", 
			      ip_address_string,
			      netmask_string,
			      gateway_string);

    //rtems_rdbg_initialize();
    //enterRdbg();
    //send_udp("192.168.0.1");
    //test_sock();
}


