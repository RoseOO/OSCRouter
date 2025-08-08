// Win_AsyncSocketServ.h: interface for Creating and controlling the 
// windows-specific IAsyncSocketServ derivative class.
//
//////////////////////////////////////////////////////////////////////

#ifndef _WIN_ASYNCSOCKETINTERFACE_H_
#define _WIN_ASYNCSOCKETINTERFACE_H_

//This class inherits the pure interface
#ifndef _ASYNCSOCKETINTERFACE_H_
#error "#include error: Win_AsyncSocketInterface.h requires AsyncSocketInterface.h"
#endif

class IWinAsyncSocketServ : public IAsyncSocketServ  
{
public:
	//Call this function to allocate an instance,
	//Then call Startup.
	//To destroy, call Shutdown and then call DestroyInstance.
	static IWinAsyncSocketServ* CreateInstance();

	//Destroys the pointer.  Call Shutdown first
	static void DestroyInstance(IWinAsyncSocketServ* p);

	//Startup and shutdown functions -- these should be called once directly from the app that
	//has the class instance
	virtual bool Startup(int threadpriority = THREAD_PRIORITY_NORMAL) = 0;
	virtual void Shutdown() = 0;
};

#endif // !defined(_WIN_ASYNCSOCKETINTERFACE_H_)
