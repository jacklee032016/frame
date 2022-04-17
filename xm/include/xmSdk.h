/*
* header used by client code
*/

#ifndef	__IENSO_XM_SDK_H__
#define	__IENSO_XM_SDK_H__

#ifdef __cplusplus
extern "C" {
#endif

extern	bpool_factory *sdkMemFactory;

int sdkInit(int logLevel/* max=6*/);
void sdkDestroy(void);


typedef	struct _SocketClient
{
	int					sockType;	/* bSOCK_STREAM(), bSOCK_DGRAM() */
	const char			*server;
	int					port;
}SocketClient;

int socketClientRun(SocketClient *client);

int socktClientSearch(void);

#ifdef __cplusplus
}
#endif

#endif

