//ObjectSync.cpp  Implementation of the Linux version of object sync
#include <pthread.h>
#include <deque>  
#include <vector>

#include "deftypes.h"
#include "ReaderWriter.h"
#include "ObjectSync.h"

void* HarvestThread(void* pObjectSync)
{
	CObjectSync *p = static_cast<CObjectSync*>(pObjectSync);

   std::deque<void*> msgs;

	do
	{
		//We'll grab all the messages currently in the critical section, and process them in the context
		//of one WriteLock

		msgs.clear();  //If m_msgs was empty we don't want to process old messages.

		pthread_mutex_lock(&p->m_qlock);
		if(!p->m_msgs.empty())
		{
			msgs.assign(p->m_msgs.begin(), p->m_msgs.end());
			p->m_msgs.clear();
		}
		else if(!p->m_shutdown) //Wait for a signal that we should wake up and process on the next loop (or terminate)
			pthread_cond_wait(&p->m_qsignal, &p->m_qlock);
		pthread_mutex_unlock(&p->m_qlock);

		if(!msgs.empty())
		{
			if(p->m_lock_per_message)
			{
				for(std::deque<void*>::iterator it = msgs.begin(); it != msgs.end(); ++it)
				{
					if(p->m_shutdown)
						p->DeleteQMsg(*it);
					else if(p->GetWriteLock(p->m_threadlockwaitms))
					{
						p->ReceiveQMsg(*it);
						p->ReleaseWriteLock();
					}
					else
						p->DeleteQMsg(*it);
				}
			}
			else
			{
				if(p->GetWriteLock(p->m_threadlockwaitms))
				{
					for(std::deque<void*>::iterator it = msgs.begin(); it != msgs.end(); ++it)
					{
						if(!p->m_shutdown)
							p->ReceiveQMsg(*it);
						else
							p->DeleteQMsg(*it);
					}
					p->ReleaseWriteLock();			
				}
				else   //couldn't get lock at all, clean up 
				{
					for(std::deque<void*>::iterator it = msgs.begin(); it != msgs.end(); ++it)
						p->DeleteQMsg(*it);
				}
			}
		}
	}
	while(!p->m_shutdown);
	
	return 0;
}


CObjectSync::CObjectSync()
{
	m_initted = false;
	m_maxsize = 0;  
	m_threadlockwaitms = -1;
	m_shutdown = true;
	m_plock = NULL;
}

CObjectSync::~CObjectSync()
{

}

bool CObjectSync::ObjectStartup(bool uselock, bool lock_per_message, unsigned int qlen, int threadlockwaitms)
{
	if(m_initted)
		return true;  //Only actually startup once

	if(uselock)
	{
		m_plock = new CXReadWriteLock;
		if(!m_plock)
			return false;
	}

	m_shutdown = false;
	m_maxsize = qlen;
	m_threadlockwaitms = threadlockwaitms;
	m_lock_per_message = lock_per_message;
	
	if(0 != pthread_mutex_init(&m_qlock, NULL))
		return false;
	
	if(0 != pthread_cond_init(&m_qsignal, NULL))
	{
		pthread_mutex_destroy(&m_qlock);
		return false;
	}

	if(0 == pthread_create(&m_thread, NULL, HarvestThread, this))
		m_initted = true;
	else
	{
		pthread_mutex_destroy(&m_qlock);
		pthread_cond_destroy(&m_qsignal);
		return false;
	}
	
	return m_initted;
}


//	An optional call to stop the message processing before full destruction on shutdown
//	Useful if you want to stop messaging but still keep the objectsync lock to keep other
//	threads out while shutting down (and then calling ObjectShutdown)
void CObjectSync::ObjectStopReceiving()
{
	m_shutdown = true;
		
	if(m_initted) //Time to shut down the thread and other elements
	{
		pthread_mutex_lock(&m_qlock);
		pthread_cond_signal(&m_qsignal);
		pthread_mutex_unlock(&m_qlock);
		pthread_join(m_thread, NULL);  //Wait for the thread to die
		
		pthread_cond_destroy(&m_qsignal);
		pthread_mutex_destroy(&m_qlock);
		
		m_initted = false;
	}
}

//	Shutdown the object and clean up
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

//	Safely adds a message to the message queue.  The memory is NOT copied
bool CObjectSync::AddQMsg(void *pmsg)
{
	if(m_shutdown)
		return false;
	
	bool result = true;
	
	pthread_mutex_lock(&m_qlock);
	if((m_maxsize != 0) && (m_msgs.size() >= m_maxsize))
		result = false;
	else
	{
		m_msgs.push_back(pmsg);
		pthread_cond_signal(&m_qsignal);
	}
	pthread_mutex_unlock(&m_qlock);
	
	return result;
}

//	Safely adds a message to the message queue.  The memory is NOT copied
bool CObjectSync::AddQMsgs(std::vector<void*>::const_iterator begin, std::vector<void*>::const_iterator end)
{
	if(m_shutdown)
		return false;
	
	bool result = true;
	bool send_event = false;
	
	pthread_mutex_lock(&m_qlock);
	for(std::vector<void*>::const_iterator it = begin; it != end; ++it)
	{
		if((m_maxsize != 0) && (m_msgs.size() >= m_maxsize))
		{
			result = false;
			break;
		}
		else
		{
			m_msgs.push_back(*it);
			send_event = true;
		}
	}

	if(send_event)
		pthread_cond_signal(&m_qsignal);

	pthread_mutex_unlock(&m_qlock);
	
	return result;
}

//Safely get the queue size
uint CObjectSync::ObjectQSize()
{
	if(m_shutdown)
		return 0;
	
	uint result = 0;
	
	pthread_mutex_lock(&m_qlock);
	result = m_msgs.size();
	pthread_mutex_unlock(&m_qlock);
	
	return result;
}

