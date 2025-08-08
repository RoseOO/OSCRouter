/******************************************************************************
//
// MODULE NAME:  ReaderWriter.cpp
//
// DESCRIPTION:  This module defines reader/writer locks for use in windows
// 
// It should be pretty easy to port this to other platforms, just use the best
// mutual exclusion mechanism for the OS (binary semaphore, whatever),
// and a mechanism for interlocked increment/decrement
//
******************************************************************************/

#include <windows.h>
#include "deftypes.h"
#include "..\..\Tock\Tock.h"  //This implementation requires the tock library
#include "readerwriter.h" 

/*
	The concept of a reader writer lock is fairly straighforward.  Any number of 
	readers may enter and read, but a writer will block other writers and readers
	coming in and wait for existing readers to leave.

	This implementation uses one blocking mechanism, and a count for the number
	of readers.  The semaphore will keep extra readers and writers out, and WriteLock
	will check the read count to make sure the existing readers have gone away.
*/

CXReadWriteLock::CXReadWriteLock() :
	m_COUNTMAX(20000), m_readcount(0)
{
	InitializeCriticalSection(&m_cs);
}

CXReadWriteLock::~CXReadWriteLock()
{
	DeleteCriticalSection(&m_cs);
}

bool CXReadWriteLock::ReadLock()
{
	bool res = true;
	EnterCriticalSection(&m_cs);
	
	if(m_readcount >= m_COUNTMAX)  //We have way too many locks
		res = false;
	else
		InterlockedIncrement(&m_readcount);

	LeaveCriticalSection(&m_cs);  //We don't want to block other readers out

	return res;
}

void CXReadWriteLock::ReadUnlock()
{
	//Decrement number of readers
	InterlockedDecrement(&m_readcount);

	//Do a quick underflow check, Attempting to safely remove negative numbers
	while(m_readcount < 0)
		InterlockedIncrement(&m_readcount);
}

bool CXReadWriteLock::WriteLock(int millitimeout)
{
	int timeout = millitimeout;
	if(timeout < 0)
		timeout = INFINITE;

	bool res = true;
	EnterCriticalSection(&m_cs);
	
	if(res)
	{
		ttimer tout;
		
		if(timeout != INFINITE)
			tout.SetInterval(timeout);
		
		//see if there are no readers -- keeping the lock so no new readers can get in
		while((m_readcount > 0) &&
			  ((INFINITE == millitimeout) || (!tout.Expired())))
		{
			Sleep(1); 
		}

		if(m_readcount > 0)  //We timed out -- otherwise, just keep the lock
		{
			LeaveCriticalSection(&m_cs);
			res = false;
		}
	}
	return res;
}

void CXReadWriteLock::WriteUnlock()
{
	//We still have the lock at this point
	LeaveCriticalSection(&m_cs);
}


