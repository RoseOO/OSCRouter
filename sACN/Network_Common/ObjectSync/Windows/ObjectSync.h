//ObjectSync.h: The abstract way to synchronise access to an object,
//along with a message queue.
//
// This is the windows version 
// Ports of this version to new OS's should not change the API
// of at least the following functions:
// GetWriteLock
// ReleaseWriteLock
// GetReadLock
// ReleaseReadLock
// AddQMsg
// ReceiveQMsg
//
// This object contains a reader/writer lock, and a threaded queue.  Entries
// from the harvested from the queue will grab a writer lock
//
// It should be noted that this is not intended to be a generic API.
// Startup/shutdown, for example can be very OS specific, since each platform has
// different support.

#if !defined(_OBJECTSYNC_H_)
#define _OBJECTSYNC_H_

#ifndef _WINDOWS_
#error "#include error: ObjectSync.h needs windows.h"
#endif
#ifndef _DEQUE_
#error "#include error: ObjectSync.h needs <deque>"
#endif
#ifndef _READER_WRITER_H_
#error "#include error: ObjectSync.h needs ReaderWriter.h"
#endif

//Test for debugging timing on windows
#define OBJECT_SYNC_TIMING 0

class CObjectSync
{
public:
	CObjectSync();
	virtual ~CObjectSync();

	//Startup with all the necessary params for internal thread tweaking
	//The use of the lock is optional.  Uselock controls this.
	//The thread harvests entire queues at a time.  lock_per_message controls whether 
	//the lock is grabbed for each message, or for the entire queue processing.
	//threadpriority: since the windows version has a queue harvesting thread,
	//				  the priority of the thread is needed (in the process context of
	//				  the creator/application
	//threadlockwaitms: the amount of time to wait on the write lock in the thread before
	//					giving up (note the message isn't popped until then)
	//					a timeout < 0 is infinite
	bool ObjectStartup(bool uselock, bool lock_per_message, 
		                int threadpriority = THREAD_PRIORITY_NORMAL, int threadlockwaitms = -1);
	
	//An optional call to stop the message processing before full destruction on shutdown
	//Useful if you want to stop messaging but still keep the objectsync lock to keep other
	//threads out while shutting down (and then calling ObjectShutdown)
	void ObjectStopReceiving();

	//Shutdown the object and clean up
	void ObjectShutdown();

	//Safely get the queue size
	uint ObjectQSize();

	//Windows only -- get the thread handle
	HANDLE ObjectGetThreadHandle() {return m_thread;}
	unsigned ObjectGetThreadID() {return m_thread_id;}

protected:
	//Attempts to grab a write lock.  Returns false if no lock or timeout
	//A timeout < 0 is infinite.
	bool GetWriteLock(int timeoutms = -1);

	//Releases a write lock
	void ReleaseWriteLock();

	//Attempts to grab a read lock.  Returns false if no lock or too many readers
	bool GetReadLock();

	//Releases a read lock
	void ReleaseReadLock();

   //Safely adds a message to the message queue.  The memory is NOT copied
	bool AddQMsg(void *pmsg);

	//Like AddQMsg, but does it for each item in the vector iterator range.
	//The message pointers are not removed from the vector.
	bool AddQMsgs(std::vector<void*>::const_iterator begin, std::vector<void*>::const_iterator end);

	//The child class must override this to receive messages.
	//When this is called, the child has a write lock
	//After this call returns, the write lock is released.
	//The memory is in the control of the child class.
	//This should process the message relatively quickly so messages don't bunch up
	virtual void ReceiveQMsg(void *pmsg) = 0;

	//The child class must override this to clean up unreceived messages.
	//This is called on all unharvested messages when ObjectShutdown is called
	//The override should simply to the appropriate delete and return;
	virtual void DeleteQMsg(void *pmsg) = 0;

protected:
	int m_threadpriority;
	int m_threadlockwaitms;	 

	CXReadWriteLock* m_plock;   //The access lock -- only allocated if used -- the lock functions all return true
	//The windows version has a queue and a harvesting thread, along with an auto-reset
	//event to signal the thread to read.
	CRITICAL_SECTION m_qcs;
	std::deque<void*> m_msgs;
	bool m_lock_per_message;
	HANDLE m_thread;
	unsigned m_thread_id;
	HANDLE m_readevent;
	bool m_shutdown;
	friend unsigned __stdcall HarvestThread(void* pObjectSync);
};

/////////////////////////////////////////////////////
// Some quick inlines

#if !(OBJECT_SYNC_TIMING)

inline bool CObjectSync::GetWriteLock(int timeoutms)
{
	//If the lock isn't used, we always succeed
	return !m_plock || m_plock->WriteLock(timeoutms);
}

inline void CObjectSync::ReleaseWriteLock()
{
	if(m_plock)
        m_plock->WriteUnlock();
}


inline bool CObjectSync::GetReadLock()
{
	//If the lock isn't used, we always succeed
	return !m_plock || m_plock->ReadLock();
}

inline void CObjectSync::ReleaseReadLock()
{
	if(m_plock)
		m_plock->ReadUnlock();
}
#endif

#endif  //_OBJECT_SYNC_H_
