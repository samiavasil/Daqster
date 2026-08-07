---
id: PUB-001
kind: docs
status: CLOSED
close_reason: completed
resolved: 2026-08-04
priority: medium
created: 2026-08-04
owner:
related_req:
---

## Описание
Reconcile stale references in public docs, discovered during the full docs
review (2026-08-04). The macro architecture is correctly documented (3-layer
plugin pattern, ShutdownHandler, QProcessManager, QPluginManager, create_plugin()
all match the code); these are stale path/version/Qt claims that could mislead
new plugin developers. ai_studio_plugin already complies — no code changes needed.

## Acceptance Criteria
- [x] 1. `docs/Architecture/README.md` + `docs/Architecture/plugins/PluginDevelopment.md`: shared headers documented at `src/plugins/capabilities/`, actual path is `src/plugins/common/` (with `capabilities/` subdir) — Medium
- [x] 2. `docs/Architecture/BuildSystemArchitecture.md`: "NodeEditor is Qt5-only" — nodeeditor has `USE_QT6` option, Qt6 build works — Medium
- [x] 3. `docs/Architecture/BuildSystemArchitecture.md`: QtCoinTrader "Qt5-only" — Qt6 build includes it — Low
- [x] 4. `docs/Architecture/README.md` note "include/ е премахнат" — headers still at `base/src/include/` — Low
- [x] 5. `README.en.md` directory tree stale (`node_editor_app`, `src/plugins/libs/node_editor_ide`) — Low
- [x] 6. `docs/plugins/demo_nodeditor_nodes/README.md`: PLUGIN_VERSION 0.3.0 vs code 0.2.0 — Trivial
- [x] 7. `docs/Architecture/.../QPluginManager.md` flow: unqualified names, now `PluginDiscovery::isCandidatePluginFile` — Low
