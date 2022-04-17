
#include <libBase.h>
#include <libUtil.h>
#include "xmSdk.h"

bpool_factory *sdkMemFactory;

//int param_log_decor = BASE_LOG_HAS_NEWLINE | BASE_LOG_HAS_TIME | BASE_LOG_HAS_MICRO_SEC;
int param_log_decor = BASE_LOG_HAS_NEWLINE | BASE_LOG_HAS_SENDER|
	BASE_LOG_HAS_COLOR;


bcaching_pool caching_pool;
int sdkInit(int logLevel/* max=6*/)
{
	int rc = 0;

	sdkMemFactory = &caching_pool.factory;

	blog_set_level(logLevel);
	blog_set_decor(param_log_decor);

	/* error */
	blog_set_color(3, BASE_TERM_COLOR_R);//|BASE_TERM_COLOR_BRIGHT);
	/* warn */
	blog_set_color(4, BASE_TERM_COLOR_R|BASE_TERM_COLOR_G);//|BASE_TERM_COLOR_BRIGHT);
	/* info */
	blog_set_color(5, BASE_TERM_COLOR_G|BASE_TERM_COLOR_B);//|BASE_TERM_COLOR_BRIGHT);
	/* debug */
	blog_set_color(6, BASE_TERM_COLOR_G);//|BASE_TERM_COLOR_BRIGHT);

	rc = libBaseInit();
	if (rc != 0)
	{
		sdkPerror("libBaseInit() error!!", rc);
		return rc;
	}

	rc = libUtilInit();
	bassert(rc == 0);

//	bConfigDump();
//	bcaching_pool_init( &caching_pool, &bpool_factory_default_policy, 0 );
	bcaching_pool_init( &caching_pool, NULL, 0 );

	return rc;
}


void sdkDestroy(void)
{
	/* libutil : no shutdown */

	libBaseShutdown();
}



