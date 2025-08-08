#include <vector>
#include <map>
#include <string.h>

#include "deftypes.h"
#include "defpack.h"
#include "CID.h"
#include "tock.h"
#include "ipaddr.h"
#include "AsyncSocketInterface.h"
#include "../StreamACNSrvInterface.h"
#include "../Src/StreamServer.h"
#include "../../Common/streamcommon.h"

CStreamServer::CStreamServer()
{
	m_psocklib = NULL;
}

CStreamServer::~CStreamServer()
{
}

//Call this to init the library after creation. 
//Can be used right away (if it returns true)
bool CStreamServer::InternalStartup(IAsyncSocketServ* psocket)
{
	m_psocklib = psocket;
	//Create the socket per interface -- Even though we create one on each
	//interface, the CreateUniverse can limit the interfaces used
	int n = psocket->GetNumInterfaces();
	IAsyncSocketServ::netintinfo *pinfo = new IAsyncSocketServ::netintinfo [n];
	if(pinfo)
	{
		psocket->CopyInterfaceList(pinfo);
		for(int i = 0; i < n; ++i)
		{
			sockid newsock = SOCKID_INVALID;
			IPPort newport = 0;  //We're not listening to these messages, so a 
									  //random port is fine
			if(psocket->CreateUnicastSocket(this, pinfo[i].id, newsock, newport, 1500))
				m_sockets.insert(std::pair<netintid, sockid>(pinfo[i].id, newsock));
		}

		delete [] pinfo;
	}
	return true;
}

//Call this to de-init the library before destruction
void CStreamServer::InternalShutdown()
{
	//Clean up the multiverse
	for(verseiter it = m_multiverse.begin(); it != m_multiverse.end(); ++it)
	{
		if(it->psend)
			delete [] it->psend;
	}

	//Clean up the sockets
	for(sockiter it2 = m_sockets.begin(); it2 != m_sockets.end(); ++it2)
		m_psocklib->DestroySocket(it2->second);
  
	//Clean up the sequence numbers
	for(seqiter it3 = m_seqmap.begin(); it3 != m_seqmap.end(); ++it3)
		if(it3->second.second)
			delete it3->second.second;
}


//Returns a pointer to the storage location for the universe, adding if 
//need be.
//The newly-added location contains sequence number 0.
uint1* CStreamServer::GetPSeq(uint2 universe)
{
	seqiter it = m_seqmap.find(universe);
	if(it != m_seqmap.end())
	{
		++it->second.first;
		return it->second.second;
	}
	uint1 * p = new uint1;
	if(!p)
		return NULL;
	*p = 0;
	m_seqmap.insert(std::pair<uint2, seqref>(universe, seqref(1, p)));
	return p;
}

//Removes a reference to the storage location for the universe, removing 
//completely if need be.
void CStreamServer::RemovePSeq(uint2 universe)
{
	seqiter it = m_seqmap.find(universe);
	if(it != m_seqmap.end())
	{
		--it->second.first;
		if(it->second.first <= 0)
		{
			delete it->second.second;
			m_seqmap.erase(it);
		}
	}
}

  
/***************************************************************************
 * IStreamACNSrv overrides -- It is assumed that the platform wrapper locks 
 * around each call
 **************************************************************************/
  
//Since both Tick and SendUniversesNow do similar things, this does the 
//real sequencing & sending
void CStreamServer::SeqSendUniverse(universe* puni)
{
	if(puni->psend)
	{
		uint1* pseq = puni->pseq;

		if(pseq)
		{
			SetStreamHeaderSequence(puni->psend, *pseq);
			++*pseq;
		}
		else
			SetStreamHeaderSequence(puni->psend, 0);
      
		for(senditer sit = puni->wheretosend.begin(); sit != puni->wheretosend.end(); ++sit)
			m_psocklib->SendPacket(*sit, puni->sendaddr, puni->psend, puni->sendsize);
	}		
}

//In the event that you want to send out a message for particular 
//universes (and start codes) in between ticks, call this function.  
//This does not affect the dirty bit for the universe, inactivity count, 
//etc, and the tick will still operate normally when called.
void CStreamServer::SendUniversesNow(uint* handles, uint hcount)
{
	if(handles && hcount)
	{
		for(uint i = 0; i < hcount; ++i)
			SeqSendUniverse(&m_multiverse[handles[i]]);
	}
}


//Must be called at your DMX rate -- usually 22 or 23 ms
//Yes, MUST be called at your DMX rate.  This function processes the inactivity timers,
// and calling this function at a slower rate may cause an inactivity timer to be triggered at an
// time point that is past the universe transmission timeout, causing your sinks to consider
// you offline.  The absolute minumum rate that this function can be called is 10hz (every 100ms).
// Sends out any Dirty universes, universes that have hit their send_interval,
// and depending on how you created each universe performs the DMX inactivity logic.
//This function returns the current number of valid universes in the system.  
//This function also handles sending the extra packets with the terminated flag set, and destroys
// the universe for you, so call this function for at least a few more cycles after you
// call DestroyUniverse (or until tick returns 0 if you know you aren't creating any more universes)
//Depending on your algorithm, you may want to call SetUniversesDirty separately as needed, 
// or you may pass in additional universes here to set them dirty just before the tick.  
// Doing so will save you a lock access.  If you don't want to set any additional universes
// dirty, pass in NULL, 0
int CStreamServer::Tick(uint* dirtyhandles, uint hcount)
{
	int valid_count = 0;

	//Check if we should mark any universes dirty
	if(dirtyhandles && hcount)
		CStreamServer::SetUniversesDirty(dirtyhandles, hcount);  
  
	for(verseiter it = m_multiverse.begin(); it != m_multiverse.end(); ++it)
	{
		if(it->psend)
			++valid_count;

		//If this has been send 3 times (or more?) with a termination flag
		//then it's time to kill it
		if(it->num_terminates >= 3)
		{
			DoDestruction(it->handle);
		}
		//If valid, either a dirty, inactivity count < 3 (if we're using that 
		//logic), or send_interval will cause a send, but only after the first time
		//the universe was marked dirty.
		if(it->psend && (it->isdirty ||
				 (it->waited_for_dirty && 
				   ((!it->ignore_inactivity && it->inactive_count < 3) || 
				    it->send_interval.Expired()))))
		{
			//Before the send, properly reset state
			if(it->isdirty)
				it->inactive_count = 0;  //To recover from inactivity
			else if(it->inactive_count < 3)  //We don't want the Expired case to reset the inactivity count
				++it->inactive_count;
	  
			//Add the sequence number and send
			SeqSendUniverse(&(*it));
			if(GetStreamTerminated(it->psend))
			{
				it->num_terminates++;
			}

			//Finally, set the timing/dirtiness for the next interval
			it->isdirty = false;
			it->send_interval.Reset();
		}
	}

	return valid_count;
}

//Use this to create a universe for a source cid, startcode, etc.
//if netiflist is NULL (or the size is 0), this universe will be created for any valid interface.
//If netiflist is non-NULL the function will fail if any selected network is unavailable.
//If it returns true, two parameters are filled in: The data buffer for the values that can
//  be manipulated directly, and the handle to use when calling the rest of these functions.
//Note that a universe number of 0 is invalid.
//Set reserved to be 0 and options to either be 0 or PREVIEW_DATA_OPTION (can be changed later
//  by OptionsPreviewData).
//if ignore_inactivity_logic is set to false (the default for DMX), Tick will handle sending the
//  3 identical packets at the lower frequency that sACN requires.  It sends these packets at
//  send_intervalms intervals (again defaulted for DMX).  Note that even if you are not using the
//  inactivity logic, send_intervalms expiry will trigger a resend of the current universe packet.
//Data on this universe will not be initially sent until marked dirty.
//Also: If you want to change any of these parameters, you can call CreateUniverse again with the
//      same start_code and universe.  It will destroy and reallocate pslots, however.
bool CStreamServer::CreateUniverse(const CID& source_cid, netintid* netiflist,
				   int netiflist_size, 
				   const char* source_name, uint1 priority, 
				   uint2 reserved, uint1 options, 
				   uint1 start_code, uint2 universe, 
				   uint2 slot_count, uint1*& pslots, 
				   uint& handle, bool ignore_inactivity_logic,
				   uint send_intervalms)
{
	if(universe == 0)
		return false;
  
	//Before we attempt to create the universe, make sure 
	//we can create the buffer.
	uint sendsize = STREAM_HEADER_SIZE + slot_count;
	uint1* pbuf = NULL;
	pbuf = new uint1 [sendsize];

	if(!pbuf)
		return false;
	memset(pbuf, 0, sendsize);

	//Find an empty spot for the universe 
	bool found = false;
	for(uint i = 0; i < m_multiverse.size(); ++i)
	{
		if(m_multiverse[i].number == universe && 
			m_multiverse[i].start_code == start_code)
		{
			found = true;
			handle = i;
			//get rid of the old one first
			DoDestruction(i);
			break;
		}
		if(!found && m_multiverse[i].psend == NULL)
		{
			found = true;
			handle = i;
			//			break;
		}
	}
	if(!found)
	{
		struct universe tmp;
		handle = m_multiverse.size();
		m_multiverse.push_back(tmp);
	}
  
	//Init/reinit the state
	m_multiverse[handle].number = universe;
	m_multiverse[handle].handle = handle;
	m_multiverse[handle].start_code = start_code;
	m_multiverse[handle].isdirty = false;
	m_multiverse[handle].waited_for_dirty = false;
	m_multiverse[handle].num_terminates=0;
	m_multiverse[handle].ignore_inactivity = ignore_inactivity_logic;
	m_multiverse[handle].inactive_count = 0;
	m_multiverse[handle].send_interval.SetInterval(send_intervalms);
	m_multiverse[handle].pseq = GetPSeq(universe);

	GetUniverseAddress(universe, m_multiverse[handle].sendaddr);
	if(!netiflist || netiflist_size == 0)
	{
		for(sockiter it = m_sockets.begin(); it != m_sockets.end(); ++it)
			m_multiverse[handle].wheretosend.push_back(it->second);
	}
	else
	{
		for(int i = 0; i < netiflist_size; ++i)
		{
			sockiter it = m_sockets.find(netiflist[i]);
			if(it != m_sockets.end())
				m_multiverse[handle].wheretosend.push_back(it->second);
			//The user specified a list, and one of the members wasn't there.
			else
			{
				delete [] pbuf;
				m_multiverse[handle].psend = NULL;  //Just in case
				return false;
			}
		}
	}

	InitStreamHeader(pbuf, source_cid, source_name, priority, reserved, options, 
	                 start_code, universe, slot_count);
	m_multiverse[handle].psend = pbuf;
	m_multiverse[handle].sendsize = sendsize;
	pslots = pbuf + STREAM_HEADER_SIZE;
	return true;
}


//After you add data to the data buffer, call this to trigger the data send
//on the next Tick boundary.
//Otherwise, the data won't be sent until the inactivity or send_interval 
//time.
//Due to the fact that access to the universes needs to be thread safe,
//this function allows you to set an array of universes dirty at once 
//(to incur the lock overhead only once).  If your algorithm is to 
//SetUniversesDirty and then immediately call Tick, you will save a lock 
//access by passing these in directly to Tick.
void CStreamServer::SetUniversesDirty(uint* handles, uint hcount)
{
	if(handles && hcount)
	{
		for(uint i = 0; i < hcount; ++i)
		{
			m_multiverse[handles[i]].isdirty = true;
			m_multiverse[handles[i]].waited_for_dirty = true;
		}
	}
}
//Use this to destroy a priority universe but keep the dmx universe alive
void CStreamServer::DEBUG_DESTROY_PRIORITY_UNIVERSE(uint handle)
{
	DoDestruction(handle);
}

//Use this to destroy a universe.  While this is thread safe internal to 
//the library, this does invalidate the pslots array that CreateUniverse 
//returned, so do not access that memory after or during this call.
void CStreamServer::DestroyUniverse(uint handle)
{
	if(m_multiverse[handle].psend)
		SetStreamTerminated(m_multiverse[handle].psend, true);
  //       DoDestruction(handle);
}

//Perform the logical destruction and cleanup of a universe and its related 
//objects.
void CStreamServer::DoDestruction(uint handle)
{
	if(m_multiverse[handle].psend)
	{
		m_multiverse[handle].num_terminates=0;
		delete [] m_multiverse[handle].psend;
		m_multiverse[handle].psend = NULL;
		m_multiverse[handle].wheretosend.clear();

		RemovePSeq(m_multiverse[handle].number);
		m_multiverse[handle].pseq = NULL;
	}
}

/*DEBUG USAGE ONLY --causes packets to be "dropped" on a particular universe*/
void CStreamServer::DEBUG_DROP_PACKET(uint handle, uint1 decrement)
{
	*(m_multiverse[handle].pseq) = *(m_multiverse[handle].pseq) - decrement;  
  //-= causes size problem
}

/**************************************************************************
 * IAsyncSocketClient functions that do nothing, as we don't actually read 
 * anything
 **************************************************************************/

//This is the notification that a socket has gone bad/closed.
//After this call, the socket is considered disconnected, but not
//invalid... Call DestroySocket to remove it.
void CStreamServer::SocketBad(sockid /*id*/)
{
  //We'll just ignore the problem
}

//This function caches the packet and sends to CSDT functionality later
void CStreamServer::ReceivePacket(sockid /*id*/, const CIPAddr& /*from*/, 
				  uint1* pbuffer, uint buflen)
{
	//We shouldn't be receiving anything... Just ignore and clean up
	m_psocklib->DeletePacket(pbuffer, buflen);
}

//sets the preview_data bit of the options field
void CStreamServer::OptionsPreviewData(uint handle, bool preview)
{
	if(m_multiverse[handle].psend)
		SetPreviewData(m_multiverse[handle].psend, preview);
}

//sets the stream_terminated bit of the options field
void CStreamServer::OptionsStreamTerminated(uint handle, bool terminated)
{
	if(m_multiverse[handle].psend)
	SetStreamTerminated(m_multiverse[handle].psend, terminated);
}
