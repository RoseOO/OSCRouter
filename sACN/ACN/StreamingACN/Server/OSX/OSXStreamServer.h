/*OSXStreamServer.h  OSX implementation IStreamACNSrv class.
*/

#ifndef _OSXSTREAMSERVER_H_
#define _OSXSTREAMSERVER_H_

#ifndef _OSX_STREAMACNSRVINTERFACE_H_
#error "#include error: OSXStreamServer.h requires OSXStreamACNSrvInterface.h"
#endif
#ifndef _STREAMSERVER_H_
#error "#include error: OSXStreamServer.h requires StreamServer.h"
#endif

class COSXStreamServer : public IOSXStreamACNSrv, public CStreamServer
{
public:
   /***************************************
	 * IOSX_StreamACNSrv overrides
	 ***************************************/
	//Call this to init the library after creation. Can be used right away (if it returns true)
	virtual bool Startup(IAsyncSocketServ* psocket);

	//Call this to de-init the library before destruction
	virtual void Shutdown();

	/****************************************
	 * IStreamACNSrv overrides
	 ****************************************/
	virtual int Tick(uint* dirtyhandles, uint hcount);  
	virtual bool CreateUniverse(const CID& source_cid, netintid* netiflist, int netiflist_size, 
				    const char* source_name, uint1 priority, uint2 reserved, uint1 options, uint1 start_code, 
							          uint2 universe, uint2 slot_count, uint1*& pslots, uint& handle,
										 bool ignore_inactivity_logic, uint send_intervalms);
	virtual void SetUniversesDirty(uint* handles, uint hcount);
	virtual void DestroyUniverse(uint handle);
	virtual void DEBUG_DESTROY_PRIORITY_UNIVERSE(uint handle);
	virtual void SendUniversesNow(uint* handles, uint hcount);
	virtual void DEBUG_DROP_PACKET(uint handle, uint1 decrement);
	virtual void OptionsPreviewData(uint handle, bool preview);
	virtual void OptionsStreamTerminated(uint handle, bool terminated);

	COSXStreamServer();  
	virtual ~COSXStreamServer();
						
protected:
	bool m_lockinitted;
	pthread_mutex_t m_lock;  //We'll do a simple mutex around the library calls.
	bool GetLock();
	void ReleaseLock();
};

#endif

