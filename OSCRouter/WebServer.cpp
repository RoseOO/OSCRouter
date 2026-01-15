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

#include "WebServer.h"
#include "NetworkUtils.h"
#include <QRegularExpression>

////////////////////////////////////////////////////////////////////////////////

WebServer::WebServer(QObject *parent)
  : QObject(parent)
  , m_Server(nullptr)
  , m_Status("Stopped")
{
}

////////////////////////////////////////////////////////////////////////////////

WebServer::~WebServer()
{
  Stop();
}

////////////////////////////////////////////////////////////////////////////////

bool WebServer::Start(quint16 port)
{
  if (m_Server)
  {
    Stop();
  }

  m_Server = new QTcpServer(this);
  connect(m_Server, &QTcpServer::newConnection, this, &WebServer::onNewConnection);

  if (!m_Server->listen(QHostAddress::Any, port))
  {
    delete m_Server;
    m_Server = nullptr;
    return false;
  }

  m_Status = QString("Running on port %1").arg(port);
  AddLogMessage(QString("Web server started on port %1").arg(port), "info");
  return true;
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::Stop()
{
  if (m_Server)
  {
    m_Server->close();
    delete m_Server;
    m_Server = nullptr;
    m_Status = "Stopped";
    AddLogMessage("Web server stopped", "info");
  }
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::AddLogMessage(const QString &message, const QString &type)
{
  LogEntry entry;
  entry.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
  entry.message = message;
  entry.type = type;

  m_LogMessages.push_back(entry);
  
  if (m_LogMessages.size() > MAX_LOG_MESSAGES)
  {
    m_LogMessages.pop_front();
  }
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::SetStatus(const QString &status)
{
  m_Status = status;
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::SetRoutes(const Router::ROUTES &routes)
{
  m_Routes = routes;
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::SetConnections(const Router::CONNECTIONS &connections)
{
  m_Connections = connections;
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::SetSettings(const Router::Settings &settings)
{
  m_Settings = settings;
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::SetItemStateTable(const ItemStateTable &itemStateTable)
{
  m_ItemStateTable = itemStateTable;
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::onNewConnection()
{
  while (m_Server->hasPendingConnections())
  {
    QTcpSocket *socket = m_Server->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &WebServer::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &WebServer::onDisconnected);
  }
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::onReadyRead()
{
  QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
  if (!socket)
    return;

  QByteArray requestData = socket->readAll();
  QString request = QString::fromUtf8(requestData);

  QStringList lines = request.split("\r\n");
  if (lines.isEmpty())
  {
    SendNotFound(socket);
    return;
  }

  QStringList requestLine = lines[0].split(" ");
  if (requestLine.size() < 2)
  {
    SendNotFound(socket);
    return;
  }

  QString method = requestLine[0];
  QString path = requestLine[1];

  int bodyStart = request.indexOf("\r\n\r\n");
  QByteArray body;
  if (bodyStart >= 0)
  {
    body = requestData.mid(bodyStart + 4);
  }

  HandleRequest(socket, method, path, body);
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::onDisconnected()
{
  QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
  if (socket)
  {
    socket->deleteLater();
  }
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::HandleRequest(QTcpSocket *socket, const QString &method, const QString &path, const QByteArray &body)
{
  Q_UNUSED(body);

  if (method != "GET")
  {
    SendResponse(socket, 405, "text/plain", "Method Not Allowed");
    return;
  }

  if (path == "/" || path == "/index.html")
  {
    SendHtmlResponse(socket, GetIndexHtml());
  }
  else if (path == "/api/status")
  {
    SendJsonResponse(socket, QJsonDocument(GetStatusJson()));
  }
  else if (path == "/api/config")
  {
    SendJsonResponse(socket, QJsonDocument(GetConfigJson()));
  }
  else if (path == "/api/logs")
  {
    QJsonObject obj;
    obj["logs"] = GetLogsJson();
    SendJsonResponse(socket, QJsonDocument(obj));
  }
  else
  {
    SendNotFound(socket);
  }
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::SendResponse(QTcpSocket *socket, int statusCode, const QString &contentType, const QByteArray &body)
{
  QString statusText;
  switch (statusCode)
  {
    case 200: statusText = "OK"; break;
    case 404: statusText = "Not Found"; break;
    case 405: statusText = "Method Not Allowed"; break;
    default: statusText = "Unknown"; break;
  }

  QByteArray response;
  response.append(QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText).toUtf8());
  response.append(QString("Content-Type: %1\r\n").arg(contentType).toUtf8());
  response.append(QString("Content-Length: %1\r\n").arg(body.size()).toUtf8());
  response.append("Access-Control-Allow-Origin: *\r\n");
  response.append("Connection: close\r\n");
  response.append("\r\n");
  response.append(body);

  socket->write(response);
  socket->flush();
  socket->disconnectFromHost();
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::SendHtmlResponse(QTcpSocket *socket, const QString &html)
{
  SendResponse(socket, 200, "text/html; charset=utf-8", html.toUtf8());
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::SendJsonResponse(QTcpSocket *socket, const QJsonDocument &json)
{
  SendResponse(socket, 200, "application/json", json.toJson(QJsonDocument::Indented));
}

////////////////////////////////////////////////////////////////////////////////

void WebServer::SendNotFound(QTcpSocket *socket)
{
  SendResponse(socket, 404, "text/plain", "Not Found");
}

////////////////////////////////////////////////////////////////////////////////

QJsonObject WebServer::GetStatusJson() const
{
  QJsonObject status;
  status["server_status"] = m_Status;
  status["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
  status["routes_count"] = static_cast<int>(m_Routes.size());
  status["connections_count"] = static_cast<int>(m_Connections.size());
  
  QJsonArray itemStates;
  const ItemStateTable::LIST &list = m_ItemStateTable.GetList();
  for (size_t i = 0; i < list.size(); i++)
  {
    const ItemState &item = list[i];
    QJsonObject itemObj;
    
    QString stateName;
    ItemState::GetStateName(item.state, stateName);
    itemObj["id"] = static_cast<int>(i);
    itemObj["state"] = stateName;
    itemObj["activity"] = item.activity;
    itemObj["mute"] = item.mute;
    
    itemStates.append(itemObj);
  }
  status["item_states"] = itemStates;
  
  return status;
}

////////////////////////////////////////////////////////////////////////////////

QJsonObject WebServer::GetConfigJson() const
{
  QJsonObject config;
  
  QJsonArray routes;
  for (const Router::sRoute &route : m_Routes)
  {
    QJsonObject routeObj;
    routeObj["label"] = route.label;
    routeObj["enabled"] = route.enable;
    routeObj["muted"] = route.mute;
    
    QJsonObject src;
    src["ip"] = route.src.addr.ip;
    src["port"] = static_cast<int>(route.src.addr.port);
    routeObj["source"] = src;
    
    QJsonObject dst;
    dst["ip"] = route.dst.addr.ip;
    dst["port"] = static_cast<int>(route.dst.addr.port);
    routeObj["destination"] = dst;
    
    routes.append(routeObj);
  }
  config["routes"] = routes;
  
  QJsonArray connections;
  for (const Router::sConnection &conn : m_Connections)
  {
    QJsonObject connObj;
    connObj["label"] = conn.label;
    connObj["server"] = conn.server;
    connObj["ip"] = conn.addr.ip;
    connObj["port"] = static_cast<int>(conn.addr.port);
    
    connections.append(connObj);
  }
  config["connections"] = connections;
  
  QJsonObject settings;
  settings["sACN_IP"] = m_Settings.sACNIP;
  settings["artNet_IP"] = m_Settings.artNetIP;
  settings["level_changes_only"] = m_Settings.levelChangesOnly;
  config["settings"] = settings;
  
  return config;
}

////////////////////////////////////////////////////////////////////////////////

QJsonArray WebServer::GetLogsJson() const
{
  QJsonArray logs;
  
  for (const LogEntry &entry : m_LogMessages)
  {
    QJsonObject logObj;
    logObj["timestamp"] = entry.timestamp;
    logObj["message"] = entry.message;
    logObj["type"] = entry.type;
    logs.append(logObj);
  }
  
  return logs;
}

////////////////////////////////////////////////////////////////////////////////

QString WebServer::GetIndexHtml() const
{
  return R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OSCRouter - Status Dashboard</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: linear-gradient(135deg, #1a1a1a 0%, #2d2d2d 100%);
            color: #e0e0e0;
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
        }
        
        header {
            text-align: center;
            margin-bottom: 40px;
            padding: 20px;
            background: rgba(255, 255, 255, 0.05);
            border-radius: 12px;
            backdrop-filter: blur(10px);
        }
        
        h1 {
            color: #ff8e33;
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 0 0 20px rgba(255, 142, 51, 0.3);
        }
        
        .subtitle {
            color: #a0a0a0;
            font-size: 1.1em;
        }
        
        .dashboard {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        
        .card {
            background: rgba(255, 255, 255, 0.05);
            border-radius: 12px;
            padding: 25px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            transition: all 0.3s ease;
        }
        
        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);
            border-color: rgba(255, 142, 51, 0.3);
        }
        
        .card h2 {
            color: #ff8e33;
            margin-bottom: 15px;
            font-size: 1.4em;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .card-content {
            color: #c0c0c0;
            line-height: 1.8;
        }
        
        .status-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
            animation: pulse 2s infinite;
        }
        
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        
        .status-running { background: #4caf50; box-shadow: 0 0 10px #4caf50; }
        .status-stopped { background: #f44336; box-shadow: 0 0 10px #f44336; }
        .status-connecting { background: #ff9800; box-shadow: 0 0 10px #ff9800; }
        
        .stat-value {
            font-size: 2em;
            font-weight: bold;
            color: #ff8e33;
            margin: 10px 0;
        }
        
        .log-container {
            background: rgba(0, 0, 0, 0.3);
            border-radius: 8px;
            padding: 15px;
            max-height: 400px;
            overflow-y: auto;
            font-family: 'Courier New', monospace;
            font-size: 0.9em;
        }
        
        .log-entry {
            padding: 8px;
            margin: 4px 0;
            border-left: 3px solid #ff8e33;
            background: rgba(255, 255, 255, 0.02);
            border-radius: 4px;
        }
        
        .log-timestamp {
            color: #808080;
            margin-right: 10px;
        }
        
        .log-info { border-left-color: #2196F3; }
        .log-warning { border-left-color: #ff9800; }
        .log-error { border-left-color: #f44336; }
        
        .config-item {
            padding: 10px;
            margin: 8px 0;
            background: rgba(255, 255, 255, 0.03);
            border-radius: 6px;
            border-left: 3px solid #ff8e33;
        }
        
        .config-label {
            color: #ff8e33;
            font-weight: bold;
            margin-bottom: 5px;
        }
        
        .refresh-btn {
            background: linear-gradient(135deg, #ff8e33 0%, #ff6b1a 100%);
            color: white;
            border: none;
            padding: 12px 30px;
            border-radius: 8px;
            cursor: pointer;
            font-size: 1em;
            font-weight: bold;
            margin-top: 15px;
            transition: all 0.3s ease;
            box-shadow: 0 4px 15px rgba(255, 142, 51, 0.3);
        }
        
        .refresh-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(255, 142, 51, 0.5);
        }
        
        .refresh-btn:active {
            transform: translateY(0);
        }
        
        @media (max-width: 768px) {
            .dashboard {
                grid-template-columns: 1fr;
            }
            
            h1 {
                font-size: 1.8em;
            }
            
            .card {
                padding: 15px;
            }
        }
        
        .loading {
            text-align: center;
            padding: 40px;
            color: #a0a0a0;
            font-size: 1.2em;
        }
        
        .error-message {
            background: rgba(244, 67, 54, 0.1);
            border: 1px solid #f44336;
            color: #f44336;
            padding: 15px;
            border-radius: 8px;
            margin: 20px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>&#127979;&#65039; OSCRouter</h1>
            <p class="subtitle">Real-time Status Dashboard</p>
        </header>
        
        <div id="dashboard" class="dashboard">
            <div class="loading">Loading dashboard...</div>
        </div>
        
        <button class="refresh-btn" onclick="loadDashboard()">&#128260; Refresh Dashboard</button>
    </div>
    
    <script>
        function formatTimestamp(isoString) {
            try {
                const date = new Date(isoString);
                return date.toLocaleTimeString();
            } catch (e) {
                return isoString;
            }
        }
        
        async function loadDashboard() {
            try {
                const [statusRes, configRes, logsRes] = await Promise.all([
                    fetch('/api/status'),
                    fetch('/api/config'),
                    fetch('/api/logs')
                ]);
                
                const status = await statusRes.json();
                const config = await configRes.json();
                const logsData = await logsRes.json();
                
                renderDashboard(status, config, logsData.logs);
            } catch (error) {
                document.getElementById('dashboard').innerHTML = `
                    <div class="error-message">
                        <strong>Error loading dashboard:</strong> ${error.message}
                    </div>
                `;
            }
        }
        
        function renderDashboard(status, config, logs) {
            const dashboard = document.getElementById('dashboard');
            
            const statusIndicator = status.server_status.includes('Running') 
                ? '<span class="status-indicator status-running"></span>' 
                : '<span class="status-indicator status-stopped"></span>';
            
            dashboard.innerHTML = `
                <div class="card">
                    <h2>&#128202; Server Status</h2>
                    <div class="card-content">
                        <div>${statusIndicator}${status.server_status}</div>
                        <div style="margin-top: 10px; color: #808080;">Last updated: ${formatTimestamp(status.timestamp)}</div>
                    </div>
                </div>
                
                <div class="card">
                    <h2>&#128200; Statistics</h2>
                    <div class="card-content">
                        <div>Routes: <span class="stat-value">${status.routes_count}</span></div>
                        <div>Connections: <span class="stat-value">${status.connections_count}</span></div>
                        <div>Item States: <span class="stat-value">${status.item_states ? status.item_states.length : 0}</span></div>
                    </div>
                </div>
                
                <div class="card" style="grid-column: 1 / -1;">
                    <h2>&#128221; Recent Logs</h2>
                    <div class="card-content">
                        <div class="log-container">
                            ${logs.slice(-50).reverse().map(log => `
                                <div class="log-entry log-${log.type}">
                                    <span class="log-timestamp">${formatTimestamp(log.timestamp)}</span>
                                    <span>${log.message}</span>
                                </div>
                            `).join('')}
                        </div>
                    </div>
                </div>
                
                <div class="card" style="grid-column: 1 / -1;">
                    <h2>&#9881;&#65039; Current Configuration</h2>
                    <div class="card-content">
                        <div class="config-item">
                            <div class="config-label">Routes (${config.routes.length})</div>
                            ${config.routes.slice(0, 10).map(route => `
                                <div style="margin-left: 15px; margin-top: 5px; color: #c0c0c0;">
                                    ${route.label || 'Unnamed'}: ${route.source.ip}:${route.source.port} &rarr; ${route.destination.ip}:${route.destination.port}
                                    ${route.enabled ? '&#10003;' : '&#10007;'} ${route.muted ? '&#128263;' : ''}
                                </div>
                            `).join('')}
                            ${config.routes.length > 10 ? `<div style="margin-left: 15px; margin-top: 5px; color: #808080;">... and ${config.routes.length - 10} more</div>` : ''}
                        </div>
                        
                        <div class="config-item">
                            <div class="config-label">TCP Connections (${config.connections.length})</div>
                            ${config.connections.map(conn => `
                                <div style="margin-left: 15px; margin-top: 5px; color: #c0c0c0;">
                                    ${conn.label || 'Unnamed'}: ${conn.ip}:${conn.port} ${conn.server ? '(Server)' : '(Client)'}
                                </div>
                            `).join('')}
                        </div>
                        
                        <div class="config-item">
                            <div class="config-label">Settings</div>
                            <div style="margin-left: 15px; margin-top: 5px; color: #c0c0c0;">
                                sACN IP: ${config.settings.sACN_IP || 'Auto'}<br>
                                ArtNet IP: ${config.settings.artNet_IP || 'Auto'}<br>
                                Level Changes Only: ${config.settings.level_changes_only ? 'Yes' : 'No'}
                            </div>
                        </div>
                    </div>
                </div>
            `;
        }
        
        // Auto-refresh every 5 seconds
        loadDashboard();
        setInterval(loadDashboard, 5000);
    </script>
</body>
</html>
)";
}

////////////////////////////////////////////////////////////////////////////////
