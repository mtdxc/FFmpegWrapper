#include <Windows.h>
/// 临界区
class Mutex
{	
	// Constructor
public:
	/// 构造函数
	Mutex()
	{ ::InitializeCriticalSection(&m_sect); }

	// Attributes
public:
	/// 转型函数
	operator CRITICAL_SECTION*()
	{ return (CRITICAL_SECTION*) &m_sect; }
	CRITICAL_SECTION m_sect;	///< 临界区

	// Operations
public:
	/// 解锁
	BOOL Unlock()
	{ ::LeaveCriticalSection(&m_sect); return TRUE; }
	/// 加锁
	BOOL Lock()
	{ ::EnterCriticalSection(&m_sect); return TRUE; }
	/// 加锁
	BOOL Lock(DWORD dwTimeout)
	{ return Lock(); }

	// Implementation
	/// 析构函数
	virtual ~Mutex()
	{ ::DeleteCriticalSection(&m_sect); }
};
/// 自动加解锁
class AutoLock
{
public:
	/// 构造函数 自动加锁
	AutoLock(Mutex& s):sec(&s)
	{
		sec && sec->Lock();	
	}
	AutoLock(Mutex* s):sec(s)
	{
		sec && sec->Lock();	
	}
	/// 析构函数 自动解锁
	~AutoLock()
	{
		sec && sec->Unlock();
	}
protected:
	Mutex* sec;	///< 锁引用
};