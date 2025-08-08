//ObjectSync.cpp  Implementation of the windows version of object sync

#include <windows.h>
#include <process.h>
#include <deque>  
#include <vector>
#include <map>

#include "deftypes.h"
#include "ReaderWriter.h"
#include "ObjectSync.h"

#if OBJECT_SYNC_TIMING  //Test code for timing object sync
#include "..\..\Tock\Tock.h"
FILE* object_pfile;
void Time_EnterCriticalSection(LPCRITICAL_SECTION psec)
{
	tock t1 = Tock_GetTock();
	EnterCriticalSection(psec);
	tock t2 = Tock_GetTock();

	if(t2 - t1 > 10)
	{
		fprintf(object_pfile, "%u: Objectsync (thread %d): EnterCriticalSection took %d ms\n", t2, GetCurrentThreadId(), t2 - t1);
		fflush(object_pfile);
	}
}

void Time_LeaveCriticalSection(LPCRITICAL_SECTION psec)
{
	tock t1 = Tock_GetTock();
	LeaveCriticalSection(psec);
	tock t2 = Tock_GetTock();

	if(t2 - t1 > 10)
	{
		fprintf(object_pfile, "%u: Objectsync (thread %d): LeaveCriticalSection took %d ms\n", t2, GetCurrentThreadId(), t2 - t1);
		fflush(object_pfile);
	}
}
#endif

unsigned __stdcall HarvestThread(void* pObjectSync)
{
	CObjectSync *p = static_cast<CObjectSync*>(pObjectSync);

	//We'll process the queue in units of a hopefully reasonable size
	int MSG_SIZE = 200;
	void** msgs = new void* [MSG_SIZE];
	int msgs_end = 0;


	do
	{
		//We'll grab all the messages currently in the critical section, and process them in the
		//context of one WriteLock

		msgs_end = 0;   //If m_msgs was empty we don't process old messages

		EnterCriticalSection(&(p->m_qcs));

		if(!p->m_msgs.empty())
		{
			std::deque<void*>::iterator it;
			for(it = p->m_msgs.begin(); 
				(it != p->m_msgs.end()) && (msgs_end < MSG_SIZE); 
				++it, ++msgs_end)
			{
				msgs[msgs_end] = *it;
			}
			p->m_msgs.erase(p->m_msgs.begin(), it);
		}

		LeaveCriticalSection(&(p->m_qcs));

		if(msgs_end > 0)
		{
			if(!p->m_lock_per_message)
			{
				if(p->GetWriteLock(p->m_threadlockwaitms))
				{
					for(int i = 0; i < msgs_end; ++i)
					{
						if(!p->m_shutdown)
							p->ReceiveQMsg(msgs[i]);
						else
							p->DeleteQMsg(msgs[i]); 
					}
					p->ReleaseWriteLock();			
				}
				else   //couldn't get lock at all, clean up 
				{
					for(int i = 0; i < msgs_end; ++i)
						p->DeleteQMsg(msgs[i]); 
				}
			}
			else
			{
				for(int i = 0; i < msgs_end; ++i)
				{
					if(p->m_shutdown)
						p->DeleteQMsg(msgs[i]);
					else if(p->GetWriteLock(p->m_threadlockwaitms))
					{
						p->ReceiveQMsg(msgs[i]);
						p->ReleaseWriteLock();
					}
					else
						p->DeleteQMsg(msgs[i]);
				}
			}

			//Now that we've processed the messages, see if we need to bump the internal queue size
			if(msgs_end >= MSG_SIZE)
			{
				MSG_SIZE *= 2;
				delete [] msgs;
				msgs = new void* [MSG_SIZE];
			}
		}
		else  //No message, wait for the next signal -- We'll get signalled on shutdown as well.
		{
			WaitForSingleObject(p->m_readevent, INFINITE);
		}
	}
	while(!p->m_shutdown);

	delete [] msgs;

	return 0;
}

CObjectSync::CObjectSync()
{
	m_threadpriority = THREAD_PRIORITY_NORMAL;
	m_threadlockwaitms = -1;
	m_thread = NULL;
	m_readevent = NULL;
	m_shutdown = true;
	m_plock = NULL;
}

CObjectSync::~CObjectSync()
{

}

bool CObjectSync::ObjectStartup(bool uselock, bool lock_per_message, int threadpriority, int threadlockwaitms)
{
	if(!m_shutdown)
		return true;  //Only actually startup once

#if OBJECT_SYNC_TIMING  //Test code for objectsync timing
	object_pfile = fopen("objectsync.txt", "w");
#endif

	if(uselock)
	{
		m_plock = new CXReadWriteLock;
		if(!m_plock)
			return false;
	}
	
	m_shutdown = false;
	m_threadpriority = threadpriority;
	m_threadlockwaitms = threadlockwaitms;
	m_lock_per_message = lock_per_message;
	InitializeCriticalSection(&m_qcs);
	m_readevent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if(!m_readevent)
	{
		ObjectShutdown();
		return false;
	}
	m_thread = (HANDLE)_beginthreadex( NULL, 0, &HarvestThread, this, CREATE_SUSPENDED , &m_thread_id );
	if(!m_thread)
	{
		ObjectShutdown();
		return false;
	}
	SetThreadPriority(m_thread, m_threadpriority);
	ResumeThread(m_thread);
	return true;
}


//An optional call to stop the message processing before full destruction on shutdown
//Useful if you want to stop messaging but still keep the objectsync lock to keep other
//threads out while shutting down (and then calling ObjectShutdown)
void CObjectSync::ObjectStopReceiving()
{
	if(m_shutdown)
		return;  //Only actually shutdown once.

	m_shutdown = true;
	if(m_thread) //Time to shut down the thread
	{
		if(m_readevent)
			SetEvent(m_readevent);  //Signal it to wake up

		if(WaitForSingleObject(m_thread, 5000) != WAIT_OBJECT_0)
			SuspendThread(m_thread);
		CloseHandle(m_thread);
		m_thread = NULL;
		
		if(m_readevent)
		{
            CloseHandle(m_readevent);
			m_readevent = NULL;
		}
	}
	DeleteCriticalSection(&m_qcs);

#if OBJECT_SYNC_TIMING //Test code for objectsync timing
	fclose(object_pfile);
#endif
}

//Shutdown the object and clean up
void CObjectSync::ObjectShutdown()
{
	//Stop receiving
	ObjectStopReceiving();

	//Destroy the lock
	if(m_plock)
	{
		delete m_plock;
		m_plock = NULL;
	}

	//Finally, destroy any remaing message queue elements
	while(!m_msgs.empty())
	{
		DeleteQMsg(m_msgs.front());
		m_msgs.pop_front();
	}
}

//Safely adds a message to the message queue.  The memory is NOT copied
bool CObjectSync::AddQMsg(void *pmsg)
{
	if(m_shutdown)
		return false;

	EnterCriticalSection(&m_qcs);
	m_msgs.push_back(pmsg);
	LeaveCriticalSection(&m_qcs);

#if OBJECT_SYNC_TIMING //Test code for objectsync timen
	tock t1 = Tock_GetTock();
#endif

	SetEvent(m_readevent);  //Signal the thread to wake up

#if OBJECT_SYNC_TIMING
	tock t2 = Tock_GetTock();
	if(t2-t1 > 10)
	{
		fprintf(object_pfile, "AddQMsg: SetEvent took %d ms\n", t2 - t1);
		fflush(object_pfile);
	}
#endif

	return true;
}

//Like AddQMsg, but does it for each item in the vector iterator range.
//The message pointers are not removed from the vector.
bool CObjectSync::AddQMsgs(std::vector<void*>::const_iterator begin, std::vector<void*>::const_iterator end)
{
	if(m_shutdown)
		return false;
	
	EnterCriticalSection(&m_qcs);
	for(std::vector<void*>::const_iterator it = begin; it != end; ++it)
		m_msgs.push_back(*it);
	LeaveCriticalSection(&m_qcs);

#if OBJECT_SYNC_TIMING
	tock t1 = Tock_GetTock();
#endif

	SetEvent(m_readevent);  //Signal the thread to wake up

#if OBJECT_SYNC_TIMING
	tock t2 = Tock_GetTock();
	if(t2-t1 > 10)
	{
		fprintf(object_pfile, "AddQMsgs: SetEvent took %d ms\n", t2 - t1);
		fflush(object_pfile);
	}
#endif

	return true;
}

//Safely get the queue size
uint CObjectSync::ObjectQSize()
{
	if(m_shutdown)
		return 0;

	uint retval;
	EnterCriticalSection(&m_qcs);
	retval = m_msgs.size();
	LeaveCriticalSection(&m_qcs);
	return retval;
}

#if OBJECT_SYNC_TIMING
bool CObjectSync::GetWriteLock(int timeoutms)
{
	if(!m_plock)
		return true;
	else
	{
		tock t1 = Tock_GetTock();
		bool result = m_plock->WriteLock(timeoutms);
		tock t2 = Tock_GetTock();

		if(t2 - t1 > 10)
		{
			fprintf(object_pfile, "%u: Writelock (thread %d) took %d ms\n", t2, GetCurrentThreadId(), t2 - t1);
			fflush(object_pfile);
		}

		return result;
	}
	//If the lock isn't used, we always succeed
	//return !m_plock || m_plock->WriteLock(timeoutms);
}

void CObjectSync::ReleaseWriteLock()
{
	if(m_plock)
	{
		tock t1 = Tock_GetTock();
		m_plock->WriteUnlock();
		tock t2 = Tock_GetTock();

		if(t2 - t1 > 10)
		{
			fprintf(object_pfile, "%u: WriteUnlock (thread %d)took %d ms\n", t2, GetCurrentThreadId(), t2 - t1);
			fflush(object_pfile);
		}
	}
}


bool CObjectSync::GetReadLock()
{
	if(!m_plock)
		return true;
	else
	{
		tock t1 = Tock_GetTock();
		bool result = m_plock->ReadLock();
		tock t2 = Tock_GetTock();

		if(t2 - t1 > 10)
		{
			fprintf(object_pfile, "%u: ReadLock (thread %d) took %d ms\n", t2, GetCurrentThreadId(), t2 - t1);
			fflush(object_pfile);
		}

		return result;
	}

	//If the lock isn't used, we always succeed
	//return !m_plock || m_plock->ReadLock();
}

void CObjectSync::ReleaseReadLock()
{
	if(m_plock)
	{
		tock t1 = Tock_GetTock();
		m_plock->ReadUnlock();
		tock t2 = Tock_GetTock();

		if(t2 - t1 > 10)
		{
			fprintf(object_pfile, "%u: ReadUnlock (thread %d)took %d ms\n", t2, GetCurrentThreadId(), t2 - t1);
			fflush(object_pfile);
		}
	}
}

#endif
