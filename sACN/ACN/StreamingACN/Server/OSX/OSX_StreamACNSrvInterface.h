// OSX_StreamACNSrvInterface.h: Interface for creating and controlling the OSX-specific
// StreamACNSrv derivative class.  See StreamACNSrvInterface.h for the main methods of use, but
// use the functions in this interface for creation and control.
//
//////////////////////////////////////////////////////////////////////

#if !defined(_OSX_STREAMACNSRVINTERFACE_H_)
#define _OSX_STREAMACNSRVINTERFACE_H_

#ifndef _STREAMACNSRVINTERFACE_H_
#error "#include error: OSX_StreamACNSrvInterface.h requires StreamACNSrvInterface.h"
#endif
#ifndef _ASYNCSOCKETINTERFACE_H_
#error "#include error: OSX_StreamACNSrvInterface.h requires AsyncSocketInterface.h"
#endif

class IOSXStreamACNSrv : public IStreamACNSrv
{
public:
	//The overall creator.  Call Startup on the returned pointer.
	//To clean up, call Shutdown on the pointer and call DestroyInstance;
	static IOSXStreamACNSrv* CreateInstance();

	//The overall destructor.  Call Shutdown before this function
	static void DestroyInstance(IOSXStreamACNSrv* p);

	//Call this to init the library after creation. Can be used right away (if it returns true)
	virtual bool Startup(IAsyncSocketServ* psocket) = 0;

	//Call this to de-init the library before destruction
	virtual void Shutdown() = 0;
};

#endif
