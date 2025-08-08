/* StreamClient.h  implementation of the IStreamACNCli class.
 * See StreamACNCliInterface.h  for details.
 */

#ifndef _STREAMCLIENT_H_
#define _STREAMCLIENT_H_

#ifndef _STREAMACNCLIINTERFACE_H_
#error "#include error: StreamClient.h requires StreamACNCliInterface.h"
#endif
#ifndef _TOCK_H_
#error "#include error: StreamClient.h requires Tock.h"
#endif


class CStreamClient : public IStreamACNCli, public IAsyncSocketClient
{     
public	:
  CStreamClient();  
  virtual ~CStreamClient();
  
  //Call this to init the library after creation. Can be used right away (if 
  //it returns true)
  //this version supports legacy calls
  bool InternalStartup(IAsyncSocketServ* psocket, 
		       IStreamACNCliNotify* pnotify);

  //Call this to init the library after creation. Can be used right away (if 
  //it returns true)
  //this is the specified version
  bool InternalStartup(IAsyncSocketServ* psocket, 
		       IStreamACNCliNotify* pnotify, 
		       listenTo version);

  //Call this to init the library after creation. Can be used right away (if 
  //it returns true)
  //this can be called with an option to set the Universal Hold Last Look time
  bool InternalStartup(IAsyncSocketServ* psocket, 
		       IStreamACNCliNotify* pnotify, 
		       listenTo version,
		       int universal_hold_last_look_time);

  //Call this to de-init the library before destruction
  void InternalShutdown();
  
  //The derivitive calls this upon receipt of a packet via 
  //AsyncSocket::ReceiveInto.
  //Buffer memory is owned by the caller.
  void ParsePacket(const CIPAddr& fromaddr, uint1* pbuffer, uint buflen);

  /****************************************
   * IStreamACNCli overrides
   ****************************************/
  //Call this every 200-300ms to detect any expired sources.  If any expired 
  //sources are found, a SourceExpired is generated for each source on each 
  //universe
  virtual void FindExpiredSources();

  //Use this to start listening on a universe
  //If netiflist (or size) is NULL, the library will listen on any valid 
  //network interface.
  //If netiflist is non-NULL, this call will fail if a specified interface is 
  //invalid.
  virtual bool ListenUniverse(uint2 universe, netintid* netiflist, 
			      int netiflist_size);

  //Use this to stop listening on a universe
  virtual void EndUniverse(uint2 universe);
  
  virtual void toggleListening(listenTo version, bool on_or_off);

  //return which versions (draft, spec, all, none) we are listening to
  virtual int listeningTo();

  //sets the amount of time in milliseconds we will wait for a backup to 
  //appear after a source drops off line
  //returns the value being set on success and -1 on failure
  virtual int setUniversalHoldLastLook(int hold_time);
  
  //returns the amount of time in milliseconds we will wait for a backup 
  //to appear after a source drops off line 
  virtual int getUniversalHoldLastLook();

  
  /****************************************
   *IAsyncSocketClient overrides
   ****************************************/
  //This function should not be caled, as we are using manual_recv sockets
  virtual void ReceivePacket(sockid id, const CIPAddr& from, uint1* pbuffer, 
			     uint buflen);
  
  //This is the notification that a socket has gone bad/closed.
  virtual void SocketBad(sockid id);
  
  /****************************************
   * Derivitaves need to support these
   ****************************************/
  //Grabs the simple lock for the source tree and socket list, without a timeout
  virtual bool GetLock() = 0;
  
  //Frees the simple lock
  virtual void ReleaseLock() = 0;
  
  //This class uses manual receive sockets, so the derivative needs to call 
  //AsyncSocket::ReceiveInto
  //and call ParsePacket with the result.  ReceiveInto is blocking, and 
  //requires a read thread to drive it, so this function initiates the thread 
  //in the OS class.  The OS class Shutdown() function will clean up.
  virtual bool SpawnSocketThread(sockid id) = 0;
						
protected:
  IAsyncSocketServ* m_psock;
  IStreamACNCliNotify* m_pnotify;
  
  //what version(s) of the specification we are listening to
  int m_listening;
  
  //We need to track the list of TOTAL network interfaces
  int m_numifaces;
  IAsyncSocketServ::netintinfo* m_pifaces;
  
  //user-configurable time to hold last look from a source that has 
  //dropped off line
  int m_hold_last_look_time;

  //I'm tracking two things, the list of sockets, and the multicast addresses 
  //subscribed to.
  //These are protected by a lock but only to prevent subscribe universe, end 
  //universe, and Bad Socket from happening at the same time -- reads are done
  //in their thread with the socket, and don't reference this list
  std::vector<sockid> m_sockets;
  typedef std::vector<sockid>::iterator sockiter;
  std::map<CIPAddr, sockid> m_subs;
  typedef std::map<CIPAddr, sockid>::iterator subiter;

  //Helper routine that creates a new socket on the network interface, or if
  //the full mcast address is !null, creates a standalone mcast socket on that.
  //Returns SOCKID_INVALID on error, otherwise the newly created socket
  sockid CreateSocket(netintid iface, CIPAddr* maddr);
  
  //Subscribes to a universe on a network interface.
  //It may attempt to create a new socket for that subscription if the current
  //ones 
  //are full for that network interface.
  bool SubscribeUniverse(uint2 universe, netintid iface);
  
  //Unsubscribes from a universe on a network interface.
  //If that socket is no longer useable, it will be removed
  bool UnsubscribeUniverse(uint2 universe, netintid iface);
  
  //Each source can send on multiple universes -- and the sequence number and 
  //timing will be tracked per universe.  Since we only care about the context
  //of the source and universe together, we'll use the pair of them as a key
  typedef std::pair< CID, uint2 > universekey;
  class unikeyless
  {
  public:
    bool operator()(const universekey& a, const universekey& b) const
    {
      if(a.first < b.first)
	return true;
      if(a.first == b.first)
	return a.second < b.second;
      return false;
    }
  };

  struct universedata
  {
    //The ttimer parameters need to be set after adding to the multiverse, 
    //as they don't copy
    ttimer packetdelta;	
	 //If true, we have received a data packet for this universe
	 bool doing_dmx;  
    //Used for the initial notification, which is throttled by prioritydelta
    bool waited_for_dd;  
    //If true, we are tracking per-channel priority messages for this source
    bool doing_per_channel;
    //if !waited_for_dd, used to track if a source is finally detected 
    ttimer prioritydelta;  
    //(either by receiving priority or timeout).  If doing_per_channel,
    //used to time out the 0xdd packets to see if we lost per-channel priority
    uint1 seq;
  };

  struct universe_sample
  {
    bool sampling;
    ttimer sample_timer;
  };

  //Protected by the lock
  std::map< universekey, universedata, unikeyless > m_multiverse;  
  typedef std::map< universekey, universedata, unikeyless >::iterator multiter;
  std::map< uint2, universe_sample > m_sample;
  typedef std::map< uint2, universe_sample >::iterator sampleiter;
  
  //Call this every 200-300ms to detect any expired sample periods.  
  //If any expired sample periods are found, a SamplingEnded is generated for 
  //each universe
  void FindExpiredSamples();

};

#endif

