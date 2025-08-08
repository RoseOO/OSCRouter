//ObjectSync.h: The abstract way to synchronise access to an object,
//along with a message queue.
//
// This is the OSX version 
// Ports of this version to new OS's should not change the API
// of at least the following functions:
// GetWriteLock
// ReleaseWriteLock
// GetReadLock
// ReleaseReadLock
// AddQMsg
// ReceiveQMsg
//
// This object contains a reader/writer lock, and a thread that waits to harvest
// a queue.  Entries from the harvested from the queue will grab a writer lock.
//
// It should be noted that this is not intended to be a generic API.
// Startup/shutdown, for example can be very OS specific, since each platform has
// different support.

#if !defined(_OBJECTSYNC_H_)
#define _OBJECTSYNC_H_

#ifndef _READER_WRITER_H_
#error "#include error: ObjectSync.h needs ReaderWriter.h"
#endif

class CObjectSync
{
public:
	CObjectSync();
	virtual ~CObjectSync();

	//Startup with all the necessary params for internal thread tweaking
	//The use of the lock is optional.  uselock controls this
   //The thread harvests entire queues at a time.  lock_per_message controls whether
   //the lock is grabbed for each message, or for the entire queue processing.
	//qlen:			The maximum number of queue messages (0 is infinite)
	//threadlockwaitms: the amount of time to wait on the write lock in the thread before
	//					giving up (note the message isn't popped until then)
	//					a timeout < 0 is infinite

	bool ObjectStartup( bool uselock, bool lock_per_message,
						unsigned int qlen = 500,
						int threadlockwaitms = -1);
	
	//An optional call to stop the message processing before full destruction on shutdown
	//Useful if you want to stop messaging but still keep the objectsync lock to keep other
	//threads out while shutting down (and then calling ObjectShutdown)
	void ObjectStopReceiving();

	//Shutdown the object and clean up
	void ObjectShutdown();

   //Safely get the queue size
   uint ObjectQSize();

protected:
	//Attempts to grab a write lock.  Returns false if no lock or timeout
	//A timeout < 0 is infinite.
	bool GetWriteLock(int timeoutms = -1);

	//Releases a write lock
	void ReleaseWriteLock();

	//Attempts to grab a read lock.  Returns false if no lock or timeout
	//A timeout < 0 is infinite.
	bool GetReadLock(int timeoutms = -1);

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

private:
	CXReadWriteLock* m_plock;   //The access lock, only used if !NULL

	//This version has a queue and a harvesting thread.  There's a mutex
	//around the queue, and a signal that the thread should read the queue
	pthread_mutex_t m_qlock;
	pthread_cond_t m_qsignal;
	std::deque<void*> m_msgs;
	bool m_lock_per_message;
	pthread_t m_thread;
	bool m_initted;  //if the q and thread has been initted.
	unsigned int m_maxsize;  //So we can limit the queue memory, 0 is infinite
	int m_threadlockwaitms;  //<0 is infinite
	
	bool m_shutdown;
	friend void* HarvestThread(void* pObjectSync);
};

/////////////////////////////////////////////////////
// Some quick inlines

inline bool CObjectSync::GetWriteLock(int timeoutms)
{
	//if no lock always succeedes
	return !m_plock || m_plock->WriteLock(timeoutms);
}

inline void CObjectSync::ReleaseWriteLock()
{
	if(m_plock)
		m_plock->WriteUnlock();
}


inline bool CObjectSync::GetReadLock(int timeoutms)
{
	return !m_plock || m_plock->ReadLock(timeoutms);
}

inline void CObjectSync::ReleaseReadLock()
{
	if(m_plock)
		m_plock->ReadUnlock();
}


#endif  //_OBJECT_SYNC_H_


