# Daqster REST API & Remote Control Architecture

> **Status:** IMPLEMENTED (v0.3 — control plane live in both runtime hosts; extras pending)
> **Related REQ:** REQ-SW-PL-054
> **Depends on:** REQ-SW-PL-053 (NodeEditorIde Split → dual-mode runner), REQ-SW-PL-048 (shared AppBootstrap)
> **Date:** 2026-09-28 (draft) · 2026-10-11 (as-built rewrite)
> **Author:** Daqster Team

This document describes the **as-built** control plane. It replaced an earlier
(draft) design that assumed `QHttpServer`/`QWebSocketServer` and a separate
`RestApiServer` class; the implementation deliberately differs — see
[§9 Qt5 vs Qt6](#9-qt5-vs-qt6--single-implementation). Anything still only
planned is marked **(planned)**.

---

## 1. Overview

```
┌──────────────────────────────────────────────────────────────────────────┐
│                          RUNNER (NodeRunner / Daqster)                     │
│                                                                            │
│  AppBootstrap (widget-free, REQ-SW-PL-048)                                 │
│    parses --rest-*  ──► IRuntimeHost::configureControlPlane(ControlPlaneOptions)
│                                        │                                   │
│  ┌─────────────────────────────────────▼──────────────────────────────┐   │
│  │ Runtime host (RuntimeController; headless/GUI)                      │   │
│  │  ┌────────────────────┐   ┌────────────────────────────────────┐   │   │
│  │  │ DataFlowGraphModel │   │ ControlPlaneServer                 │   │   │
│  │  │  node models       │◄──│  HTTP/1.1  (QTcpServer)            │   │   │
│  │  │  IStartable/       │   │  WebSocket (RFC 6455, same port)   │   │   │
│  │  │  IStoppable        │   │  OpenAPI + Swagger UI + web UI     │   │   │
│  │  └────────────────────┘   └────────────────────────────────────┘   │   │
│  └────────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────┘
                                   │  HTTP/WS (JSON) over TCP
        ┌──────────────────────────┼──────────────────────────┐
        │                          │                          │
  curl / scripts / CI        bundled web console        Swagger UI
                             (GET /)                    (GET /api/docs)
```

Two seams keep the surface host-agnostic:

- **`RuntimeController`** (`framework/base/src/control/RuntimeController.h`) — a
  plain, widget-free interface (`nodes`, `nodeConfig`, `setNodeConfig`,
  `startNode`, `stopNode`, `flowJson`, `nodeSchema`, `reloadFlow`). The HTTP
  server talks only to this, so the **same** control plane serves both runtime
  hosts: `HeadlessEngine` (`--headless`) and the GUI `RuntimeShell` (`--run`).
  The GUI host is started by the bootstrap *before* `RunRuntime()` builds the
  canvas, so the controller resolves the `DataFlowGraphModel` lazily and an early
  request simply sees zero nodes.
- **`ControlPlaneOptions`** (`plugins/common/capabilities/IRuntimeHost.h`) — the
  parsed `--rest-*` options. A host that cannot serve remote control returns
  `false` from `configureControlPlane()` and the runner fails loudly.

---

## 2. Starting the server

```bash
./build_qt5/bin/NodeRunner --headless --run tests/data/system_monitor_test.flow \
    --rest-port 18080 \
    --rest-token secret123 \
    [--rest-bind 127.0.0.1] \
    [--rest-web-root ./rest-web-ui]
```

| Flag | Meaning | Default |
|------|---------|---------|
| `--rest-port <port>` | **Opt-in.** Enables the server on `<port>` | disabled |
| `--rest-bind <addr>` | Bind address | `127.0.0.1` (loopback) |
| `--rest-token <token>` | Bearer token; empty = no auth | empty |
| `--rest-web-root <dir>` | Directory with the web UI / Swagger assets | empty |

**Deviation from the original REQ:** the port is **disabled by default**
(explicit `--rest-port` required). A listener on a fixed port by default is a
surprise surface on any machine that runs a flow; opt-in is safer.

**Fail-closed:** binding a **non-loopback** address without `--rest-token`
refuses to start (the runner exits non-zero). `--rest-tls-cert/key` (mTLS) is
**not implemented** — the flags do not exist yet **(planned)**.

---

## 3. Authentication

- **Bearer token.** Every **data / mutation** route requires
  `Authorization: Bearer <token>`. Comparison is constant-time.
- **Public routes** (reachable by plain browser navigation, no token):
  `GET /`, `GET /assets/*`, `GET /api/docs`, `GET /api/openapi.json`.
- **WebSocket** cannot carry an `Authorization` header from a browser, so
  `GET /api/events?token=<token>` is also accepted (and a Bearer header is
  honoured for non-browser clients).
- With **no token configured**, the server must be loopback-only; every request
  is accepted.

mTLS, token rotation and RBAC are **(planned)**.

---

## 4. API Contract

Base: `http://<host>:<port>` · WebSocket: `ws://<host>:<port>/api/events`.
The machine-readable spec is `GET /api/openapi.json` (OpenAPI 3.0.3); the human
UI is `GET /api/docs`.

### Error format

Errors are a flat JSON object (not the nested draft format):

```json
{ "error": "not found" }
```

Status codes: `200`, `204` (CORS preflight), `400`, `401`, `404`, `405`,
`500`. A `405` body is `{"error":"method not allowed"}`.

### `GET /api/health`

```json
{ "status": "ok", "nodes": 2 }
```

### `GET /api/nodes`

```json
{
  "nodes": [
    { "id": 0, "type": "SystemMonitor", "name": "System Monitor", "state": "running" },
    { "id": 1, "type": "NumberResult",  "name": "NumberResult",  "state": "stopped" }
  ]
}
```

`type` is `NodeDelegateModel::name()`, `name` is `caption()`, `state` is
`running` | `stopped`. Richer per-node fields from the draft (`category`,
`ports`, `auto_start`, …) are **(planned)**.

### `GET /api/nodes/{id}` and `GET /api/nodes/{id}/config`

Both return the node's raw `NodeDelegateModel::save()` object (there is no
wrapper envelope):

```json
{ "interval": 500 }
```

`404` when the id does not exist.

### `POST /api/nodes/{id}/config`

Body is the JSON patch passed to `NodeDelegateModel::load()`. Because `load()`
returns `void`, the engine snapshots `save()` first and rolls back if the model
throws — a bad patch cannot take the engine down.

```json
{ "applied": true }
```

`400` when rejected / malformed body. On success emits a
`nodeConfigChanged` event.

### `POST /api/nodes/{id}/start` · `POST /api/nodes/{id}/stop`

Call `IStartable::start()` / `IStoppable::stop()`.

```json
{ "ok": true }
```

`400` when the node does not implement the corresponding interface.

### `GET /api/nodes/{id}/schema`

JSON Schema (Draft 7, best-effort) generated from a throw-away model instance's
`save()` output — see [§7](#7-generic-save--load-mapping). Numbers all map to
`number` and every key is `required`; range/enum/default metadata is
**(planned)**.

### `GET /api/flow`

```json
{ "nodes": [ ... ], "connections": [ ... ], "ui": { "version": 1, ... } }
```

### `POST /api/flow` — hot reload

Stops every running node, rebuilds the graph and starts the new scene's
`autoStart` nodes. The body is **either** a full flow object **or** a path
reference:

```json
{ "path": "/data/flows/new_flow.flow" }
```

Response:

```json
{ "applied": true }
```

`400` with `{"applied":false,"error":"..."}` on a bad body, unreadable file, or
a structurally invalid document (`nodes[]`, `connections[]`, `ui{}` are type
checked — a malformed graph is rejected rather than silently loaded as empty).

### `GET /api/openapi.json` · `GET /api/docs`

The full OpenAPI 3.0.3 document (10 paths + component schemas), and a Swagger UI
page that loads it. `/api/docs` references locally vendored Swagger assets
under `/assets/swagger-ui/` when present, otherwise the unpkg CDN. To vendor:

```bash
./scripts/fetch-swagger-ui.sh ./rest-web-ui      # writes rest-web-ui/assets/swagger-ui/
# then:  --rest-web-root ./rest-web-ui
```

`FETCH` is optional; the directory is a gitignored build artifact.

### `GET /` — bundled web console

A dependency-free single-page console (node list with start/stop, live event
log). Served from `<web-root>/index.html` when present, otherwise built into the
binary. `GET /assets/*` serves files from the web root (path-traversal safe).

### `WS /api/events`

Connect with `ws://<host>:<port>/api/events?token=<secret>` (or a Bearer
header). Server → client only:

```json
{ "type": "nodeStarted", "data": { "nodeId": 0 } }
```

| `type` | `data` | Status |
|--------|--------|--------|
| `nodeStarted` | `{ "nodeId": <int> }` | ✅ |
| `nodeStopped` | `{ "nodeId": <int> }` | ✅ |
| `nodeConfigChanged` | `{ "nodeId": <int> }` | ✅ |
| `nodeError` | `{ "message": "<string>" }` | ✅ |
| `dataPreview` | — | **(planned)** (no producer yet) |
| `flowLoaded`, `nodeStateChanged`, `engineShutdown` | — | **(planned)** |

The server answers `ping` with `pong` and honours `close`. Per-node subscription
filtering (`?nodes=1,2,3`) is **(planned)**.

---

## 5. Server implementation

`ControlPlaneServer` (`src/frame_work/base/src/control/ControlPlaneServer.{h,cpp}`)
speaks HTTP/1.1 directly on a `QTcpServer`: header/body framing, routing,
constant-time auth, static files and a hand-rolled RFC 6455 WebSocket — **one
implementation for Qt5 and Qt6** (see §9). Each plain HTTP request opens a
connection, gets one response, and the server closes it (`Connection: close`);
WebSocket connections are upgraded in place and kept open on the same port.

Events are wired in `HeadlessEngine::startControlPlane()`:

```cpp
connect(this, &HeadlessEngine::nodeStarted, m_controlPlane.get(), [this](QtNodes::NodeId id) {
    m_controlPlane->broadcastEvent(QStringLiteral("nodeStarted"),
                                   {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
});
// nodeStopped, nodeConfigChanged, errorOccurred → nodeError
```

---

## 6. CLI integration

Options are declared and parsed once in `AppBootstrap` (shared by `NodeRunner`
and `Daqster`) and handed to the selected host:

```cpp
struct ControlPlaneOptions {
    bool    enabled     = false;                 // only when --rest-port is given
    quint16 port        = 8080;
    QString bindAddress = QStringLiteral("127.0.0.1");
    QString authToken;                           // empty = no auth (loopback only)
    QString webRoot;                             // --rest-web-root
};
```

`IRuntimeHost::configureControlPlane()` returns whether the host accepted the
configuration; a requested-but-unsupported control plane aborts startup.

---

## 7. Generic `save()` / `load()` mapping

"Zero boilerplate": the API never enumerates node fields. Configuration is the
node's own `save()` JSON and updates go through `load()`.

```cpp
// GET /api/nodes/{id}/config
QJsonObject save;
controller->nodeConfig(id, save);      // model->save()

// POST /api/nodes/{id}/config
controller->setNodeConfig(id, patch);  // snapshot save() → model->load(patch)
                                       // roll back on throw
```

**Schema generation** (`HeadlessEngine::nodeSchema`): instantiate the node type,
call `save()`, and walk the result —

- `bool → boolean`, `double → number`, `string → string`,
  `array → array` (+ `items` from the first element), `object → object`
  (recursing into `properties`), `null → null`;
- every top-level key is listed in `required`;
- `$schema` (Draft 7) and `title` are added.

Richer metadata (ranges, enums, defaults, descriptions) would require
per-model hints and is **(planned)**.

---

## 8. Security model (as built)

| Aspect | Behaviour |
|--------|-----------|
| Bind | loopback by default; non-loopback requires a token (fail-closed) |
| Auth | constant-time `Bearer` compare; WS accepts `?token=` |
| Public | `/`, `/assets/*`, `/api/docs`, `/api/openapi.json` |
| CORS | `Access-Control-Allow-Origin: *`, `Allow-Headers: Authorization, Content-Type`, `Allow-Methods: GET, POST, OPTIONS`; unauthenticated `OPTIONS` preflight |
| Static serving | canonical-path check — never escapes `--rest-web-root` |
| mTLS / TLS | **(planned)** |

RBAC, rate limiting and token rotation remain **(planned)**.

---

## 9. Qt5 vs Qt6 — single implementation

The original plan was `QHttpServer` on Qt6 and a hand-written `QTcpServer` on
Qt5. That would have meant two divergent servers. Instead there is **one**
HTTP/WS implementation on `QTcpServer`, because:

- Qt5 has no `QtHttpServer` module at all;
- `QWebSocketServer` would have forced a **second listening port**, breaking
  `ws://host:<port>/api/events`;
- one code path keeps API and auth behaviour identical across Qt lines.

So neither `Qt::HttpServer` nor `Qt::WebSockets` is a dependency of
`FrameworkCore`.

---

## 10. Testing

```bash
# Start headless with the control plane
./build_qt5/bin/NodeRunner --headless --run tests/data/system_monitor_test.flow \
    --rest-port 18080 --rest-token secret123 --log-console-enabled 1 &

# The identical control plane serves the GUI host (no --headless)
./build_qt6/bin/NodeRunner --run tests/data/video_4views_grid1.flow \
    --rest-port 18090 --rest-token secret123 --log-console-enabled 1 &

# REST
curl -s -H "Authorization: Bearer secret123" http://localhost:18080/api/health
curl -s -H "Authorization: Bearer secret123" http://localhost:18080/api/nodes
curl -s -H "Authorization: Bearer secret123" http://localhost:18080/api/nodes/0/schema
curl -s -H "Authorization: Bearer secret123" -X POST \
     -H "Content-Type: application/json" -d '{"interval":500}' \
     http://localhost:18080/api/nodes/0/config
curl -s -H "Authorization: Bearer secret123" -X POST \
     -d '{"path":"/abs/path/to/new.flow"}' http://localhost:18080/api/flow

# Docs / spec / console
curl -s http://localhost:18080/api/docs          # Swagger UI
curl -s http://localhost:18080/api/openapi.json  # spec

# WebSocket
python3 -c 'import websocket; print(websocket.create_connection("ws://localhost:18080/api/events?token=secret123").recv())'
```

Verified on Qt5 + Qt6 (headless): health/nodes/flow/openapi, config
GET→POST→GET in place, schema, start/stop lifecycle events over WS, flow reload
by JSON and by path, `400` on malformed flow, `401` without token, `204` OPTIONS
preflight, path-traversal blocked.

Verified on Qt6 in **GUI mode** (`--run`): the same endpoints, config
GET→POST→GET, start/stop, flow reload by JSON and by path, WS events, bearer
`401`/`200`, and fail-closed `--rest-bind 0.0.0.0` without a token. Qt5 GUI mode
currently aborts on an unrelated pre-existing `QWidget*` metatype conflict
(also present without `--rest-port`); it is tracked separately.

Automated unit/integration tests for the server are **(planned)**.

---

## 11. Future extensions (post PL-054)

- **`dataPreview`** events + a `POST /api/flow/save` endpoint.
- **Remote GUI / thin client**, **video/audio streaming**, **multi-user RBAC**,
  **gRPC/Protobuf** — separate future requirements (IDs not yet allocated).
- **mTLS** and richer JSON-Schema metadata.

---

## 12. Changelog

| Date | Version | Author | Changes |
|------|---------|--------|---------|
| 2026-09-28 | 0.1 | Daqster Team | Initial draft (planned) |
| 2026-10-11 | 0.2 | Daqster Team | As-built rewrite: `ControlPlaneServer` on `QTcpServer`; AC1–AC13 implemented (REST, WS events, CORS, OpenAPI + Swagger UI + web console); removed the divergent `QHttpServer`/`RestApiServer` design. |
| 2026-10-11 | 0.3 | Daqster Team | GUI mode: `RuntimeShell` implements `RuntimeController` and owns the `ControlPlaneServer`, so `--run --rest-port` serves the same API; shared `NodeSchemaBuilder.h`. |
