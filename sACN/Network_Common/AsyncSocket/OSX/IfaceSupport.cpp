// AsyncSocketServ_IfaceSupport.cpp
//
// The intent of the OSX AsyncSocketServ source is to provide a few 
// different implementations of socket IO.  This file contains all the
// AsyncSocketServ functions pertaining to general browsing of the 
// network ifaces, etc., that doesn't need to be copied between IO files.
// This file assumes that these functions all live within a CAsyncSocket class.
//////////////////////////////////////////////////////////////////////

#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <sys/sysctl.h>

#include "deftypes.h"
#include "ipaddr.h"
#include "AsyncSocketInterface.h"
#include "IfaceSupport.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIfaceSupport::CIfaceSupport()
{
	m_defaultiface = NETID_INVALID;
}

CIfaceSupport::~CIfaceSupport()
{
}

////////////////////////////////////////////////////////////////
// IAsyncSocketServ functions

//A quick util to do the casting, etc to fill in the address of a CIPAddr
void CIfaceSupport::SetAddress(CIPAddr& addr, struct sockaddr* psa)
{
	if(AF_INET == psa->sa_family)
		addr.SetV4Address(ntohl(((struct sockaddr_in*)(psa))->sin_addr.s_addr));
	else if(AF_INET6 == psa->sa_family)
		addr.SetV6Address(((struct sockaddr_in6*)(psa))->sin6_addr.s6_addr);
	else
		addr.SetV4Address(0);	
}

//The real setup functionality for a particular protocol family -- does the same thing but filters based on protocol
CIfaceSupport::SETUP_RESULT CIfaceSupport::SetUpProtIfaces(int protocol)
{
	/*TODO: Validate IPv6 support!!!*/

	//We only currently support ip and ipv6
	if((protocol != AF_INET) && (protocol != AF_INET6))
		return SETUP_BADFAMILY;
		
	SETUP_RESULT result = SETUP_OK;

	struct ifaddrs* paddrs;
	if(0 != getifaddrs(&paddrs))
		return SETUP_BADIOCTL;
	
	//Step through the list, filtering out addresses for our protocol.  The Mac gives an AF_LINK
	//address before the other protocol addresses for that interface.
	char* link_name;
	struct sockaddr_dl * link_addr;
	
	while (paddrs) 
	{
		bool useaddr = false;
		
		//Filter out loopback and down interfaces immediately
		if(!(paddrs->ifa_flags & IFF_LOOPBACK) && (paddrs->ifa_flags & IFF_UP))
		{
			switch (paddrs->ifa_addr->sa_family)
			{
				case AF_LINK:
					link_name = paddrs->ifa_name;
					link_addr = (struct sockaddr_dl*)(paddrs->ifa_addr);
					break;
				case AF_INET:
					if(AF_INET == protocol)
						useaddr = true;
					break;
				case AF_INET6:
					if(AF_INET6 == protocol)
						useaddr = true;
					break;
				default:
					//Just ignore the interface
					break;				
			}
		}

		//Create an interface record and pop it in the list if we have everything we need
		if(useaddr)
		{
			IAsyncSocketServ::netintinfo info;
			
			strncpy(info.name, paddrs->ifa_name, IAsyncSocketServ::NETINTID_STRLEN -1);
			info.name[IAsyncSocketServ::NETINTID_STRLEN-1] = '\0';
			strncpy(info.desc, paddrs->ifa_name, IAsyncSocketServ::NETINTID_STRLEN -1);
			info.desc[IAsyncSocketServ::NETINTID_STRLEN-1] = '\0';
			
			if(0 == strncmp(paddrs->ifa_name, link_name, IAsyncSocketServ::NETINTID_STRLEN))
			{
				info.ifindex = link_addr->sdl_index;
				if(IAsyncSocketServ::NETINTID_MACLEN == link_addr->sdl_alen)
					memcpy(info.mac, link_addr->sdl_data + link_addr->sdl_nlen, IAsyncSocketServ::NETINTID_MACLEN);
			}
			
			SetAddress(info.addr, paddrs->ifa_addr);
			SetAddress(info.mask, paddrs->ifa_netmask);
			
			info.id = m_ifaces.size();
			m_ifaces.push_back(info);
		}
		
		
		paddrs = paddrs->ifa_next;	
	}
	
	freeifaddrs(paddrs);
	
	//Now that the iface list is set, fill in gateways
	if(!FillInGateways(protocol))
		return SETUP_NOROUTETABLE;
	
	return result;
}

CIfaceSupport::SETUP_RESULT CIfaceSupport::SetUpIfaces()
{
	SETUP_RESULT repl = SetUpProtIfaces(AF_INET);
	//TODO: We don't fully support IPv6 yet in our products.  When we do, turn this on (and validate support again).
	//if(repl == SETUP_OK) 
		//repl = SetUpProtIfaces(AF_INET6);
	return repl;
}


/*These macros and get_rtaddrs are grabbed from "Unix Network Programming" to get the socket address under an RTM_GET message*/
#define ROUNDUP(a,size) (((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))  //This macro rounds up 'a' to the next multiple of 'size' which must be a power of two
#define NEXT_SA(ap) ap = (struct sockaddr*) ((caddr_t) ap + (ap->sa_len ? ROUNDUP(ap->sa_len, sizeof(u_long)) : sizeof(u_long)))  //This macro steps to the next socket address structure.  If sa_len is 0, assume sizeof(u_long)
void get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int i;
	for(i = 0; i < RTAX_MAX; ++i)
	{
		if(addrs & (1 << i))
		{
			rti_info[i] = sa;
			NEXT_SA(sa);
		}
		else
			rti_info[i] = NULL;
	}
}

//Searches the routing table for a protocol family and sets the default gateway and
//gateway fields of the ifaces in the pool
bool CIfaceSupport::FillInGateways(int protfamily)
{
	/*This is very loosly based on the sysctl and routing socket code in "Unix Network Programming"*/
	int mib[6];
	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = protfamily;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	
	uint1* pdata = NULL;
	size_t len = 0;
	size_t usedlen = 0;
	
	sysctl(mib, 6, pdata, &len, NULL, 0);
	if(len != 0)
	{
		pdata = new uint1[len];
		if(!pdata)
			return false;
		
		if(0 != sysctl(mib, 6, pdata, &len, NULL, 0))
		{
			delete [] pdata;
			return false;
		}
		
		//Note: The BSD stack only gives the network address of the default gateway
		//all other gateways are identified by the network index.  The upshot of this
		//is that we can only report the true gateway of the network interface that is
		//also the default interface.  The other interfaces will currently report themselves
		//as their gateway.  In the future I can play around with the OS X System Configuration
		//dictionary, but since this is purely informational anyway it didn't seem like it was
		//necessary to take the time to do it and force everyone to link in the appropriate frameworks..
			
		while(usedlen < len)
		{
			struct rt_msghdr * rmsg = (struct rt_msghdr*)(pdata + usedlen);
			struct sockaddr * addr_start = (struct sockaddr*)(rmsg + 1);

			struct sockaddr * rti_info [RTAX_MAX];
			get_rtaddrs(rmsg->rtm_addrs, addr_start, rti_info);
			
			
			//The only time we care about a route is if a dst, gateway and netmask are provided (otherwise it's for a route we don't care about). 
			if(rti_info[RTAX_DST] && rti_info[RTAX_GATEWAY] && rti_info[RTAX_NETMASK])
			{
				//Get the iterator corresponding to the address in RTAX_GATEWAY, either through the iface link or the real default gateway address (which is why we need to mask)
				ifaceiter it_iface = FindIfaceBySa(rti_info[RTAX_GATEWAY], true);
				if(it_iface != m_ifaces.end())
				{
					//If netmask == 0, we've found the default route.
					if(0 == rti_info[RTAX_NETMASK]->sa_len)
						m_defaultiface = it_iface->id;
					
					//Set the gateway address
					if(rti_info[RTAX_GATEWAY]->sa_family != AF_LINK)  //We've got a real gateway address here -- override any set address.
						SetAddress(it_iface->gate, rti_info[RTAX_GATEWAY]);
					else if(it_iface->gate == CIPAddr())
						it_iface->gate = it_iface->addr;
				}
			}
		
			usedlen += rmsg->rtm_msglen;			
		}
		
		//If we haven't found a default interface, pick one.
		if((m_defaultiface == NETID_INVALID) && (!m_ifaces.empty()))
			m_defaultiface = m_ifaces.front().id;
		
		delete [] pdata;
		return true;
	}
	
	return false;
}
	

//Given a sockaddr, attempts to find the network interface in the list.
//The sockaddr could be AF_INET, AF_INET6, or AF_LINK.  If the sockaddr is an
//address, usenetmask determines whether or not both addrs are masked with the netmask
//before the comparison
CIfaceSupport::ifaceiter CIfaceSupport::FindIfaceBySa(struct sockaddr* sa, bool usenetmask)
{
	if(!sa)
		return m_ifaces.end();
	
	switch (sa->sa_family)
	{
		case AF_LINK:
		{
			short index = ((struct sockaddr_dl*)(sa))->sdl_index;
			for(ifaceiter it = m_ifaces.begin(); it != m_ifaces.end(); ++it)
			{
				if(it->ifindex == index)
					return it;
			}
			break;
		}
			
		case AF_INET:
		case AF_INET6:
		{
			CIPAddr addr;
			SetAddress(addr, sa);

			for(ifaceiter it = m_ifaces.begin(); it != m_ifaces.end(); ++it)
			{
				if((usenetmask && MaskCompare(addr.GetV6Address(), it->addr.GetV6Address(), it->mask.GetV6Address())) ||
				   (!usenetmask && (addr == it->addr)))
					return it;
			}
			break;
		}
			
		default:
			break;
			
	}	
	return m_ifaces.end();
}

//Returns the current number of network interfaces on the machine
int CIfaceSupport::GetNumInterfaces()
{
	return m_ifaces.size();
}

//This function copies the list of network interfaces into a passed in array.
//list MUST contain the necessary amount of memory (new [GetNumInterfaces()] netintinfo)
//The interface numbers are only valid across this instance of the
// async socket library,  To persist the selected interfaces,
// use the ip address to identify the network interface across executions. 
void CIfaceSupport::CopyInterfaceList(IAsyncSocketServ::netintinfo* list)
{
	int numinfo = m_ifaces.size();
	for(int i = 0; i < numinfo; ++i)
		list[i] = m_ifaces[i];
}

//Fills in the network interface info for a particular interface id
//Returns false if not found
bool CIfaceSupport::CopyInterfaceInfo(netintid id, IAsyncSocketServ::netintinfo& info)
{
	if((id < 0) || (static_cast<uint>(id) >= m_ifaces.size()))
		return false;
	info = m_ifaces[id];
	return true;
}

//Returns the network interface that is used as the default
netintid CIfaceSupport::GetDefaultInterface()
{
	return m_defaultiface;
}

//Returns true if the mask of the two addresses are equal
//Assumes items passed in are what is returned from CIPAddr.GetV6Address()
bool CIfaceSupport::MaskCompare(const uint1* addr1, const uint1* addr2, const uint1* mask)
{
	//Instead of a byte compare, we'll do a int compare
	const uint4* p1 = reinterpret_cast<const uint4*>(addr1);
	const uint4* p2	= reinterpret_cast<const uint4*>(addr2);
	const uint4* pm = reinterpret_cast<const uint4*>(mask);

	for(int i = 0; i < CIPAddr::ADDRBYTES/4; ++i,++p1,++p2,++pm)
	{
		if((*p1 & *pm) != (*p2 & *pm))
			return false;
	}
	return true;
}

//Returns true if the "mask" address is all 0's (which would skew the Mask compare)
bool CIfaceSupport::MaskIsEmpty(const uint1* mask)
{
	uint4 blob = 0;

	//Instead of a byte check, we'll do an int check
	const uint4* p = reinterpret_cast<const uint4*>(mask);
	for(int i = 0; i < CIPAddr::ADDRBYTES/4; ++i,++p)
		blob |= *p;

	return blob == 0;
}

//Returns the network id (or NETID_INVALID) of the first network interface that
//could communicate directly with this address (ignoring port and iface fields).
//if isdefault is true, this was not directly resolveable and would go through 
//the default interface
netintid CIfaceSupport::GetIfaceForDestination(const CIPAddr& destaddr, bool& isdefault)
{
	isdefault = false;

	if(m_ifaces.size() == 0)
		return NETID_INVALID;

	for(ifaceiter it = m_ifaces.begin(); it != m_ifaces.end(); ++it)
	{
		if((!MaskIsEmpty(it->mask.GetV6Address())) &&
			(MaskCompare(it->addr.GetV6Address(), destaddr.GetV6Address(), it->mask.GetV6Address())))
			return it->id;
	}

	if(m_defaultiface != NETID_INVALID)
		isdefault = true;
	return m_defaultiface;
}

//Grabs the local address -- only the address portion is filled in
bool CIfaceSupport::GetLocalAddress(netintid netid, CIPAddr& addr)
{
	IAsyncSocketServ::netintinfo info;
	if(CopyInterfaceInfo(netid, info))
	{
		addr = info.addr;
		addr.SetNetInterface(netid);
		return true;

	}
	return false;
}

//Get the ifindex of a local network interface
bool CIfaceSupport::GetIFIndex(netintid netid, int& ifindex)
{
	IAsyncSocketServ::netintinfo info;
	if(CopyInterfaceInfo(netid, info))
	{
		ifindex = info.ifindex;
		return true;

	}
	return false;
}


