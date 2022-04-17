/* 
 *
 */
 
#include "testBaseTest.h"
#include <libBase.h>

#ifdef _MSC_VER
#pragma warning(disable:4127)
#endif

#define DO_TEST(test)	do { \
			    BASE_WARN(TEST_LEVEL_ITEM"Running %s...", #test);  \
			    rc = test; \
			    BASE_WARN(  \
				       TEST_LEVEL_ITEM"%s(%d)\n",  \
				       (rc ? "..ERROR" : "..success"), rc); \
			    if (rc!=0) goto on_return; \
			} while (0)


bpool_factory *mem;

int param_echo_sock_type;
const char *param_echo_server = ECHO_SERVER_ADDRESS;
int param_echo_port = ECHO_SERVER_START_PORT;
int param_log_decor = BASE_LOG_HAS_NEWLINE | BASE_LOG_HAS_TIME |BASE_LOG_HAS_MICRO_SEC|BASE_LOG_HAS_SENDER|
	BASE_LOG_HAS_COLOR;

int null_func()
{
	return 0;
}

int _baseTestInner(void)
{
	bcaching_pool caching_pool;
	const char *filename;
	int line;
	int rc = 0;

	mem = &caching_pool.factory;

	blog_set_level(5);
	blog_set_decor(param_log_decor);
	blog_set_color(3, BASE_TERM_COLOR_R|BASE_TERM_COLOR_B);//|BASE_TERM_COLOR_BRIGHT);
	blog_set_color(4, BASE_TERM_COLOR_R|BASE_TERM_COLOR_G);//|BASE_TERM_COLOR_BRIGHT);
	blog_set_color(5, BASE_TERM_COLOR_G|BASE_TERM_COLOR_B);//|BASE_TERM_COLOR_BRIGHT);
	blog_set_color(6, BASE_TERM_COLOR_G);//|BASE_TERM_COLOR_BRIGHT);

	rc = libBaseInit();
	if (rc != 0)
	{
		app_perror("libBaseInit() error!!", rc);
		return rc;
	}
	BTRACE();

	//bConfigDump();
	bcaching_pool_init( &caching_pool, NULL, 0 );

#if 0//INCLUDE_ERRNO_TEST
	DO_TEST( testBaseErrno() );
#endif

#if 0//INCLUDE_EXCEPTION_TEST
	DO_TEST( testBaseException() );
#endif

#if 0//INCLUDE_OS_TEST
	DO_TEST( testBaseOs() );
#endif

#if 0// INCLUDE_RAND_TEST
	DO_TEST( testBaseRand() );
#endif

#if 0//INCLUDE_LIST_TEST
	DO_TEST( list_test() );
#endif

#if 0//INCLUDE_POOL_TEST
	DO_TEST( pool_test() );
#endif

#if 0//INCLUDE_POOL_PERF_TEST
	DO_TEST( pool_perf_test() );
#endif

#if 0//INCLUDE_STRING_TEST
	DO_TEST( string_test() );
#endif

#if 0//INCLUDE_FIFOBUF_TEST
	DO_TEST( fifobuf_test() );
#endif

#if 0//INCLUDE_RBTREE_TEST
	DO_TEST( rbtree_test() );
#endif

#if 0//INCLUDE_HASH_TEST
	DO_TEST( hash_test() );
#endif

#if 0//INCLUDE_TIMESTAMP_TEST
	DO_TEST( testBaseTimestamp() );
#endif

#if 0//INCLUDE_ATOMIC_TEST
	DO_TEST( atomic_test() );
#endif

#if 0//INCLUDE_MUTEX_TEST
	DO_TEST( testBaseMutex() );
#endif

#if 0//INCLUDE_TIMER_TEST
	DO_TEST( testBaseTimer() );
#endif

#if 0//INCLUDE_SLEEP_TEST
	DO_TEST( sleep_test() );
#endif

#if 0//INCLUDE_THREAD_TEST
	DO_TEST( testBaseThread() );
#endif

#if 0//INCLUDE_SOCK_TEST
	DO_TEST( testBaseSock() );
#endif

#if 0//INCLUDE_SOCK_PERF_TEST
	DO_TEST( sock_perf_test() );
#endif

#if 0//INCLUDE_SELECT_TEST
	DO_TEST( select_test() );
#endif

#if 0//INCLUDE_UDP_IOQUEUE_TEST
	DO_TEST( testBaseUdpIoqueue() );
#endif

#if 0//BASE_HAS_TCP && INCLUDE_TCP_IOQUEUE_TEST
	DO_TEST( tcp_ioqueue_test() );
#endif

#if 0//INCLUDE_IOQUEUE_PERF_TEST
	DO_TEST( ioqueue_perf_test() );
#endif

#if 0//INCLUDE_IOQUEUE_UNREG_TEST
	DO_TEST( udp_ioqueue_unreg_test() );
#endif

#if 1//INCLUDE_ACTIVESOCK_TEST
	DO_TEST( activesock_test() );
#endif

#if 0//INCLUDE_FILE_TEST
	DO_TEST( testBaseFile() );
#endif

#if INCLUDE_SSLSOCK_TEST
	0//DO_TEST( ssl_sock_test() );
#endif

#if 0//INCLUDE_ECHO_SERVER
	//echo_server();
	//echo_srv_sync();
	udp_echo_srv_ioqueue();

#elif INCLUDE_ECHO_CLIENT
	if (param_echo_sock_type == 0)
		param_echo_sock_type = bSOCK_DGRAM();

//	echo_client( param_echo_sock_type, param_echo_server, param_echo_port);
#endif

	goto on_return;

on_return:

	bcaching_pool_destroy( &caching_pool );

	BASE_INFO( "\n");

	bthread_get_stack_info(bthreadThis(), &filename, &line);
	BASE_INFO("Stack max usage: %u, deepest: %s:%u", bthread_get_stack_max_usage(bthreadThis()), filename, line);
	if (rc == 0)
		BASE_INFO("Looks like everything is okay!..");
	else
		BASE_ERROR("Test completed with error(s)");

	libBaseShutdown();

	return 0;
}

#include <baseSock.h>

int testBaseMain(void)
{
	BASE_USE_EXCEPTION;

	BASE_TRY
	{
		return _baseTestInner();
	}
	BASE_CATCH_ANY
	{
		int id = BASE_GET_EXCEPTION();
		BASE_CRIT("FATAL: unhandled exception id %d (%s)", id, bexception_id_name(id));
	}
	BASE_END;

	return -1;
}

