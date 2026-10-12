# Daqster REST API & Remote Control Architecture

> **Status:** IMPLEMENTED (v0.4 — full graph control over REST + WebSocket in both runtime hosts)
> **Related REQ:** REQ-SW-PL-054 (REST control plane), REQ-SW-PL-063 (full graph control)
> **Depends on:** REQ-SW-PL-053 (NodeEditorIde Split → dual-mode runner), REQ-SW-PL-048 (shared AppBootstrap)
> **Date:** 2026-09-28 (draft) · 2026-10-11 (as-built rewrite) · 2026-10-12 (v0.4 full graph control)
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
│  │  │  IStoppable/       │   │  OpenAPI + Swagger UI + web UI     │   │   │
│  │  │  INodeConfigSchema │   │        │                           │   │   │
│  │  └────────────────────┘   │  ┌─────▼──────────────────────┐    │   │   │
│  │                            │  │ ControlPlaneRouter         │    │   │   │
│  │                            │  │ route(method,path,body)    │    │   │   │
│  │                            │  │ shared by HTTP + WS RPC    │    │   │   │
│  │                            │  └────────────────────────────┘    │   │   │
│  │                            └────────────────────────────────────┘   │   │
│  └────────────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────┘
                                   │  HTTP/WS (JSON) over TCP
        ┌──────────────────────────┼──────────────────────────┐
        │                          │                          │
  curl / scripts / CI        bundled web console        Swagger UI
                             (GET /)                    (GET /api/docs)
```

Three seams keep the surface host-agnostic and transport-agnostic:

- **`RuntimeController`** (`framework/base/src/control/RuntimeController.h`) — a
  plain, widget-free interface. Read side: `nodes`, `nodeTypes`, `connections`,
  `nodeConfig`, `nodeDetail`, `flowJson`, `nodeSchema`, `typeSchema`. Write side
  (REQ-SW-PL-063): `setNodeConfig`, `startNode`/`stopNode`, `createNode`,
  `removeNode`, `setNodePosition`, `setNodeLabel`, `addConnection`,
  `removeConnection`, `saveFlow`, `reloadFlow`. The server talks only to this, so
  the **same** control plane serves both runtime hosts: `HeadlessEngine`
  (`--headless`) and the GUI `RuntimeShell` (`--run`). The GUI host is started by
  the bootstrap *before* `RunRuntime()` builds the canvas, so the controller
  resolves the `DataFlowGraphModel` lazily and an early request simply sees zero
  nodes.
- **`ControlPlaneRouter`** (`framework/base/src/control/ControlPlaneRouter.{h,cpp}`)
  — a transport-independent `route(method, path, body, bodyValid) -> { handled,
  status, body }`. Both the HTTP transport and the WebSocket RPC path call it, so
  the API is defined exactly once and is testable without a socket. The HTML docs
  page, the OpenAPI document and static assets stay in `ControlPlaneServer`.
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

### `GET /api/node-types`

The palette: every registered model type and its category. Mirrors what the
editor's "create node" list offers.

```json
{
  "types": [
    { "type": "SystemMonitor", "category": "Sources/System" },
    { "type": "VideoEffect",   "category": "Processing/Video" }
  ]
}
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
`running` | `stopped`.

### `POST /api/nodes` — create

```json
{ "type": "VideoEffect", "position": { "x": 120, "y": 40 }, "config": { "effect": "blur" } }
```

`type` is required; `position` and `config` are optional (`config` is applied
through `load()`). Returns `201` with the new id and the full node detail:

```json
{ "id": 2, "node": { "id": 2, "type": "VideoEffect", ... } }
```

`400` for an unknown type or a malformed body. Emits a `nodeCreated` event.

### `GET /api/nodes/{id}` — detail

A superset of the config: identity, position, label, ports, state, the config
object **and** the schema.

```json
{
  "id": 0, "type": "SystemMonitor", "name": "System Monitor",
  "label": "System Monitor", "labelVisible": true, "state": "running",
  "position": { "x": -200, "y": 0 },
  "ports": { "in": [ { "index": 0, "caption": "" } ], "out": [] },
  "config": { "interval": 500 },
  "schema": { "type": "object", "properties": { ... } }
}
```

`404` when the id does not exist.

### `GET /api/nodes/{id}/config`

The raw `NodeDelegateModel::save()` object (no wrapper envelope):

```json
{ "interval": 500 }
```

`404` when the id does not exist.

### `PATCH /api/nodes/{id}` — move / label

```json
{ "position": { "x": 10, "y": 20 }, "label": "My node", "labelVisible": true }
```

Each key is optional; a missing `position`/`label` leaves it unchanged. `404`
when the id does not exist. Emits a `nodeMoved` event.

### `DELETE /api/nodes/{id}` — delete

Deletes the node and every connection attached to it. Returns
`{ "removed": true }`. Emits `connectionDeleted` (one per attached edge, if any)
and `nodeDeleted`. `404` when the id does not exist.

### `GET /api/connections` · `POST /api/connections` · `DELETE /api/connections`

`GET` lists the graph edges:

```json
{ "connections": [ { "outNodeId": 0, "outPortIndex": 0, "inNodeId": 1, "inPortIndex": 0 } ] }
```

`POST`/`DELETE` take one connection as the body. `POST` runs the graph model's
`connectionPossible()` first and returns `{"connected":true}`, or `400`
`{"error":"connection not possible"}` when the ports cannot be joined. `DELETE`
returns `{"disconnected":true}` or `404` when the edge does not exist. Both emit
a `connectionCreated` / `connectionDeleted` event.

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

JSON Schema (Draft 7) for the node's config. Enriched when the model implements
`Daqster::INodeConfigSchema` (ranges, enums, defaults, descriptions and
conditional `visibleWhen`); otherwise best-effort derived from a throw-away
model instance's `save()` output — see [§7](#7-generic-save--load-mapping).

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

### `POST /api/flow/save` — write the live scene

```json
{ "path": "/data/flows/current.flow" }
```

Serialises the live scene (`flowJson()`) to `path`. Returns
`{"saved":true,"path":"..."}`; `400` when `path` is missing or unwritable.

### `GET /api/openapi.json` · `GET /api/docs`

The full OpenAPI 3.0.3 document (13 paths + component schemas), and a Swagger UI
page that loads it. `/api/docs` references locally vendored Swagger assets
under `/assets/swagger-ui/` when present, otherwise the unpkg CDN. To vendor:

```bash
./scripts/fetch-swagger-ui.sh ./rest-web-ui      # writes rest-web-ui/assets/swagger-ui/
# then:  --rest-web-root ./rest-web-ui
```

`FETCH` is optional; the directory is a gitignored build artifact.

### `GET /` — bundled web console

A dependency-free single-page console: node list (start/stop/config/delete),
create-from-palette, connections table (disconnect), a **schema-generated**
config form (enums → selects, ranges → number inputs, `visibleWhen` hides
irrelevant fields; raw-JSON fallback), Save-flow, and a live event log fed by
the WebSocket. Served from `<web-root>/index.html` when present, otherwise built
into the binary. `GET /assets/*` serves files from the web root
(path-traversal safe).

### `WS /api/events`

Connect with `ws://<host>:<port>/api/events?token=<secret>` (or a Bearer
header). The endpoint is **bidirectional** (REQ-SW-PL-063 AC12): the server
pushes events and also accepts control commands.

**Server → client events:**

```json
{ "type": "nodeStarted", "data": { "nodeId": 0 } }
```

| `type` | `data` | Status |
|--------|--------|--------|
| `nodeStarted` | `{ "nodeId": <int> }` | ✅ |
| `nodeStopped` | `{ "nodeId": <int> }` | ✅ |
| `nodeConfigChanged` | `{ "nodeId": <int> }` | ✅ |
| `nodeCreated` | `{ "nodeId": <int> }` | ✅ |
| `nodeDeleted` | `{ "nodeId": <int> }` | ✅ |
| `nodeMoved` | `{ "nodeId": <int> }` | ✅ |
| `connectionCreated` | `{ "outNodeId", "outPortIndex", "inNodeId", "inPortIndex" }` | ✅ |
| `connectionDeleted` | same as `connectionCreated` | ✅ |
| `nodeError` | `{ "message": "<string>" }` | ✅ |
| `dataPreview` | — | **(planned)** (no producer yet) |
| `flowLoaded`, `nodeStateChanged`, `engineShutdown` | — | **(planned)** |

**Client → server commands (RPC).** A text frame is a JSON object:

```json
{ "id": 1, "method": "POST", "path": "/api/nodes", "body": { "type": "SystemMonitor" } }
```

It is routed through the *same* `ControlPlaneRouter` as HTTP, so every data-API
method/path works over the socket. The reply carries the request `id` back:

```json
{ "id": 1, "status": 201, "body": { "id": 2, "node": { ... } } }
```

An unhandled path replies `{"id":..,"status":404,"body":{"error":"not found"}}`.
Events and RPC replies are distinguishable by shape: an event has `type`, a
reply has `status`.

The server answers `ping` with `pong` and honours `close`. Per-node subscription
filtering (`?nodes=1,2,3`) is **(planned)**.

---

## 5. Server implementation

`ControlPlaneServer` (`src/frame_work/base/src/control/ControlPlaneServer.{h,cpp}`)
speaks HTTP/1.1 directly on a `QTcpServer`: header/body framing, constant-time
auth, static files and a hand-rolled RFC 6455 WebSocket — **one implementation
for Qt5 and Qt6** (see §9). Routing of the *data* API is delegated to
`ControlPlaneRouter` (`route(method, path, body, bodyValid)`); the server keeps
the OpenAPI document, the docs page and static files. Each plain HTTP request
opens a connection, gets one response, and the server closes it
(`Connection: close`); WebSocket connections are upgraded in place and kept open
on the same port.

Events are wired in `HeadlessEngine::startControlPlane()` (and mirrored in
`RuntimeShell::startControlPlane()`):

```cpp
connect(this, &HeadlessEngine::nodeStarted, m_controlPlane.get(), [this](QtNodes::NodeId id) {
    m_controlPlane->broadcastEvent(QStringLiteral("nodeStarted"),
                                   {{QStringLiteral("nodeId"), static_cast<qint64>(id)}});
});
// nodeStopped, nodeConfigChanged, errorOccurred → nodeError,
// nodeCreated, nodeDeleted, nodeMoved,
// connectionCreated, connectionDeleted
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

**Schema generation** (`HeadlessEngine::typeSchema` / `nodeSchema`): instantiate
the node type, call `save()`, and walk the result —

- `bool → boolean`, `double → number`, `string → string`,
  `array → array` (+ `items` from the first element), `object → object`
  (recursing into `properties`), `null → null`;
- every top-level key is listed in `required`;
- `$schema` (Draft 7) and `title` are added.

**Enrichment (REQ-SW-PL-063 AC10).** When the node model also implements
`Daqster::INodeConfigSchema` (a widget-free capability in
`plugins/common/capabilities/INodeConfigSchema.h`), its `configSchema()` is
merged over the derived schema — a `properties` fragment carrying `minimum`,
`maximum`, `default`, `description`, `enum`/`x-enumNames` and a `visibleWhen`
condition. This is how per-node metadata stays available on the headless path
without depending on GUI widgets. `VideoEffectNode` is the pilot. Models without
the capability still get the derived schema (same public contract; the extra
keywords are additive). The merge lives in
`NodeSchemaBuilder.h::nodeConfigSchema(defaults, type, provided)`.

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
preflight, path-traversal blocked. REQ-SW-PL-063 additionally verified:
`node-types`, create/delete nodes, connect/disconnect (incl. the
`connection not possible` `400`), move/label via `PATCH`, `flow/save`, the
enriched `VideoEffect` schema, and control over the **WebSocket RPC** path
(commands routed through `ControlPlaneRouter`, replies carrying `id`/`status`,
`nodeCreated`/`nodeDeleted` events observed on the same socket).

Verified on Qt6 in **GUI mode** (`--run`): the same endpoints, config
GET→POST→GET, create/delete/move/label, connect/disconnect, `flow/save`, WS
RPC, start/stop, flow reload by JSON and by path, bearer `401`/`200`, and
fail-closed `--rest-bind 0.0.0.0` without a token. Qt5 GUI mode currently aborts
on an unrelated pre-existing `QWidget*` metatype conflict (also present without
`--rest-port`); it is tracked separately.

**Automated test:** `tests/framework/control/control_plane_router_tests` —
26 cases over `ControlPlaneRouter::route()` (every endpoint, `400`/`404`/`405`,
bad body) and the `NodeSchemaBuilder` enrichment merge. Runs headless; gated by
`-DDAQSTER_BUILD_TESTS=ON`.

---

## 11. Future extensions (post PL-063)

- **`dataPreview`** events (streaming node values) — separate requirement.
- **Remote GUI / thin client**, **video/audio streaming**, **multi-user RBAC**,
  **gRPC/Protobuf** — separate future requirements (IDs not yet allocated).
- **mTLS**, richer schema keywords and undo/redo over REST — future work.

**Non-goals (REQ-SW-PL-063):** REST mutations deliberately bypass the editor's
`QUndoStack`; there is no RBAC/rate-limiting, no mTLS, and no thin-client or
media streaming. See the requirement file for the full list.

---

## 12. Changelog

| Date | Version | Author | Changes |
|------|---------|--------|---------|
| 2026-09-28 | 0.1 | Daqster Team | Initial draft (planned) |
| 2026-10-11 | 0.2 | Daqster Team | As-built rewrite: `ControlPlaneServer` on `QTcpServer`; AC1–AC13 implemented (REST, WS events, CORS, OpenAPI + Swagger UI + web console); removed the divergent `QHttpServer`/`RestApiServer` design. |
| 2026-10-11 | 0.3 | Daqster Team | GUI mode: `RuntimeShell` implements `RuntimeController` and owns the `ControlPlaneServer`, so `--run --rest-port` serves the same API; shared `NodeSchemaBuilder.h`. |
| 2026-10-12 | 0.4 | Daqster Team | Full graph control (REQ-SW-PL-063): `ControlPlaneRouter` seam shared by HTTP + WS RPC; `node-types`, create/delete/move/label, connections, `flow/save`; enriched schema via `INodeConfigSchema` (VideoEffect pilot); bidirectional WS (commands + new events); schema-driven web console; router unit tests. |
