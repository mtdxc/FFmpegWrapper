#include "stdafx.h"
#include "MyThread.h"

void LogOut(LPCTSTR fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	vprintf(fmt, vl);
	va_end(vl);
}

CMyThread::CMyThread(void)
{
	m_pArg = NULL;
	m_dwThreadId = 0;
	m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_hThread=NULL;
}


CMyThread::~CMyThread(void)
{
	if(m_hThread)
		Close();
	if(m_hStopEvent)
		CloseHandle(m_hStopEvent);
}

BOOL CMyThread::Start( ThreadFunc func, LPVOID arg )
{
	if(m_hThread!=NULL)
	{
		LogOut("thread has start, please call Stop first");
		return FALSE;
	}
	m_dwLastTime = 0;
	m_pArg = arg;
	ResetEvent(m_hStopEvent);
	m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, this, 0, &m_dwThreadId);
	return m_hThread!=NULL;
}

BOOL CMyThread::Close( int timeOut )
{
	if(!m_hThread){
		LogOut("Thread no start");
		return FALSE;
	}
	SetEvent(m_hStopEvent);
	if(WAIT_TIMEOUT==WaitForSingleObject(m_hThread, timeOut)){
		LogOut("wait timeout, force term thread");
		TerminateThread(m_hThread, 0);
	}
	CloseHandle(m_hThread);
	m_hThread = NULL;
	m_dwThreadId = 0;
	return TRUE;
}

BOOL CMyThread::WaitTimeOut( int delay )
{
	BOOL bRet = WaitForSingleObject(m_hStopEvent, delay) == WAIT_TIMEOUT;
	if(bRet)
		m_dwLastTime = GetTickCount();
	return bRet;
}

BOOL CMyThread::Delay( int delay )
{
	// 上次休眠时间+延迟时间-当前时间
	int delta = m_dwLastTime+delay-GetTickCount();
	if(delta<=0) delta = 0;
	BOOL bRet = WaitForSingleObject(m_hStopEvent, delta) == WAIT_TIMEOUT;
	m_dwLastTime = GetTickCount();
	return bRet;
}

BOOL CMyThread::IsAlive( int timeOut/*=0*/ )
{
	if(!m_hThread)
		return FALSE;
	if(timeOut)
	{// 采用应用层的判断
		return GetTickCount() - m_dwLastTime <= timeOut;
	}else
		return WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT;
}

int CMyThread::WaitEvent( HANDLE hEvent, int timeOut/*=INFINITE*/ )
{
	HANDLE events[2];
	events[0] = m_hStopEvent;
	events[1] = hEvent;
	switch(WaitForMultipleObjects(2, events, FALSE, timeOut))
	{
	case WAIT_TIMEOUT:
		return -1;
	case WAIT_OBJECT_0:
		return 0;
	default:
		return 1;
	}
}
