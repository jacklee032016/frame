#ifndef		__IES_IO_CTX_HPP__
#define		__IES_IO_CTX_HPP__

#include <map>
#include <memory>
#include <functional>

#include "libBase.h"

namespace	ienso
{
	namespace ctx
	{
		class IesSocket;
		class IoCtx;

		enum class TimerType
		{
			ONCE_TIME,
			RELOAD	
		};

		enum class TimerStatus
		{
			UN_INIT,
			INITTED,
			RUNNING
		};

		enum 
		{	
			TIMER_TOTAL = 10,
			SOCKET_TOTAL	= 20	
		};

		using TimerCallback = std::function<void (void *data)>;
		
		class IesTimer
		{
			public:
				IesTimer(IoCtx *ctx, int id);
				~IesTimer();
				
				bool 			init(const btime_val& delay, bool isReload, TimerCallback cb, void *data = nullptr);

				bool			load(void);
				
				bool 			run(void);
				btimer_entry 	*getEntry(void);

			private:

				TimerType						_mIsReload;
				TimerStatus						_mStatus;
				TimerCallback					_mCallback;
				
				btimer_entry 					_mEntry;
//				btimer_heap_callback			*_mCallback;
				void							*_mUserData;
				bgrp_lock_t						*_mGrpLock = nullptr;	/* for every timer entry */
				btime_val						_mDelay;

				IoCtx							*_mCtx;
				int								_mId;

		};

		using TimerMap = std::map<int, std::shared_ptr<IesTimer>>;
		
		using SocketMap = std::map<int, IesSocket* >;

		class IoCtx
		{
#define	__WITH_SOCK_POLL_THREAD		1		/* when sock poll thread is used in Windows, program can't exit normally */

			public:
				IoCtx(int logLevel = 6);
				~IoCtx();

				bool	init(void);
				bool	run(void);
				void	stop(void);

				/* return timer ID or < 0 */
				int					addTimer(const btime_val& delay, bool isReload, TimerCallback cb, void *data = nullptr);
				int					addTimer(const unsigned int delayMs, bool isReload, TimerCallback cb, void *data = nullptr);
				
				/* after timer has called, before it is reloaded */
				bool				removeTimer(int timerId);
				/* cancel and remove timer when it is loaded; called by client */
				bool				cancelTimer(int timerId);

				int 				pollTimers(void );
				int 				pollAsyncSockets(void );
			
				bpool_factory	*getPoolFactory(void);
				bpool_t			*getMemPool(void);
				btimer_heap_t	*getTimerHeap(void);

				bioqueue_t		*getIoQueue(void);

				/* return socket Id in the map */
				int				registerSocket(IesSocket *sock);
				bool			unregisterSocket(int socketId);
					
			private:

			
				bcaching_pool				_mCachingPool;
				bpool_factory				*_mPoolFactory;
				bpool_t						*_mPool;

				btimer_heap_t				*_mTimerHeap;
				block_t						*_mTimersRecursiveLock;	/* for timers heap */
				int							_mTimerId;

				TimerMap					_mTimers;
				TimerMap					_mTimerDeleteList;
				
				bthread_t 					*_mThreadTimer;
				bool						_mTimerStop;
				bthread_desc				_mThDescTimer;	/* call threadname, etc. */
				
				bthread_t 					*_mThreadSocket;
				bool						_mSocketStop;
				bthread_desc				_mThDescSocket;
				int							_mSocketId;

				SocketMap					_mSockets;
				SocketMap					_mSocketDeleteList;
				
				bool 						_initTimeCtx(void);

				int							_cancelAllTimer(void);

				/* fields about async socket */
				bioqueue_t					*_mIoqueue = nullptr;
				
				bool						_initSocketCtx(void);

		};
			
	
	}
}

#endif

