// Copyright (c) 2018 Electronic Theatre Controls, Inc., http://www.etcconnect.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once
#ifndef ROUTER_H
#define ROUTER_H

#ifndef NETWORK_UTILS_H
#include "NetworkUtils.h"
#endif

#ifndef EOS_LOG_H
#include "EosLog.h"
#endif

#ifndef OSC_PARSER_H
#include "OSCParser.h"
#endif

#ifndef ITEM_STATE_H
#include "ItemState.h"
#endif

#ifndef NETWORK_UTILS_H
#include "NetworkUtils.h"
#endif

#ifndef _DEFTYPES_H_
#include "deftypes.h"
#endif

#ifndef _CID_H_
#include "CID.h"
#endif

#ifndef _IPADDR_H_
#include "ipaddr.h"
#endif

#ifndef _ASYNCSOCKETINTERFACE_H_
#include "AsyncSocketInterface.h"
#endif

#ifndef _STREAMACNCLIINTERFACE_H_
#include "StreamACNCliInterface.h"
#endif

#ifndef _STREAMACNSRVINTERFACE_H_
#include "StreamACNSrvInterface.h"
#endif

#ifdef WIN32
#include <WinSock2.h>

#ifndef _WIN_ASYNCSOCKETINTERFACE_H_
#include "Win_AsyncSocketInterface.h"
#endif

#ifndef _WIN_STREAMACNCLIINTERFACE_H_
#include "Win_StreamACNCliInterface.h"
#endif

#ifndef _WIN_STREAMACNSRVINTERFACE_H_
#include "Win_StreamACNSrvInterface.h"
#endif

#define IPlatformAsyncSocketServ IWinAsyncSocketServ
#define IPlatformStreamACNCli IWinStreamACNCli
#define IPlatformStreamACNSrv IWinStreamACNSrv
#endif

class EosTcp;

namespace psn
{
class psn_decoder;
class psn_encoder;
};  // namespace psn

////////////////////////////////////////////////////////////////////////////////

class ScriptEngine
{
public:
  ScriptEngine() = default;

  QJSEngine &js() { return m_JS; }
  QString evaluate(const QString &script, const QString &path = QString(), const OSCArgument *args = nullptr, size_t argsCount = 0, EosPacket *packet = nullptr);

private:
  QJSEngine m_JS;
};

////////////////////////////////////////////////////////////////////////////////

class Router
{
public:
  struct sConnection
  {
    QString label;
    bool server = false;
    OSCStream::EnumFrameMode frameMode = OSCStream::FRAME_MODE_DEFAULT;
    EosAddr addr;
    ItemStateTable::ID itemStateTableId = ItemStateTable::sm_Invalid_Id;
  };

  typedef std::vector<sConnection> CONNECTIONS;

  struct sRoute
  {
    sRoute() {}
    QString label;
    EosRouteSrc src;
    ItemStateTable::ID srcItemStateTableId = ItemStateTable::sm_Invalid_Id;
    EosRouteDst dst;
    ItemStateTable::ID dstItemStateTableId = ItemStateTable::sm_Invalid_Id;
  };

  typedef std::vector<sRoute> ROUTES;

  static uint16_t GetDefaultPSNPort();
  static QString GetDefaultPSNIP();
};

////////////////////////////////////////////////////////////////////////////////

class PacketLogger : public OSCParserClient
{
public:
  PacketLogger(EosLog::EnumLogMsgType logType, EosLog &log)
    : m_LogType(logType)
    , m_pLog(&log)
  {
  }

  virtual void SetPrefix(const std::string &prefix) { m_Prefix = prefix; }
  virtual void OSCParserClient_Log(const std::string &message);
  virtual void OSCParserClient_Send(const char *, size_t) {}
  virtual void PrintPacket(OSCParser &oscParser, const char *packet, size_t size);

protected:
  EosLog::EnumLogMsgType m_LogType;
  EosLog *m_pLog;
  std::string m_Prefix;
  std::string m_LogMsg;
};

////////////////////////////////////////////////////////////////////////////////

class EosUdpInThread : public QThread
{
public:
  struct sRecvPacket
  {
    sRecvPacket(const char *data, int size, unsigned int Ip)
      : packet(data, size)
      , ip(Ip)
    {
    }
    EosPacket packet;
    unsigned int ip;
  };
  typedef std::vector<sRecvPacket> RECV_Q;

  EosUdpInThread();
  virtual ~EosUdpInThread();

  virtual void Start(const EosAddr &addr, QString multicastIP, Protocol protocol, ItemStateTable::ID itemStateTableId, unsigned int reconnectDelayMS);
  virtual void Stop();
  const EosAddr &GetAddr() const { return m_Addr; }
  Protocol GetProtocol() const { return m_Protocol; }
  ItemStateTable::ID GetItemStateTableId() const { return m_ItemStateTableId; }
  ItemState::EnumState GetState();
  virtual void Flush(EosLog::LOG_Q &logQ, RECV_Q &recvQ);

protected:
  EosAddr m_Addr;
  QString m_MulticastIP;
  Protocol m_Protocol = Protocol::kDefault;
  ItemStateTable::ID m_ItemStateTableId;
  ItemState::EnumState m_State;
  unsigned int m_ReconnectDelay;
  bool m_Run;
  EosLog m_Log;
  EosLog m_PrivateLog;
  RECV_Q m_Q;
  QRecursiveMutex m_Mutex;
  psn::psn_decoder *m_PSNDecoder = nullptr;
  std::optional<uint8_t> m_PSNFrame;

  virtual void run();
  virtual void UpdateLog();
  virtual void SetState(ItemState::EnumState state);
  virtual void RecvPacket(const QHostAddress &host, const char *data, int len, OSCParser &logParser, PacketLogger &packetLogger);
  virtual void QueuePacket(const QHostAddress &host, const char *data, int len, OSCParser &logParser, PacketLogger &packetLogger);
};

////////////////////////////////////////////////////////////////////////////////

class EosUdpOutThread : public QThread
{
public:
  EosUdpOutThread();
  virtual ~EosUdpOutThread();

  virtual void Start(const EosAddr &addr, ItemStateTable::ID itemStateTableId, unsigned int reconnectDelayMS);
  virtual void Stop();
  const EosAddr &GetAddr() const { return m_Addr; }
  ItemStateTable::ID GetItemStateTableId() const { return m_ItemStateTableId; }
  ItemState::EnumState GetState();
  virtual bool Send(const EosPacket &packet);
  virtual void Flush(EosLog::LOG_Q &logQ);

protected:
  EosAddr m_Addr;
  ItemStateTable::ID m_ItemStateTableId;
  ItemState::EnumState m_State;
  unsigned int m_ReconnectDelay;
  bool m_Run;
  EosLog m_Log;
  EosLog m_PrivateLog;
  EosPacket::Q m_Q;
  bool m_QEnabled;
  QRecursiveMutex m_Mutex;

  virtual void run();
  virtual void UpdateLog();
  virtual void SetState(ItemState::EnumState state);
};

////////////////////////////////////////////////////////////////////////////////

class EosTcpClientThread : public QThread
{
public:
  EosTcpClientThread();
  virtual ~EosTcpClientThread();

  virtual void Start(const EosAddr &addr, ItemStateTable::ID itemStateTableId, OSCStream::EnumFrameMode frameMode, unsigned int reconnectDelayMS);
  virtual void Start(EosTcp *tcp, const EosAddr &addr, ItemStateTable::ID itemStateTableId, OSCStream::EnumFrameMode frameMode, unsigned int reconnectDelayMS);
  virtual void Stop();
  const EosAddr &GetAddr() const { return m_Addr; }
  ItemStateTable::ID GetItemStateTableId() const { return m_ItemStateTableId; }
  ItemState::EnumState GetState();
  virtual bool Send(const EosPacket &packet);
  virtual bool SendFramed(const EosPacket &packet);
  virtual void Flush(EosLog::LOG_Q &logQ, EosUdpInThread::RECV_Q &recvQ);

protected:
  EosTcp *m_AcceptedTcp;
  EosAddr m_Addr;
  ItemStateTable::ID m_ItemStateTableId;
  ItemState::EnumState m_State;
  OSCStream::EnumFrameMode m_FrameMode;
  unsigned int m_ReconnectDelay;
  bool m_Run;
  EosLog m_Log;
  EosLog m_PrivateLog;
  EosUdpInThread::RECV_Q m_RecvQ;
  EosPacket::Q m_SendQ;
  QRecursiveMutex m_Mutex;

  virtual void run();
  virtual void UpdateLog();
  virtual void SetState(ItemState::EnumState state);
};

////////////////////////////////////////////////////////////////////////////////

class EosTcpServerThread : public QThread
{
public:
  struct sConnection
  {
    sConnection()
      : tcp(0)
    {
    }
    EosTcp *tcp;
    EosAddr addr;
  };
  typedef std::vector<sConnection> CONNECTION_Q;

  EosTcpServerThread();
  virtual ~EosTcpServerThread();

  virtual void Start(const EosAddr &addr, ItemStateTable::ID itemStateTableId, OSCStream::EnumFrameMode frameMode, unsigned int reconnectDelayMS);
  virtual void Stop();
  const EosAddr &GetAddr() const { return m_Addr; }
  ItemStateTable::ID GetItemStateTableId() const { return m_ItemStateTableId; }
  ItemState::EnumState GetState();
  OSCStream::EnumFrameMode GetFrameMode() const { return m_FrameMode; }
  virtual void Flush(EosLog::LOG_Q &logQ, CONNECTION_Q &connectionQ);

protected:
  EosAddr m_Addr;
  ItemStateTable::ID m_ItemStateTableId;
  ItemState::EnumState m_State;
  OSCStream::EnumFrameMode m_FrameMode;
  unsigned int m_ReconnectDelay;
  bool m_Run;
  EosLog m_Log;
  EosLog m_PrivateLog;
  CONNECTION_Q m_Q;
  QRecursiveMutex m_Mutex;

  virtual void run();
  virtual void UpdateLog();
  virtual void SetState(ItemState::EnumState state);
};

////////////////////////////////////////////////////////////////////////////////

class OSCBundleMethod : public OSCMethod
{
public:
  virtual void SetIP(unsigned int ip) { m_IP = ip; }
  virtual bool ProcessPacket(OSCParserClient &client, char *buf, size_t size);
  virtual void Flush(EosUdpInThread::RECV_Q &q);

private:
  unsigned int m_IP = 0u;
  EosUdpInThread::RECV_Q m_Q;
};

////////////////////////////////////////////////////////////////////////////////

class RouterThread : public QThread, private OSCParserClient, private IStreamACNCliNotify
{
public:
  RouterThread(const Router::ROUTES &routes, const Router::CONNECTIONS &tcpConnections, const ItemStateTable &itemStateTable, unsigned int reconnectDelayMS);
  virtual ~RouterThread();

  virtual void Stop();
  virtual void Flush(EosLog::LOG_Q &logQ, ItemStateTable &itemStateTable);

protected:
  struct sRouteDst
  {
    EosRouteDst dst;
    ItemStateTable::ID srcItemStateTableId;
    ItemStateTable::ID dstItemStateTableId;
  };

  typedef std::vector<sRouteDst> ROUTE_DESTINATIONS;

  typedef std::map<QString, ROUTE_DESTINATIONS> ROUTES_BY_PATH;
  typedef std::pair<QString, ROUTE_DESTINATIONS> ROUTES_BY_PATH_PAIR;

  struct sRoutesByIp
  {
    ROUTES_BY_PATH routesByPath;
    ROUTES_BY_PATH routesByWildcardPath;
  };

  typedef std::map<unsigned int, sRoutesByIp> ROUTES_BY_IP;
  typedef std::pair<unsigned int, sRoutesByIp> ROUTES_BY_IP_PAIR;

  typedef std::map<unsigned short, ROUTES_BY_IP> ROUTES_BY_PORT;
  typedef std::pair<unsigned short, ROUTES_BY_IP> ROUTES_BY_PORT_PAIR;

  typedef std::map<EosAddr, EosUdpInThread *> UDP_IN_THREADS;
  typedef std::map<EosAddr, EosUdpOutThread *> UDP_OUT_THREADS;

  typedef std::map<EosAddr, EosTcpClientThread *> TCP_CLIENT_THREADS;
  typedef std::map<EosAddr, EosTcpServerThread *> TCP_SERVER_THREADS;

  typedef std::vector<const ROUTE_DESTINATIONS *> DESTINATIONS_LIST;

  struct sACN
  {
    IPlatformAsyncSocketServ *net = nullptr;
    IPlatformStreamACNCli *client = nullptr;
    IPlatformStreamACNSrv *server = nullptr;
  };

  bool m_Run;
  unsigned int m_ReconnectDelay;
  Router::ROUTES m_Routes;
  Router::CONNECTIONS m_TcpConnections;
  EosLog m_Log;
  EosLog m_PrivateLog;
  ItemStateTable m_ItemStateTable;
  QRecursiveMutex m_Mutex;
  ScriptEngine *m_ScriptEngine = nullptr;
  psn::psn_encoder *m_PSNEncoder = nullptr;
  QElapsedTimer m_PSNEncoderTimer;

  virtual void run();
  virtual void BuildRoutes(ROUTES_BY_PORT &routesByPort, ROUTES_BY_PORT &routesBysACNUniverse, UDP_IN_THREADS &udpInThreads, UDP_OUT_THREADS &udpOutThreads, TCP_CLIENT_THREADS &tcpClientThreads,
                           TCP_SERVER_THREADS &tcpServerThreads);
  virtual void BuildsACN(ROUTES_BY_PORT &routesByPort, ROUTES_BY_PORT &routesBysACNUniverse, sACN &sacn);
  virtual EosUdpOutThread *CreateUdpOutThread(const EosAddr &addr, ItemStateTable::ID itemStateTableId, UDP_OUT_THREADS &udpOutThreads);
  virtual void AddRoutingDestinations(bool isOSC, const QString &path, const sRoutesByIp &routesByIp, DESTINATIONS_LIST &destinations);
  virtual void ProcessRecvQ(OSCParser &oscBundleParser, ROUTES_BY_PORT &routesByPort, DESTINATIONS_LIST &routingDestinationList, UDP_OUT_THREADS &udpOutThreads, TCP_CLIENT_THREADS &tcpClientThreads,
                            const EosAddr &addr, EosUdpInThread::RECV_Q &recvQ);
  virtual void ProcessRecvPacket(ROUTES_BY_PORT &routesByPort, DESTINATIONS_LIST &routingDestinationList, UDP_OUT_THREADS &udpOutThreads, TCP_CLIENT_THREADS &tcpClientThreads, const EosAddr &addr,
                                 bool isOSC, EosUdpInThread::sRecvPacket &recvPacket);
  virtual bool MakeOSCPacket(const QString &srcPath, const EosRouteDst &dst, OSCArgument *args, size_t argsCount, EosPacket &packet);
  virtual bool MakePSNPacket(EosPacket &osc, EosPacket &psn);
  virtual void ProcessTcpConnectionQ(TCP_CLIENT_THREADS &tcpClientThreads, OSCStream::EnumFrameMode frameMode, EosTcpServerThread::CONNECTION_Q &tcpConnectionQ);
  virtual bool ApplyTransform(OSCArgument &arg, const EosRouteDst &dst, OSCPacketWriter &packet);
  virtual void MakeSendPath(const QString &srcPath, const QString &dstPath, const OSCArgument *args, size_t argsCount, QString &sendPath);
  virtual void UpdateLog();
  virtual void SetItemState(ItemStateTable::ID id, ItemState::EnumState state);
  virtual void SetItemActivity(ItemStateTable::ID id);
  virtual void OSCParserClient_Log(const std::string &message);
  virtual void OSCParserClient_Send(const char *buf, size_t size);

  // IStreamACNCliNotify
  void SourceDisappeared(const CID &source, uint2 universe) override;
  void SourcePCPExpired(const CID &source, uint2 universe) override;
  void SamplingStarted(uint2 universe) override;
  void SamplingEnded(uint2 universe) override;
  void UniverseData(const CID &source, const char *source_name, const CIPAddr &source_ip, uint2 universe, uint2 reserved, uint1 sequence, uint1 options, uint1 priority, uint1 start_code,
                    uint2 slot_count, uint1 *pdata) override;
  void UniverseBad(uint2 universe, netintid iface) override;

  static bool HasProtocolOutput(const ROUTES_BY_PORT &routesByPort, Protocol protocol);
  static bool HasProtocolOutput(const ROUTES_BY_IP &routesByIp, Protocol protocol);
  static bool HasProtocolOutput(const ROUTES_BY_PATH &routesByPath, Protocol protocol);
  static void DestroysACN(sACN &sacn);
};

////////////////////////////////////////////////////////////////////////////////

#endif
