# REQ-SW-PL-049: UI Layout секция в .flow (MDI архитектура)

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-07
- **Родител:** REQ-SW-PL-014
- **Зависи от:** — (базирано на съществуващата flow инфраструктура)

## Описание

.flow файлът получава "ui" секция, която описва runtime layout-а на деембеднатите виджети. Всеки деембеднат виджет става **MDI sub-window** в **MDI Area** (QMdiArea), обвит в **Workspace** (QDockWidget) — така че runtime UI-то да има:
- **Tabbed/stacked/cascade/tile** подредба (вградени в QMdiArea)
- **Dock/undock** на целия workspace (QDockWidget поведение — може да плава на друг монитор)
- Персистентна геометрия: позиция, размер, maximized/minimized, tab group

Структура на "ui" секцията (JSON):

```json
{
  "ui": {
    "version": 1,
    "workspaces": [
      { "id": 0, "geometry": {"x":100,"y":50,"w":1200,"h":800,"maximized":false}, "tabbed": true }
    ],
    "nodes": {
      "0": { "deembedded": true, "workspace": 0, "geometry": {"x":10,"y":10,"w":420,"h":260,"maximized":false}, "autoStart": true }
    }
  }
}
```

## Acceptance Criteria

- [x] 1. .flow съдържа "ui" секция (JSON) с workspaces + per-node layout
- [x] 2. Per-node: deembedded флаг, workspace ID, geometry {x,y,w,h,maximized}, autoStart флаг
- [x] 3. Editor-ът записва "ui" секцията при save (включва текущата MDI подредба)
- [x] 4. Runtime режимът чете "ui" секцията и възстановява MDI layout-а
- [x] 5. Геометрията е custom формат (не QWidget::saveGeometry)
- [ ] 6. Тестове (отложени)

## Проследимост

- **Коммити:** (pending commit)
- **Код:** `src/plugins/node_editor_ide/` (FlowUiSection.{h,cpp}, NodeEditorIdeObject.{h,cpp} save/load)

## Бележка

Изискването е създадено по решение на потребителя (2026-09-07) след проучването на SDRangel (configuration persistence: версионирани per-widget blobs; settings отделно от geometry; custom geometry blob {x,y,w,h,maximized} — QWidget::saveGeometry е ненадежден за MDI). Архитектурното предложение е в `docs/Architecture/runtime-mode-architecture.md` (секция 4.1). Unit тестове са ОТЛОЖЕНИ по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП".