// AsyncSocketServ.cpp: implementation of the CAsyncSocketServ class.
//
//////////////////////////////////////////////////////////////////////

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <iphlpapi.h>
#include <process.h>

#include <map>
#include <list>
#include <vector>
#include <set>

#include "deftypes.h"
#include "defpack.h"
#include "ipaddr.h"
#include "readerwriter.h"
#include "MemPool.h"
#include "..\Src\Subscriptions.h"
#include "..\AsyncSocketInterface.h"
#include "..\Src\SockUtil.h"
#include "Win_AsyncSocketInterface.h"
#include "AsyncSocketServ.h"

#ifndef IP_MAX_MEMBERSHIPS  // Was in winsock.h, but not winsock2.h
#define IP_MAX_MEMBERSHIPS 20
#endif

// The actual creation
IWinAsyncSocketServ* IWinAsyncSocketServ::CreateInstance()
{
  return new CAsyncSocketServ;
}

// Destroys the pointer.  Call Shutdown first
void IWinAsyncSocketServ::DestroyInstance(IWinAsyncSocketServ* p)
{
  delete static_cast<CAsyncSocketServ*>(p);
}

// we are using the following constants for the IOCP completion key
const DWORD IOCP_NORMAL = 0;             // The worker thread should operate normally
const DWORD IOCP_STARTRECV = 1;          // The worker thread should initiate a RecvFrom on the info in overlapped
const DWORD IOCP_INVALID = 0xfffffffe;   // Just an init value.
const DWORD IOCP_SHUTDOWN = 0xffffffff;  // The worker thread should die

//////////////////////////////////////////////////////////////////////
// The worker thread function, two per processor, that handles socket
// events
uint _stdcall WorkThread(void* psocketsrv)
{
  if (!psocketsrv)
    return 1;
  CAsyncSocketServ* psrv = reinterpret_cast<CAsyncSocketServ*>(psocketsrv);

#if 0  // Used for thread identification
		FILE* tfile = fopen("acnthreads.txt", "a");
		if(tfile)
		{
			fprintf(tfile, "AsyncSocket Thread ID is %d\n", GetCurrentThreadId());
			fclose(tfile);
		}
#endif

  bool stopthread = false;
  while (!stopthread)
  {
    DWORD numread = 0;
    ULONG_PTR cmd = IOCP_INVALID;
    OVERLAPPED* po = NULL;

    BOOL result = GetQueuedCompletionStatus(psrv->m_iocp, &numread, &cmd, &po, INFINITE);

    // Testing shows that CONTAINING_RECORD returns NULL if po is NULL
    myoverlap* pover = CONTAINING_RECORD(po, myoverlap, over);

    if (!result)  // Either the call failed, or a socket is gone
    {
      if (!pover)
      {
        // Something is quite wrong, since we have an INFINITE timeout
        // but I'm not sure if we should stop the thread here.
        // Just sleep for 10 ms so we don't constantly take up CPU
        Sleep(10);
      }
      else
      {
        // The socket is gone, initiate clean up
        psrv->WorkerDetectClose(pover->sockid);
        if (pover->buffer.buf)
          psrv->m_pool.Free(pover->buffer.buf);
        delete pover;
      }
    }
    else  // Either we got a normal recv, we should start a receive, or the thread should die
    {
      switch (cmd)
      {
        case IOCP_SHUTDOWN:
        case IOCP_INVALID:
          stopthread = true;
          // Destroy the overlap and buffer as we're being cleaned up
          if (pover)
          {
            if (pover->buffer.buf)
              psrv->m_pool.Free(pover->buffer.buf);
            delete pover;
          }
          break;

        case IOCP_NORMAL:
          if (numread != 0)  // numread == 0 not actually an error condition
          {
            psrv->NotifyReceivePacket(pover->sockid, &pover->fromaddr, (uint1*)pover->buffer.buf, numread);

            // The app now owns the buffer, or it was cleared in NotifyReceivePacket
            pover->buffer.buf = NULL;
          }
          else  // Since it wasn't passed on, we need to clear the buffer for the next read
          {
            if (pover->buffer.buf)
              psrv->m_pool.Free(pover->buffer.buf);
            pover->buffer.buf = NULL;
          }
          // Don't break, just drop to the next case where we start the recv again

        case IOCP_STARTRECV:
        {
          // Since this library only does one overlapped operation per socket at any time (and we're
          // only doing reads), I don't need to allocate new overlapped structures for each operation.
          // We'll just get a new buffer into the existing (but cleaned) overlap.
          pover->buffer.buf = (char*)psrv->m_pool.Alloc();
          if (!pover->buffer.buf)
            break;  // Failout
          memset(&pover->fromaddr, 0, sizeof(sockaddr_in));
          memset(&pover->over, 0, sizeof(WSAOVERLAPPED));
          pover->rcvflags = 0;

          numread = 0;
          int result = WSARecvFrom(pover->sock, &(pover->buffer), 1, &numread, &(pover->rcvflags), (sockaddr*)&(pover->fromaddr), &(pover->fromaddrlen), &pover->over, 0);
          // We got an immediate recv -- just check for failure as everything else still hits the IOCP
          if ((result != 0) && (WSA_IO_PENDING != WSAGetLastError()))
          {
            psrv->WorkerDetectClose(pover->sockid);  // Signal death
            if (pover->buffer.buf)
              psrv->m_pool.Free(pover->buffer.buf);
            delete pover;
          }
          break;
        }
        default:
          // Unknown cmd, ignore it
          break;
      }
    }
  }
  return 0;
}

// This function is used for manual_recv sockets.  It does a blocking recvfrom and returns
// the number of bytes received into pbuffer (or <0 for the recvfrom socket errors). It also fills in from.
int CAsyncSocketServ::ReceiveInto(sockid id, CIPAddr& from, uint1* pbuffer, uint buflen)
{
  SOCKET sock = INVALID_SOCKET;
  netintid iface = NETID_INVALID;

  m_socklock.ReadLock();
  sockiter sockit = m_sockmap.find(id);
  if (sockit != m_sockmap.end() && sockit->second->active)
  {
    sock = sockit->second->sock;
    iface = sockit->second->boundaddr.GetNetInterface();
  }
  m_socklock.ReadUnlock();

  if (sock == INVALID_SOCKET)
    return SOCKET_ERROR;

  sockaddr_in fromaddr;
  int fromsize = sizeof(sockaddr_in);
  int numread = recvfrom(sock, (char*)pbuffer, buflen, 0, (sockaddr*)&fromaddr, &fromsize);
  if (numread >= 0)
  {
    // TODO: Assumes ipv4
    from.SetNetInterface(iface);
    from.SetIPPort(ntohs(fromaddr.sin_port));
    from.SetV4Address(ntohl(fromaddr.sin_addr.s_addr));
  }

  return numread;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CAsyncSocketServ::CAsyncSocketServ()
  : m_pool(1500)
{
  m_shuttingdown = false;
  m_newid = 0;
  m_numthreads = 0;
  m_iocp = NULL;
}

CAsyncSocketServ::~CAsyncSocketServ() {}

////////////////////////////////////////////////////////////////
// IWinAsyncSocketServ functions

// TODO: Startup logging and socket creation errors would be very useful

// Startup and shutdown functions -- these should be called once directly from the app that
// has the class instance
bool CAsyncSocketServ::Startup(int threadpriority)
{
  bool res = true;
  WSADATA wsadata;

  if (0 != WSAStartup(MAKEWORD(2, 2), &wsadata))
    return false;  // just bypass the whole thing

  m_shuttingdown = false;
  m_defaultiface = NETID_INVALID;
  m_newid = 0;

  if (res)
    res = InitNICList();

  // TODO: If we intended to support operation without any NIC, here's the spot
  //       to handle it -- either a getaddr would have issues (0.0.0.0), or perhaps
  //       the iface list is empty.

  if (res)
  {
    m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    res = (m_iocp != NULL);
  }

  if (res)
  {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    m_numthreads = info.dwNumberOfProcessors * 2;  // The rule of thumb is to have double the number of processors for total work threads

    // Start up the worker threads
    for (DWORD i = 0; i < m_numthreads; ++i)
    {
      HANDLE h = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, WorkThread, this, CREATE_SUSPENDED, NULL));
      if (h != NULL)
      {
        SetThreadPriority(h, threadpriority);
        ResumeThread(h);
        m_threadset.insert(h);
      }
      else
      {
        res = false;
        break;
      }
    }
  }

  m_pool.Reserve(20);  // jump start the amount of packets we receive

  if (!res)
    Shutdown();  // Clean up.
  return res;
}

void CAsyncSocketServ::Shutdown()
{
  /**Stop all notifications**/
  m_shuttingdown = true;

  CIPAddr addr;
  sockiter sockit;

  /**Unsubscribe and Disconnect the SOCKETs**/
  m_socklock.WriteLock();
  for (sockit = m_sockmap.begin(); sockit != m_sockmap.end(); ++sockit)
    InitiateDestruction(sockit, false);  // No need to notify
  m_socklock.WriteUnlock();

  /**Shut down the IOCP worker threads**/
  HANDLE* threads = new HANDLE[m_threadset.size()];
  // Since windows wants this list, we'll just kill the set and use the array
  DWORD threadcount = 0;
  while (!m_threadset.empty())
  {
    ++threadcount;
    threads[threadcount - 1] = *m_threadset.begin();
    m_threadset.erase(m_threadset.begin());
  }

  // Should cause the threads to terminate
  for (DWORD i = 0; i < threadcount; ++i)
    PostQueuedCompletionStatus(m_iocp, 0, IOCP_SHUTDOWN, NULL);
  WaitForMultipleObjects(threadcount, threads, TRUE, 500);  // Give them a little time

  for (DWORD j = 0; j < threadcount; ++j)
    CloseHandle(threads[j]);
  delete[] threads;

  /**Finish destroying the socket refs, including cleaning up the WSABUFs**/
  m_socklock.WriteLock();
  while (!m_sockmap.empty())
  {
    sockit = m_sockmap.begin();  // Fixes a conversion problem
    FinalizeDestruction(sockit);
  }
  m_socklock.WriteUnlock();

  /**Clean up the IOCP, etc**/
  CloseHandle(m_iocp);
  m_iocp = NULL;
  WSACleanup();
}

bool CAsyncSocketServ::InitNICList()
{
  m_defaultiface = NETID_INVALID;
  bool res = false;

  // On windows, we can use GetBestInterface instead of a routing socket solution (RTM_GET)
  // We'll look for the 0.0.0.0 destination to get the default.
  // We do this before we init the NICs, so we can do a quick compare and not store windows interface IDs
  // If there was no iface, we'll pick one.
  DWORD def_iface = (DWORD)NETID_INVALID;
  GetBestInterface(0, &def_iface);

  // Initialize the interface vector
  m_ifaces.clear();
  ULONG buflen = 0;
  uint1* buffer = NULL;
  // TODO: When we add IPv6 support, this call will need to change to GetAdaptersAddresses
  if (ERROR_BUFFER_OVERFLOW == GetAdaptersInfo(NULL, &buflen))
  {
    buffer = new uint1[buflen];
    if (buffer && (ERROR_SUCCESS == GetAdaptersInfo(reinterpret_cast<IP_ADAPTER_INFO*>(buffer), &buflen)))
    {
      res = true;  // If we got here, all should be well.
      IP_ADAPTER_INFO* p = reinterpret_cast<IP_ADAPTER_INFO*>(buffer);
      while (p)
      {
        // If this is multihomed, there may be multiple addresses
        // under the same adapter
        IP_ADDR_STRING* pip = &p->IpAddressList;
        IP_ADDR_STRING* pgate = &p->GatewayList;
        while (pip)
        {
          netintinfo info;

          char adstr[17];
          memset(adstr, 0, 17);

          strncpy(adstr, pip->IpAddress.String, 16);
          info.addr = CIPAddr::StringToAddr(adstr);

          strncpy(adstr, pip->IpMask.String, 16);
          info.mask = CIPAddr::StringToAddr(adstr);

          strncpy(adstr, pgate->IpAddress.String, 16);
          info.gate = CIPAddr::StringToAddr(adstr);

          memset(info.desc, 0, NETINTID_STRLEN);
          strncpy(info.desc, p->Description, NETINTID_STRLEN - 1);
          memset(info.name, 0, NETINTID_STRLEN);
          strncpy(info.name, p->AdapterName, NETINTID_STRLEN - 1);

          if (p->AddressLength == NETINTID_MACLEN)
            memcpy(info.mac, p->Address, NETINTID_MACLEN);
          else
            memset(info.mac, 0, NETINTID_MACLEN);

          info.id = m_ifaces.size();
          m_ifaces.push_back(info);

          // Update the default gateway
          if ((m_defaultiface == NETID_INVALID) && (p->Index == def_iface))
            m_defaultiface = info.id;

          pip = pip->Next;
          if (pgate->Next)  // Just in case there's only ever one gateway but more addrs (probably not possible)
            pgate = pgate->Next;
        }
        p = p->Next;
      }
    }
    if (buffer)
      delete[] buffer;
  }

  if ((m_defaultiface == NETID_INVALID) && (!m_ifaces.empty()))
    m_defaultiface = m_ifaces.front().id;

  return res;
}

// If this returns true, any socket that binds to a port will receive the mcast message
// if even only one of the sockets subscribed on that network interface.
bool CAsyncSocketServ::McastMessagesSharePort()
{
  return false;
}

// If this returns true, any socket that binds to a port will receive the mcast message,
// even if the one that subscribed was on a different network interface (as long as
// some socket on that interface subscribed).
bool CAsyncSocketServ::McastMessagesIgnoreSubscribedIface()
{
  return false;
}

// Returns the current number of network interfaces on the machine
int CAsyncSocketServ::GetNumInterfaces()
{
  return m_ifaces.size();
}

// This function copies the list of network interfaces into a passed in array.
// list MUST contain the necessary amount of memory (new [GetNumInterfaces()] netintinfo)
// The interface numbers are only valid across this instance of the
//  async socket library,  To persist the selected interfaces,
//  use the ip address to identify the network interface across executions.
void CAsyncSocketServ::CopyInterfaceList(netintinfo* list)
{
  int numinfo = m_ifaces.size();
  for (int i = 0; i < numinfo; ++i)
    list[i] = m_ifaces[i];
}

// Fills in the network interface info for a particular interface id
// Returns false if not found
bool CAsyncSocketServ::CopyInterfaceInfo(netintid id, netintinfo& info)
{
  if ((id < 0) || (static_cast<uint>(id) >= m_ifaces.size()))
    return false;
  info = m_ifaces[id];
  return true;
}

// Returns the network interface that is used as the default
netintid CAsyncSocketServ::GetDefaultInterface()
{
  return m_defaultiface;
}

// Returns true if the mask of the two addresses are equal
// Assumes items passed in are what is returned from CIPAddr.GetV6Address()
bool CAsyncSocketServ::MaskCompare(const uint1* addr1, const uint1* addr2, const uint1* mask)
{
  // Instead of a byte compare, we'll do a int compare
  const uint4* p1 = reinterpret_cast<const uint4*>(addr1);
  const uint4* p2 = reinterpret_cast<const uint4*>(addr2);
  const uint4* pm = reinterpret_cast<const uint4*>(mask);

  for (int i = 0; i < CIPAddr::ADDRBYTES / 4; ++i, ++p1, ++p2, ++pm)
  {
    if ((*p1 & *pm) != (*p2 & *pm))
      return false;
  }
  return true;
}

// Returns true if the "mask" address is all 0's (which would skew the Mask compare)
bool CAsyncSocketServ::MaskIsEmpty(const uint1* mask)
{
  uint4 blob = 0;

  // Instead of a byte check, we'll do an int check
  const uint4* p = reinterpret_cast<const uint4*>(mask);
  for (int i = 0; i < CIPAddr::ADDRBYTES / 4; ++i, ++p)
    blob |= *p;

  return blob == 0;
}

// Returns the network id (or NETID_INVALID) of the first network interface that
// could communicate directly with this address (ignoring port and iface fields).
// if isdefault is true, this was not directly resolveable and would go through
// the default interface
netintid CAsyncSocketServ::GetIfaceForDestination(const CIPAddr& destaddr, bool& isdefault)
{
  isdefault = false;

  if (m_ifaces.size() == 0)
    return NETID_INVALID;

  for (ifaceiter it = m_ifaces.begin(); it != m_ifaces.end(); ++it)
  {
    if ((!MaskIsEmpty(it->mask.GetV6Address())) && (MaskCompare(it->addr.GetV6Address(), destaddr.GetV6Address(), it->mask.GetV6Address())))
      return it->id;
  }

  if (m_defaultiface != NETID_INVALID)
    isdefault = true;
  return m_defaultiface;
}

/////////////////////////////////////////////////////////////////////////
// The IAsyncSocketServ interface

void CAsyncSocketServ::DeletePacket(uint1* pbuffer, uint /*buflen*/)
{
  m_pool.Free(pbuffer);
}

// Grabs the local address -- address and port
bool CAsyncSocketServ::GetLocalAddress(sockid sock, CIPAddr& addr)
{
  // Since the boundaddress for the socket IS the local address
  bool result = GetBoundAddress(sock, addr);
  if (result)
    addr.SetIPPort(0);
  return result;
}

// Grabs the local address -- only the address portion is filled in
bool CAsyncSocketServ::GetLocalAddress(netintid netid, CIPAddr& addr)
{
  netintinfo info;
  if (CopyInterfaceInfo(netid, info))
  {
    addr = info.addr;
    addr.SetNetInterface(netid);
    return true;
  }
  return false;
}

// Gets the bound address of the socket or an empty address
bool CAsyncSocketServ::safe_GetBoundAddress(sockiter it, CIPAddr& addr)
{
  if (it != m_sockmap.end())
  {
    addr = it->second->boundaddr;
    return true;
  }

  return false;
}

// Gets the bound address of the socket or an empty address
bool CAsyncSocketServ::GetBoundAddress(sockid sock, CIPAddr& addr)
{
  bool result = false;

  if (m_socklock.ReadLock())
  {
    result = safe_GetBoundAddress(m_sockmap.find(sock), addr);
    m_socklock.ReadUnlock();
  }

  return result;
}

// Returns the MTU for this socket, as set by the caller
uint CAsyncSocketServ::safe_GetMTU(sockiter it)
{
  if (it != m_sockmap.end())
    return it->second->MTU;

  return 0;
}

// Returns the MTU for this socket, as set by the caller
uint CAsyncSocketServ::GetMTU(sockid sock)
{
  uint result = 0;

  if (m_socklock.ReadLock())
  {
    result = safe_GetMTU(m_sockmap.find(sock));
    m_socklock.ReadUnlock();
  }

  return result;
}

// Returns the OS interface index
// Returns -1 if the index is unknown.
int CAsyncSocketServ::GetOSIndex(sockid /*sock*/)
{
  // TODO: Support ipv6
  return -1;  // Not supported yet
}

// Returns whether or now this socket layer is on a v6 network
bool CAsyncSocketServ::IsV6(sockid /*sock*/)
{
  // TODO: Better support with Winsock2 /interfaces
  return false;  // HACK
}

// Returns true if the socket is already subscribed to the address
bool CAsyncSocketServ::safe_IsSubscribed(sockiter it, const CIPAddr& addr)
{
  return it != m_sockmap.end() && it->second->active && it->second->subs.IsSubscribed(addr);
}

// Returns true if the socket is already subscribed to the address
bool CAsyncSocketServ::IsSubscribed(sockid id, const CIPAddr& addr)
{
  bool result = false;
  if (m_socklock.ReadLock())
  {
    result = safe_IsSubscribed(m_sockmap.find(id), addr);
    m_socklock.ReadUnlock();
  }

  return result;
}

// There may be a system-determined limit for the number of multicast
// addresses that can be subscribed to by the same socket.  Call this
// function to determine if there is room avaliable (or if
// SubscribeMulticast return false).  If the addr passed in is one the
// socket is already subscribed to, this returns true -- you can always
// keep subscribing to the same socket, as it just refcounts internally.
bool CAsyncSocketServ::safe_RoomForSubscribe(sockiter it, const CIPAddr& addr)
{
  return it != m_sockmap.end() && it->second->active &&
         // If the socket is already subscribed, it's an immediate pass
         (it->second->subs.IsSubscribed(addr) || (!it->second->subs.MaxReached(IP_MAX_MEMBERSHIPS, 2)));
}

// There may be a system-determined limit for the number of multicast
// addresses that can be subscribed to by the same socket.  Call this
// function to determine if there is room avaliable (or if
// SubscribeMulticast return false).  If the addr passed in is one the
// socket is already subscribed to, this returns true -- you can always
// keep subscribing to the same socket, as it just refcounts internally.
bool CAsyncSocketServ::RoomForSubscribe(sockid id, const CIPAddr& addr)
{
  bool result = false;
  if (m_socklock.ReadLock())
  {
    result = safe_RoomForSubscribe(m_sockmap.find(id), addr);
    m_socklock.ReadUnlock();
  }

  return result;
}

// Subscribes a multicast socket to a multicast address.
// It will return false on error, or if the maximum number of subscription
// addresses has been reached.  If McastMessagesIgnoreSubscribedIface is false,
// the Network interface must match the interface the socket is bound to.  If
// McastMessagesIgnoreSubscribedIface is true, this function turns on the
// multicast "spigot" for this iface (and in this case, an iface of NETID_INVALID
// turns on multicast for all network interfaces.
bool CAsyncSocketServ::SubscribeMulticast(sockid id, const CIPAddr& addr)
{
  if (!addr.IsMulticastAddress())
    return false;

  bool result = false;
  if (m_socklock.WriteLock())
  {
    sockiter sockit = m_sockmap.find(id);
    if (safe_RoomForSubscribe(sockit, addr))
    {
      if (sockit->second->subs.AddSubscription(addr))
        result = SocketSubscribe(sockit->second->sock, addr, sockit->second->boundaddr);
      else
        result = true;  // Already a member
    }
    m_socklock.WriteUnlock();
  }
  return result;
}

// Unsubscribes a multicast socket from a multicast address.
// Returns true if the unsubscribe actually ocurred (otherwise, the
// reference count was just lowered).
// Note that on some platforms, if you unsubscribe you can't reuse that
// socket to subscribe to another address.  can_reuse will be set to
// false in that situation (can_reuse is set whatever this function returns).
// If you can't reuse and the unsubscribe ocurred, you might as well destroy the socket.
// UnsubscribeMulticast follows the same rules as SubcribeMulticast with respect
// to the network interface of addr.
bool CAsyncSocketServ::UnsubscribeMulticast(sockid id, const CIPAddr& addr, bool& can_reuse)
{
  // Because we don't bind the socket to the multicast address, we can always reuse
  can_reuse = true;
  bool result = false;
  if (m_socklock.WriteLock())
  {
    sockiter sockit = m_sockmap.find(id);
    if (sockit != m_sockmap.end() && sockit->second->active)
    {
      if (sockit->second->subs.RemoveSubscription(addr))
      {
        SocketUnsubscribe(sockit->second->sock, addr, sockit->second->boundaddr);
        result = true;
      }
    }
    m_socklock.WriteUnlock();
  }
  return result;
}

// This is the preferred method of multicast socket creation, as it attempts to share
// sockets across subscriptions.  If this interferes with your needs,
// use CreateSingleMulticastSocket().
// Creates a multicast socket, binding to the correct port and network interface.
// This does not subscribe to an address, call SubscribeMulticast instead.
// if port == 0, a random port is assigned and a new socket is automatically
// generated.
// If manual_recv is true, the user must call ReceiveInto to receive any data,
// as the ReceivePacket notification is not used.
// On success, newsock is filled in.
bool CAsyncSocketServ::CreateMulticastSocket(IAsyncSocketClient* pnotify, netintid netid, sockid& newsock, IPPort& port, uint maxdatasize, bool manual_recv)
{
  // For now, there is no difference between multicast sockets and unicast sockets,
  // But I'd like to reserve the right later, so there's a different call
  return CreateUnicastSocket(pnotify, netid, newsock, port, maxdatasize, manual_recv);
}

// In instances where McastMessagesSharePort or McastMessagesIgnoreSubscribedIface,
// it may not be adviseable to share subscriptions if multiple protocols will be using
// that port.  In the case of sACN vs. ACN, while the protocol code does filter out
// invalid packets the packets have to get all the way to that level before they are
// filtered.  This needs to be balanced with scaling needs, however, so while sACN will
// most likely use this function, ACN will not.
// This function creates a multicast socket for the explicit use of a particular multicast
// address, and will only receive messages for that address. It also immediately subscribes
// to the address. Otherwise it works like the previous function.
// The mcast address must be completely filled in, port must not be 0.
bool CAsyncSocketServ::CreateStandaloneMulticastSocket(IAsyncSocketClient* pnotify, const CIPAddr& maddr, sockid& newsock, uint maxdatasize, bool manual_recv)
{
  IPPort tmpport = maddr.GetIPPort();
  return CreateMulticastSocket(pnotify, maddr.GetNetInterface(), newsock, tmpport, maxdatasize, manual_recv) && SubscribeMulticast(newsock, maddr);
}

// Creates, sets up, and binds a listening unicast socket.
// If port == 0, a random port is assigned.
// If manual_recv is true, the user must call ReceiveInto to receive any data,
// as the ReceivePacket notification is not used.
// On success, newsock is filled in
bool CAsyncSocketServ::CreateUnicastSocket(IAsyncSocketClient* pnotify, netintid netid, sockid& newsock, IPPort& port, uint maxdatasize, bool manual_recv)
{
  // Get the address to bind to
  const uint1 tst[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  CIPAddr addr;
  if ((!GetLocalAddress(netid, addr)) || (0 == memcmp(addr.GetV6Address(), tst, 16)))
    return false;
  addr.SetIPPort(port);

  // Otherwise, It's time to create the socket
  socketref* pref = new socketref;
  pref->active = true;
  pref->pnotify = pnotify;
  pref->MTU = maxdatasize;
  pref->boundaddr = addr;                         // Although the port can still change
  if (SocketCreate(pref->sock, pref->boundaddr))  // We successfully created the socket
  {
    if (!m_socklock.WriteLock())
    {
      delete pref;
      return false;
    }

    // To make sure we haven't flipped over, make sure the id isn't used already
    // If all the sockets are in use, the socket creation fails
    sockid testsock = m_newid;
    while (m_sockmap.find(m_newid) != m_sockmap.end())
    {
      IncID(m_newid);
      if (m_newid == testsock)
      {
        m_socklock.WriteUnlock();
        delete pref;
        return false;
      }
    }
    m_sockmap.insert(sockval(m_newid, pref));
    newsock = m_newid;
    port = pref->boundaddr.GetIPPort();
    IncID(m_newid);
    m_socklock.WriteUnlock();

    if (!manual_recv)
    {
      if (!SocketStart(pref, newsock))  // We need to start after the socket is in the map
      {
        // But there was an error, so destroy the socket and remove it from the map
        DestroySocket(newsock);
        return false;
      }
    }
    return true;
  }
  // Couldn't Create
  if (pref)
    delete pref;
  return false;
}

// Destroys the socket if all references have been destroyed.
// All messages for that socket will be sent before destruction
// The socket id should no longer be used for that socket, and may be reused on another create.
void CAsyncSocketServ::DestroySocket(sockid id)
{
  if (m_socklock.WriteLock())
  {
    sockiter sockit = m_sockmap.find(id);
    if (sockit != m_sockmap.end())
    {
      // We got here in two ways. Either the socket was active and we need to initiate destruction
      // or the socket is inactive already and we need to finish destruction
      if (sockit->second->active)
        InitiateDestruction(sockit, false);  // We were told by api, so don't notify badsocket
      else                                   // Do the final destruction
        FinalizeDestruction(sockit);
    }
    m_socklock.WriteUnlock();
  }
}

// Used by the worker threads to signal that a socket has been detected as closed
// IOCP gets a message queued (with no bytes read) when the socket closes/shuts down
void CAsyncSocketServ::WorkerDetectClose(sockid id)
{
  if (m_socklock.WriteLock())
  {
    sockiter sockit = m_sockmap.find(id);
    if (sockit != m_sockmap.end())
    {
      // If we got here and the socket was still active, there's a socket error
      // Notify (if we aren't shutting down) and do the initial clean up, but DestroySocket will eventually do the rest
      if (sockit->second->active)
        InitiateDestruction(sockit, !m_shuttingdown);
      else  // Normal destruction, where the app initiated it, and the thread detected it left
        FinalizeDestruction(sockit);
    }
    m_socklock.WriteUnlock();
  }
}

// Used by the worker threads to signal that a socket has received data
void CAsyncSocketServ::NotifyReceivePacket(sockid id, struct sockaddr_in* fromaddr, uint1* pbuffer, uint numread)
{
  bool done = false;

  if (m_socklock.ReadLock())
  {
    sockiter sockit = m_sockmap.find(id);
    if (sockit != m_sockmap.end() && sockit->second && sockit->second->active && sockit->second->pnotify)
    {
      // TODO: IPv4 assumed..
      CIPAddr from(sockit->second->boundaddr.GetNetInterface(), ntohs(fromaddr->sin_port), ntohl(fromaddr->sin_addr.s_addr));

      // We're calling process packet here, since we know it is handled async
      sockit->second->pnotify->ReceivePacket(id, from, pbuffer, numread);
      done = true;
    }
    m_socklock.ReadUnlock();
  }

  if (!done)  // The buffer wasn't used, drop it
    m_pool.Free(pbuffer);
}

// Shuts the socket down, but doesn't remove.  May notify the app
void CAsyncSocketServ::InitiateDestruction(sockiter& sockit, bool notify)
{
  sockit->second->active = false;
  CIPAddr addr;
  int refcnt;
  while (sockit->second->subs.PopSubscription(addr, refcnt))
    SocketUnsubscribe(sockit->second->sock, addr, sockit->second->boundaddr);
  SocketDestroy(sockit->second->sock);  // We'll kill the actual socket here, but not remove it from the map
  if (notify && sockit->second->pnotify)
    sockit->second->pnotify->SocketBad(sockit->first);
}

// Removes the socket from the map, etc.
void CAsyncSocketServ::FinalizeDestruction(sockiter& sockit)
{
  if (sockit->second)
    delete sockit->second;
  m_sockmap.erase(sockit);
}

// Sends the packet to a particular address.
// pbuffer is considered INVALID by this library after this function
// returns, as the calling code may reuse or delete it.
// If there is an error, this will only trigger a SocketBad notification
// if error_is_failure is true.  Note that in many cases (like SDT), an error on
// sending should actually be ignored.
void CAsyncSocketServ::SendPacket(sockid id, const CIPAddr& addr, const uint1* pbuffer, uint buflen, bool error_is_failure)
{
  bool sendresult = true;
  if (m_socklock.ReadLock())
  {
    CAsyncSocketServ::sockiter it = m_sockmap.find(id);
    if (it != m_sockmap.end() && it->second->active)
      sendresult = SocketSendTo(it->second->sock, addr, pbuffer, buflen);
    m_socklock.ReadUnlock();
  }

  if (!sendresult && error_is_failure)  // Error, clean up
  {
    if (m_socklock.WriteLock())
    {
      CAsyncSocketServ::sockiter it = m_sockmap.find(id);
      if (it != m_sockmap.end())
        InitiateDestruction(it, true);
      m_socklock.WriteUnlock();
    }
  }
}

// The version that takes async_chunk instead of a straight buffer.  The etcchunk
// chain should be considered INVALID by this library after this function returns,
// as the calling code may reuse or delete it.
void CAsyncSocketServ::SendPacket(sockid id, const CIPAddr& addr, const async_chunk* chunks, bool error_is_failure)
{
  // Allocate a new buffer to hold the entire packet
  uint count = 0;
  const async_chunk* pchunk = NULL;

  for (pchunk = chunks; pchunk != NULL; pchunk = pchunk->pnext)
    count += pchunk->len;

  uint1* pbuffer = new uint1[count];
  if (pbuffer)
  {
    uint1* pbuf = pbuffer;
    for (pchunk = chunks; pchunk != NULL; pchunk = pchunk->pnext)
    {
      memcpy(pbuf, pchunk->pbuf, pchunk->len);
      pbuf += pchunk->len;
    }
    SendPacket(id, addr, pbuffer, count, error_is_failure);
    delete[] pbuffer;
  }
}

// Creates the socket, but doesn't actually start the read cycle
// Fills in both parameters with the new socket and bound port
// at least the bound address and netid should be non 0, but the port can be for automatic client
bool CAsyncSocketServ::SocketCreate(SOCKET& sock, CIPAddr& boundaddr)
{
  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // No need to call WSASocket -- this does exactly what we want
  if (sock == INVALID_SOCKET)
    return false;

  bool valid = true;  // Whether we continue operating on the socket

  if (valid)
  {
    BOOL option = 0;  // If this is enabled, on 2000 and XP a PORT_UNREACHEABLE will stop our receiving.  Very BAD.
    BOOL tmp;
    DWORD numbytes;
    valid = (0 == WSAIoctl(sock, SIO_UDP_CONNRESET, &option, sizeof(option), &tmp, sizeof(tmp), &numbytes, NULL, NULL));
  }

  // Windows Vista and 7 aren't currently as efficient at receiving things.
  // We need to up the receive buffer size so eos can send file transfers better, and sACN can listen to more universes.
  // This matches the buffer size used in Linux, so it must be good. :)
  // Just to be on the safe side, though, if this setsockopt fails we won't fail the whole thing.  The app would just have
  // a little more difficulty receiving bursts
  if (valid)
  {
    int bufsize = 110592;
    // valid = (0 == setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, sizeof(bufsize)));
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&bufsize, sizeof(bufsize));
  }

  if (valid)
  {
    BOOL option = 1;  // Very important for our multicast needs
    valid = (0 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&option, sizeof(option)));
  }

  if (valid)
  {
    int value = 20;  // A more reasonable TTL limit, but probably unnecessary
    valid = (0 == setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)(&value), sizeof(value)));
  }

  if (valid)
  {
    // This one is critical for multicast sends to go over the correct interface.
    IN_ADDR v4addr;  // different socket option type between v4 and IPv6
    v4addr.S_un.S_addr = htonl(boundaddr.GetV4Address());
    valid = (0 == setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (const char*)(&v4addr), sizeof(v4addr)));
  }

  struct sockaddr_in sa;  // used for binding and getting the port
  if (valid)
  {
    // Bind socket to address for IPv4
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(boundaddr.GetV4Address());
    sa.sin_port = htons(boundaddr.GetIPPort());
    valid = (0 == bind(sock, (struct sockaddr*)&sa, sizeof(sa)));
  }

  if (valid && (boundaddr.GetIPPort() == 0))
  {
    int length = sizeof(sa);
    valid = (0 == getsockname(sock, (struct sockaddr*)&sa, &length));
    if (valid)
      boundaddr.SetIPPort(ntohs(sa.sin_port));
  }

  if (!valid)
  {
    closesocket(sock);
    sock = INVALID_SOCKET;
    return false;
  }
  return true;
}

// Starts the socket and the read cycle
bool CAsyncSocketServ::SocketStart(socketref* psocket, sockid id)
{
  if (psocket->sock == INVALID_SOCKET)
    return false;

  // Associate the socket to the IOCP
  if (NULL == CreateIoCompletionPort((HANDLE)psocket->sock, m_iocp, IOCP_NORMAL, 0))
    return false;

  // Do the initial read
  return SocketStartRecv(psocket, id);
}

// Closes the socket and sets sock to INVALID_SOCKET
void CAsyncSocketServ::SocketDestroy(SOCKET& sock)
{
  if (sock != INVALID_SOCKET)
  {
    closesocket(sock);
    sock = INVALID_SOCKET;
  }
}

// Socket level subscription
bool CAsyncSocketServ::SocketSubscribe(SOCKET sock, const CIPAddr& subaddr, const CIPAddr& boundaddr)
{
  if ((sock == INVALID_SOCKET) || (!subaddr.IsMulticastAddress()))
    return false;

  struct ip_mreq multireq;  // again, IPv4
  multireq.imr_multiaddr.s_addr = htonl(subaddr.GetV4Address());
  multireq.imr_interface.s_addr = htonl(boundaddr.GetV4Address());
  return 0 == setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&multireq, sizeof(multireq));
}

// Socket level unsubscription
void CAsyncSocketServ::SocketUnsubscribe(SOCKET sock, const CIPAddr& subaddr, const CIPAddr& boundaddr)
{
  if ((sock != INVALID_SOCKET) && (subaddr.IsMulticastAddress()))
  {
    struct ip_mreq multireq;  // again, IPv4
    multireq.imr_multiaddr.s_addr = htonl(subaddr.GetV4Address());
    multireq.imr_interface.s_addr = htonl(boundaddr.GetV4Address());
    setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&multireq, sizeof(multireq));
  }
}

bool CAsyncSocketServ::SocketSendTo(SOCKET sock, const CIPAddr& addr, const uint1* pbuffer, uint buflen)
{
  if (sock == INVALID_SOCKET)
    return false;

  // This is not an overlapped sendto, because the passed-in buffer will go away
  // when this call completes...  Therefore, we will do a non-overlapped send.

  WSABUF buf;
  buf.buf = (char*)pbuffer;  // Yuck, but not really another option
  buf.len = buflen;

  struct sockaddr_in sa;
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(addr.GetV4Address());
  sa.sin_port = htons(addr.GetIPPort());

  DWORD numsent = 0;

  // Attempt the send
  return (SOCKET_ERROR != WSASendTo(sock, &buf, 1, &numsent, 0, (const sockaddr*)&sa, sizeof(sa), NULL, 0));
}

// Starts the recv process on the socket
bool CAsyncSocketServ::SocketStartRecv(socketref* psocket, sockid id)
{
  if (psocket->sock == INVALID_SOCKET)
    return false;

  // Allocate the initial overlap structure
  myoverlap* pover = new myoverlap;
  if (!pover)
    return false;
  memset(pover, 0, sizeof(myoverlap));
  pover->fromaddrlen = sizeof(sockaddr_in);
  pover->sockid = id;
  pover->sock = psocket->sock;
  pover->buffer.len = std::max(uint(1500), psocket->MTU);  // The length is constant

  if (!PostQueuedCompletionStatus(m_iocp, 0, IOCP_STARTRECV, &pover->over))
  {
    delete pover;
    pover = NULL;
    return false;
  }
  return true;
}
