#include <vector>
#include <map>
#include <windows.h>

#include "deftypes.h"
#include "cid.h"
#include "tock.h"
#include "ipaddr.h"
#include "AsyncSocketInterface.h"
#include "..\StreamACNSrvInterface.h"
#include "..\src\streamserver.h"
#include "Win_StreamACNSrvInterface.h"
#include "winstreamserver.h"

IWinStreamACNSrv* IWinStreamACNSrv::CreateInstance()
{
	return new CWinStreamServer;
}

//The overall destructor.  Call Shutdown before this function
void IWinStreamACNSrv::DestroyInstance(IWinStreamACNSrv* p)
{
	delete static_cast<CWinStreamServer*>(p);
}

CWinStreamServer::CWinStreamServer()
{
}

CWinStreamServer::~CWinStreamServer()
{
}

//Call this to init the library after creation. Can be used right away (if it returns true)
bool CWinStreamServer::Startup(IAsyncSocketServ* psocket)
{
	if(InternalStartup(psocket))
	{
		InitializeCriticalSection(&m_lock);
		return true;
	}
	return false;
}

//Call this to de-init the library before destruction
void CWinStreamServer::Shutdown()
{
	//We'll attempt to be graceful, but we'll eventually just clean up
	if(GetLock())
	{
		InternalShutdown();
		ReleaseLock();
	}
	else
		InternalShutdown();

	DeleteCriticalSection(&m_lock);
}

bool CWinStreamServer::GetLock()
{
	EnterCriticalSection(&m_lock);
	return true;
}

void CWinStreamServer::ReleaseLock()
{
	LeaveCriticalSection(&m_lock);
}


/****************************************
* IStreamACNSrv overrides
****************************************/
int CWinStreamServer::Tick(uint* dirtyhandles, uint hcount)
{
	int result = 0;
	if(GetLock())
	{
		result = CStreamServer::Tick(dirtyhandles, hcount);
		ReleaseLock();
	}
	return result;
}

bool CWinStreamServer::CreateUniverse(const CID& source_cid, netintid* netiflist, int netiflist_size, 
				      const char* source_name, uint1 priority, uint2 reserved, uint1 options, uint1 start_code, 
													uint2 universe, uint2 slot_count, uint1*& pslots, uint& handle,
													bool ignore_inactivity_logic, uint send_intervalms)
{
	bool result = false;
	if(GetLock())
	{
		result = CStreamServer::CreateUniverse(source_cid, netiflist, netiflist_size, source_name, priority, reserved, options, 
															start_code, universe, slot_count, pslots, handle, 
															ignore_inactivity_logic, send_intervalms);
		ReleaseLock();
	}
	return result;
}

void CWinStreamServer::SetUniversesDirty(uint* handles, uint hcount)
{
	if(GetLock())
	{
		CStreamServer::SetUniversesDirty(handles, hcount);
		ReleaseLock();
	}
}

void CWinStreamServer::DestroyUniverse(uint handle)
{
	if(GetLock())
	{
		CStreamServer::DestroyUniverse(handle);
		ReleaseLock();
	}
}

void CWinStreamServer::DEBUG_DESTROY_PRIORITY_UNIVERSE(uint handle)
{
	if(GetLock())
	{
		CStreamServer::DEBUG_DESTROY_PRIORITY_UNIVERSE(handle);
		ReleaseLock();
	}
}

void CWinStreamServer::SendUniversesNow(uint* handles, uint hcount)
{
	if(GetLock())
	{
		CStreamServer::SendUniversesNow(handles, hcount);
		ReleaseLock();
	}
}

void CWinStreamServer::DEBUG_DROP_PACKET(uint handle, uint1 decrement)
{
	if(GetLock())
	{
		CStreamServer::DEBUG_DROP_PACKET(handle, decrement);
		ReleaseLock();
	}
}

void CWinStreamServer::OptionsPreviewData(uint handle, bool preview)
{
	if(GetLock())
	{
		CStreamServer::OptionsPreviewData(handle, preview);
		 ReleaseLock();
	}
}

void CWinStreamServer::OptionsStreamTerminated(uint handle, bool terminated)
{
	if(GetLock())
	{
		CStreamServer::OptionsStreamTerminated(handle, terminated);
		ReleaseLock();
	}
}
