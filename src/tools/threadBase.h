
#ifndef _THREAD_BASE_H_
#define _THREAD_BASE_H_

#ifdef WIN32
#include <Windows.h>
#else
#include <pthread.h>
#endif

#ifdef WIN32
#define  thread_t  DWORD  
#define create_thread(id,attr,handler,arg)         CreateThread(NULL, 0, handler, arg, 0, &id)
#define exit_thread(v)                              ExitThread((v))  
#define thread_id()                             GetCurrentThreadId()
#else
#define  thread_t  pthread_t  
#define create_thread(id,attr,handler,arg)         pthread_create(&id, attr, handler, arg)
#define exit_thread(v)                             pthread_exit(&(v))
#define thread_id()                            pthread_self()
#endif


class ThreadBase
{
public:
	ThreadBase()
	{
		m_thread_id = 0;
	}

	virtual ~ThreadBase()
	{
/*#ifdef WIN32
		WaitForSingleObject(m_thread_id, INFINITE);
#else
		pthread_join(m_thread_id,NULL);
#endif*/
	}

#ifdef WIN32
	static DWORD WINAPI
#else
	static void*
#endif
	thread(void* arg)
	{
		ThreadBase* pThread = (ThreadBase*)arg;
		if (pThread)
			pThread->run();
		return NULL;
	}
	virtual void stop_thread() {
#ifdef WIN32
		DWORD code=0;
#else
		int code;
#endif
		exit_thread(code);
	}
	virtual void start_thread(int threads = 1)
	{
		for (int i = 0; i < threads; i++)
		{
			create_thread(m_thread_id, NULL, ThreadBase::thread, this);
		}
	}
	virtual void run(void) = 0;
protected:
	thread_t m_thread_id;
};

#endif