// AsyncSocketInterface.h: interface for the IAsyncSocketServ class.
//
//*****************************************************************
//This is a pure interface -- Use the platform specific interface to 
//actually create and control the AsyncSocketServ class instance.
//*****************************************************************
//
//	The IAsyncSocketServ-derived class is used by the application to 
//	receive send and receive UDP packets.  The application registers itself 
//  through this interface as being interested in the recv'd packets, etc.
//
//	Multicast sockets may only be bound to one port but subscribed to 
//  more than one multicast address

//	All IAsyncSocketClient notifications MUST be buffered and handled asynchronously.

//	TODO: Add CDL/CDI support for registering the network errors.

//	This interface assumes that the socket library was already started and initialized,
//  probably by the application.
//
//////////////////////////////////////////////////////////////////////
#ifndef _ASYNCSOCKETINTERFACE_H_
#define _ASYNCSOCKETINTERFACE_H_

//This header file depends on the following header files:
//ipaddr.h
//
#ifndef _IPADDR_H_
#error "#include error: AsyncSocketInterface.h needs ipaddr.h"
#endif

//The socket identifier, used whenever communicating via these interface.
//Socket ids are unique for all sockets being served.  A socket is 
//created on a particular network interface.
typedef uint4 sockid;

const uint4 SOCKID_INVALID = 0xffffffff;

//A small structure used for sending blobs of data in chains
struct async_chunk
{
	const uint1* pbuf;
	uint len;
	struct async_chunk* pnext;
};

//////////////////////////////////////////////////////////////////////////////
// The Socket Client interface -- These must be handled buffered and handled
// asynchronously
class IAsyncSocketClient
{
public:
   virtual ~IAsyncSocketClient(){}

	//This function processes the complete packet that came in on a socket.
	//The buffer is now under the ownership of the callback.
	//Call DeletePacket to free the packet.
	virtual void ReceivePacket(sockid id, const CIPAddr& from, uint1* pbuffer, uint buflen) = 0;

	//This is the notification that a socket has gone bad/closed.
	//After this call, the socket is considered disconnected, but not
	//invalid... Call DestroySocket to remove it.
	virtual void SocketBad(sockid id) = 0;
};

//////////////////////////////////////////////////////////////////////////////
// The Socket Server interface
class IAsyncSocketServ  
{
public:
   virtual ~IAsyncSocketServ(){}
	///////////////////////////////////////////////////////////////////////////////
	// Multicast Capabilities.
	//
	// Each platform has slightly different characteristics as to how sockets that
	// are bound to the same port receive multicast packets.  The Windows implementation,
	// for example, only passes on mcast packets to a socket that has a subscription,
	// and the sockets may be bound to a network interface to further filter based on iface.
	// Linux on the other hand treats subscriptions as a spigot to the process -- when
	// a subscription occurs for a socket on an iface, any sockets bound to the same port
	// will get the mcast message, regardless if they subscribed.  While this spigot is per
	// network interface, if two subscribed sockets bound to the same port are on different
	// network interfaces they will each see packets for both interfaces.
	
	//If this returns true, any socket on a network interface that binds to a port
   //will receive the mcast message sent to that port, even if only one of the sockets 
   //had subscribed
	virtual bool McastMessagesSharePort() = 0;
	
	//If this returns true, any socket that binds to a port will receive the mcast messages
   //sent to that port, even if the subscribing socket was on a separate interface.
   //When this is true, SubscribeMulticast will allow the network interface to be something
   //other than what the socket is bound to.
	virtual bool McastMessagesIgnoreSubscribedIface() = 0; 

	///////////////////////////////////////////////////////////////////////////////
	// Network interface support

	//Network interfaces are only detected upon library startup, as new interfaces coming
	// on-line could confuse the libraries and applications.

	static const int NETINTID_STRLEN = 150;
	static const int NETINTID_MACLEN = 6;
	
	//This is the structure used to describe a network interface
	//Note that none of the CIPAddrs in this structure have an associated
	//network interface ID or IP Port!!
	struct netintinfo
	{
		netintid id;	//The network interface identifier to be used
		int ifindex;  //The OS-specific network interface number.  Not used on all OSes.  May need to be cast'd away from int (e.g. uint) on your OS.
		CIPAddr addr;	//The interface ip address (the real identifier)
		CIPAddr mask;	//The ip mask for this interface
		CIPAddr gate;	//The ip address of the gateway for this interface
		char name [NETINTID_STRLEN];	//The adapter name as a string (on windows, this is a UUID)
		char desc [NETINTID_STRLEN];    //The adapter description as a string
		char mac  [NETINTID_MACLEN];	//The adapter MAC address
	};

	//Returns the current number of network interfaces on the machine
	virtual int GetNumInterfaces() = 0;

	//This function copies the list of network interfaces into a passed in array.
	//list MUST contain the necessary amount of memory (new [GetNumInterfaces()] netintinfo)
	//The interface numbers are only valid across this instance of the
	// async socket library,  To persist the selected interfaces,
	// use the ip address to identify the network interface across executions. 
	virtual void CopyInterfaceList(netintinfo* list) = 0;

	//Fills in the network interface info for a particular interface id
	//Returns false if not found
	virtual bool CopyInterfaceInfo(netintid id, netintinfo& info) = 0;

	//Returns the network interface that is used as the default
	virtual netintid GetDefaultInterface() = 0;

	//Returns the network id (or NETID_INVALID) of the first network interface that
	//could communicate directly with this address (ignoring port and iface fields).
	//if isdefault is true, this was not directly resolveable and would go through 
	//the default interface
	virtual netintid GetIfaceForDestination(const CIPAddr& destaddr, bool& isdefault) = 0;

	///////////////////////////////////////////////////////////////////////////////
	// Socket commands to process

	//Invalidates packet memory, possibly getting it ready for reuse
	//buflen is the size of the total buffer memory, NOT just the packet size.
	//Call this when you are finished processing the packet from ReceivePacket
	virtual void DeletePacket(uint1* pbuffer, uint buflen) = 0;

	//This is the preferred method of multicast socket creation, as it attempts to share
	//sockets across subscriptions.  If this interferes with your needs, 
	//use CreateStandaloneMulticastSocket().
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
										bool manual_recv = false) = 0;
	
	//In instances where McastMessagesSharePort or McastMessagesIgnoreSubscribedIface,
	//it may not be adviseable to share subscriptions if multiple protocols will be using
	//that port.  In the case of sACN vs. ACN, while the protocol code does filter out
	//invalid packets the packets have to get all the way to that level before they are
	//filtered.  This needs to be balanced with scaling needs, however, so while sACN will
	//possibly use this function, ACN will not.
	//This function creates a multicast socket for the explicit use of a particular multicast
	//address, and will only receive messages for that address. It also immediately subscribes
	//to the address. Otherwise it works like the previous function. 
	//The mcast address must be completely filled in, port must not be 0.
   //Note: on most platforms sockets created with this API must NOT be used for sending. This
   //is because binding to the multicast address may cause packets being sent from this socket
   //to have the multicast address as the "from" address!
	virtual bool CreateStandaloneMulticastSocket(IAsyncSocketClient* pnotify,
												const CIPAddr& maddr,
												sockid& newsock,
												uint maxdatasize,
												bool manual_recv = false) = 0;
											 
	
	//There may be a system-determined limit for the number of multicast
	//addresses that can be subscribed to by the same socket.  Call this
	//function to determine if there is room avaliable (or if
	//SubscribeMulticast return false).  If the addr passed in is one the
	//socket is already subscribed to, this returns true -- you can always
	//keep subscribing to the same socket, as it just refcounts internally.
	virtual bool RoomForSubscribe(sockid id, const CIPAddr& addr) = 0;
	
	//Returns true if the socket is already subscribed to the address
	virtual bool IsSubscribed(sockid id, const CIPAddr& addr) = 0;

	//Subscribes a multicast socket to a multicast address.  
	//It will return false on error, or if the maximum number of subscription
	//addresses has been reached.  If McastMessagesIgnoreSubscribedIface is false,
	//the Network interface must match the interface the socket is bound to.  If
	//McastMessagesIgnoreSubscribedIface is true, this function turns on the 
	//multicast "spigot" for this iface (and in this case, an iface of NETID_INVALID
	//turns on multicast for all network interfaces.
	virtual bool SubscribeMulticast(sockid id, const CIPAddr& addr) = 0;

	//Unsubscribes a multicast socket from a multicast address.  
	//Returns true if the unsubscribe actually ocurred (otherwise, the
	//reference count was just lowered).
	//Note that on some platforms, if you unsubscribe you can't reuse that
	//socket to subscribe to another address.  can_reuse will be set to 
	//false in that situation (can_reuse is set whatever this function returns).
   //If you can't reuse and the unsubscribe ocurred, you might as well destroy the socket.
	//UnsubscribeMulticast follows the same rules as SubcribeMulticast with respect
	//to the network interface of addr.
	virtual bool UnsubscribeMulticast(sockid id, const CIPAddr& addr, bool& can_reuse) = 0;

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
										bool manual_recv = false) = 0;

	//Destroys the socket.
	//All messages for that socket will be sent before destruction
	//The socket id should no longer be used for that socket, and may be reused on another create.
	virtual void DestroySocket(sockid id) = 0;

	//This function is used for manual_recv sockets.  It does a blocking recvfrom and returns
	//the number of bytes received into pbuffer (or < 0 for the recvfrom socket errors). It also fills in from.
	virtual int ReceiveInto(sockid id, CIPAddr& from, uint1* pbuffer, uint buflen) = 0;

	//Returns the local address of the machine through a socket id (which handles network interface as well)
	//Only the address portion and network identifier are filled in
	virtual bool GetLocalAddress(sockid sock, CIPAddr& addr) = 0;

	//Or, directly get the local address of a network interface
	// Only the address portion and network identifier are filled in.
	virtual bool GetLocalAddress(netintid netid, CIPAddr& addr) = 0;

	//Gets the bound address of the socket or an empty address
	virtual bool GetBoundAddress(sockid sock, CIPAddr& addr) = 0;

	//Returns the MTU for this socket -- remember that this was set on creation 
	virtual uint GetMTU(sockid sock) = 0;

	//Returns whether or not this socket is on a v6 network
	virtual bool IsV6(sockid sock) = 0;

	//Sends the packet to a particular address.  
	//pbuffer is considered INVALID by this library after this function
	//returns, as the calling code may reuse or delete it.
	//If there is an error, this will only trigger a SocketBad notification
	//if error_is_failure is true.  Note that in many cases (like SDT), an error on 
	//sending should actually be ignored.
	virtual void SendPacket(sockid id, const CIPAddr& addr, const uint1* pbuffer, uint buflen, bool error_is_failure = false) = 0;
	
	//The version that takes async_chunks instead of a straight buffer.  The Asyncchunk
	//chain should be considered INVALID by this library after this function returns,
	//as the calling code may reuse or delete it.
	virtual void SendPacket(sockid id, const CIPAddr& addr, const async_chunk* chunks, bool error_is_failure = false) = 0;

	//TODO: CDI/CDL!!!, especially logging why startup and socket creation/subscription failed!!
};

#endif // !defined(_ASYNCSOCKETINTERFACE_H_)
