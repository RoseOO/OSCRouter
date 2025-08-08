// OSX_AsyncSocketServ.h: interface for Creating and controlling the 
// OSX-specific IAsyncSocketServ derivative class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _OSX_ASYNCSOCKETINTERFACE_H_
#define _OSX_ASYNCSOCKETINTERFACE_H_

//This class inherits the pure interface
#ifndef _ASYNCSOCKETINTERFACE_H_
#error "#include error: OSX_AsyncSocketInterface.h requires AsyncSocketInterface.h"
#endif

class IOSXAsyncSocketServ : public IAsyncSocketServ  
{
public:
	//Call this function to allocate an instance,
	//Then call Startup.
	//To destroy, call Shutdown and then call DestroyInstance.
	static IOSXAsyncSocketServ* CreateInstance();

	//Destroys the pointer.  Call Shutdown first
	static void DestroyInstance(IOSXAsyncSocketServ* p);

	//Startup and shutdown functions -- these should be called once directly from the app that
	//has the class instance
	virtual bool Startup() = 0;
	virtual void Shutdown() = 0;
};

#endif // !defined(_OSX_ASYNCSOCKETINTERFACE_H_)

