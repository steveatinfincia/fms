#ifndef _cancelablethread_
#define _cancelablethread_

#include "cancelablerunnable.h"
#include "../global.h"

#include <Poco/Thread.h>
#include <Poco/Mutex.h>
#ifndef POCO_VERSION	// Used to be in Foundation.h, as of 1.4 it's in Version.h, but not included by default
#include <Poco/Version.h>
#endif

/*
	Poco::Thread does not have a virtual destructor, so we can't inherit safely.
*/
class CancelableThread
{
public:
	CancelableThread():m_runnable(0),m_thread(new Poco::Thread),m_wasjoined(false)								{}
	CancelableThread(const std::string &name):m_runnable(0),m_thread(new Poco::Thread(name)),m_wasjoined(false)	{}
	CancelableThread(CancelableRunnable *runnable):m_runnable(runnable),m_thread(new Poco::Thread),m_wasjoined(false)									{ if(m_thread && m_runnable) { Guard guard(m_threadmutex); m_thread->start(*runnable); } }
	CancelableThread(CancelableRunnable *runnable, const std::string &name):m_runnable(runnable),m_thread(new Poco::Thread(name)),m_wasjoined(false)	{ if(m_thread && m_runnable) { Guard guard(m_threadmutex); m_thread->start(*runnable); } }
	~CancelableThread()
	{
		Guard guard(m_threadmutex);
		if(m_thread)
		{
			if(m_thread->isRunning() && IsCancelled()==false)
			{
				Cancel();
			}
			if(m_wasjoined==false)
			{
				try
				{
					m_thread->tryJoin(5000);
				}
				catch(...)
				{
				}
			}
			delete m_thread;
		}
		if(m_runnable)
		{
			delete m_runnable;
		}
	}

	// CancelableThread takes ownership of runnable and will destroy it in the destructor
	void Start(CancelableRunnable *runnable)	{ Guard guard(m_threadmutex); m_runnable=runnable; m_thread->start(*runnable); }

	void Cancel()				{ Guard guard(m_threadmutex); if(m_runnable) { m_runnable->Cancel(); } }
	const bool IsCancelled()	{ Guard guard(m_threadmutex); return m_runnable ? m_runnable->IsCancelled() : false; }

	// these methods implemented from Poco::Thread
	void join()					{ Guard guard(m_threadmutex); if(m_thread) { m_thread->join(); m_wasjoined=true; } }
	bool isRunning()			{ Guard guard(m_threadmutex); if(m_thread) { return m_thread->isRunning(); } else { return false; } }

	const int GetThreadID()
	{
		Guard guard(m_threadmutex);
		if(m_thread)
		{
#if defined(_WIN32) && defined(POCO_VERSION) && POCO_VERSION>=0x01030600 && !defined(__APPLE__) && !defined(__DARWIN__)
			return static_cast<int>(m_thread->tid());
#else
			return static_cast<int>(m_thread->id());
#endif
		}
		return 0;
	}

private:

	Poco::Thread *m_thread;
	Poco::FastMutex m_threadmutex;
	CancelableRunnable *m_runnable;
	bool m_wasjoined;				// We must call join on ALL Poco threads, running or not, before deletion.  Otherwise there is a potential TLS leak in POSIX Thread implementation.

};

#endif	// _cancelablethread_
