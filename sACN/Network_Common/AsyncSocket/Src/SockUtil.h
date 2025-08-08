/*SockUtil.h -- quick utility functions*/

#ifndef _SOCK_UTIL_H_
#define _SOCK_UTIL_H_

//Safely increments a sockid, so it does not == SOCKID_INVALID
//Returns the id as well.
inline sockid IncID(sockid& sid)
{
	if((++sid) == SOCKID_INVALID)
		++sid;
	return sid;	
}

#endif
