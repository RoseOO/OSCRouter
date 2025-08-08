/******************************************************************************
//
// MODULE NAME:  ReaderWriter.cpp
//
// DESCRIPTION:  This module defines reader/writer locks for use in OSX
// This is relatively trivial for OSX, which has posix rw locks

******************************************************************************/

#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include "deftypes.h"
#include "ReaderWriter.h"
#include "tock.h"

CXReadWriteLock::CXReadWriteLock() 
{
	pthread_rwlock_init(&m_rwlock, NULL);
}

CXReadWriteLock::~CXReadWriteLock()
{
	pthread_rwlock_destroy(&m_rwlock);
}

bool CXReadWriteLock::ReadLock(int millitimeout)
{
	bool result = false;

	if(millitimeout < 0)
		result = (0 == pthread_rwlock_rdlock(&m_rwlock));
	else if(millitimeout == 0)
		result = (0 == pthread_rwlock_tryrdlock(&m_rwlock));
	else
	{
		//Unfortunately, OSX doesn't support pthread_rwlock_timedrdlock, so I need to fake it with a ttimer and sleep -- not perfect since we don't have a constant grab on the lock..
		ttimer timer(millitimeout);
		while (!timer.Expired())
		{
			if(0 == pthread_rwlock_tryrdlock(&m_rwlock))
				return true;
			usleep(1000);  //Sleep for 1ms
		}
	}

	return result;
}

void CXReadWriteLock::ReadUnlock()
{
	pthread_rwlock_unlock(&m_rwlock);
}

bool CXReadWriteLock::WriteLock(int millitimeout)
{
	bool result = false;
	
	if(millitimeout < 0)
		result = (0 == pthread_rwlock_wrlock(&m_rwlock));
	else if(millitimeout == 0)
		result = (0 == pthread_rwlock_trywrlock(&m_rwlock));
	else
	{
		//Unfortunately, OSX doesn't support pthread_rwlock_timedwrlock, so I need to fake it with a ttimer and sleep
		ttimer timer(millitimeout);
		while (!timer.Expired())
		{
			if(0 == pthread_rwlock_trywrlock(&m_rwlock))
				return true;
			usleep(1000);  //Sleep for 1ms
		}
	}

	return result;
}

void CXReadWriteLock::WriteUnlock()
{
	pthread_rwlock_unlock(&m_rwlock);
}


