/* 
 *
 */
#include "testUtilTest.h"
#include <libBase.h>
#include <libUtil.h>

void app_perror(const char *msg, bstatus_t rc)
{
	char errbuf[256];

	BASE_CHECK_STACK();

	extStrError(rc, errbuf, sizeof(errbuf));
	BASE_CRIT( "%s: [bstatus_t=%d] %s", msg, rc, errbuf);
}

#define DO_TEST(test)	do { \
			    BASE_INFO("Running %s...", #test);  \
			    rc = test; \
			    BASE_INFO(  \
				       "%s(%d)",  \
				       (char*)(rc ? "..ERROR" : "..success"), rc); \
			    if (rc!=0) goto on_return; \
			} while (0)


bpool_factory *mem;

int param_log_decor = BASE_LOG_HAS_NEWLINE | BASE_LOG_HAS_TIME | 
		      BASE_LOG_HAS_MICRO_SEC;

static int test_inner(void)
{
	bcaching_pool caching_pool;
	int rc = 0;

	mem = &caching_pool.factory;

	blog_set_level(3);
	blog_set_decor(param_log_decor);

	rc = libBaseInit();
	if (rc != 0)
	{
		app_perror("libBaseInit() error!!", rc);
		return rc;
	}

	rc = libUtilInit();
	bassert(rc == 0);

	bConfigDump();
//	bcaching_pool_init( &caching_pool, &bpool_factory_default_policy, 0 );
	bcaching_pool_init( &caching_pool, NULL, 0 );

#if INCLUDE_XML_TEST
	DO_TEST(xml_test());
#endif

#if INCLUDE_JSON_TEST
	DO_TEST(json_test());
#endif

#if INCLUDE_ENCRYPTION_TEST
	DO_TEST(encryption_test());
	DO_TEST(encryption_benchmark());
#endif

#if INCLUDE_STUN_TEST
	DO_TEST(stun_test());
#endif

#if INCLUDE_RESOLVER_TEST
	DO_TEST(resolver_test());
#endif

#if INCLUDE_HTTP_CLIENT_TEST
	DO_TEST(http_client_test());
#endif

on_return:
	return rc;
}

int test_main(void)
{
    BASE_USE_EXCEPTION;

    BASE_TRY {
        return test_inner();
    }
    BASE_CATCH_ANY {
        int id = BASE_GET_EXCEPTION();
        BASE_CRIT( "FATAL: unhandled exception id %d (%s)", id, bexception_id_name(id));
    }
    BASE_END;

    return -1;
}

