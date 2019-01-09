#ifndef _IM_LOCK_H
#define _IM_LOCK_H

#ifdef WIN32
#include<Windows.h>
#else
#include <pthread.h>
#endif


#ifdef WIN32
#define rwlock_t                  SRWLOCK
#define init_rwlock(v)            InitializeSRWLock(&(v))
#define rlock_rwlock(v)           AcquireSRWLockShared(&(v))
#define unrlock_rwlock(v)         ReleaseSRWLockShared(&(v))
#define wlock_rwlock(v)           AcquireSRWLockExclusive(&(v))
#define unwlock_rwlock(v)         ReleaseSRWLockExclusive(&(v))
#define destroy_rwlock(v)  
#else
#define rwlock_t				pthread_rwlock_t
#define init_rwlock(v)          pthread_rwlock_init(&(v), NULL)
#define rlock_rwlock(v)         pthread_rwlock_rdlock(&(v))
#define unrlock_rwlock(v)       pthread_rwlock_unlock(&(v))
#define wlock_rwlock(v)         pthread_rwlock_wrlock(&(v))
#define unwlock_rwlock(v)       pthread_rwlock_unlock(&(v))
#define destroy_rwlock(v)       pthread_rwlock_destroy(&(v))
#endif


#ifdef WIN32
#define mutex_t                 CRITICAL_SECTION
#define init_mutex(v)           InitializeCriticalSection(&(v))
#define lock_mutex(v)           EnterCriticalSection(&(v))
#define unlock_mutex(v)         LeaveCriticalSection(&(v))
#define try_lock_mutex(v)       TryEnterCriticalSection(&(v))
#define destroy_mutex(v)        DeleteCriticalSection(&(v))
#else
#define mutex_t                 pthread_mutex_t
#define init_mutex(v)           pthread_mutex_init(&(v),NULL)
#define lock_mutex(v)           pthread_mutex_lock(&(v))
#define unlock_mutex(v)         pthread_mutex_unlock(&(v))
#define try_lock_mutex(v)       pthread_mutex_trylock(&(v))
#define destroy_mutex(v)        pthread_mutex_destroy(&(v))
#endif

#ifdef WIN32
//适用于vista以及server 2008及以上系统
#define cond_t                   CONDITION_VARIABLE
#define init_cond(c)             InitializeConditionVariable(&(c))
#define wait_cond(c,m)           SleepConditionVariableCS(&(c), &(m), 2*1000)
#define signal_cond(c)           WakeConditionVariable(&(c))
#define broadcast_cond(c)        WakeAllConditionVariable(&(c))
#define destroy_cond(c) 
#else
#define cond_t				    pthread_cond_t
#define init_cond(c)            pthread_cond_init(&(c), NULL)
#define wait_cond(c,m)          pthread_cond_wait(&(c),&(m))
#define signal_cond(c)          pthread_cond_signal(&(c))
#define broadcast_cond(c)       pthread_cond_broadcast(&(c))
#define destroy_cond(c)         pthread_cond_destroy(&(c))
#endif

class CLock
{
public:
    CLock();
    virtual ~CLock();
    void lock();
    void unlock();
    virtual void  try_lock();
private:
	mutex_t m_lock;
};

class CAutoLock
{
public:
	CAutoLock(CLock* pLock);
	virtual ~CAutoLock();
private:
	CLock* m_pLock;
};

class CRWLock
{
public:
    CRWLock();
    virtual ~CRWLock();
    void rlock();
    void wlock();
	void unrlock();
	void unwlock();
private:
	rwlock_t m_lock;
};

class CAutoRWLock
{
public:
    CAutoRWLock(CRWLock* pLock, char mode = 'r');
    virtual ~CAutoRWLock();
private:
    CRWLock* m_pLock;
	char m_mode;
};


#endif
