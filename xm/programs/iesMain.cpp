
#include "iesDeviceMgr.hpp"
#include "iesDevice.hpp"
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

int main(int argc, char *argv[])
{
//	int rc = 0;
	ienso::ctx::IoCtx ctx(6);

//	_ctx = &ctx;
	
	if(ctx.init() == false)
	{
		return -1;
	}
	
//	testTimers();
	ienso::sdk::Device *dev;
#if 1
	ienso::sdk::DeviceMgr devMgr(&ctx, "192.168.1.1", static_cast<buint16_t>(XmServicePort::UDP_BOARDCAST_PORT));
	dev = devMgr.findDevice("192.168.1.148");
#else
	ienso::sdk::DeviceMgr devMgr(&ctx, "192.168.12.9", static_cast<buint16_t>(XmServicePort::UDP_BOARDCAST_PORT));
dev = devMgr.findDevice("192.168.12.11");
#endif
	if(dev != nullptr )
	{
		dev->startSession("admin", "abc123");
	}

	ctx.run();

	BASE_DEBUG( "%s exit normally!", argv[0]);
	return 0;
}


