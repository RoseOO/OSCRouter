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
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <deque>

#ifndef EOS_LOG_H
#include "EosLog.h"
#endif

#ifndef ITEM_STATE_H
#include "ItemState.h"
#endif

#ifndef ROUTER_H
#include "Router.h"
#endif

////////////////////////////////////////////////////////////////////////////////

class WebServer : public QObject
{
  Q_OBJECT

public:
  static constexpr quint16 DEFAULT_PORT = 8081;
  static constexpr size_t MAX_LOG_MESSAGES = 1000;
  static constexpr int MAX_ROUTES_DISPLAYED = 10;
  static constexpr int MAX_LOGS_DISPLAYED = 50;

  WebServer(QObject *parent = nullptr);
  virtual ~WebServer();

  bool Start(quint16 port = DEFAULT_PORT);
  void Stop();
  bool IsRunning() const { return m_Server && m_Server->isListening(); }
  quint16 GetPort() const { return m_Server ? m_Server->serverPort() : 0; }

  void AddLogMessage(const QString &message, const QString &type = "info");
  void SetStatus(const QString &status);
  void SetRoutes(const Router::ROUTES &routes);
  void SetConnections(const Router::CONNECTIONS &connections);
  void SetSettings(const Router::Settings &settings);
  void SetItemStateTable(const ItemStateTable &itemStateTable);

private slots:
  void onNewConnection();
  void onReadyRead();
  void onDisconnected();

private:
  struct LogEntry
  {
    QString timestamp;
    QString message;
    QString type;
  };

  QTcpServer *m_Server;
  QString m_Status;
  Router::ROUTES m_Routes;
  Router::CONNECTIONS m_Connections;
  Router::Settings m_Settings;
  ItemStateTable m_ItemStateTable;
  std::deque<LogEntry> m_LogMessages;

  void HandleRequest(QTcpSocket *socket, const QString &method, const QString &path, const QByteArray &body);
  void SendResponse(QTcpSocket *socket, int statusCode, const QString &contentType, const QByteArray &body);
  void SendHtmlResponse(QTcpSocket *socket, const QString &html);
  void SendJsonResponse(QTcpSocket *socket, const QJsonDocument &json);
  void SendNotFound(QTcpSocket *socket);

  QJsonObject GetStatusJson() const;
  QJsonObject GetConfigJson() const;
  QJsonArray GetLogsJson() const;
  QString GetIndexHtml() const;
};

////////////////////////////////////////////////////////////////////////////////

#endif
