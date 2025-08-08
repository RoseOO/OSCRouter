/* StreamClient.cpp  The implementation of the CStreamClient class, which 
 * handles acting as a streaming ACN client that tracks source expiry and 
 *filters out of sequence packets.
 */

#include <map>
#include <vector>

#include "deftypes.h"
#include "defpack.h"
#include "tock.h"
#include "CID.h"
#include "ipaddr.h"
#include "AsyncSocketInterface.h"
#include "../StreamACNCliInterface.h"
#include "../../Common/streamcommon.h"
#include "StreamClient.h"

CStreamClient::CStreamClient()
{
  m_pnotify = NULL;
  m_psock = NULL;
  m_numifaces = 0;
  m_pifaces = NULL;
  m_listening = ALL;
  m_hold_last_look_time = DEFAULT_HOLD_LAST_LOOK_TIME;
}

CStreamClient::~CStreamClient()
{
  if(m_psock != NULL)
    InternalShutdown();
}

//Helper routine that creates a new socket on the network interface, or if
//the full mcast address is !null, creates a standalone mcast socket on that.
//Returns SOCKID_INVALID on error, otherwise the newly created socket
sockid CStreamClient::CreateSocket(netintid iface, CIPAddr* maddr)
{
  //Assumes the socket list is already locked by SubscribeUniverse
  sockid newsock = SOCKID_INVALID;
  //Always listen on the ACN multicast port
  IPPort newport = STREAM_IP_PORT;  
  bool create_res = false;
  if(maddr)
    create_res = m_psock->CreateStandaloneMulticastSocket(this, *maddr, newsock, 1500, true);
  else
    create_res = m_psock->CreateMulticastSocket(this, iface, newsock, newport, 1500, true);

  if(create_res)
  {
      //Start the receive thread
      if(SpawnSocketThread(newsock))	
			m_sockets.push_back(newsock);
      else
		{
		  m_psock->DestroySocket(newsock);
		  newsock = SOCKID_INVALID;
		}
  }
  return newsock;

}

//Subscribes to a universe on a network interface.
//It may attempt to create a new socket for that subscription if the current 
//ones are full for that nework interface.
bool CStreamClient::SubscribeUniverse(uint2 universe, netintid iface)
{
  CIPAddr addr;
  GetUniverseAddress(universe, addr);
  addr.SetNetInterface(iface);
  sockid sock = SOCKID_INVALID;
  
	//Set to true if we can share sockets -- ONLY if the socket library does not share subscriptions
	//across ports (e.g. linux).  In that case, we create individual standalone multicast sockets 
	//-- otherwise other applications/sockets listening to full ACN or other sACN universes will 
	//cause unnecessary traffic to be filtered from our applications.
	//In fact, on OSX it's currently critical to not share, as OSX has problems with multiple app sockets bound
	//to the same port, and an application using both sACN and full ACN would have problems.
	//TODO: Perhaps have API to allow this for linux as well, even though extra packets will be processed??
  bool sharesockets = !(m_psock->McastMessagesSharePort()) 
                      && !(m_psock->McastMessagesIgnoreSubscribedIface());
  
  bool result = false;

  if(GetLock())
  {
		//First, check if a socket is already subscribed to that address.  
	   //Whether or not we're sharing sockets for multiple mcast addresses, 
	   //a socket that is already subscribed to that address will be shared.
      subiter it = m_subs.find(addr);
      if(it != m_subs.end())
		{
			sock = it->second;
			result = m_psock->SubscribeMulticast(sock, addr);
		}
      else if(sharesockets)
		{
			//No existing subscription, see if we can add the addr to an 
			//existing socket.
			for(sockiter it = m_sockets.begin(); it != m_sockets.end(); ++it)
			{
				//Make sure the port and network interface are correct 
				//(depending on the platform)
				CIPAddr baddr;
				if((m_psock->GetBoundAddress(*it, baddr)) &&
					(baddr.GetIPPort() == addr.GetIPPort()) &&
					(baddr.GetNetInterface() == addr.GetNetInterface()) &&
					(m_psock->RoomForSubscribe(*it, addr)))
				{
					sock = *it;
					break;
				}
			}
			  
			//Didn't find a socket, attempt full create
			if(SOCKID_INVALID == sock)
				sock = CreateSocket(addr.GetNetInterface(), NULL);

			//Now, do the subscribe to the socket we created/found
			result = (sock != SOCKID_INVALID) 
						 && (m_psock->SubscribeMulticast(sock, addr));
		}
      //Not sharing
      else 
		{
			sock = CreateSocket(addr.GetNetInterface(), &addr);
			result = (sock != SOCKID_INVALID);
		}

      //Finally, add the subscription to the list
      if(result)
			m_subs.insert(std::pair<CIPAddr, sockid>(addr, sock));		
      
      ReleaseLock();
	}

	return result;
}

//Unsubscribes from a universe on a network interface.
//If that socket is no longer useable, it will be removed
bool CStreamClient::UnsubscribeUniverse(uint2 universe, netintid iface)
{
	bool result = false;
	CIPAddr addr;
	GetUniverseAddress(universe, addr);
	addr.SetNetInterface(iface);
  
	if(GetLock())
	{
		subiter it = m_subs.find(addr);
		if(it != m_subs.end())
		{
			bool can_reuse = false;  
	  
			//Either we can fully unsubscribe, in which can_reuse is used,
			//otherwise we pretend that we can reuse so the socket isn't removed
			if(!m_psock->UnsubscribeMulticast(it->second, it->first, can_reuse))
				can_reuse = true;
	  
			if(!can_reuse)
			{
				m_psock->DestroySocket(it->second);
				for(sockiter sockit = m_sockets.begin(); sockit != m_sockets.end(); ++sockit)
				{
					if(*sockit == it->second)
					{
						m_sockets.erase(sockit);
						break;
					}
				}
				m_subs.erase(it);
			}
		}
		ReleaseLock();
		result = true;
	}

  return result;
}

//Call this to init the library after creation. Can be used right away (if it 
//returns true)
bool CStreamClient::InternalStartup(IAsyncSocketServ* psocket, 
				    IStreamACNCliNotify* pnotify)
{
	m_psock = psocket;
	m_pnotify = pnotify;
	m_listening = ALL;

	//Init the interface list that ListenUniverse will use to create the sockets.
	m_numifaces = psocket->GetNumInterfaces();
	m_pifaces = new IAsyncSocketServ::netintinfo [m_numifaces];
	if(!m_pifaces)
		return false;
  
	psocket->CopyInterfaceList(m_pifaces);

	return true;
}

//Call this to init the library after creation. Can be used right away (if it 
//returns true)
bool CStreamClient::InternalStartup(IAsyncSocketServ* psocket, 
				    IStreamACNCliNotify* pnotify, 
				    listenTo version)
{
	m_psock = psocket;
	m_pnotify = pnotify;
	m_listening = version;

	//Init the interface list that ListenUniverse will use to create the sockets.
	m_numifaces = psocket->GetNumInterfaces();
	m_pifaces = new IAsyncSocketServ::netintinfo [m_numifaces];
	if(!m_pifaces)
		return false;
  
	psocket->CopyInterfaceList(m_pifaces);

	return true;
}

//Call this to init the library after creation. Can be used right away (if it 
//returns true)
bool CStreamClient::InternalStartup(IAsyncSocketServ* psocket, 
				    IStreamACNCliNotify* pnotify, 
				    listenTo version,
				    int universal_hold_last_look_time)
{
	m_psock = psocket;
	m_pnotify = pnotify;
	m_listening = version;

	//if it's not in the valid range, we will eat you
	if(setUniversalHoldLastLook(universal_hold_last_look_time) == -1)
	{
		return false;
	}

	//Init the interface list that ListenUniverse will use to create the sockets.
	m_numifaces = psocket->GetNumInterfaces();
	m_pifaces = new IAsyncSocketServ::netintinfo [m_numifaces];
	if(!m_pifaces)
		return false;
  
	psocket->CopyInterfaceList(m_pifaces);
  
	return true;
}

//Call this to de-init the library before destruction
void CStreamClient::InternalShutdown()
{
	//Kill off the notification in case some packets are sneaking through
	m_pnotify = NULL;

	//Nothing to do to clean up the multiverse, just kill the sockets.
	if(m_psock)
	{
		for(sockiter it = m_sockets.begin(); it != m_sockets.end(); ++it)
			m_psock->DestroySocket(*it);
      
		m_psock = NULL;
	}
  
	if(m_pifaces)
	{
		delete [] m_pifaces;
		m_pifaces = NULL;
		m_numifaces = 0;
	}
}

//The derivitive calls this upon receipt of a packet via 
//AsyncSocket::ReceiveInto.
//Buffer memory is owned by the caller.
void CStreamClient::ParsePacket(const CIPAddr& fromaddr, uint1* pbuffer, 
				uint buflen)
{
	CID source_cid;
	uint1 start_code;
	uint1 sequence;
	uint2 universe;
	uint2 slot_count;
	uint1* pdata;
	char source_name [SOURCE_NAME_SIZE];
	uint1 priority;
	bool is_sampling = false;

	//These only apply to the ratified version of the spec, so we will hardwire
	//them to be 0 just in case they never get set.
	uint2 reserved = 0;
	uint1 options = 0;

	if(!ValidateStreamHeader(pbuffer, buflen, source_cid, source_name, priority, 
			   start_code, reserved, sequence, options, universe, 
			   slot_count, pdata))
		return;
  

	int root_vect = UpackB4(pbuffer + ROOT_VECTOR_ADDR);
	bool notify = true;
	universekey key(source_cid, universe);
  
	//We're not going to deal with this if we're not supposed to
	//If we only listen to the officially specced version, and this is
	//carrying the vector for the draft, then we drop it on the floor.
	//If we are only listening to the draft version, and this is carrying
	//the vector for the official spec, we will stomp on it.
	//If we are listening to all of them, then we continue to listen.
	if((root_vect == ROOT_VECTOR && m_listening == DRAFT)
		||(root_vect == DRAFT_ROOT_VECTOR && m_listening == SPEC)
		|| m_listening == NOTHING)
	{
		return;
	}
  
	//Update the source tree
	if(GetLock())
	{
		is_sampling = m_sample[universe].sampling;

		multiter it = m_multiverse.find(key);
		if(it != m_multiverse.end())
		{
			//check to see if the 'stream_terminated' bit is set in the options
			if((root_vect == ROOT_VECTOR) && ((options & 0x40) == 0x40))
			{
				//by setting this flag to false, 0xdd packets that may come in while the terminated data
				//packets come in won't reset the priority delta timer
				it->second.waited_for_dd = false;

				it->second.packetdelta.SetInterval(m_hold_last_look_time);

				//even if it's just sent with one of the packets, we actually want to kill both prioroty and data
				if(it->second.doing_per_channel)
					it->second.prioritydelta.SetInterval(m_hold_last_look_time);
	      
				ReleaseLock();
				return;
			}

			//Based on the start code, update the timers
			if(start_code == STARTCODE_DMX)  
			{
				//No matter how valid, we got something -- but we'll tweak the interval for any hll change
				it->second.doing_dmx = true;
				it->second.packetdelta.SetInterval(WAIT_OFFLINE + m_hold_last_look_time);  
			}
			else if(start_code == STARTCODE_PRIORITY && it->second.waited_for_dd)
			{
				//The source could have stopped sending dd for a while.
				it->second.doing_per_channel = true;  
				it->second.prioritydelta.Reset();
			}

			//Validate the sequence number, updating the stored one
			//The two's complement math is to handle rollover, and we're 
			//explicitly doing assignment to force the type sizes.  A 
			//negative number means we got an "old" one, but we assume that 
			//anything really old is possibly due the device having rebooted 
			//and starting the sequence over. 
			int1 result = ((int1)sequence) - ((int1)(it->second.seq));
			if((result <= 0) && (result > -20))
			{
				//drop the packet
				notify = false;  
			}
			else
				it->second.seq = sequence;   
		}
      else //Add a new one
		{
			universedata d;
			it = m_multiverse.insert(std::pair<universekey, universedata>(key, d)).first;
			it->second.packetdelta.SetInterval(WAIT_OFFLINE + m_hold_last_look_time);  
			it->second.seq = sequence;
			it->second.doing_dmx = (start_code == STARTCODE_DMX);
			//If we are in the sample period, we are not going to wait for dd packets
			if(is_sampling)
			{
				it->second.waited_for_dd = true;
				it->second.doing_per_channel = (start_code == STARTCODE_PRIORITY);
				it->second.prioritydelta.SetInterval(WAIT_OFFLINE + m_hold_last_look_time);
			}
			else
			{
				it->second.waited_for_dd = false;
				it->second.doing_per_channel = false;
				//The initial wait.  When reusing this, the timeout shifts to 
				//WAIT_OFFLINE
				it->second.prioritydelta.SetInterval(WAIT_PRIORITY);  
			}
		}

		//This next bit is a little tricky.  We want to wait for dd packets (sampling period
		//tweaks aside) and notify them with the dd packet first, but we don't want to do that
		//if we've never seen a dmx packet from the source.
		if(!it->second.doing_dmx)  
		{
			notify = false;
			it->second.prioritydelta.Reset();  //We don't want to let the priority timer run out
		}
		else if(!it->second.waited_for_dd)
		{
			if(start_code == STARTCODE_PRIORITY)
			{
				it->second.waited_for_dd = true;
				it->second.doing_per_channel = true;
				it->second.prioritydelta.SetInterval(WAIT_OFFLINE + m_hold_last_look_time);
			}
			else if(it->second.prioritydelta.Expired())
			{
				it->second.waited_for_dd = true;
				//In case the source starts sending later
				it->second.prioritydelta.SetInterval(WAIT_OFFLINE + m_hold_last_look_time);  
			}
			else
			{
				notify = false;
			}
		}
		
		ReleaseLock();
	}
     
	if(notify && m_pnotify)
	{
		m_pnotify->UniverseData(source_cid, source_name, fromaddr, universe, 
					reserved, sequence, options, priority, 
					start_code, slot_count, pdata);

	}
}

/****************************************
 * IStreamACNCli overrides
 ****************************************/
//Call this every 200-300ms to detect any expired sample periods.  
//If any expired sample periods are found, a SamplingEnded is generated for 
//each universe
void CStreamClient::FindExpiredSamples()
{	
	for(sampleiter it = m_sample.begin(); it != m_sample.end(); ++it)
	{
		if(it->second.sampling && it->second.sample_timer.Expired())
		{
			//stop sampling
			it->second.sampling = false;
			//notify
			if(m_pnotify)
				m_pnotify->SamplingEnded(it->first);
		}
	}
}

//Call this every 200-300ms to detect any expired sources.  If any expired 
//sources are found, a SourceExpired is generated for each source on each 
//universe
void CStreamClient::FindExpiredSources()
{
	//We need to perform the notifications outside of the lock, so we'll cache 
	//the info.
	std::vector< universekey > data_cache;
	std::vector< universekey > priority_cache;

	if(GetLock())
	{
		//Since this will remove the expired src, we need to be a bit careful 
		//how we access the map
		multiter it = m_multiverse.begin();
		while(it != m_multiverse.end())
		{
			multiter curit = it;  //In the packet expired case, we want to remove the iterator safely
			++it;

			//The packet delta could expire if we received per channel without ever receiving dmx
			if(curit->second.packetdelta.Expired())  
			{
				if(curit->second.doing_dmx)
					data_cache.push_back(curit->first);
				m_multiverse.erase(curit);
			}
			else if (curit->second.doing_per_channel && curit->second.prioritydelta.Expired())
			{
				curit->second.doing_per_channel = false;
				priority_cache.push_back(curit->first);
			}
		}

		//Since we have the lock, find the expired samples as well
		FindExpiredSamples();

      ReleaseLock();
	}

	if(m_pnotify)
	{
		for(std::vector< universekey >::iterator cacheit = data_cache.begin(); 
			cacheit != data_cache.end(); ++cacheit)
		{
			m_pnotify->SourceDisappeared(cacheit->first, cacheit->second);
		}
      for(std::vector< universekey >::iterator prit = priority_cache.begin(); 
			 prit != priority_cache.end(); ++prit)
		{
			m_pnotify->SourcePCPExpired(prit->first, prit->second);
		}
	}
}

//Use this to start listening on a universe
//If netiflist (or size) is NULL, the library will listen on any valid network interface.
//If netiflist is non-NULL, this call will fail if a specified interface is invalid.
bool CStreamClient::ListenUniverse(uint2 universe, netintid* netiflist, int netiflist_size)
{
	bool result = true;

	if(!netiflist || !netiflist_size)
	{
		//We ignore subscribe errors in the Any case.
		for(int i = 0; i < m_numifaces; ++i)
		{
			SubscribeUniverse(universe, m_pifaces[i].id);
		}
	}
	else
	{
		for(int i = 0; i < netiflist_size; ++i)
		{
			if(!SubscribeUniverse(universe, netiflist[i]))
			{
				//Failure to subscribe means we need to clean up the other subscribes
				EndUniverse(universe);
				result = false;
				break;
			}
		}
	}

	if(result)
	{
		m_sample[universe].sampling = true;
		m_sample[universe].sample_timer.SetInterval(SAMPLE_TIME);
  		if(m_pnotify)
			m_pnotify->SamplingStarted(universe);
	}

	return result;
}

//Use this to stop listening on a universe
void CStreamClient::EndUniverse(uint2 universe)
{
	//Just unsubscribe from all network interfaces
	for(int i = 0; i < m_numifaces; ++i)
		UnsubscribeUniverse(universe, m_pifaces[i].id);
  
	//MN: if we're sampling on this universe and end it, we should maybe stop?
	if(m_sample[universe].sampling)
	{
		m_sample[universe].sampling = false; //just for completeness sake
		m_sample[universe].sample_timer.SetInterval(0);
		m_sample.erase(universe);
		m_pnotify->SamplingEnded(universe);
	}
	
   multiter it = m_multiverse.begin();
	while(it != m_multiverse.end())
	{
		multiter oldit = it;
		++it;
	   if(oldit->first.second == universe)
			m_multiverse.erase(oldit);
	}
}

//return which versions (draft, spec, all, none) we are listening to
int CStreamClient::listeningTo()
{
	return (int) m_listening;
}

void CStreamClient::toggleListening(listenTo version, bool on_or_off)
{
	if(m_listening == NOTHING)
	{
		if(on_or_off)
			m_listening = version;
	}
	else if(m_listening == ALL)
	{
		if(!on_or_off)
			m_listening -= version;
	}
	else if(m_listening == version)
	{
		if(!on_or_off)
			m_listening = NOTHING;
	}
	else if(m_listening != version)
	{
		if(on_or_off)
			m_listening = ALL;
	}	  
}

//sets the amount of time in milliseconds we will wait for a backup to 
//appear after a source drops off line
//returns the value being set on success and -1 on failure
int CStreamClient::setUniversalHoldLastLook(int hold_time)
{
	if(hold_time < 0 || hold_time > MAX_HOLD_LAST_LOOK_TIME)
	{
		return -1;
	}
	return m_hold_last_look_time = hold_time;
}

//returns the amount of time in milliseconds we will wait for a backup 
//to appear after a source drops off line 
int CStreamClient::getUniversalHoldLastLook()
{
	return m_hold_last_look_time;
}

/****************************************
 *IAsyncSocketClient overrides
 ****************************************/

//This function should not be called, as we are using manual_recv sockets
void CStreamClient::ReceivePacket(sockid /*id*/, const CIPAddr& /*from*/, 
				  uint1* pbuffer, uint buflen)
{
  if(m_psock)
    m_psock->DeletePacket(pbuffer, buflen);
}

//This is the notification that a socket has gone bad/closed.
void CStreamClient::SocketBad(sockid id)
{
	netintid netid = NETID_INVALID;
	std::vector<uint2> bad_unis;

	//Just clean up the socket tracking -- the sources can expire naturally
	if(GetLock())
	{
		//Find all subscriptions on this socket and clean them up
		subiter it = m_subs.begin();
		while(it != m_subs.end())
		{
			if(it->second == id)
			{
				netid = it->first.GetNetInterface();
				//Quick hack to translate the addr back to the universe.  This 
				//should be a separate function.
				bad_unis.push_back(UpackB2(it->first.GetV6Address() + 14));

				subiter tmp = it;
				++it;
				m_subs.erase(tmp);
			}
			else
			++it;
		}	

		for(sockiter sit = m_sockets.begin(); sit != m_sockets.end(); ++sit)
		{
			if(*sit == id)
			{
				m_sockets.erase(sit);
				break;
			}
		}
      
		ReleaseLock();
	}	

	while(m_pnotify && !bad_unis.empty())
	{
		m_pnotify->UniverseBad(bad_unis.front(), netid);
		bad_unis.erase(bad_unis.begin());
	}
}
