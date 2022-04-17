/* 
 *
 */
#ifndef __BASE_LIB_TEST_H__
#define __BASE_LIB_TEST_H__

#include <_baseTypes.h>

#define	TEST_LEVEL_ITEM			""	/* level 1: such as timer, thread */
#define	TEST_LEVEL_CASE			"   "	/* level 2: cases in one item */
#define	TEST_LEVEL_RESULT			"      "	/* level 2: result of one case */


#define GROUP_LIBC									1
#define GROUP_OS									1
#define GROUP_DATA_STRUCTURE						1
#define GROUP_NETWORK								1
#define GROUP_FILE									1

#define INCLUDE_ERRNO_TEST							GROUP_LIBC
#define INCLUDE_TIMESTAMP_TEST					GROUP_OS

#define INCLUDE_EXCEPTION_TEST	    GROUP_LIBC
#define INCLUDE_RAND_TEST							GROUP_LIBC
#define INCLUDE_LIST_TEST	    GROUP_DATA_STRUCTURE
#define INCLUDE_HASH_TEST	    GROUP_DATA_STRUCTURE
#define INCLUDE_POOL_TEST	    GROUP_LIBC
#define INCLUDE_POOL_PERF_TEST	    GROUP_LIBC
#define INCLUDE_STRING_TEST	    GROUP_DATA_STRUCTURE
#define INCLUDE_FIFOBUF_TEST	    0	// GROUP_DATA_STRUCTURE
#define INCLUDE_RBTREE_TEST	    GROUP_DATA_STRUCTURE
#define INCLUDE_TIMER_TEST							GROUP_DATA_STRUCTURE
#define INCLUDE_ATOMIC_TEST         GROUP_OS
#define INCLUDE_MUTEX_TEST	    (BASE_HAS_THREADS && GROUP_OS)
#define INCLUDE_SLEEP_TEST          GROUP_OS
#define INCLUDE_OS_TEST             GROUP_OS
#define INCLUDE_THREAD_TEST         (BASE_HAS_THREADS && GROUP_OS)
#define INCLUDE_SOCK_TEST	    GROUP_NETWORK
#define INCLUDE_SOCK_PERF_TEST	    GROUP_NETWORK
#define INCLUDE_SELECT_TEST	    GROUP_NETWORK
#define INCLUDE_UDP_IOQUEUE_TEST    GROUP_NETWORK
#define INCLUDE_TCP_IOQUEUE_TEST    GROUP_NETWORK
#define INCLUDE_ACTIVESOCK_TEST	    GROUP_NETWORK
#define INCLUDE_SSLSOCK_TEST	    (BASE_HAS_SSL_SOCK && GROUP_NETWORK)
#define INCLUDE_IOQUEUE_PERF_TEST			(BASE_HAS_THREADS && GROUP_NETWORK)
#define INCLUDE_IOQUEUE_UNREG_TEST		(BASE_HAS_THREADS && GROUP_NETWORK)
#define INCLUDE_FILE_TEST					GROUP_FILE

#define INCLUDE_ECHO_SERVER         0
#define INCLUDE_ECHO_CLIENT         0


#define ECHO_SERVER_MAX_THREADS     2
#define ECHO_SERVER_START_PORT      65000
#define ECHO_SERVER_ADDRESS         "compaq.home"
#define ECHO_SERVER_DURATION_MSEC   (60*60*1000)

#define ECHO_CLIENT_MAX_THREADS     6

BASE_BEGIN_DECL

extern int testBaseErrno(void);
extern int testBaseException(void);
extern int testBaseOs(void);
extern int testBaseRand(void);

extern int testBaseTimestamp(void);
extern int list_test(void);
extern int hash_test(void);
extern int pool_test(void);
extern int pool_perf_test(void);
extern int string_test(void);
extern int fifobuf_test(void);
extern int testBaseTimer(void);

extern int rbtree_test(void);
extern int atomic_test(void);
extern int testBaseMutex(void);
extern int sleep_test(void);
extern int testBaseThread(void);
extern int testBaseSock(void);
extern int sock_perf_test(void);
extern int select_test(void);
extern int testBaseUdpIoqueue(void);
extern int udp_ioqueue_unreg_test(void);
extern int tcp_ioqueue_test(void);
extern int ioqueue_perf_test(void);
extern int activesock_test(void);

extern int testBaseFile(void);

extern int ssl_sock_test(void);

extern int echo_server(void);
extern int echo_client(int sock_type, const char *server, int port);

extern int echo_srv_sync(void);
extern int udp_echo_srv_ioqueue(void);
extern int echo_srv_common_loop(batomic_t *bytes_counter);


extern bpool_factory *mem;

extern int          testBaseMain(void);
extern void         app_perror(const char *msg, bstatus_t err);
extern bstatus_t  app_socket(int family, int type, int proto, int port,
                               bsock_t *ptr_sock);
extern bstatus_t  app_socketpair(int family, int type, int protocol,
                                   bsock_t *server, bsock_t *client);
extern int	    null_func(void);

#define HALT(msg)   { BASE_LOG(3,(THIS_FILE,"%s halted",msg)); for(;;) sleep(1); }

BASE_END_DECL

#endif

