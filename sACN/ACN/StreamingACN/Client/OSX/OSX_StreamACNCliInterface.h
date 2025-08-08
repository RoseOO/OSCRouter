// OSX_StreamACNCliInterface.h: Interface for creating and controlling the OSX-specific
// StreamACNCli derivative class.  See StreamACNCliInterface.h for the main methods of use, but
// use the functions in this interface for creation and control.
//
//////////////////////////////////////////////////////////////////////

#if !defined(_OSX_STREAMACNCLIINTERFACE_H_)
#define _OSX_STREAMACNCLIINTERFACE_H_

#ifndef _STREAMACNCLIINTERFACE_H_
#error "#include error: OSX_StreamACNCliInterface.h requires StreamACNCliInterface.h"
#endif
#ifndef _ASYNCSOCKETINTERFACE_H_
#error "#include error: OSX_StreamACNCliInterface.h requires AsyncSocketInterface.h"
#endif

class IOSXStreamACNCli : public IStreamACNCli
{
public:
	//The overall creator.  Call Startup on the returned pointer.
	//To clean up, call Shutdown on the pointer and call DestroyInstance;
	static IOSXStreamACNCli* CreateInstance();

	//The overall destructor.  Call Shutdown before this function
	static void DestroyInstance(IOSXStreamACNCli* p);

	//Call this to init the library after creation. Can be used right away (if it returns true)
	virtual bool Startup(IAsyncSocketServ* psocket, IStreamACNCliNotify* pnotify, listenTo verson = ALL) = 0;

	//Call this to de-init the library before destruction
	virtual void Shutdown() = 0;
};

#endif
