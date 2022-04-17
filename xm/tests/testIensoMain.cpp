
#include "iesDeviceMgr.hpp"
#include "iesIoCtx.hpp"

#include "xm.hpp"
#include <libBase.h>

int testSockets(void);

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

int testTimers(void)
{
#define	__TIMER_MAX		2
	std::shared_ptr<ienso::ctx::IoCtx> ctx = std::make_shared<ienso::ctx::IoCtx>();
	btime_val delay;
	batomic_t *total;
	int i, ret;
	int	timerIDs[__TIMER_MAX+1];

	if(ctx->init() == false)
	{
		return -1;
	}
	
	bstatus_t status = batomic_create(ctx->getMemPool(), 0, &total);
	if (status != BASE_SUCCESS)
	{
		sdkPerror(status, "Unable to create atomic");
		return -1;
	}

	for(i = 0; i< 2; i++)
	{
		delay.sec = brand()%2;
		delay.msec = brand() % 999;
		timerIDs[i] = i;

		ret = ctx->addTimer(delay, (i%2==0)?false:true, [total](void *data)
			{
				int *index = (int *)data;
				BASE_DEBUG("Timer#%d emitted now", *index);
				batomic_inc(total);
			}
			, &timerIDs[i]);
		if(ret < 0)
		{
			BASE_ERROR("Timer#%d add failed", i);
			return -1;
		}
		timerIDs[i] = ret;
		BASE_DEBUG("Timer(ID:%d) added", ret);
	}

	delay.sec = 10;
	delay.msec = 0;
	timerIDs[i] = i;
	/* lambda capture by reference */
	ret = ctx->addTimer(delay, false, [&ctx, total](void *data)
		{
			int *index = (int *)data;
			BASE_DEBUG("Timer#%d will stop all now", *index);
			
			batomic_inc(total);
			ctx->stop();
		}
		, &timerIDs[i]);

	if(ret < 1)
	{
		BASE_ERROR("Timer#%d add failed", i);
		return -1;
	}
	BASE_DEBUG("Timer(ID:%d) added", ret);

	ctx->run();

	BASE_DEBUG("Timer test over, total %d timers have been emitted", batomic_get(total));

	batomic_destroy(total);

	return 0;
}

int main(int argc, char *argv[])
{
	int rc = 0;
#if 0
	SocketClient client;

	client.sockType = bSOCK_STREAM();
	client.server = "192.168.1.12";
	client.port = 34567;

	BASE_UNUSED_ARG(argc);
	BASE_UNUSED_ARG(argv);

	init_signals();

	sdkInit(6);

	
	socktClientSearch();
	
//	socketClientRun(&client);

	sdkDestroy();
#else
//	testTimers();

	testSockets();

//	ienso::sdk::DeviceMgr devMgr(6, "192.168.1.1");
//	devMgr.lookupDevice();
//	devMgr.addDevice("192.168.1.12", 34567, "admin", "", 0);
#endif

	BASE_DEBUG( "%s exit normally!", argv[0]);
	return rc;
}


