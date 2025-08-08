// AsyncSocketServ_select.h: interface for the CAsyncSocketServ class.
//
//This class implements the OSX-Specific IAsyncSocketServ implementation
//of socket support that uses blocking sockets, with a thread that uses select
//to determine when sockets are ready for reading.
//////////////////////////////////////////////////////////////////////

#ifndef _ASYNCSOCKETSERV_H_
#define _ASYNCSOCKETSERV_H_

#ifndef _ASYNCSOCKETINTERFACE_H_
#error "#include error: AsyncSocketServ_select.h requires AsyncSocketInterface.h"
#endif
#ifndef _OSX_ASYNCSOCKETINTERFACE_H_
#error "#include error: AsyncSocketServ_select.h requires OSXAsyncSocketInterface.h"
#endif
#ifndef _SUBSCRIPTIONS_H_
#error "#include error: AsyncSocketServ_select.h requires Subscriptions.h"
#endif
#ifndef _READER_WRITER_H_
#error "#include error: AsyncSocketServ_select.h requires ReaderWriter.h"
#endif
#ifndef _MEMPOOL_H_
#error "#include error: AsyncSocketServ_select.h requires MemPool.h"
#endif
#ifndef _IFACESUPPORT_H_
#error "#include error: AsyncSocketServ_select.h requires IfaceSupport.h"
#endif


class CAsyncSocketServ : public IOSXAsyncSocketServ
{
public:
	CAsyncSocketServ();
	virtual ~CAsyncSocketServ();

	////////////////////////////////////////////////////////////////
	// IOSXAsyncSocketServ functions

	//Startup and shutdown functions -- these should be called once directly from the app that
	//has the class instance
	virtual bool Startup();
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

private:
	/*OSX lion changed the way mcast worked.  Before it was like Linux, where you receive messages even if you haven't subscribed.  
	 Now it's mor like Windows, where only sockets that have subscribed will receive the packet*/
	bool lion_or_better;  
	
	CIfaceSupport m_ifs;

	//The socket chain is simply map of sockets, sorted by id.
	//TODO: Perhaps some sort of lookup table for address usage as well??
	struct socketref
	{
      bool m_ismanual;		//Whether or not this is a manual-read socket
      bool m_standalone;		//Whether or not this socket is a standalone socket.
      int m_Socket; 			//The actual socket we'll use
      sockid m_id;            //Socket ID for callback parameter
      bool m_Connected;
      CIPAddr boundaddr;		//The address we're bound/subscribed to
      uint readsize;			//The size of the read buffer
      CSubscriptions sublist;  //If Lion, the subscription list.  If !lion, subscriptions are actually handled/tracked by the special subscription list, 
								 //but we need to know what this socket thinks it subscribed to.  
		                         //This member is only used to limit the subscription count in lion or better.
      IAsyncSocketClient* m_sockcb;   //Pointer to callbacks for notifications

      socketref() : m_ismanual(false), m_standalone(false), m_Socket(0), m_id(SOCKID_INVALID),
                      m_Connected(false), boundaddr(), readsize(0),
                      sublist(), m_sockcb(NULL)
      {
      }
	};
    // The socket map contains socketref structures and is indexed by socket id.
	typedef std::map<sockid, socketref>::iterator sockiter;
	typedef std::pair<sockid, socketref> sockval;
	std::map<sockid, socketref> m_sockmap;
	
	//If we're not using lion or better:
	//In addition to the actual socket map, there's a multimap of special sockets used only
	//to control subscriptions, the are never read or sent from, nor are they ever bound to a port
	//This is to get around the fact that OSX treats multicast like a spigot,
	//and any socket that is bound to a port will receive the mcast regardless of whether
	//or not it subscribed.  The only way around this would be to bind to the mcast address,
	//but that doesn't scale well -- we need to get as many subscriptions per socket as possible.
	//Therefore, we use this list to get around the IP_MAX_MEMBERSHIPS constant -- having a max
	//yet everyone receives everything doesn't seem to scale :)
	struct subref
	{
		int sockfd;
		netintid sockiface; 
		CSubscriptions subaddrs;    //Handles reference counting of multicast subscription
		subref(): sockfd(-1), subaddrs() {}
	};
	typedef std::vector<subref>::iterator subiter;
	std::vector<subref> m_submap;
	
    //There's a lock around the sockets map and the subscription map
	CXReadWriteLock m_socklock;

	sockid m_id;  //The next id to assign to a socket

	bool m_terminated;
	pthread_t m_task_id;
		
	int m_readsize;  //The current size of the read buffer (starts at MTU)
	uint1* m_readbuffer;  //The readbuffer, allocated to m_readsize
	bool AssureReadSize(int size); //Re-allocates read buffer to larger size

	CMemPool m_recvpool;  //The pool of buffers used for socket receives (at MTU);
	
	//Only one instance of this thread does the epolling, and does a
	//read lock on the sockmap
	friend void* AsyncSocket_ReadThread(void* psrv);

	//The real socket creation function -- whether or not we are doing unicast
	//affects what address we bind to, along with whether or not mcastbind is NULL.
	bool RealCreateSocket(bool unicast, IAsyncSocketClient* pnotify,
										netintid netid,
										sockid& newsock,
										IPPort& port,
										uint maxdatasize,
										bool manual_recv,
										CIPAddr* mcast_bind);  //If !null, this is a standalone socket bound to the mcast address.

	/*All of these helper functions assume a write lock, or on a read lock where
	 * nothing is accessing the same socket (since this stuff is in a map)
	 */
	int SubscribeSocketAddSubscription(const CIPAddr& maddr, bool& dosocksubscribe); //Only used if we're not doing Lion or better
	int SubscribeSocketRemoveSubscription(const CIPAddr& maddr);       //Only used if we're not doing Lion or better.
	int SocketConnect(socketref* pref, bool unicast, CIPAddr* mcast_bind);
	int SocketDisconnect( socketref* pref );
	int SocketUnicastBind(socketref* pref, bool isv4);
	int SocketMcastBind(int sockfd, bool isv4, IPPort port, CIPAddr* mcast_bind);
	bool SocketStartAutoRecv(socketref* pref); 
	int SocketRead(socketref* pref);
	
	/*Versions of the "get" functions that assume the lock has already been placed and an iterator
	 * found -- will check the iterator for validity and active before doing their functionality*/
	bool safe_RoomForSubscribe(sockiter it, const CIPAddr& addr);
	bool safe_IsSubscribed(sockiter it, const CIPAddr& addr);
	bool safe_GetBoundAddress(sockiter it, CIPAddr& addr);
	uint safe_GetMTU(sockiter it);
	int safe_GetOSIndex(sockiter it);
	bool safe_IsV6(sockiter it);
};


#endif // !defined(_ASYNCSOCKETSERV_H_)


