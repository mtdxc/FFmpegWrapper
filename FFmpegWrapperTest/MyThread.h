#pragma once

class CMyThread;
/// 线程处理函数(写定参数防止用错)
typedef DWORD (*ThreadFunc)(CMyThread* pThread);

/// 线程封装类
class CMyThread
{
public:
	CMyThread(void);
	~CMyThread(void);

	/*
	开启线程
	@param func 线程处理函数 
	@param arg 参数指针
	@return BOOL 成功创建则返回TRUE
	*/
	BOOL Start(ThreadFunc func, LPVOID arg);
	/*
	关闭线程，并等待退出
	@param timeOut 等待超时，如在超时时间内还未推出则强制退出线程
	*/
	BOOL Close(int timeOut=INFINITE);
	/// 每次循环都等delay时间
	BOOL WaitTimeOut(int delay);
	/*
	等待某个事件的产生
	@param hEvent 事件
	@param timeOut 等待时间
	@return int 
	@retval 0 线程退出
	@retval 1 事件可用
	@retval -1 等待超时
	*/
	int WaitEvent(HANDLE hEvent, int timeOut=INFINITE);
	/// 按照delay进行间隔休眠
	BOOL Delay(int delay);
	/// 返回参数
	LPVOID GetArg(){return m_pArg;}
	/*
	判断线程是否活着
	@param timeOut 如果为0，则判断线程是否存在，
	否则采用应用层的判断，即根据m_dwLastTime和timeOut值来判断线程是否还活着，或是堵在某个地方
	@return BOOL 活着则返回TRUE，否则FALSE
	*/
	BOOL IsAlive(int timeOut=0);
protected:
	// 保存由start传入的arg
	LPVOID m_pArg;
	/// 用于dealy和WaitTimeOut处理
	DWORD m_dwLastTime;
	/// 线程ID
	DWORD m_dwThreadId;
	/// 线程句柄
	HANDLE m_hThread;
	/// 退出事件(用于Delay、WaitTimeOut和Close中判断退出)
	HANDLE m_hStopEvent;
};

