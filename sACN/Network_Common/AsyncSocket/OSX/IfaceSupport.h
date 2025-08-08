// IfaceSupport.h
// The OSX socket support may have several different implementations
// for I/O (threadpersocket, select, kqueue, etc).  This helper class provides
// the network interface utility functions that all these classes need to support
// for the AsyncSocket API.  It also contains the list of network interfaces, and
// some utility functions for the implementations.
//
//////////////////////////////////////////////////////////////////////

#ifndef _IFACESUPPORT_H_
#define _IFACESUPPORT_H_

#ifndef _ASYNCSOCKETINTERFACE_H_
#error "#include error: AsyncSocketServ.h requires AsyncSocketInterface.h"
#endif

class CIfaceSupport
{
public:
	CIfaceSupport();
	virtual ~CIfaceSupport();

	/////////////////////////////////////////////////////////////////
	// IAsyncSocketServ support functions

	//Returns the current number of network interfaces on the machine
	int GetNumInterfaces();

	//This function copies the list of network interfaces into a passed in array.
	//list MUST contain the necessary amount of memory (new [GetNumInterfaces()] netintinfo)
	//The interface numbers are only valid across this instance of the
	// async socket library,  To persist the selected interfaces,
	// use the ip address to identify the network interface across executions. 
	void CopyInterfaceList(IAsyncSocketServ::netintinfo* list);

	//Fills in the network interface info for a particular interface id
	//Returns false if not found
	bool CopyInterfaceInfo(netintid id, IAsyncSocketServ::netintinfo& info);
	
	//Returns the network interface that is used as the default
	netintid GetDefaultInterface();

	//Returns the network id (or NETID_INVALID) of the first network interface that
	//could communicate directly with this address (ignoring port and iface fields).
	//if isdefault is true, this was not directly resolveable and would go through 
	//the default interface
	netintid GetIfaceForDestination(const CIPAddr& destaddr, bool& isdefault);

	//Get the local address of a network interface
	//Only the address portion and network interface are filled in
	bool GetLocalAddress(netintid netid, CIPAddr& addr);
	
	//Get the ifindex of a local network interface
	bool GetIFIndex(netintid netid, int& ifindex);

	/////////////////////////////////////////////////////////////////
	//Functions for use by the socket implementations
	
	//Sets up the network interface list.  Must be called in the socket
	//implementation's Startup functionality.  Because there's a few
	//meaningful ways that this could fail, here's an enumeration:
	enum SETUP_RESULT
	{
		SETUP_OK, 	//Success!
		SETUP_BADFAMILY,  //Unsupported protocol family
		SETUP_NOROUTETABLE, //Couldn't get the routing table from the OS -- may not be an error if you don't care about gateways (but ACN does..)
		SETUP_BADIOCTL	//Couldn't execute the ioctl to enumerate the interfaces, or some other socket error
	};
	SETUP_RESULT SetUpIfaces();

	//This is the vector of network interfaces, as created on startup
	typedef std::vector<IAsyncSocketServ::netintinfo>::iterator ifaceiter;
	std::vector<IAsyncSocketServ::netintinfo> m_ifaces;
	netintid m_defaultiface;

	//Returns true if the mask of the two addresses are equal
	//Assumes items passed in are what is returned from CIPAddr.GetV6Address()
	bool MaskCompare(const uint1* addr1, const uint1* addr2, const uint1* mask);

	//Returns true if the "mask" address is all 0's (which would skew the Mask compare)
	bool MaskIsEmpty(const uint1* mask);

private:
	//A quick util to do the casting, etc to fill in the address of a CIPAddr
	void SetAddress(CIPAddr& addr, struct sockaddr* psa);
	
	//The real setup functionality for a particular protocol family
	SETUP_RESULT SetUpProtIfaces(int protocol);

	//Searches the routing table for a protocol family and sets the default gateway and
	//gateway fields of the ifaces in the pool
	bool FillInGateways(int protfamily);
	
	//Given a sockaddr, attempts to find the network interface in the list.
	//The sockaddr could be AF_INET, AF_INET6, or AF_LINK.  If the sockaddr is an
	//address, usenetmask determines whether or not both addrs are masked with the netmask
	//before the comparison
	ifaceiter FindIfaceBySa(struct sockaddr* sa, bool usenetmask);
};

#endif // !defined(_IFACESUPPORT_H_)


