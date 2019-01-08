#ifndef _GROUP_QUEUE_H_
#define _GROUP_QUEUE_H_
#include <Concurrentqueue.h>
#include <queue>
#include <Lock.h>

#define LOCK_DEQUE 1
template <typename T>
class IMQueue
{

public:
	IMQueue()
	{
	}
	~IMQueue()
	{
	}
	bool push(T msg)
	{
#if LOCK_DEQUE
		CAutoLock lock(&m_lock);
		//        m_lock.lock();
		m_queue.push_back(msg);
		//  m_lock.unlock();
		return true;
#else
		while (!m_queue.enqueue(msg));
		return true;
#endif
	}
	void release()
	{
	}

	bool empty() {
#if LOCK_DEQUE
		return m_queue.empty();
#else
		return 0 == m_queue.size_approx();
#endif
	}
	int getsize()
	{
#if LOCK_DEQUE
		return m_queue.size();
#else
		return m_queue.size_approx();
#endif
	}
	bool pop(T  &msg)
	{
#if LOCK_DEQUE
		bool bRet = false;
		CAutoLock lock(&m_lock);
		//m_lock.lock();
		typename std::deque<T>::iterator it = m_queue.begin();
		if (it != m_queue.end())
		{
			msg = *it;
			bRet = true;
			m_queue.pop_front();
		}
		//    m_lock.unlock();
		return bRet;
#else
		return m_queue.try_dequeue(msg);
#endif
	}
private:
#if LOCK_DEQUE
	std::deque<T> m_queue;
	//Atomic  m_lock;
	CLock  m_lock;
#else
	moodycamel::ConcurrentQueue<T>m_queue;
#endif
};
#endif
