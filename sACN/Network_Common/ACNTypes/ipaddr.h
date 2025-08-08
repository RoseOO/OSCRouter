/********************************************************************************

  ipaddr.h

  Provides a standard definition of an ip address, useable for both version 4 and 6
  ip address.  

  Incorporates the concept of a network interface.
  
*********************************************************************************/

#ifndef _IPADDR_H_
#define _IPADDR_H_

#ifndef _DEFTYPES_H_
#error "#include error: ipaddr.h requires deftypes.h"
#endif

//The run-time identifier of a NIC interface, used wherever a NIC needs to be identified.  
typedef int netintid;
const netintid NETID_INVALID = -1;

typedef uint2 IPPort;  //The port number in HOST order
typedef uint4 IPv4;    //The integer representation of the ipv4 address in HOST order

class CIPAddr
{
public:
	enum {
		ADDRBYTES = 16,	//The number of bytes in a v6 address
		ADDRSTRINGBYTES =  60  //The maximum number of bytes in a ipaddress string, INCLUDING terminating NULL
	};
	
	CIPAddr();			//The default is an all zero address and port, invalid interface
	CIPAddr(netintid id, IPPort port, IPv4 addr);  //Construct from a port, v4 address, and interface
	CIPAddr(netintid id, IPPort port, const uint1* addr);  //Construct from a port, v6 address, and interface
	CIPAddr(const CIPAddr& addr);
	virtual ~CIPAddr();

	CIPAddr& operator=(const CIPAddr& addr);
	friend bool operator<(const CIPAddr& a1, const CIPAddr& a2);
	friend bool operator==(const CIPAddr& a1, const CIPAddr& a2);
	friend bool operator!=(const CIPAddr& a1, const CIPAddr& a2);

	void SetNetInterface(netintid id);
	netintid GetNetInterface() const;

	void SetIPPort(IPPort port);
	IPPort GetIPPort() const;

	bool IsV4Address() const;
	void SetV4Address(IPv4 addr);
	IPv4 GetV4Address() const;
	void SetV6Address(const uint1* addr);
	const uint1* GetV6Address() const;

	bool IsMulticastAddress() const;
	bool IsLinkLocalAddress() const;

	//Returns an address based on the string, which must be of one of the following forms:
	//d.d.d.d		for ipv4 address
	//d.d.d.d:d		for ipv4 address and port
	//d.d.d.d:d,d	for ipv4 address: port, network interface
	//[x:x:x:x:x:x:x:x]		for ipv6 address
	//[x:x:x:x:x:x:x:x]:d	for ipv6 address and port
	//[x:x:x:x:x:x:x:x]:d,d	for ipv6 address: port, network interface
	static CIPAddr StringToAddr(const char* ptext);

	//Translates an address to a preallocated text string of ADDRSTRINGBYTES, including the terminating NULL
	//Prints it in the forms listed for StringToAddr
	//Note that if showint is true, showport is assumed to be true
	static void AddrIntoString(const CIPAddr& addr, char* ptxt, bool showport, bool showint);


private:
	netintid m_netid;
	IPPort m_port;
	uint1 m_addr [ADDRBYTES];  //Address in big endian format -- upper bytes all 0's for ipv4
};

bool operator<(const CIPAddr& a1, const CIPAddr& a2);
bool operator==(const CIPAddr& a1, const CIPAddr& a2);
bool operator!=(const CIPAddr& a1, const CIPAddr& a2);

#endif


