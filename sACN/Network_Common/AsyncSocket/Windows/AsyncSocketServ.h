// AsyncSocketServ.h: interface for the CAsyncSocketServ class.
//
// This class implements the Windows-Specific IAsyncSocketServ implementation, 
// and makes a lot of use of Winsock2 functionality for better windows performance.

//	TODO: IPV6 support.  Windows didn't fully support it at the time of this writing
//         iphlpapi, for example, is still IPv4 only.
//
//////////////////////////////////////////////////////////////////////

#ifndef _ASYNCSOCKETSERV_H_
#define _ASYNCSOCKETSERV_H_

#ifndef _ASYNCSOCKETINTERFACE_H_
#error "#include error: AsyncSocketServ.h requires AsyncSocketInterface.h"
#endif
#ifndef _WIN_ASYNCSOCKETINTERFACE_H_
#error "#include error: AsyncSocketServ.h requires WinAsyncSocketInterface.h"
#endif
#ifndef _READER_WRITER_H_
#error "#include error: AsyncSocketServ.h requires readerwriter.h"
#endif
#ifndef _SUBSCRIPTIONS_H_
#error "#include error: AsyncSocketServ.h requires subscriptions.h"
#endif
#ifndef _MEMPOOL_H_
#error "#include error: AsyncSocketServ.h requires MemPool.h"
#endif

//This structure is used for all overlapped reads
struct myoverlap
{
	WSAOVERLAPPED over;

	//Our state fields
	WSABUF  buffer;				//The buffer for the operation.
	sockid  sockid;		//The library socket id we are
	SOCKET sock;			//the real socket to use for the WSARecvFrom
	sockaddr_in fromaddr;			//The address the message was received from
	int		  fromaddrlen;			//The length of addr
	DWORD	  rcvflags;			//Used when receiving
};

//This structure is used by the socket collective to represent a real socket
struct socketref
{
	bool active;			//If false, the socket is dying
	SOCKET sock;			//The actual socket we'll use
	uint MTU;				//The max data size
	CIPAddr boundaddr;		//The address we're bound to
	IAsyncSocketClient* pnotify;  //The notification interface
	CSubscriptions subs;	//The list of subscriptions this socket is on
};

class CAsyncSocketServ : public IWinAsyncSocketServ  
{
public:
	CAsyncSocketServ();
	virtual ~CAsyncSocketServ();

	////////////////////////////////////////////////////////////////
	// IWinAsyncSocketServ functions

	//Startup and shutdown functions -- these should be called once directly from the app that
	//has the class instance
	virtual bool Startup(int threadpriority);
	virtual void Shutdown();


	/////////////////////////////////////////////////////////////////
	// IAsyncSocketServ functions

	//If this returns true, any socket that binds to a port will receive the mcast message
	//if even only one of the sockets subscribed on that network interface.
	virtual bool McastMessagesSharePort();
	
	//If this returns true, any socket that binds to a port will receive the mcast message,
	//even if the one that subscribed was on a different network interface (as long as 
	//some socket on that interface subscribed).
	virtual bool McastMessagesIgnoreSubscribedIface(); 

	//Returns the current number of network interfaces on the machine
	virtual int GetNumInterfaces();

	//This function copies the list of network interfaces into a passed in array.
	//list MUST contain the necessary amount of memory (new [GetNumInterfaces()] netintinfo)
	//The interface numbers are only valid across this instance of the
	// async socket library,  To persist the selected interfaces,
	// use the ip address to identify the network interface across executions. 
	virtual void CopyInterfaceList(netintinfo* list);

	//Fills in the network interface info for a particular interface id
	//Returns false if not found
	virtual bool CopyInterfaceInfo(netintid id, netintinfo& info);
	
	//Returns the network interface that is used as the default
	virtual netintid GetDefaultInterface();

	//Returns the network id (or NETID_INVALID) of the first network interface that
	//could communicate directly with this address (ignoring port and iface fields).
	//if isdefault is true, this was not directly resolveable and would go through 
	//the default interface
	virtual netintid GetIfaceForDestination(const CIPAddr& destaddr, bool& isdefault);

	//Invalidates packet memory, possibly getting it ready for reuse
	//buflen is the size of the total buffer memory, NOT just the packet size.
	//Call this when you are finished processing the packet from ReceivePacket
	virtual void DeletePacket(uint1* pbuffer, uint buflen);

	//This is the preferred method of multicast socket creation, as it attempts to share
	//sockets across subscriptions.  If this interferes with your needs, 
	//use CreateSingleMulticastSocket().
	//Creates a multicast socket, binding to the correct port and network interface.
	//This does not subscribe to an address, call SubscribeMulticast instead.
	//if port == 0, a random port is assigned and a new socket is automatically
	//generated.
	//If manual_recv is true, the user must call ReceiveInto to receive any data,
	//as the ReceivePacket notification is not used.
	//On success, newsock is filled in.
	virtual bool CreateMulticastSocket(IAsyncSocketClient* pnotify,
									   netintid netid,
									   sockid& newsock,
									   IPPort& port, 
									   uint maxdatasize,
									   bool manual_recv);
										
	//In instances where McastMessagesSharePort or McastMessagesIgnoreSubscribedIface,
	//it may not be adviseable to share subscriptions if multiple protocols will be using
	//that port.  In the case of sACN vs. ACN, while the protocol code does filter out
	//invalid packets the packets have to get all the way to that level before they are
	//filtered.  This needs to be balanced with scaling needs, however, so while sACN will
	//most likely use this function, ACN will not.
	//This function creates a multicast socket for the explicit use of a particular multicast
	//address, and will only receive messages for that address. It also immediately subscribes
	//to the address. Otherwise it works like the previous function. 
	//The mcast address must be completely filled in, port must not be 0.
	virtual bool CreateStandaloneMulticastSocket(IAsyncSocketClient* pnotify,
												const CIPAddr& maddr,
												sockid& newsock,
												uint maxdatasize,
												bool manual_recv);


	//There may be a system-determined limit for the number of multicast
	//addresses that can be subscribed to by the same socket.  Call this
	//function to determine if there is room avaliable (or if
	//SubscribeMulticast return false).  If the addr passed in is one the
	//socket is already subscribed to, this returns true -- you can always
	//keep subscribing to the same socket, as it just refcounts internally.
	virtual bool RoomForSubscribe(sockid id, const CIPAddr& addr);

	//Returns true if the socket is already subscribed to the address
	virtual bool IsSubscribed(sockid id, const CIPAddr& addr);

	//Subscribes a multicast socket to a multicast address.  
	//It will return false on error, or if the maximum number of subscription
	//addresses has been reached.  If McastMessagesIgnoreSubscribedIface is false,
	//the Network interface must match the interface the socket is bound to.  If
	//McastMessagesIgnoreSubscribedIface is true, this function turns on the 
	//multicast "spigot" for this iface.  An iface of NETID_INVALID turns on multicast
	//for all network interfaces.
	virtual bool SubscribeMulticast(sockid id, const CIPAddr& addr);

   //Unsubscribes a multicast socket from a multicast address.  
   //Returns true if the unsubscribe actually ocurred (otherwise, the
   //reference count was just lowered).
   //Note that on some platforms, if you unsubscribe you can't reuse that
   //socket to subscribe to another address.  can_reuse will be set to 
   //false in that situation (can_reuse is set whatever this function returns).
   //If you can't reuse and the unsubscribe ocurred, you might as well destroy the socket.
   //UnsubscribeMulticast follows the same rules as SubcribeMulticast with respect
   //to the network interface of addr.
   virtual bool UnsubscribeMulticast(sockid id, const CIPAddr& addr, bool& can_reuse);

	//Creates, sets up, and binds a listening unicast socket.
	//If port == 0, a random port is assigned.
	//If manual_recv is true, the user must call ReceiveInto to receive any data,
	//as the ReceivePacket notification is not used.
	//On success, newsock is filled in
	virtual bool CreateUnicastSocket(IAsyncSocketClient* pnotify,
									    netintid netid,
										sockid& newsock,
										IPPort& port,
										uint maxdatasize,
										bool manual_recv);

	//Destroys the socket if all references have been destroyed.
	//All messages for that socket will be sent before destruction
	//The socket id should no longer be used for that socket, and may be reused on another create.
	virtual void DestroySocket(sockid id);

	//This function is used for manual_recv sockets.  It does a blocking recvfrom and returns
	//the number of bytes received into pbuffer (or < 0 for the recvfrom socket errors). It also fills in from.
	virtual int ReceiveInto(sockid id, CIPAddr& from, uint1* pbuffer, uint buflen);

	//Returns the local address of the machine through a socket id (which handles network interface as well)
	//Only the address portion and network interface are filled in
	virtual bool GetLocalAddress(sockid sock, CIPAddr& addr);

	//Get the local address of a network interface
	//Only the address portion and network interface are filled in
	virtual bool GetLocalAddress(netintid netid, CIPAddr& addr);

	//Gets the bound address of the socket or an empty address
	virtual bool GetBoundAddress(sockid sock, CIPAddr& addr);

	//Returns the MTU for this socket -- remember that this was set on creation 
	virtual uint GetMTU(sockid sock);

	//Returns the OS interface index 
	//Returns -1 if the index is unknown.
	virtual int GetOSIndex(sockid sock);

	//Returns whether or not this socket is on a v6 network
	virtual bool IsV6(sockid sock);

	//Sends the packet to a particular address.
	//pbuffer is considered INVALID by this library after this function
	//returns, as the calling code may reuse or delete it.
	//If there is an error, this will only trigger a SocketBad notification
	//if error_is_failure is true.  Note that in many cases (like SDT), an error on
	//sending should actually be ignored.
	virtual void SendPacket(sockid id, const CIPAddr& addr, const uint1* pbuffer, uint buflen, bool error_is_failure);

	//The version that takes async_chunk instead of a straight buffer.  The etcchunk
	//chain should be considered INVALID by this library after this function returns,
	//as the calling code may reuse or delete it.
	virtual void SendPacket(sockid id, const CIPAddr& addr, const async_chunk* chunks, bool error_is_failure);

	HANDLE m_iocp;  //The actual I/O completion port -- exposed, since only the worker threads access the class directly
	bool m_shuttingdown;  //True if Shutdown has been called.  Used by the worker threads to stop notifications
	
	//Used by the worker threads to signal that a socket has been detected as closed
	//IOCP gets a message queued (with no bytes read) when the socket closes/shuts down
	void WorkerDetectClose(sockid id);

	//Used by the worker threads to signal that a socket has received data
	void NotifyReceivePacket(sockid id, struct sockaddr_in* fromaddr, uint1* pbuffer, uint numread);

	//The memory pool used for the packet buffers
	CMemPool m_pool;

private:
	//This is the vector of network interfaces, as created in InitNICList()
	typedef std::vector<netintinfo>::iterator ifaceiter;
	std::vector<netintinfo> m_ifaces;
	netintid m_defaultiface;

	//The socket collective is protected by a read/write lock
	//Only socket creation/destruction garners a write lock
	CXReadWriteLock m_socklock;

	//The socket collective is simply map of sockets and info, sorted by id.
	typedef std::map<sockid, socketref*>::iterator sockiter;
	typedef std::pair<sockid, socketref*> sockval;
	std::map<sockid, socketref*> m_sockmap;

	sockid m_newid;  //The id to assign to the next created socket -- This is also protected by the write lock

	//This class uses IO Completion ports for sockets, so we need work threads
	DWORD m_numthreads;
	std::set<HANDLE> m_threadset;

	/*Helper functions*/
	//Initializes the static list of network interfaces
	//Also inits the default gateway, since I don't store true windows interface ids.
	bool InitNICList(); 

	//Returns true if the mask of the two addresses are equal
	//Assumes items passed in are what is returned from CIPAddr.GetV6Address()
	bool MaskCompare(const uint1* addr1, const uint1* addr2, const uint1* mask);

	//Returns true if the "mask" address is all 0's (which would skew the Mask compare)
	bool MaskIsEmpty(const uint1* mask);

	//Socket destruction happens in two phases in pretty much every situation
	//These assume a Write lock has already been grabbed.
	void InitiateDestruction(sockiter& sockit, bool notify);  //Shuts the socket down, but doesn't remove.  May notify the app
	void FinalizeDestruction(sockiter& sockit);  //Removes the socket from the map, etc.

	/*Socket functions operate on the sockets and assume the proper lock is on the 
	  socketmap, if needed.  All of these functions ignore a socket of INVALID_SOCKET*/

	//Creates the socket, but doesn't actually start the read cycle
	//Fills in both parameters with the new socket and bound port
	//at least the bound address and netid should be non 0, but the port can be for automatic client
	bool SocketCreate(SOCKET& sock, CIPAddr& boundaddr);

	//Starts the socket and the read cycle
	bool SocketStart(socketref* psocket, sockid id);

	//Closes the socket and sets sock to INVALID_SOCKET
	void SocketDestroy(SOCKET& sock);

	//Socket level subscription
	bool SocketSubscribe(SOCKET sock, const CIPAddr& subaddr, const CIPAddr& boundaddr);

	//Socket level unsubscription
	void SocketUnsubscribe(SOCKET sock, const CIPAddr& subaddr, const CIPAddr& boundaddr);

	//Does a single SendTo
	bool SocketSendTo(SOCKET sock, const CIPAddr& addr, const uint1* pbuffer, uint buflen);
	
	//Starts the Recv process on a socket
	bool SocketStartRecv(socketref* psocket, sockid id);

	/*Versions of the "get" functions that assume the lock has already been placed and an iterator
	found -- will check the iterator for validity and active before doing their functionality */
	bool safe_RoomForSubscribe(sockiter it, const CIPAddr& addr);
	bool safe_IsSubscribed(sockiter it, const CIPAddr& addr);
	bool safe_GetBoundAddress(sockiter it, CIPAddr& addr);
	uint safe_GetMTU(sockiter it);
	//int safe_GetOSIndex(sockiter it);
	//bool safe_IsV6(sockiter it);


};

#endif // !defined(_ASYNCSOCKETSERV_H_)
