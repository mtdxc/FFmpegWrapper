// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include "libavcodec/avcodec.h"
#ifdef FFLOCK_PTHTREAD
static int ff_lockmgr(void **mutex, enum AVLockOp op)
{
	pthread_mutex_t** pmutex = (pthread_mutex_t**) mutex;
	switch (op) {
	case AV_LOCK_CREATE:
		*pmutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(*pmutex, NULL);
		break;
	case AV_LOCK_OBTAIN:
		pthread_mutex_lock(*pmutex);
		break;
	case AV_LOCK_RELEASE:
		pthread_mutex_unlock(*pmutex);
		break;
	case AV_LOCK_DESTROY:
		pthread_mutex_destroy(*pmutex);
		free(*pmutex);
		break;
	}
	return 0;
}
#else
static int ff_lockmgr(void **mutex, enum AVLockOp op)
{
	CRITICAL_SECTION **critSec = (CRITICAL_SECTION **)mutex;
	switch (op) {
	case AV_LOCK_CREATE:
		*critSec = new CRITICAL_SECTION();
		InitializeCriticalSection(*critSec);
		break;
	case AV_LOCK_OBTAIN:
		EnterCriticalSection(*critSec);
		break;
	case AV_LOCK_RELEASE:
		LeaveCriticalSection(*critSec);
		break;
	case AV_LOCK_DESTROY:
		DeleteCriticalSection(*critSec);
		delete *critSec;
		break;
	}
	return 0;
}
#endif

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		// register all codecs and demux
		av_register_all();
		av_lockmgr_register(&ff_lockmgr);
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		av_lockmgr_register(NULL);
		break;
	}
	return TRUE;
}

