# REQ-SW-PL-054: REST API и Remote Control за Headless Mode

> **Parent:** REQ-SW-PL-051 (Core/GUI разделение + headless)
> **Depends on:** REQ-SW-PL-053 (NodeEditorIde Split → чист headless binary)
> **Scope:** Generic REST API + WebSocket за remote control/monitoring на headless сървър

## Проблем

Headless сървърът (`DaqsterHeadless` / `NodeRunner --headless`) работи на сървър/edge устройство без GUI. Операторите/инженерите са на други машини и се нуждаят от:
- Programmatic control (CI/CD, automation scripts)
- Remote GUI (Daqster Editor или Web UI на клиентска машина)
- Real-time monitoring (node state, data preview, performance metrics)
- Multi-user access с роли (admin, operator, viewer)

## Цел

Добавяне на **generic REST API + WebSocket** в `HeadlessEngine`, което:
1. Експортира **цялата конфигурация и състояние** на всеки нод чрез `NodeDelegateModel::save()/load()`
2. Позволява **start/stop/config** на нодове през HTTP
3. Изпраща **real-time events** през WebSocket
4. Е **zero-boilerplate** за нови нодове (работи автоматично за всяка модель с `save()/load()`)

## Acceptance Criteria

| AC | Description | Status |
|---|---|---|
| 1 | `HeadlessEngine` стартира HTTP сървър на конфигурируем порт (default 8080) | ☐ |
| 2 | `GET /api/nodes` — списък на всички нодове (id, type, name, state) | ☐ |
| 3 | `GET /api/nodes/{id}/schema` — JSON Schema генериран от `Model::save()` на празен модел | ☐ |
| 4 | `GET /api/nodes/{id}/config` — текуща конфигурация (`model->save()`) | ☐ |
| 5 | `POST /api/nodes/{id}/config` — актуализация (`model->load()` с валидация) | ☐ |
| 6 | `POST /api/nodes/{id}/start` — `dynamic_cast<IStartable>->start()` | ☐ |
| 7 | `POST /api/nodes/{id}/stop` — `dynamic_cast<IStoppable>->stop()` | ☐ |
| 8 | `GET /api/flow` — текущия flow JSON (nodes + connections + ui) | ☐ |
| 9 | `POST /api/flow` — hot-reload на нов flow файл | ☐ |
| 10 | `WS /api/events` — real-time: `nodeStarted`, `nodeStopped`, `nodeConfigChanged`, `nodeError`, `dataPreview` | ☐ |
| 11 | Authentication: Bearer token (config file) + опционално mTLS | ☐ |
| 12 | CORS headers за Web UI достъп | ☐ |
| 13 | OpenAPI/Swagger spec генерирана автоматично | ☐ |
| 14 | Документация: `docs/Architecture/rest-api-remote-control.md` | ☐ |

## Non-Goals (Out of Scope)

- **Video/Audio streaming** — отделен REQ (REQ-SW-PL-057)
- **Remote GUI widgets (Thin Client)** — отделен REQ (REQ-SW-PL-056)
- **Multi-user flow ownership / RBAC** — отделен REQ (REQ-SW-PL-058)
- **gRPC / Protobuf** — REST/JSON е sufficient за v1

## Implementation Plan

1. **Добави `RestApiServer` клас** в `src/frame_work/base/src/engine/`
   - Използва `QHttpServer` (Qt6) или custom `QTcpServer` + `QJsonDocument` (Qt5/Qt6)
   - Рутира заявки към `HeadlessEngine` методи
   
2. **Разширение на `HeadlessEngine`**:
   - `startRestServer(quint16 port, QString authToken)`
   - `stopRestServer()`
   - Helper методи: `getNodeModel(NodeId)`, `getAllNodes()`, `validateConfig()`

3. **Generic JSON Schema генерация** от `save()`:
   - Инстанцира временен модел → `save()` → анализира типове → генерира JSON Schema Draft 7

4. **WebSocket endpoint** за events:
   - `HeadlessEngine` emits → `RestApiServer` broadcast към всички WS клиенти

5. **CLI флагове** за `DaqsterHeadless` / `NodeRunner`:
   - `--rest-port <port>` (default 8080, 0 = disabled)
   - `--rest-token <token>` (optional)
   - `--rest-tls-cert/key` (optional, за mTLS)

6. **Тестове**:
   - Unit: `RestApiServer` endpoints с mock `HeadlessEngine`
   - Integration: Start headless → curl POST config → verify model changed
   - WS: Connect → trigger nodeStart → verify event received

## Architecture Reference

**Архитектурна документация:** `docs/Architecture/rest-api-remote-control.md`

Там са описани:
- Общ диаграма (Server ↔ Client)
- API Contract (всички endpoints + JSON примери)
- Generic `save()/load()` mapping
- Authentication / Security model
- WebSocket event schema
- Future extensions (streaming, thin client, multi-user)

## Dependencies

- REQ-SW-PL-053: NodeEditorIde split (headless binary без QtWidgets)
- Qt6: `QtHttpServer` module (Qt 6.3+)
- Qt5: Custom `QTcpServer` implementation (no external deps)
- OpenSSL (за TLS, optional)

## Verification

```bash
# Start headless with REST API
./build_qt5/bin/DaqsterHeadless --run tests/data/number_graph.flow --rest-port 8080 --rest-token secret123 --log-console-enabled 1

# Test endpoints
curl -H "Authorization: Bearer secret123" http://localhost:8080/api/nodes
curl -H "Authorization: Bearer secret123" http://localhost:8080/api/nodes/0/schema
curl -H "Authorization: Bearer secret123" -X POST -H "Content-Type: application/json" -d '{"number": "42"}' http://localhost:8080/api/nodes/0/config

# WebSocket test (wscat)
wscat -c "ws://localhost:8080/api/events?token=secret123"
```

## Rollback Tag

`req-53-done` — REQ-SW-PL-053 complete state

## Notes

- **Status:** PLANNED (не е имплементирано)
- **Priority:** Medium (след REQ-053)
- **Estimate:** 3-5 дни за core API, +2 дни за tests/docs
- Този REQ е **foundational** — може би Blocking за REQ-056/057/058