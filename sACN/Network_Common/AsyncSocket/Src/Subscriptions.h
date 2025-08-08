//Subscriptions.h  Provides CSubscriptions.  
//
//This class tracks the number of subscriptions currently subscribed to on
//a class instance, along with reference counting of those subscriptions.
//This allows the add and remove subscription functions to
//track whether or not real socket work needs to be done, which in turn
//allows the application to only actually do the socket-level subscribe
//or unsubscribe when needed.
//
//THIS CLASS IS NOT THREAD SAFE.  You must provide your own locking around
//this object if such functionality is needed.

#ifndef _SUBSCRIPTIONS_H_
#define _SUBSCRIPTIONS_H_

#ifndef _IPADDR_H_
#error "#include error: Subscriptions.h requires ipaddr.h"
#endif

class CSubscriptions
{
public:
	CSubscriptions();
	CSubscriptions(const CSubscriptions& subs);
	virtual ~CSubscriptions();
	
	//Returns true if socket-level subscribe should occur
	bool AddSubscription(const CIPAddr& newaddr);

	//Returns true if socket-level unsubscribe should occur
	bool RemoveSubscription(const CIPAddr& addr);

	//Returns true if is currently a subscription to that address
	bool IsSubscribed(const CIPAddr& addr);

	//If your platform has a maximum on the number of subscriptions on
	//a socket, use this to determine if the maximum (within threshold)
	//has been reached.  Note this is the number of addresses, not the 
	//number of "subscriptions" on the address, as those are just refcounts.
	bool MaxReached(uint max, uint threshold);

	//Used mainly for forceable destruction of sockets, where unsubscribes
	//are needed.  Pops the next subscription in the list, or returns false if empty
	bool PopSubscription(CIPAddr& addr, int& refcnt);

private:
	std::map<CIPAddr, int> addrmap;
	typedef std::map<CIPAddr, int>::iterator addriter;
};

#endif //_SUBSCRIPTIONS_H_
