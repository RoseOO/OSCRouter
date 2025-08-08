/* StreamServer.h  Definition of the CStreamServer class.
 * This class acts as a streaming ACN server.  It allows you to instance 
 * universes, and will automatically send the buffer for a universe 
 * periodically, based on a call to Tick
 *
 * Normal usage:
 * -CreateUniverse on each universe you want to control, storing the
 * universe handle and data buffer. 0 is not a valid universe number.
 * -call Tick at your DMX rate (e.g. every 23 ms) to drive the sending rate.
 * -Whenever you change data in the buffer, set the universe dirty
 * -When you're done with a universe, destroy it.
 */

#ifndef _STREAMSERVER_H_
#define _STREAMSERVER_H_

#ifndef _STREAMACNSRVINTERFACE_H_
#error "#include error: StreamServer.h requires StreamACNSrvInterface.h"
#endif
#ifndef _TOCK_H_
#error "#include error: StreamServer.h requires tock.h"
#endif

class CStreamServer : public IStreamACNSrv, public IAsyncSocketClient
{
public:
  CStreamServer();  
  virtual ~CStreamServer();
  
  //Call this to init the library after creation. 
  //Can be used right away (if it returns true)
  bool InternalStartup(IAsyncSocketServ* psocket);
  
  //Call this to de-init the library before destruction
  void InternalShutdown();
  
  
  /**************************************************************************
   * IStreamACNSrv overrides -- It is assumed that the platform wrapper locks 
   * around each call
   **************************************************************************/

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
  virtual int Tick(uint* dirtyhandles, uint hcount);  

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
  virtual bool CreateUniverse(const CID& source_cid, netintid* netiflist, 
			      int netiflist_size, const char* source_name, 
			      uint1 priority, uint2 reserved, uint1 options, 
			      uint1 start_code, uint2 universe, 
			      uint2 slot_count, uint1*& pslots, uint& handle,
			      bool ignore_inactivity_logic, 
			      uint send_intervalms);

  //After you add data to the data buffer, call this to trigger the data send
  //on the next Tick boundary.
  //Otherwise, the data won't be sent until the inactivity or send_interval 
  //time.
  //Due to the fact that access to the universes needs to be thread safe,
  //this function allows you to set an array of universes dirty at once 
  //(to incur the lock overhead only once).  If your algorithm is to 
  //SetUniversesDirty and then immediately call Tick, you will save a lock 
  //access by passing these in directly to Tick.
  virtual void SetUniversesDirty(uint* handles, uint hcount);

  //Use this to destroy a universe.  While this is thread safe internal to the library,
  //this does invalidate the pslots array that CreateUniverse returned, so do not access
  //that memory after or during this call.  This function also handles the logic to
  //mark the stream as Terminated and send a few extra terminated packets.
  virtual void DestroyUniverse(uint handle);

  //Use this to destroy a priority universe but keep the dmx universe alive
  virtual void DEBUG_DESTROY_PRIORITY_UNIVERSE(uint handle);

  //In the event that you want to send out a message for particular 
  //universes (and start codes) in between ticks, call this function.  
  //This does not affect the dirty bit for the universe, inactivity count, 
  //etc, and the tick will still operate normally when called.
  virtual void SendUniversesNow(uint* handles, uint hcount);

  //sets the preview_data bit of the options field
  virtual void OptionsPreviewData(uint handle, bool preview);

  //sets the stream_terminated bit of the options field
  //Note that DestroyUniverse does this for you.
  virtual void OptionsStreamTerminated(uint handle, bool terminated);

  /*
   * DEBUG USAGE ONLY 
   * --causes packets to be "dropped" on a particular universe
   */
  virtual void DEBUG_DROP_PACKET(uint handle, uint1 decrement);

  /**************************************************************************
   * IAsyncSocketClient functions that do nothing, as we don't actually read 
   * anything
   **************************************************************************/
  //This is the notification that a socket has gone bad/closed.
  //After this call, the socket is considered disconnected, but not
  //invalid... Call DestroySocket to remove it.
  virtual void SocketBad(sockid id);
  
  //This function caches the packet and sends to CSDT functionality later
  virtual void ReceivePacket(sockid id, const CIPAddr& from, uint1* pbuffer, 
			     uint buflen);
private:
  IAsyncSocketServ* m_psocklib;
  
  //We'll have a socket set up on each network interface to use for all 
  //sending on that iface.
  //This is only a reference -- for speed each universe will contain the list
  //of sockids to send over.  We can get away with this because we aren't 
  //currently recovering from a socket going bad.  Otherwise we would need 
  //lookups, protection around the socket map, etc.

  std::map<netintid, sockid> m_sockets;
  typedef std::map<netintid, sockid>::iterator sockiter;
  
  //Each universe shares its sequence numbers across start codes.
  //This is the central storage location, along with a refcount
  typedef std::pair<int, uint1*> seqref;
  std::map<uint2, seqref > m_seqmap;
  typedef std::map<uint2, seqref >::iterator seqiter;
  
  //Returns a pointer to the storage location for the universe, adding if 
  //need be.
  //The newly-added location contains sequence number 0.
  uint1* GetPSeq(uint2 universe);
	
  //Removes a reference to the storage location for the universe, removing 
  //completely if need be.
  void RemovePSeq(uint2 universe);
  
  //Each universe is just the full buffer and some state
  struct universe
  {
    //together, number and start_code define a unique handle
    uint2 number; //The universe number
    uint1 start_code; //The start code 
    uint handle; //The handle.  This is needed to help deletions.
    uint1 num_terminates; //The number of consecutive times the 
                          //stream_terminated option flag has been set.
    uint1* psend; //The full sending buf--user can access the data portion.
                  //If NULL, this is not an active universe (just a hole in 
                  //the vector)
    uint sendsize;
    bool isdirty;
	 bool waited_for_dirty;  //If false, we never received the first dirty and this universe doesn't count.
    bool ignore_inactivity; //If true, don't bother looking at inactive_count
    uint inactive_count; //After 3 of these, we start sending at send_interval
    ttimer send_interval; //Whether it's time to send a non-dirty packet
    uint1* pseq; //The storage location of the universe sequence number

    CIPAddr sendaddr; //The multicast address we're sending to, ignoring 
                      //network interface
    std::vector<sockid> wheretosend;  //The sockets to send the message over.

    //and the constructor
    universe():number(0),handle(0),num_terminates(0),psend(NULL),
	       isdirty(false),waited_for_dirty(false),inactive_count(0),pseq(NULL) {}
  };

  //The handle is the vector index
  std::vector<universe> m_multiverse;
  typedef std::vector<universe>::iterator verseiter;
  typedef std::vector<sockid>::iterator senditer;  //Used for universe, but I
                                                   //didn't want the scope 
                                                   //for simplicity
  
  //Since both Tick and SendUniversesNow do similar things, this does the 
  //real sequencing & sending
  void SeqSendUniverse(universe* puni);
  
  //Perform the logical destruction and cleanup of a universe 
  //and its related objects.
  void DoDestruction(uint handle);
  


};

#endif

