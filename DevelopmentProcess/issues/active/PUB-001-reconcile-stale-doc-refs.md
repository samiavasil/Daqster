---
id: PUB-001
kind: docs
status: OPEN
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
- [ ] 1. `docs/Architecture/README.md` + `docs/Architecture/plugins/PluginDevelopment.md`: shared headers documented at `src/plugins/capabilities/`, actual path is `src/plugins/common/` (with `capabilities/` subdir) — Medium
- [ ] 2. `docs/Architecture/BuildSystemArchitecture.md`: "NodeEditor is Qt5-only" — nodeeditor has `USE_QT6` option, Qt6 build works — Medium
- [ ] 3. `docs/Architecture/BuildSystemArchitecture.md`: QtCoinTrader "Qt5-only" — Qt6 build includes it — Low
- [ ] 4. `docs/Architecture/README.md` note "include/ е премахнат" — headers still at `base/src/include/` — Low
- [ ] 5. `README.en.md` directory tree stale (`node_editor_app`, `src/plugins/libs/node_editor_ide`) — Low
- [ ] 6. `docs/plugins/demo_nodeditor_nodes/README.md`: PLUGIN_VERSION 0.3.0 vs code 0.2.0 — Trivial
- [ ] 7. `docs/Architecture/.../QPluginManager.md` flow: unqualified names, now `PluginDiscovery::isCandidatePluginFile` — Low
