//OSXStreamServer.cpp
//
#include <vector>
#include <map>

#include <pthread.h>

#include "deftypes.h"
#include "CID.h"
#include "tock.h"
#include "ipaddr.h"
#include "AsyncSocketInterface.h"
#include "../StreamACNSrvInterface.h"
#include "../Src/StreamServer.h"
#include "OSX_StreamACNSrvInterface.h"
#include "OSXStreamServer.h"

IOSXStreamACNSrv* IOSXStreamACNSrv::CreateInstance()
{
	return new COSXStreamServer;
}

//The overall destructor.  Call Shutdown before this function
void IOSXStreamACNSrv::DestroyInstance(IOSXStreamACNSrv* p)
{
	delete static_cast<COSXStreamServer*>(p);
}

COSXStreamServer::COSXStreamServer()
{
	m_lockinitted = false;
}

COSXStreamServer::~COSXStreamServer()
{

}

//Call this to init the library after creation. Can be used right away (if it returns true)
bool COSXStreamServer::Startup(IAsyncSocketServ* psocket)
{
	if(InternalStartup(psocket))
	{
		m_lockinitted = (0 == pthread_mutex_init(&m_lock, NULL));
		return m_lockinitted;
	}

	return false;
}

//Call this to de-init the library before destruction
void COSXStreamServer::Shutdown()
{
	GetLock();
	InternalShutdown();
	ReleaseLock();

	if(m_lockinitted)
		pthread_mutex_destroy(&m_lock);
}

bool COSXStreamServer::GetLock()
{
	return (m_lockinitted && (0 == pthread_mutex_lock(&m_lock)));
}

void COSXStreamServer::ReleaseLock()
{
	if(m_lockinitted)
		pthread_mutex_unlock(&m_lock);
}


/****************************************
* IStreamACNSrv overrides
****************************************/
int COSXStreamServer::Tick(uint* dirtyhandles, uint hcount)
{
	int result = 0;
	if(GetLock())
	{
		result = CStreamServer::Tick(dirtyhandles, hcount);
		ReleaseLock();
	}
	return result;
}

bool COSXStreamServer::CreateUniverse(const CID& source_cid, netintid* netiflist, int netiflist_size, 
				      const char* source_name, uint1 systemid, uint2 reserved, uint1 options, uint1 start_code, 
													uint2 universe, uint2 slot_count, uint1*& pslots, uint& handle,
													bool ignore_inactivity_logic, uint send_intervalms)
{
	bool result = false;
	if(GetLock())
	{
		result = CStreamServer::CreateUniverse(source_cid, netiflist, netiflist_size, source_name, systemid, reserved, options, 
															start_code, universe, slot_count, pslots, handle, 
															ignore_inactivity_logic, send_intervalms);
		ReleaseLock();
	}
	return result;
}

void COSXStreamServer::SetUniversesDirty(uint* handles, uint hcount)
{
	if(GetLock())
	{
		CStreamServer::SetUniversesDirty(handles, hcount);
		ReleaseLock();
	}
}

void COSXStreamServer::DestroyUniverse(uint handle)
{
	if(GetLock())
	{
		CStreamServer::DestroyUniverse(handle);
		ReleaseLock();
	}
}

void COSXStreamServer::DEBUG_DESTROY_PRIORITY_UNIVERSE(uint handle)
{
	if(GetLock())
	{
		CStreamServer::DEBUG_DESTROY_PRIORITY_UNIVERSE(handle);
		ReleaseLock();
	}
}

void COSXStreamServer::SendUniversesNow(uint* handles, uint hcount)
{
	if(GetLock())
	{
		CStreamServer::SendUniversesNow(handles, hcount);
		ReleaseLock();
	}
}

void COSXStreamServer::DEBUG_DROP_PACKET(uint handle, uint1 decrement)
{
	if(GetLock())
	{
		CStreamServer::DEBUG_DROP_PACKET(handle, decrement);
		ReleaseLock();
	}
}

void COSXStreamServer::OptionsPreviewData(uint handle, bool preview)
{
	if(GetLock())
	{
		CStreamServer::OptionsPreviewData(handle, preview);
		 ReleaseLock();
	}
}

void COSXStreamServer::OptionsStreamTerminated(uint handle, bool terminated)
{
	if(GetLock())
	{
		CStreamServer::OptionsStreamTerminated(handle, terminated);
		ReleaseLock();
	}
}
