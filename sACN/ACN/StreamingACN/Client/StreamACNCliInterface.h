/*StreamACNCliInterface.h  Definition of the StreamACN Client.
 * This class acts as a streaming ACN client.  It allows you to select
 * universes to listen to, and on those universes it will track
 * source sequence numbers and expiry.

*****************************************************************
This is a pure interface -- Use the platform specific interface to 
actually create and control the StreamACNCli class instance.
*****************************************************************

Normal usage:
-Start up Tock and AsyncSocket libraries.
-Call CreateInstance to create an instance of the library. (p = IWinStreamACNCli::CreateInstance();)
-Call Startup on that instance, setting up the socket usage and registering your callback.
-Call ListenUniverse on all universes that you are interested in.
-Process all callbacks from the library as quickly as possible, as it will impede the receipt of the next message
-Call FindExpiredSources every 200-300ms to determine if any sources have expired
-When you're done listening to a universe, call EndUniverse
-Call Shutdown on the library
-Call DestroyInstance to destroy the library instance.

***
A sampling period exists in the API of this library.  
The sampling period is used to ensure that all active sources are seen before making
a decision about which source or sources win control in our prioritized HTP scheme.  
Failure to use the SamplingStarted and SamplingEnded notifications means that you will 
sometimes see and act on a lower priority source before a higher priority source as you
begin listening to network traffic.  This failure in control can result in lights flickering
or at worst has caused lamps to be blown in some fixtures that inadvertently were re-struck 
too quickly.  Therefore:

   - On subscription to a new universe, the library issues a SamplingStarted notification.
	  The library will start a 1.5 second sampling period timer for that universe.  
	  After the timer ends, the library will issue a SamplingEnded notification.
	  During this sample period, all packets will be forwarded to the application without
	  waiting to determine if that source is a per-channel-priority source (receiving a
	  packet with a startcode of 0xdd or waiting 1.5 seconds).  After this sample period,
	  the library goes back to waiting for the 0xdd or 1.5 seconds before forwarding data
	  packets from the source.

	- To take care of potential flicker due seeing the wrong source first,
	  do as follows:
	  Buffer the latest UniverseData (level and priority data) of each source that appears in the
	  sample period. When that period has ended, process (HTP, etc) the stored data as a group
	  to obtain the correct control levels (that are then used directly or converted to DMX)
 
     Note: Because your application may see level data before priority for that data it is 
	  essential to avoid executing merge algorithms until the end of the sample period.


This library's API also contains a user-configurable Universal Hold-Last-Look-Per-Source 
time, which defaults to 1000ms.  This is how long the library waits before 
it issues a notification that a source has dropped offline after the protocol 
detects the source has disappeared (either 0 or 2.5 seconds, depending on how 
gracefully the source leaves).  This may be used as a system wide setting to allow
backups to have more time to take over from their masters.

 */

#ifndef _STREAMACNCLIINTERFACE_H_
#define _STREAMACNCLIINTERFACE_H_

#ifndef _DEFTYPES_H_ 
#error "#include error: StreamACNCliInterface.h requires deftypes.h"
#endif
#ifndef _CID_H_
#error "#include error: StreamACNCliInterface.h requires CID.h"
#endif
#ifndef _ASYNCSOCKETINTERFACE_H_
#error "#include error: StreamACNCliInterface.h requires AsyncSocketInterface.h"
#endif

enum listenTo
{
	 NOTHING = 0,
	 DRAFT = 1,
	 SPEC = 2,
	 ALL = 3
};

//These definitions are for all supported startcodes
#ifndef STARTCODE_PRIORITY
#define STARTCODE_DMX 0             /*The payload is up to 512 1-byte dmx values*/
#define STARTCODE_PRIORITY 0xDD     /*The payload is the per-channel priority (0-200), where 0 means "ignore my values on this channel"*/
#endif

//When not in a sampling period, 
//This library doesn't notify the client of a new source until it has received a STARTCODE_PRIORITY
//packet or WAIT_PRIORITY ms have expired.
#define WAIT_PRIORITY 1500

//The standard time to wait before declaring a source off-line, either as a source of data or a
//source of per-channel priorities.
#define WAIT_OFFLINE 2500

//The default amount of time to hold a look after its source has disappeared
#define DEFAULT_HOLD_LAST_LOOK_TIME 1000

//The maximum time, in milliseconds, to allow the user to hold the last
//look.  This value is based on the comparable timing in the _gateways,
//where it is currently a uint2
#define MAX_HOLD_LAST_LOOK_TIME 65535000

//The time during which to sample
#define SAMPLE_TIME 1500

//This is the notification callback for the client library.
//Note that these could be called by different threads, so you will have to 
//do your own thread protection if, for example, you are using multiple NICs.
class IStreamACNCliNotify
{
public:
   virtual ~IStreamACNCliNotify(){}

	//This notification is triggered whenever a source has expired.
	virtual void SourceDisappeared(const CID& source, uint2 universe) = 0;

	//This notification is triggered whenever a source was sending per-channel-priority but stopped.
	//Note that the source could send a new 0xdd packet at any time, starting up per-channel-priority again.
	virtual void SourcePCPExpired(const CID& source, uint2 universe) = 0;

	//This is called by the library when a sampling period begins, currently only when a universe is
	//first subscribed to. Due to thread timing, it is possible to receive the UniverseData for a source
	//before this notification is called, so assume you are in the sampling period when you first call
	//ListenUniverse.
	//When this library is able to detect local ip address changes, the sample period might restart
	virtual void SamplingStarted(uint2 universe) = 0;

	//This is called by the library when a sampling period is complete
	virtual void SamplingEnded(uint2 universe) = 0;

	//This is the universe data.  The buffer is owned by the library, so copy out what you need
	//before returning.  The same goes for the source_name.  
	//Eacn network interface has a thread that could drive this, so be careful that the callback is thread safe.
	//When not in a sample period, this library only sends this notification after a 
	//per-channel-priority packet has been received (in which case the universedata is a 
	//startcode_priority packet), or if the timeout has occured.

        //The reserved data field is a field that currently has no purpose, 
        //other than to act as a placeholder for some future data.
        //A server can set the bits in the options flag to indicate information
        //about the packet being sent:
        //- Preview_Data
        //   * if bit 7 (the most significant bit) is high, then the data in 
        //     this packet is intended for use in visualization or media 
        //     server preview applications and should not be used to generate 
        //     live output
        //- Stream_Terminated
        //   * bit 6 indicates that the data source has stopped transmitting on
        //     this universe.  This code shall be sent three times upon
        //     termination of a stream    
        //- Bits 0-5 are not currently in use.
	virtual void UniverseData(const CID& source, const char* source_name, const CIPAddr& source_ip, 
                             uint2 universe, uint2 reserved, uint1 sequence, uint1 options, 
									  uint1 priority, uint1 start_code,
                             uint2 slot_count, uint1* pdata) = 0; 

	//Due to a socket error, this universe is no longer subscribed to on this network interface.
	//Any sources on this universe/network iface will expire naturally.
	virtual void UniverseBad(uint2 universe, netintid iface) = 0;
};

class IStreamACNCli
{
public:
	virtual ~IStreamACNCli(){}

	//Call this every 200-300ms to detect any expired sources.  If any expired sources are
	//found, a SourceExpired is generated for each source on each universe
	virtual void FindExpiredSources() = 0;

	//Use this to start listening on a universe.  
	//If netiflist (or size) is NULL, the library will listen on any valid network interface.
	//If netiflist is non-NULL, this call will fail if a specified interface is invalid.
	virtual bool ListenUniverse(uint2 universe, netintid* netiflist, int netiflist_size) = 0;

	//Use this to stop listening on a universe -- Note that this will cause SourceExpired
	//notifications for that universe.
	virtual void EndUniverse(uint2 universe) = 0;

	//Turn on or off the monitoring of one version of the specification
	virtual void toggleListening(listenTo version, bool on_or_off) = 0;

	//return which versions (draft, spec, all, none) we are listening to
	virtual int listeningTo() = 0;

	//sets the amount of time in milliseconds we will wait for a backup to 
	//appear after a source drops off line
	//returns the value being set on success and -1 on failure
	virtual int setUniversalHoldLastLook(int hold_time) = 0;

	//returns the amount of time in milliseconds we will wait for a backup 
	//to appear after a source drops off line 
	virtual int getUniversalHoldLastLook() = 0;
};

 
#endif

