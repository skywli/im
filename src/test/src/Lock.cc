#include "Lock.h"
CLock::CLock()
{
	init_mutex(m_lock);
}

CLock::~CLock()
{
	destroy_mutex(m_lock);
}

void CLock::lock()
{
	lock_mutex(m_lock);
}

void CLock::unlock()
{
	unlock_mutex(m_lock);
}

void CLock::try_lock()
{
	try_lock_mutex(m_lock);
}

CRWLock::CRWLock()
{
	init_rwlock(m_lock);
}

CRWLock::~CRWLock()
{
	destroy_rwlock(m_lock);
}

void CRWLock::rlock()
{
	rlock_rwlock(m_lock);
}

void CRWLock::wlock()
{
	wlock_rwlock(m_lock);
}

void CRWLock::unrlock()
{
	unrlock_rwlock(m_lock);
}
void CRWLock::unwlock()
{
	unwlock_rwlock(m_lock);
}

CAutoRWLock::CAutoRWLock(CRWLock * pLock, char mode)
{
	m_mode = mode;
	m_pLock = pLock;
	if (NULL != m_pLock)
	{
		if (m_mode=='r')
		{
			m_pLock->rlock();
		}
		else
		{
			m_pLock->wlock();
		}
	}
}

CAutoRWLock::~CAutoRWLock()
{
    if(NULL != m_pLock)
    {
		if (m_mode == 'r')
		{
			m_pLock->unrlock();
		}
		else
		{
			m_pLock->unwlock();
		}
    }
}

CAutoLock::CAutoLock(CLock* pLock)
{
    m_pLock = pLock;
    if(NULL != m_pLock)
        m_pLock->lock();
}

CAutoLock::~CAutoLock()
{
    if(NULL != m_pLock)
        m_pLock->unlock();
}

