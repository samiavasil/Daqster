# Traceability Matrix — Daqster Framework Tools

Матрица за проследимост на изискванията за **общите инструменти** на frame_work.
Изискванията на плъгините и частните репота се следят в съответните им `requirements/`
директории.

## Framework Tools (този repo)

| REQ ID | Заглавие | Статус | Коммит(и) | Код | Тестове |
|--------|----------|--------|-----------|-----|---------|
| REQ-FW-001 | General Requirements Viewer/Editor Tool | ACTIVE | — | `src/plugins/requirements_manager/` | Qt5/Qt6 builds |

## Plugin Framework (за справка — пълен trace в DaqsterAiStudio)

| REQ ID | Заглавие | Статус | Коммит(и) |
|--------|----------|--------|-----------|
| REQ-PLG-001 | AppImage & Search Path Normalization | DONE | `9ea787c`, `2412e56`, `04c57f4` |
| REQ-PLG-002 | Robust Plugin Discovery (non-"plugin" naming) | DONE | `032e357` |
| REQ-PLG-003 | Plugin Deduplication & Stale Persistency Pruning | DONE | `032e357` |
