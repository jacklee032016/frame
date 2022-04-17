
#include "iesIoCtx.hpp"
#include "iesIoSocket.hpp"
#include "xm.hpp"

#include "libUtil.h"

namespace ienso
{
	namespace ctx
	{

	void _timerCallback(btimer_heap_t *timer_heap, btimer_entry *entry)
	{
		IesTimer *timer = (IesTimer *)entry->user_data;

		if(timer != nullptr)
		{
			timer->run();
		}
	}
	

	IesTimer::IesTimer(IoCtx *ctx,   int id)
		:_mCtx(ctx),
		_mId(id),
		_mIsReload(TimerType::ONCE_TIME),
		_mStatus(TimerStatus::UN_INIT),
		_mCallback(nullptr),
		_mUserData(nullptr),
		_mGrpLock(nullptr)
	{
	}
	
	IesTimer::~IesTimer()
	{
		if(_mStatus != TimerStatus::UN_INIT)
		{
			if(_mGrpLock != nullptr )
			{
				bgrp_lock_destroy(_mGrpLock);
			}
		}

		BASE_DEBUG("IesTimer#%d destroied", _mId);
	}
	
	bool IesTimer::init(const btime_val& delay, bool isReload, TimerCallback cb, void *data)
	{
		_mDelay = delay;
		_mCallback = cb;
		_mUserData = data;
		if(isReload == true)
		{
			_mIsReload = TimerType::RELOAD;
		}
		
		/* group lock */
		bstatus_t status = bgrp_lock_create(_mCtx->getMemPool(), NULL, &_mGrpLock);
		if (status != BASE_SUCCESS)
		{
			sdkPerror(status, "Unable to create group lock for timer");
			return false;
		}
		bgrp_lock_add_ref(_mGrpLock);

		/* create timer and schedule it on timers heap */
		btimer_entry_init(&_mEntry, _mId, this/* user data*/, &_timerCallback);

		_mStatus = TimerStatus::INITTED;
		
		return load();
	}

	bool IesTimer::load(void)
	{
		btime_val_normalize(&_mDelay);
		bstatus_t status = btimer_heap_schedule_w_grp_lock(_mCtx->getTimerHeap(), &_mEntry, &_mDelay, _mId, _mGrpLock);
		if (status != BASE_SUCCESS)
		{
			sdkPerror(status, "Unable to schedule timer entry");
			return false;
		}

		_mStatus = TimerStatus::RUNNING;
		return true;
	}

	bool IesTimer::run(void)
	{
		if(_mCallback != nullptr)
		{
			_mCallback(_mUserData);
		}

		if(_mIsReload == TimerType::RELOAD)
		{
			load();
		}
		else
		{/* ctx will destory IesTimer right now if remove from the map */
			_mCtx->removeTimer(_mId);
		}

		return true;
	}

	btimer_entry* IesTimer::getEntry(void)
	{
		return &_mEntry;
	}



	static int timerTask(void *arg)
	{
		IoCtx *ctx = (IoCtx *)arg;

		return ctx->pollTimers();
	}

	static int asyncSockTask(void *arg)
	{
		IoCtx *ctx = (IoCtx *)arg;

//		return ctx->pollAsyncSockets();
		ctx->pollAsyncSockets();
		BTRACE();
		return 0;
	}
	
	IoCtx::IoCtx(int logLevel)
		:_mTimerId(1),
		_mTimerHeap(nullptr),
		_mPool(nullptr),
		_mTimersRecursiveLock(nullptr),
		_mThreadTimer(nullptr),
		_mTimerStop(false),		
		_mThreadSocket(nullptr),
		_mSocketStop(false),
		_mSocketId(1),
		_mIoqueue(nullptr)
	{
		int rc = 0;
		
		//int param_log_decor = BASE_LOG_HAS_NEWLINE | BASE_LOG_HAS_TIME | BASE_LOG_HAS_MICRO_SEC;
		int param_log_decor = BASE_LOG_HAS_NEWLINE | BASE_LOG_HAS_SENDER|
			BASE_LOG_HAS_COLOR;
//		sdkMemFactory = &_mCachingPool.factory;
	
		blog_set_level(logLevel);
		blog_set_decor(param_log_decor);
	
		/* error */
		blog_set_color(3, BASE_TERM_COLOR_R);//|BASE_TERM_COLOR_BRIGHT);
		/* warn */
		blog_set_color(4, BASE_TERM_COLOR_R|BASE_TERM_COLOR_G);//|BASE_TERM_COLOR_BRIGHT);
		/* info */
		blog_set_color(5, BASE_TERM_COLOR_G);//|BASE_TERM_COLOR_BRIGHT);
		/* debug */
		blog_set_color(6, BASE_TERM_COLOR_R|BASE_TERM_COLOR_G|BASE_TERM_COLOR_B);//|BASE_TERM_COLOR_BRIGHT);
	
		rc = libBaseInit();
		if (rc != 0)
		{
			sdkPerror(rc, "libBaseInit() error!!");
			return;
		}
	
		rc = libUtilInit();
		bassert(rc == 0);

	//	bConfigDump();
//		bcaching_pool_init( &_mCachingPool, &bpool_factory_default_policy, 0 );
		bcaching_pool_init( &_mCachingPool, NULL, 0 );
	_mPoolFactory = &_mCachingPool.factory;

		btime_val now;

		BASE_INFO("IoCtx initialization");

		bgettimeofday(&now);
		bsrand(now.sec);

//		if(_mPoolFactory == NULL || _mPoolFactory->create_pool )
		if( _mCachingPool.factory.create_pool == NULL )
		{
			BASE_ERROR("Pool Factory is null");
			return;// false;
		}
		_mPool = bpool_create(&_mCachingPool.factory, NULL, 128, 128, NULL);
//		_mPool = bpool_create(_mPoolFactory, NULL, 128, 128, NULL);
		if (!_mPool)
		{
			BASE_ERROR("Unable to create pool for timers");
			return;// false;
		}
	}
	
	IoCtx::~IoCtx()
	{
		BASE_DEBUG("IoCtx begin to destroy...");
		if(_mTimerHeap != nullptr)
		{
			btimer_heap_destroy(_mTimerHeap);
			_mTimerHeap = nullptr;
			_mTimersRecursiveLock = nullptr;
		}

#if 0
		/* init as DELETE auto with time heap */
		if(_mTimersRecursiveLock != nullptr )
		{
			block_destroy(_mTimersRecursiveLock);
		}
#endif		
		if(_mIoqueue != nullptr )
		{
			bioqueue_destroy(_mIoqueue);
			_mIoqueue = nullptr;
		}

		/* pool must be freed finally, because all resources in this class dependent on this pool */
		if(_mPool != nullptr)
		{
			bpool_release(_mPool);
		}

		/* libutil : no shutdown */
		
		libBaseShutdown();
		BASE_DEBUG("IoCtx destroied");
	}
		
	bool IoCtx::_initTimeCtx(void)
	{
		bstatus_t status;
		
		
		/* Create timer heap */
		status = btimer_heap_create(_mPool, TIMER_TOTAL, &_mTimerHeap);
		if (status != BASE_SUCCESS)
		{
			sdkPerror(status, "Unable to create timers heap");
			return false;
		}
		
		/* Set recursive lock for the timers heap. */
		status = block_create_recursive_mutex(_mPool, "TimerHeapRecursiveLock", &_mTimersRecursiveLock);
		if (status != BASE_SUCCESS)
		{
			sdkPerror(status, "Unable to create recursive lock for timers");
			return false;
		}
		btimer_heap_set_lock(_mTimerHeap, _mTimersRecursiveLock, BASE_TRUE);

		return true;
	}

	int IoCtx::addTimer(const unsigned int delayMs, bool isReload, TimerCallback cb, void *data)
	{
		btime_val delay;
		delay.sec = delayMs/1000;
		delay.msec = delayMs%100;

		return addTimer(delay, isReload, cb, data);
	}

	/* return timerId or negative value */
	int IoCtx::addTimer(const btime_val& delay, bool isReload, TimerCallback cb, void *data )
	{
		std::shared_ptr<IesTimer> timer = std::make_shared<IesTimer>(this, _mTimerId);

		if(timer->init(delay, isReload, cb, data) == false)
		{
			return -1;
		}

		_mTimers[_mTimerId] = timer;
		_mTimerId++;
		
		return (_mTimerId-1);/* current timer ID */
	}

	bool IoCtx::removeTimer(int timerId)
	{
		auto find = _mTimers.find(timerId);

		if(find != _mTimers.end() )
		{
			_mTimerDeleteList[find->first] = find->second;
			_mTimers.erase(find); /* remove() op on iterator */
			return true;
		}

		return false;
	}

	bool IoCtx::cancelTimer(int timerId)
	{
		auto find = _mTimers.find(timerId);

		if(find != _mTimers.end())
		{
			if(btimer_heap_cancel(_mTimerHeap, find->second->getEntry() ) == 0)
			{
				BASE_WARN("No Timer#%d is found in heap", timerId);
			}
			
//			_mTimerDeleteList[find->first] = find->second;
			_mTimers.erase(find); /* remove() op on iterator */
			return true;
		}

		return false;
	}

	int IoCtx::_cancelAllTimer(void)
	{
		int total = 0;
		for(auto t : _mTimers )
		{
			if(btimer_heap_cancel(_mTimerHeap, t.second->getEntry() ) == 0)
			{
				BASE_WARN("No Timer#%d is found in heap", t.first);
			}
			total++;
		}
		BASE_INFO("Total %d timers have been cancelled", total);

		return total;
	}

	int IoCtx::pollTimers(void )
	{
//		struct thread_param *tparam = (struct thread_param*)arg;
//		int idx;
	
//		idx = batomic_inc_and_get(tparam->idx);
//		tparam->stat[idx].is_poll = BASE_TRUE;
		bstatus_t status = bthreadRegister("TimerPoll", _mThDescTimer, &_mThreadTimer);
		if(status != BASE_SUCCESS)
		{
			sdkPerror(status, "Unable to register thread");
//			bthreadDestroy(_mThreadTimer);
			return -1;
		}
	
		BASE_INFO("Thread #%s (poll) started", bthreadGetName(_mThreadTimer) );
		while (!_mTimerStop )
		{
			unsigned count;
			count = btimer_heap_poll(_mTimerHeap, NULL);
			if (count > 0)
			{/* Count expired entries */
				BASE_DEBUG("Thread #%s called %d entries", bthreadGetName(_mThreadTimer), count);
//				tparam->stat[idx].cnt += count;
			}
			else
			{
				bthreadSleepMs(10);
			}

			for(auto f = _mTimerDeleteList.begin(); f != _mTimerDeleteList.end(); f++)
			{
				BASE_DEBUG("erase timer#%d", f->first);
				_mTimerDeleteList.erase(f);
				break;
			}
			
		}

		_cancelAllTimer();
		_mTimers.clear();
		
		BASE_INFO("Thread #%s stopped", bthreadGetName(_mThreadTimer) );
	
		return 0;
	}

	int IoCtx::pollAsyncSockets(void )
	{
#if __WITH_SOCK_POLL_THREAD
		bstatus_t status = bthreadRegister("AsyncSocketPoll", _mThDescSocket, &_mThreadSocket);
		if(status != BASE_SUCCESS)
		{
			sdkPerror(status, "Unable to register thread");
//			bthreadDestroy(_mThreadTimer);
			return -1;
		}
	
		BASE_INFO("Thread #%s started", bthreadGetName(_mThreadSocket) );
#else
		BASE_INFO("Thread #AsyncSocketPoll started" );
#endif
		while (!_mSocketStop )
		{
			unsigned count;

		    btime_val timeout = {0, 10};
			count = bioqueue_poll(_mIoqueue, &timeout);
	    	if ( count < 0)
	    	{
				break;
	    	}
			else if (count > 0)
			{/* Count expired entries */
#if __WITH_SOCK_POLL_THREAD
				BASE_DEBUG("Thread #%s called %d ASocket entries", bthreadGetName(_mThreadSocket), count);
#else
				BASE_DEBUG("Thread #AsyncSocketPoll called %d ASocket entries", count);
#endif
			}
			else
			{
				bthreadSleepMs(10);
			}

			for(auto sock : _mSockets)
			{/* remove async link */
				sock.second->removeAsyncs();
			}

			for(auto s = _mSocketDeleteList.begin(); s != _mSocketDeleteList.end(); ++s)
			{/* only remove one socket item everytime */
				_mSocketDeleteList.erase(s->first);
				break;
			}
		}

#if __WITH_SOCK_POLL_THREAD
		BASE_INFO("Thread #%s stopped", bthreadGetName(_mThreadSocket) );
#else
		BASE_INFO("Thread #AsyncSocketPoll stopped");
#endif
		return 0;
	}


	bpool_factory* IoCtx::getPoolFactory(void)
	{
//		return &_mCachingPool.factory;
		return _mPoolFactory;
	}

	bpool_t* IoCtx::getMemPool(void)
	{
		return _mPool;
	}

	btimer_heap_t * IoCtx::getTimerHeap(void)
	{
		return _mTimerHeap;
	}

	bioqueue_t* IoCtx::getIoQueue(void)
	{
		return _mIoqueue;
	}

	bool IoCtx::run(void)
	{
		bstatus_t status;
#if 1
		status = bthreadCreate(getMemPool(), "TimerPoll", &timerTask, this, 0, 0, &_mThreadTimer);
		if (status != BASE_SUCCESS)
		{
			sdkPerror(status, "Unable to create timer thread");
			return false;
		}
#endif

#if __WITH_SOCK_POLL_THREAD
		status = bthreadCreate(getMemPool(), "AsyncSocketPoll", &asyncSockTask, this, 0, 0, &_mThreadSocket);
		if (status != BASE_SUCCESS)
		{
			sdkPerror(status, "Unable to create Async Socket thread");
			return false;
		}
#else
		pollAsyncSockets();
#endif
		BTRACE();
		bthreadJoin(_mThreadTimer);
		BTRACE();
		bthreadDestroy(_mThreadTimer);
		BTRACE();

#if 0		
		bthreadJoin(_mThreadSocket);
		BTRACE();
		bthreadDestroy(_mThreadSocket);
		BTRACE();
#endif
		return true;
	}

	void IoCtx::stop(void)
	{
		_mTimerStop = true;

		_mSocketStop = true;
	}

	bool IoCtx::init(void)
	{
		if(_initSocketCtx() == false)
		{
			return false;
		}
		
		return _initTimeCtx();
	}

	bool IoCtx::_initSocketCtx(void)
	{
		if (bioqueue_create(_mPool, SOCKET_TOTAL, &_mIoqueue) != BASE_SUCCESS)
		{
			return false;
		}

		return true;
	}

	/* return socket Id in the map */
	int IoCtx::registerSocket(IesSocket *socket)
	{
//		IesSocket *sock = (IesSocket *)socket;
		_mSockets[_mSocketId++] = socket;

		return (_mSocketId-1);
	}
	
	bool IoCtx::unregisterSocket(int socketId)
	{
		auto sock = _mSockets.find(socketId);

		if(sock != _mSockets.end() )
		{
			_mSocketDeleteList[sock->first] = sock->second;
			
			_mSockets.erase(sock->first);
			return true;
		}

		return false;
	}


	}
}

