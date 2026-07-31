# Traceability Matrix — Requirements Manager & Framework Tools

Матрица за проследимост на изискванията за **Requirements Manager** инструмент (`REQ-SW-*`).

## Изисквания (`REQ-SW-*`)

| REQ ID | Заглавие | Статус | Родител | Зависи от | Коммит(и) | Код | Тестове |
|--------|----------|--------|---------|-----------|-----------|-----|---------|
| `REQ-SW-001` | Requirements Viewer/Editor Tool (Base) | ACTIVE | — | — | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-002` | Data Model & Parsing Extensions | ACTIVE | `REQ-SW-001` | `REQ-SW-001` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-003` | Requirement Creation Form UI | ACTIVE | `REQ-SW-001` | `REQ-SW-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-004` | Requirement Lifecycle & Status Management | ACTIVE | `REQ-SW-001` | `REQ-SW-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-005` | Built-in Help & Documentation Tab / Dialog | ACTIVE | `REQ-SW-001` | `REQ-SW-001` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-006` | Hierarchy Tree View | ACTIVE | `REQ-SW-001` | `REQ-SW-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-007` | Dependency Link Management & Navigation | ACTIVE | `REQ-SW-001` | `REQ-SW-002` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-008` | Requirements Validation & Consistency Engine | ACTIVE | `REQ-SW-001` | `REQ-SW-006`, `REQ-SW-007` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-009` | Interactive Dependency Graph Viewer | ACTIVE | `REQ-SW-001` | `REQ-SW-007`, `REQ-SW-008` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-010` | Traceability Matrix View & Export | ACTIVE | `REQ-SW-001` | `REQ-SW-002`, `REQ-SW-008` | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |
| `REQ-SW-011` | Requirements Search Engine (Backlog) | BACKLOG | `REQ-SW-001` | `REQ-SW-002` | — | `src/plugins/requirements_manager/` | — |

## Plugin Framework (за справка — пълен trace в DaqsterAiStudio)

| REQ ID | Заглавие | Статус | Коммит(и) |
|--------|----------|--------|-----------|
| REQ-PLG-001 | AppImage & Search Path Normalization | DONE | `9ea787c`, `2412e56`, `04c57f4` |
| REQ-PLG-002 | Robust Plugin Discovery (non-"plugin" naming) | DONE | `032e357` |
| REQ-PLG-003 | Plugin Deduplication & Stale Persistency Pruning | DONE | `032e357` |
