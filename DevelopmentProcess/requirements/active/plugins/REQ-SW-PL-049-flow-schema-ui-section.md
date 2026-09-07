# REQ-SW-PL-049: Разширение на .flow схемата с UI layout секция

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-07
- **Родител:** REQ-SW-PL-014
- **Зависи от:** — (базирано на съществуващата flow инфраструктура REQ-SW-PL-037/038)

## Описание

.flow файлът получава опционална "ui" секция, която описва runtime състоянието
на нодовете: deembedded флаг, геометрия на прозореца {x,y,w,h,maximized},
autoStart флаг. Секцията е backward compatible — стари .flow файлове без "ui"
секция се зареждат без промяна. Геометрията се записва с custom формат (урок
от SDRangel: QWidget::saveGeometry е ненадежден за MDI sub-windows).

Примерна структура на секцията:

```json
{
  "nodes": [...],
  "connections": [...],
  "groups": [...],
  "ui": {
    "version": 1,
    "nodes": {
      "0": { "deembedded": true, "geometry": {"x":100,"y":50,"w":420,"h":260,"maximized":false}, "autoStart": true }
    }
  }
}
```

## Acceptance Criteria

- [ ] 1. .flow поддържа опционална "ui" секция (JSON, human-readable)
- [ ] 2. Per-node: deembedded флаг, geometry {x,y,w,h,maximized}, autoStart флаг
- [ ] 3. Стари .flow файлове се зареждат без промяна (backward compatible)
- [ ] 4. Editor-ът записва "ui" секцията при save
- [ ] 5. Runtime режимът чете "ui" секцията при старт
- [ ] 6. Геометрията се записва с custom формат (не QWidget::saveGeometry)
- [ ] 7. Тестове (отложени по текущата инструкция)

## Проследимост

- **Коммити:** (pending commit)
- **Код:** `src/plugins/node_editor_ide/` (DataFlowGraphModel save/load,
  NodeEditorWidget save/load), `src/plugins/external_libs/nodeeditor/`
  (scene serialization)

## Бележка

Изискването е създадено по решение на потребителя (2026-09-07) след
проучването на SDRangel (configuration persistence: версионирани per-widget
blobs; settings отделно от geometry; custom geometry blob {x,y,w,h,maximized} —
QWidget::saveGeometry е ненадежден за MDI). Архитектурното предложение е в
`docs/Architecture/runtime-mode-architecture.md` (секция 4.1). Unit тестове са
ОТЛОЖЕНИ по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП".