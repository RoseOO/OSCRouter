/******************************************************************************
//
// MODULE NAME:  MemPool.cpp
//
// DESCRIPTION:  This module defines a memory pool/freelist for fixed size blocks in windows
// 
// It should be pretty easy to port this to other platforms, just use the best
// mutual exclusion mechanism for the OS (binary semaphore, whatever),
// and a mechanism for interlocked increment/decrement
//
******************************************************************************/

#include <windows.h>
#include "deftypes.h"
#include "mempool.h"

/*
	This is a simple freelist for a fixed size block.  When a block is in the
	freelist, the data can be anything, so we overwrite the first n bytes with
	the next pointer.
*/

#define LOCK_PTR (CRITICAL_SECTION*)m_lock

CMemPool::CMemPool(uint blocksize, int max) : m_BLOCKSIZE(blocksize), m_COUNTMAX(max)
{
	m_lock = NULL;
	m_curcount = 0;
	m_head = NULL;
}

CMemPool::~CMemPool()
{
	ReleaseFreelist();
	if(LOCK_PTR)
	{
		DeleteCriticalSection(LOCK_PTR);
		delete LOCK_PTR;
	}
}

//Because we don't know what the platform specific lock is, this function is called
//internally to set up the lock
bool CMemPool::AssureLock()
{
	if(m_lock)
		return true;

	m_lock = new CRITICAL_SECTION;
	if(LOCK_PTR)
	{
		InitializeCriticalSection(LOCK_PTR);
		return true;
	}
	return false;
}


//Reserves a number of free blocks at once
void CMemPool::Reserve(uint count)
{
	for(uint i = 0; i < count; ++i)
		Free(Alloc(true));
}

//Returns a pointer to the block, or NULL if you hit max or a memory error
void* CMemPool::Alloc(bool force)
{
	void* result = NULL;
	
	if(!LOCK_PTR)  //Save a function call if we've got one 
		AssureLock();
	if(!LOCK_PTR)
		return NULL;

	EnterCriticalSection(LOCK_PTR);

	if((NULL == m_head) || force)
	{
		if((-1 == m_COUNTMAX) || (m_curcount < m_COUNTMAX))
		{
			++m_curcount;
			result = new uint1 [m_BLOCKSIZE];
		}
	}
	else
	{
		result = m_head;
		m_head = *((uint1**)m_head);
	}

	LeaveCriticalSection(LOCK_PTR);

	return result;
}

//"Free"s a pointer back to the pool
void CMemPool::Free(void* pblock)
{
	if(!LOCK_PTR)  //Save a function call if we've got one
		AssureLock();

	if(LOCK_PTR && pblock)
	{
		//Note, we never decrement m_curcount on a free, because we only check when we are doing a real alloc
		EnterCriticalSection(LOCK_PTR);

		*((uint1**)pblock) = m_head;  //Overwrite the first n bytes with the next pointer
		m_head = (uint1*)pblock;

		LeaveCriticalSection(LOCK_PTR);
	}
}

//Releases the current freelist back to the memory heap
void CMemPool::ReleaseFreelist()
{
	if(!LOCK_PTR)   //Save a function call if we've got one
		AssureLock();
	
	if(LOCK_PTR)
	{
		EnterCriticalSection(LOCK_PTR);

		m_curcount = 0;
		
		while(m_head)
		{
			uint1* tmp = m_head;
			m_head = *((uint1**)m_head);
			delete [] tmp;
		}

		LeaveCriticalSection(LOCK_PTR);
	}
}

