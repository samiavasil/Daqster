# Daqster REST API & Remote Control Architecture

> **Status:** PLANNED — Future Feature (не е имплементирано)
> **Related REQ:** REQ-SW-PL-054
> **Depends on:** REQ-SW-PL-053 (NodeEditorIde Split → чист headless binary)
> **Date:** 2026-09-28
> **Author:** Daqster Team

---

## 1. Общ преглед (High-Level Architecture)

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                    HEADLESS SERVER                                      │
│  ┌────────────────────────────────────────────────────────────────────────────────────┐  │
│  │                            HeadlessEngine (QCoreApplication)                       │  │
│  │  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────────────────────┐  │  │
│  │  │   FlowLoader     │  │ DataFlowGraphModel│  │        RestApiServer            │  │  │
│  │  │                  │  │                  │  │  ┌────────────────────────────┐  │  │  │
│  │  │                  │  │                  │  │  │  HTTP Router (QHttpServer) │  │  │  │
│  │  │                  │  │                  │  │  │  - GET  /api/nodes         │  │  │  │
│  │  │                  │  │                  │  │  │  - GET  /api/nodes/{id}/   │  │  │  │
│  │  │                  │  │                  │  │  │    schema|config|start|stop │  │  │  │
│  │  │                  │  │                  │  │  │  - POST /api/nodes/{id}/   │  │  │  │
│  │  │                  │  │                  │  │  │    config|start|stop        │  │  │  │
│  │  │                  │  │                  │  │  │  - GET  /api/flow          │  │  │  │
│  │  │                  │  │                  │  │  │  - POST /api/flow          │  │  │  │
│  │  │                  │  │                  │  │  │  - WS   /api/events        │  │  │  │
│  │  │                  │  │                  │  │  │  - Auth: Bearer Token      │  │  │  │
│  │  │                  │  │                  │  │  │  - CORS enabled            │  │  │  │
│  │  │                  │  │                  │  │  └────────────────────────────┘  │  │  │
│  │  └────────┬─────────┘  └────────┬─────────┘  └──────────────┬────────────────┘  │  │
│  │           │                     │                           │                   │  │
│  │           ▼                     ▼                           ▼                   │  │
│  │  ┌──────────────────────────────────────────────────────────────────────────┐  │  │
│  │  │                    NodeDelegateModelRegistry                             │  │  │
│  │  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐      │  │  │
│  │  │  │AudioModel│ │VideoModel│ │LLamaModel│ │NetModel  │ │ ...      │      │  │  │
│  │  │  │(IStart/  │ │(IStart/  │ │(IStart/  │ │(IStart/  │ │          │      │  │  │
│  │  │  │ IStop)   │ │ IStop)   │ │ IStop)   │ │ IStop)   │ │          │      │  │  │
│  │  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘      │  │  │
│  │  └──────────────────────────────────────────────────────────────────────────┘  │  │
│  └────────────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────────────┘
                                           │
                              HTTP/WS (JSON) over TCP
                                           │
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                      CLIENTS                                            │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐  ┌────────────────┐  │
│  │  Daqster Editor  │  │   Web Dashboard  │  │  CLI / Scripts   │  │  CI/CD         │  │
│  │  (Remote Mode)   │  │   (React/Vue)    │  │  (curl, python)  │  │  Pipelines     │  │
│  └──────────────────┘  └──────────────────┘  └──────────────────┘  └────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. API Contract (OpenAPI 3.0 Specification)

### 2.1 Base URL

```
http://<server-host>:<port>/api
WebSocket: ws://<server-host>:<port>/api/events
```

Default port: **8080** (configurable via `--rest-port`)

### 2.2 Authentication

Всички endpoints (освен `/api/health`) изискват **Bearer Token**:

```
Authorization: Bearer <token>
```

Token се задава при стартиране: `--rest-token <secret>`

За WebSocket: `ws://host:8080/api/events?token=<secret>`

### 2.3 Error Response Format

```json
{
  "error": {
    "code": "NODE_NOT_FOUND",
    "message": "Node with id 42 not found",
    "details": {}
  }
}
```

HTTP Status Codes:
- `200` — OK
- `400` — Bad Request (invalid JSON, validation failed)
- `401` — Unauthorized (missing/invalid token)
- `403` — Forbidden (token valid, но недостатъчни права — future)
- `404` — Not Found (node/flow не съществува)
- `500` — Internal Server Error

### 2.4 Endpoints

#### `GET /api/health`
Health check (no auth required).

**Response:**
```json
{
  "status": "ok",
  "version": "0.3.2",
  "uptime_seconds": 3600,
  "nodes_count": 12,
  "flow_loaded": true
}
```

---

#### `GET /api/nodes`
Списък на всички нодове в текущия flow.

**Response:**
```json
{
  "nodes": [
    {
      "id": 0,
      "name": "VideoFileSource",
      "type": "VideoFileSource",
      "category": "Sources/Video",
      "state": "running",
      "ports": {
        "inputs": [],
        "outputs": [{"index": 0, "name": "video", "type": "VideoFrameData"}]
      },
      "auto_start": true,
      "deembedded": false
    },
    {
      "id": 1,
      "name": "VideoEffect",
      "type": "VideoEffect",
      "category": "Processing/Video",
      "state": "running",
      "ports": {
        "inputs": [{"index": 0, "name": "input", "type": "VideoFrameData"}],
        "outputs": [{"index": 0, "name": "output", "type": "VideoFrameData"}]
      },
      "auto_start": false,
      "deembedded": true
    }
  ]
}
```

**Node States:** `stopped` | `starting` | `running` | `stopping` | `error`

---

#### `GET /api/nodes/{id}/schema`
JSON Schema (Draft 7) за конфигурацията на нодата. Генерира се **динамично** от `Model::save()` на новосъздаден модел.

**Response:**
```json
{
  "node_id": 1,
  "type": "VideoEffect",
  "schema": {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "type": "object",
    "title": "VideoEffect Configuration",
    "properties": {
      "blurRadius": {
        "type": "integer",
        "minimum": 0,
        "maximum": 50,
        "default": 0,
        "description": "Gaussian blur radius in pixels"
      },
      "brightness": {
        "type": "integer",
        "minimum": -100,
        "maximum": 100,
        "default": 0,
        "description": "Brightness adjustment (-100 to 100)"
      },
      "contrast": {
        "type": "integer",
        "minimum": 0,
        "maximum": 200,
        "default": 100,
        "description": "Contrast factor (0-200, 100 = normal)"
      },
      "flipMode": {
        "type": "string",
        "enum": ["none", "horizontal", "vertical", "both"],
        "default": "none",
        "description": "Flip video frame"
      }
    },
    "required": ["blurRadius", "brightness", "contrast", "flipMode"],
    "additionalProperties": false
  }
}
```

**Generation Logic:**
1. `RestApiServer` създава временен модел: `auto model = registry->createModel(type)`
2. Извиква `model->save()` → получава `QJsonObject`
3. Анализира всеки property: type, range (от metadata/validators), default, description
4. Генерира JSON Schema

---

#### `GET /api/nodes/{id}/config`
Текущата конфигурация на нодата (реално `model->save()`).

**Response:**
```json
{
  "node_id": 1,
  "type": "VideoEffect",
  "config": {
    "blurRadius": 5,
    "brightness": 10,
    "contrast": 110,
    "flipMode": "horizontal"
  },
  "timestamp": "2026-09-28T10:30:00Z"
}
```

---

#### `POST /api/nodes/{id}/config`
Актуализация на конфигурацията. **Partial update** — изпращат се само променените полета.

**Request:**
```json
{
  "blurRadius": 8,
  "contrast": 120
}
```

**Response:**
```json
{
  "node_id": 1,
  "type": "VideoEffect",
  "config": {
    "blurRadius": 8,
    "brightness": 10,
    "contrast": 120,
    "flipMode": "horizontal"
  },
  "applied": true,
  "timestamp": "2026-09-28T10:30:05Z"
}
```

**Validation:**
- Server валидира срещу schema (range, enum, required)
- Ако валидация falle → `400 Bad Request` с детайли
- Ако `model->load()` връща false → `500 Internal Server Error`
- Успешна промяна → emit `nodeConfigChanged` event през WS

---

#### `POST /api/nodes/{id}/start`
Стартира нодата (извиква `IStartable::start()`).

**Request:** (empty body)

**Response:**
```json
{
  "node_id": 1,
  "state": "running",
  "timestamp": "2026-09-28T10:30:10Z"
}
```

**Errors:**
- `400` — Node does not implement IStartable
- `409` — Node already running
- `500` — start() failed

---

#### `POST /api/nodes/{id}/stop`
Стопва нодата (извиква `IStoppable::stop()`).

**Request:** (empty body)

**Response:**
```json
{
  "node_id": 1,
  "state": "stopped",
  "timestamp": "2026-09-28T10:30:15Z"
}
```

---

#### `GET /api/flow`
Пълният flow JSON (nodes + connections + ui section).

**Response:**
```json
{
  "nodes": [...],
  "connections": [...],
  "ui": {
    "version": 1,
    "nodes": {...},
    "workspaces": [...]
  },
  "metadata": {
    "loaded_at": "2026-09-28T10:00:00Z",
    "file_path": "/data/flows/video_4views.flow"
  }
}
```

---

#### `POST /api/flow`
Hot-reload на нов flow файл.

**Request:**
```json
{
  "flow_path": "/data/flows/new_flow.flow",
  "stop_current": true
}
```

**Response:**
```json
{
  "success": true,
  "nodes_loaded": 8,
  "connections_loaded": 7,
  "timestamp": "2026-09-28T10:35:00Z"
}
```

---

### 2.5 WebSocket: `/api/events`

Real-time event stream. Клиентът се свързва с `ws://host:8080/api/events?token=<secret>`.

#### Event Format

```json
{
  "event": "nodeStarted",
  "timestamp": "2026-09-28T10:30:10.123Z",
  "node_id": 1,
  "data": {}
}
```

#### Event Types

| Event | Description | Data Payload |
|-------|-------------|--------------|
| `nodeStarted` | Node успешно стартирана | `{}` |
| `nodeStopped` | Node спряна | `{}` |
| `nodeConfigChanged` | Конфигурация променена | `{"config": {...}, "changed_keys": ["blurRadius"]}` |
| `nodeError` | Грешка в нода | `{"error": "Failed to open video file", "code": "FILE_NOT_FOUND"}` |
| `nodeStateChanged` | Промяна на state | `{"old_state": "stopped", "new_state": "running"}` |
| `dataPreview` | Preview на данни (опционално) | `{"port": 0, "type": "VideoFrameData", "preview": "base64_thumbnail_or_stats"}` |
| `flowLoaded` | Нов flow зареден | `{"nodes_count": 8, "file_path": "..."}` |
| `engineShutdown` | HeadlessEngine спира | `{}`

#### Subscription Filtering (Optional)

Клиентът може да се абонерира само за определени нодове:
```
ws://host:8080/api/events?token=secret&nodes=1,2,3
```

---

## 3. Generic save()/load() Mapping — Как работи "Zero Boilerplate"

### 3.1 NodeDelegateModel Contract

Всеки нод в Daqster имплементира:

```cpp
class NodeDelegateModel {
public:
    virtual QJsonObject save() const = 0;
    virtual void load(const QJsonObject& obj) = 0;
    // ...
};
```

### 3.2 REST API Server Logic

```cpp
// GET /api/nodes/{id}/config
QJsonObject RestApiServer::handleGetConfig(NodeId id) {
    auto* model = m_engine->getNodeModel(id);
    if (!model) throw NotFoundException();
    return model->save();  // Всяка подробност от модела
}

// POST /api/nodes/{id}/config
void RestApiServer::handlePostConfig(NodeId id, const QJsonObject& config) {
    auto* model = m_engine->getNodeModel(id);
    if (!model) throw NotFoundException();
    
    // Merge: текущ config + новите стойности
    QJsonObject current = model->save();
    for (auto it = config.begin(); it != config.end(); ++it) {
        current[it.key()] = it.value();
    }
    
    // Validate against schema (optional but recommended)
    if (!validateAgainstSchema(model->type(), current)) {
        throw ValidationException("Invalid config");
    }
    
    // Apply
    if (!model->load(current)) {
        throw InternalException("Model load failed");
    }
    
    // Emit event
    m_wsServer.broadcast(Event::nodeConfigChanged(id, current));
}
```

### 3.3 Schema Generation Algorithm

```cpp
QJsonObject RestApiServer::generateSchema(const QString& modelType) {
    // 1. Create temporary model instance
    auto model = m_registry->createModel(modelType);
    if (!model) return {};
    
    // 2. Get save() output
    QJsonObject sample = model->save();
    
    // 3. Convert each property to JSON Schema
    QJsonObject schema;
    schema["$schema"] = "http://json-schema.org/draft-07/schema#";
    schema["type"] = "object";
    schema["title"] = modelType + " Configuration";
    
    QJsonObject properties;
    QJsonArray required;
    
    for (auto it = sample.begin(); it != sample.end(); ++it) {
        QString key = it.key();
        QJsonValue val = it.value();
        
        QJsonObject prop;
        switch (val.type()) {
            case QJsonValue::Bool:
                prop["type"] = "boolean";
                break;
            case QJsonValue::Double:
                prop["type"] = val.toInt() == val.toDouble() ? "integer" : "number";
                break;
            case QJsonValue::String:
                prop["type"] = "string";
                // Check if enum (heuristic: if model has enum metadata)
                if (model->hasEnumMetadata(key)) {
                    prop["enum"] = model->getEnumValues(key);
                }
                break;
            case QJsonValue::Array:
                prop["type"] = "array";
                break;
            case QJsonValue::Object:
                prop["type"] = "object";
                break;
            default:
                prop["type"] = "string";
        }
        
        // Try to get range/default from model metadata
        if (model->hasRangeMetadata(key)) {
            auto [min, max] = model->getRange(key);
            prop["minimum"] = min;
            prop["maximum"] = max;
        }
        if (model->hasDefaultMetadata(key)) {
            prop["default"] = model->getDefault(key);
        }
        if (model->hasDescriptionMetadata(key)) {
            prop["description"] = model->getDescription(key);
        }
        
        properties[key] = prop;
        required.append(key);
    }
    
    schema["properties"] = properties;
    schema["required"] = required;
    schema["additionalProperties"] = false;
    
    return schema;
}
```

### 3.4 Model Metadata Extensions (Optional)

За по-богати schemas, моделите могат да имплементират:

```cpp
class NodeDelegateModel {
public:
    // Optional metadata for richer API
    virtual bool hasRangeMetadata(const QString& key) const { return false; }
    virtual std::pair<double, double> getRange(const QString& key) const { return {0, 0}; }
    virtual bool hasDefaultMetadata(const QString& key) const { return false; }
    virtual QJsonValue getDefault(const QString& key) const { return {}; }
    virtual bool hasEnumMetadata(const QString& key) const { return false; }
    virtual QJsonArray getEnumValues(const QString& key) const { return {}; }
    virtual bool hasDescriptionMetadata(const QString& key) const { return false; }
    virtual QString getDescription(const QString& key) const { return {}; }
};
```

---

## 4. Implementation Details

### 4.1 RestApiServer Class

```cpp
// src/frame_work/base/src/engine/RestApiServer.h
class RestApiServer : public QObject {
    Q_OBJECT
public:
    explicit RestApiServer(HeadlessEngine* engine, QObject* parent = nullptr);
    ~RestApiServer();
    
    bool start(quint16 port, const QString& authToken = "", 
               const QString& tlsCert = "", const QString& tlsKey = "");
    void stop();
    
    quint16 port() const { return m_port; }
    bool isRunning() const { return m_running; }

signals:
    void serverStarted(quint16 port);
    void serverStopped();
    void requestReceived(const QString& method, const QString& path);
    void errorOccurred(const QString& message);

private:
    HeadlessEngine* m_engine;
    // Qt6: QHttpServer* m_httpServer;
    // Qt5: QTcpServer* m_tcpServer + custom HTTP parser
    quint16 m_port = 0;
    QString m_authToken;
    bool m_running = false;
    
    // WebSocket server
    // Qt6: QHttpServer* m_wsServer (same instance);
    // Qt5: QWebSocketServer* m_wsServer;
    
    QSet<QWebSocket*> m_wsClients;
    
    // Route handlers
    QJsonObject handleGetNodes();
    QJsonObject handleGetNodeSchema(NodeId id);
    QJsonObject handleGetNodeConfig(NodeId id);
    QJsonObject handlePostNodeConfig(NodeId id, const QJsonObject& body);
    QJsonObject handlePostNodeStart(NodeId id);
    QJsonObject handlePostNodeStop(NodeId id);
    QJsonObject handleGetFlow();
    QJsonObject handlePostFlow(const QJsonObject& body);
    
    // WebSocket
    void onWsNewConnection();
    void onWsMessageReceived(const QByteArray& message);
    void onWsClientDisconnected();
    void broadcastEvent(const QJsonObject& event);
    
    // Auth
    bool validateAuth(const QHttpServerRequest& request);
    bool validateWsAuth(QWebSocket* client, const QUrl& url);
    
    // Schema generation
    QJsonObject generateSchema(const QString& modelType);
    bool validateAgainstSchema(const QString& modelType, const QJsonObject& config);
};
```

### 4.2 HeadlessEngine Integration

```cpp
// HeadlessEngine.h additions
class HeadlessEngine : public QObject {
    // ...
public:
    bool startRestServer(quint16 port = 8080, 
                         const QString& authToken = "",
                         const QString& tlsCert = "",
                         const QString& tlsKey = "");
    void stopRestServer();
    bool isRestServerRunning() const;
    
    // Helper для API server
    QtNodes::NodeDelegateModel* getNodeModel(QtNodes::NodeId id) const;
    QVector<QtNodes::NodeId> getAllNodeIds() const;
    QString getNodeType(QtNodes::NodeId id) const;
    QString getNodeState(QtNodes::NodeId id) const;
    QJsonObject getFlowJson() const;
    bool loadFlow(const QString& flowPath, bool stopCurrent = true);
    
signals:
    // За WS broadcasting
    void nodeStarted(QtNodes::NodeId id);
    void nodeStopped(QtNodes::NodeId id);
    void nodeConfigChanged(QtNodes::NodeId id, const QJsonObject& config);
    void nodeError(QtNodes::NodeId id, const QString& error, const QString& code);
    void nodeStateChanged(QtNodes::NodeId id, const QString& oldState, const QString& newState);
    void dataPreview(QtNodes::NodeId id, int portIndex, const QJsonObject& preview);
    void flowLoaded(const QString& path, int nodeCount);
    
private:
    RestApiServer* m_restServer = nullptr;
};
```

### 4.3 CLI Integration

```cpp
// NodeRunner main.cpp additions
parser.addOption(QCommandLineOption("rest-port",
    "Enable REST API server on <port> (0 to disable)", "port", "8080"));
parser.addOption(QCommandLineOption("rest-token",
    "Bearer token for REST API authentication", "token"));
parser.addOption(QCommandLineOption("rest-tls-cert",
    "TLS certificate file for HTTPS/WSS", "cert"));
parser.addOption(QCommandLineOption("rest-tls-key",
    "TLS private key file", "key"));

// In main():
if (parser.isSet("rest-port")) {
    int port = parser.value("rest-port").toInt();
    if (port > 0) {
        QString token = parser.value("rest-token");
        QString cert = parser.value("rest-tls-cert");
        QString key = parser.value("rest-tls-key");
        if (!engine.startRestServer(port, token, cert, key)) {
            qCCritical(lcApp) << "Failed to start REST API server on port" << port;
            return 1;
        }
    }
}
```

---

## 5. Security Model

### 5.1 Authentication

| Method | Description |
|--------|-------------|
| **Bearer Token** | Simple, stateless. Token в header: `Authorization: Bearer <token>` |
| **mTLS (Optional)** | Client certificate validation. За production environments. |
| **Token Rotation** | Не в v1. За v2: `/api/auth/rotate` endpoint. |

### 5.2 Authorization (Future / v2)

```
Role          | GET /nodes | GET /config | POST /config | POST /start|stop | POST /flow | WS
--------------|------------|-------------|--------------|------------------|----------|---
admin         |     ✅     |      ✅     |      ✅      |        ✅        |    ✅    |  ✅
operator      |     ✅     |      ✅     |      ✅      |        ✅        |    ❌    |  ✅
viewer        |     ✅     |      ✅     |      ❌      |        ❌        |    ❌    |  ✅
```

### 5.3 Network Security

- **Bind address:** По подразбиране `127.0.0.1` (localhost only). За remote access: `--rest-bind 0.0.0.0`
- **TLS:** Опционално, за production. Self-signed certs за dev.
- **Rate Limiting:** Не в v1. За v2: token bucket per client IP.
- **CORS:** Enabled по подразбиране за Web UI достъп.

---

## 6. Qt5 vs Qt6 Implementation

### 6.1 Qt6 (Preferred) — QHttpServer

```cmake
# CMakeLists.txt
find_package(Qt6 REQUIRED COMPONENTS HttpServer)
target_link_libraries(RestApiServer PRIVATE Qt6::HttpServer)
```

```cpp
// Qt6 implementation
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>

m_httpServer = new QHttpServer(this);
m_httpServer->route("/api/nodes", [this](const QHttpServerRequest& req) {
    return handleGetNodes(req);
});
// ...
m_httpServer->listen(QHostAddress::Any, m_port);
```

### 6.2 Qt5 — Custom QTcpServer

```cpp
// Qt5 implementation (no external deps)
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>

m_tcpServer = new QTcpServer(this);
connect(m_tcpServer, &QTcpServer::newConnection, this, &RestApiServer::onNewConnection);

void RestApiServer::onNewConnection() {
    while (m_tcpServer->hasPendingConnections()) {
        QTcpSocket* socket = m_tcpServer->nextPendingConnection();
        // Parse HTTP manually, route, respond
    }
}
```

**WebSocket:**
- Qt6: `QHttpServer` с `upgrade()` за WebSocket
- Qt5: `QWebSocketServer` (от `QtWebSockets` module)

---

## 7. Testing Strategy

### 7.1 Unit Tests

```cpp
// tests/RestApiServerTest.cpp
void RestApiServerTest::testGetNodeConfig() {
    // Setup mock HeadlessEngine with mock NodeDelegateModel
    MockHeadlessEngine engine;
    MockNodeModel model;
    EXPECT_CALL(model, save()).WillOnce(Return(QJsonObject{{"param", 42}}));
    engine.registerMockNode(1, &model);
    
    RestApiServer server(&engine);
    server.start(0, "test-token"); // random port
    
    // HTTP GET request
    QJsonObject response = httpGet(server.port(), "/api/nodes/1/config", "test-token");
    
    ASSERT_EQ(response["config"]["param"].toInt(), 42);
}

void RestApiServerTest::testPostNodeConfigValidation() {
    // Invalid value (out of range)
    QJsonObject invalidConfig{{"blurRadius", 100}}; // max is 50
    
    QJsonObject response = httpPost(server.port(), "/api/nodes/1/config", invalidConfig, "test-token");
    
    ASSERT_EQ(response["error"]["code"].toString(), "VALIDATION_FAILED");
}
```

### 7.2 Integration Tests

```bash
# Start headless server
./build_qt5/bin/NodeRunner --headless --run tests/data/video_effect_chain.flow \
    --rest-port 18080 --rest-token test123 --log-console-enabled 1 &

SERVER_PID=$!
sleep 2

# Test endpoints
curl -s -H "Authorization: Bearer test123" http://localhost:18080/api/health | jq .
curl -s -H "Authorization: Bearer test123" http://localhost:18080/api/nodes | jq .
curl -s -H "Authorization: Bearer test123" http://localhost:18080/api/nodes/1/schema | jq .
curl -s -H "Authorization: Bearer test123" -X POST -H "Content-Type: application/json" \
    -d '{"blurRadius": 10}' http://localhost:18080/api/nodes/1/config | jq .

# WebSocket test
timeout 5 wscat -c "ws://localhost:18080/api/events?token=test123" &

# Cleanup
kill $SERVER_PID
```

### 7.3 Load/Stress Tests

- 100 concurrent WebSocket connections
- 1000 req/sec POST /config
- Memory leak check (valgrind/ASAN)

---

## 8. Future Extensions (Post-REQ-054)

### 8.1 REQ-SW-PL-056: Remote GUI (Thin Client)
- Generic `RemoteWidget` base class
- Auto-generate UI от `/schema` endpoint
- Bidirectional binding: widget ↔ REST API

### 8.2 REQ-SW-PL-057: Video/Audio Streaming
- WebRTC / WebSocket streaming proxy
- `RemoteVideoDisplayWidget`, `RemoteAudioDisplayWidget`
- Hardware encoding (VAAPI/NVENC)

### 8.3 REQ-SW-PL-058: Multi-User / RBAC
- User management, roles
- Flow ownership, permissions
- Audit log

### 8.4 gRPC / Protobuf (Alternative Transport)
- За high-performance internal communication
- Code-generated client stubs

---

## 9. Configuration File (Optional)

```json
// /etc/daqster/headless-rest.json
{
  "rest_api": {
    "enabled": true,
    "port": 8080,
    "bind_address": "127.0.0.1",
    "auth_token": "CHANGE_ME_IN_PRODUCTION",
    "tls": {
      "enabled": false,
      "cert_file": "/etc/daqster/certs/server.pem",
      "key_file": "/etc/daqster/certs/server.key"
    },
    "cors": {
      "enabled": true,
      "allowed_origins": ["*"]
    },
    "rate_limit": {
      "enabled": false,
      "requests_per_second": 100
    }
  }
}
```

---

## 10. Open Questions / Decisions Needed

| # | Question | Options | Recommendation |
|---|----------|---------|----------------|
| 1 | Qt5 HTTP implementation | Custom QTcpServer vs `cpp-httplib` (header-only) | Custom — no external deps |
| 2 | JSON Schema generation | Runtime от save() vs Compile-time от metadata | Runtime — works for all existing nodes |
| 3 | WebSocket binary vs text | Text (JSON) vs Binary (MessagePack) | Text — debuggable, sufficient |
| 4 | Data preview in WS | Base64 thumbnails vs Stats only | Stats only v1; thumbnails v2 |
| 5 | Config persistence | REST API saves to .flow file? | No — runtime only; separate `POST /api/flow/save` v2 |

---

## 11. References

- [REQ-SW-PL-054](DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-054-rest-api-remote-control.md) — Requirements
- [REQ-SW-PL-053](DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-053-nodeeditoride-split.md) — Prerequisite
- [runtime-mode-architecture.md](runtime-mode-architecture.md) — Phase 2 context
- [core-gui-split.md](core-gui-split.md) — Core/GUI separation
- Qt6 QHttpServer: https://doc.qt.io/qt-6/qhttpserver.html
- JSON Schema Draft 7: https://json-schema.org/draft-07/
- WebSocket RFC 6455: https://tools.ietf.org/html/rfc6455

---

## 12. Changelog

| Date | Version | Author | Changes |
|------|---------|--------|---------|
| 2026-09-28 | 0.1 | Daqster Team | Initial draft — PLANNED status |