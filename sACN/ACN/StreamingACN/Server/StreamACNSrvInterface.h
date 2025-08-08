/*StreamACNSrvInterface.h  Definition of the StreamingACN Server
This class acts as a streaming ACN server.  It allows you to instance universes,
and will automatically send the buffer for a universe periodically, based on a 
call to Tick

*****************************************************************
This is a pure interface -- Use the platform specific interface to 
actually create and control the StreamACNSrv class instance.
*****************************************************************

Normal usage:
-Call CreateInstance to create an instance of the library. (p = IWinStreamACNSrv::CreateInstance();)
-Call Startup on that instance.
-For each universe you want to source:
   - If you are sourcing data with per-channel-priorities, call
     CreateUniverse with a startcode of STARTCODE_PRIORITY, ignore_inactivity_logic
     set to IGNORE_INACTIVE_PRIORITY, and send_intervalms set to SEND_INTERVAL_PRIORITY.
     You will receive a data buffer and universe handle that you can use to manipulate
     the per-channel priorities.  0 is not a valid universe number
   - Call CreateUniverse with a startcode of STARTCODE_DMX.  You will receive a data buffer
     and handle that you can use to manipulate the data you are sourcing.

-call Tick at your DMX rate (e.g. every 23ms) to drive the sending rate.
-Whenever you change data in the buffer, set the universe dirty
-When you're done with a universe, destroy it.
-Call Shutdown on the library.
-Call DestroyInstance to destroy the library instance.
*/

#ifndef _STREAMACNSRVINTERFACE_H_
#define _STREAMACNSRVINTERFACE_H_

#ifndef _DEFTYPES_H_ 
#error "#include error: StreamACNSrvInterface.h requires deftypes.h"
#endif
#ifndef _CID_H_
#error "#include error: StreamACNSrvInterface.h requires CID.h"
#endif
#ifndef _ASYNCSOCKETINTERFACE_H_
#error "#include error: StreamACNSrvInterface.h requires AsyncSocketInterface.h"
#endif

//These definitions are for all supported startcodes
#ifndef STARTCODE_PRIORITY
#define STARTCODE_DMX 0             /*The payload is up to 512 1-byte dmx values*/
#define STARTCODE_PRIORITY 0xDD     /*The payload is the per-channel priority (0-200), where 0 means "ignore my values on this channel"*/
#endif

//These definitions are to be used with the ignore_inactivity_logic field of CreateUniverse
#define IGNORE_INACTIVE_DMX  false 
#define IGNORE_INACTIVE_PRIORITY false /*Any priority change should send three packets anyway, around your frame rate*/

//These definitions are to be used with the send_intervalms parameter of CreateUniverse
#define SEND_INTERVAL_DMX	850	/*If no data has been sent in 850ms, send another DMX packet*/
#define SEND_INTERVAL_PRIORITY 1000	/*By default, per-channel priority packets are sent once per second*/

//Bitflags for the options parameter of Create Universe. 
//Alternatively, you can directly set them while a universe is running with 
//OptionsPreviewData. 
#define PREVIEW_DATA_OPTION 0x80

class IStreamACNSrv
{
public:
   virtual ~IStreamACNSrv(){}

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
	virtual int Tick(uint* dirtyhandles, uint hcount) = 0;  

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
	virtual bool CreateUniverse(const CID& source_cid, netintid* netiflist, int netiflist_size, 
	                            const char* source_name, uint1 priority, uint2 reserved, uint1 options,
	                            uint1 start_code, uint2 universe, uint2 slot_count, uint1*& pslots, 
										 uint& handle, bool ignore_inactivity_logic = IGNORE_INACTIVE_DMX, 
										 uint send_intervalms = SEND_INTERVAL_DMX) = 0;


	//After you add data to the data buffer, call this to trigger the data send on
	//the next Tick boundary.
	//Non-dirty data won't be sent until the inactivity or send_interval time,
   //but that timer is still driven by the Tick call.
	//Due to the fact that access to the universes needs to be thread safe,
	//this function allows you to set an array of universes dirty at once (to incur the lock
	//overhead only once).  If your algorithm is to SetUniversesDirty and then immediately
	//call Tick, you will save a lock access by passing these in directly to Tick.
	virtual void SetUniversesDirty(uint* handles, uint hcount) = 0;

	//In the event that you want to send out a message for particular universes (and start codes)
	//in between ticks, call this function.  This does not affect the dirty bit for the universe,
	//inactivity count, etc, and the tick will still operate normally when called.
   //This is particularly useful if you want to ensure a priority change goes
   //out before a DMX value change.
	virtual void SendUniversesNow(uint* handles, uint hcount) = 0;

	//Use this to destroy a universe.  While this is thread safe internal to the library,
	//this does invalidate the pslots array that CreateUniverse returned, so do not access
	//that memory after or during this call.  This function also handles the logic to
	//mark the stream as Terminated so that the Tick function can send a few extra terminated packets.
	virtual void DestroyUniverse(uint handle) = 0;
  
	//sets the preview_data bit of the options field
	virtual void OptionsPreviewData(uint handle, bool preview) = 0;

	//sets the stream_terminated bit of the options field
	//Note that DestroyUniverse does this for you.
	virtual void OptionsStreamTerminated(uint handle, bool terminated)= 0;

	//Use this to destroy a priority universe but keep the dmx universe alive
	virtual void DEBUG_DESTROY_PRIORITY_UNIVERSE(uint handle) = 0;

	/*DEBUG USAGE ONLY --causes packets to be "dropped" on a particular universe*/
	virtual void DEBUG_DROP_PACKET(uint handle, uint1 decrement) = 0;
};  

#endif

