//Subscription.cpp --  Implementation of the CSubscriptions class
#include <map>

#include "deftypes.h"
#include "ipaddr.h"
#include "Subscriptions.h"

CSubscriptions::CSubscriptions()
{

}

CSubscriptions::CSubscriptions(const CSubscriptions& subs)
{
	addrmap = subs.addrmap;
}

CSubscriptions::~CSubscriptions()
{

}
	
//Returns true if socket-level subscribe should occur
bool CSubscriptions::AddSubscription(const CIPAddr& newaddr)
{
	if(!newaddr.IsMulticastAddress())
		return false;
		
	addriter it = addrmap.find(newaddr);
	if(it != addrmap.end())
	{
		++(it->second);
		return false;
	}
	std::pair<addriter, bool> result = addrmap.insert(std::pair<CIPAddr, int>(newaddr, 1));
	return result.first != addrmap.end();
}

//Returns true if socket-level unsubscribe should occur
bool CSubscriptions::RemoveSubscription(const CIPAddr& addr)
{
	addriter it = addrmap.find(addr);
	if(it != addrmap.end())
	{
		--(it->second);
		if(it->second <= 0)
		{
			addrmap.erase(it);
			return true;
		}
	}
	return false;
}

//Returns true if is currently a subscription to that address
bool CSubscriptions::IsSubscribed(const CIPAddr& addr)
{
	return 0 != addrmap.count(addr);
}

//If your platform has a maximum on the number of subscriptions on
//a socket, use this to determine if the maximum (within threshold)
//has been reached.  Note this is the number of addresses, not the 
//number of "subscriptions" on the address, as those are just refcounts.
bool CSubscriptions::MaxReached(uint max, uint threshold)
{
	return (addrmap.size() + threshold) >= max;
}

//Used for forceable socket destruction.  Pops the next subscription in
//the list, or returns false if empty
bool CSubscriptions::PopSubscription(CIPAddr& addr, int& refcnt)
{
	if(!addrmap.empty())
	{
		addr = addrmap.begin()->first;
		refcnt = addrmap.begin()->second;
		addrmap.erase(addrmap.begin());
		return true;
	}
	return false;
}
