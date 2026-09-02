# REQ-SW-PL-037: NodeEditor Scene Save/Load with Missing-Node Handling

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-02
- **Родител:** REQ-SW-PL-014
- **Зависи от:** —

## Описание

NodeEditor IDE-то трябва да позволява запазване и зареждане на сцената във/от файл:

1. **Save/Load на сцената:** Потребителят може да запази текущата NodeEditor сцена
   във файл (JSON, `.flow` разширение) и да я зареди обратно. Нодовете, връзките
   и параметрите на всеки нод се възстановяват коректно.
2. **Липсващи нодове:** Ако файлът реферира нод тип, който не е регистриран в
   текущата среда (напр. plugin-ът не е наличен), зареждането НЕ трябва да
   crash-ва. Непознатите нодове се пропускат, връзките към тях се премахват,
   и потребителят получава предупреждение (диалог или лог) с имената на
   пропуснатите типове.
3. **UI контроли:** NodeEditor IDE-то има File меню с "Save Scene…" и
   "Load Scene…" действия (с Ctrl+S / Ctrl+O shortcut-и), които отварят
   файлов диалог.

## Acceptance Criteria

- [x] 1. `NodeEditorWidget` излага `scene()` accessor (DataFlowGraphicsScene*).
- [x] 2. IDE-то има File меню с "Save Scene…" и "Load Scene…" действия.
- [x] 3. Save записва `.flow` JSON файл; Load го чете и възстановява сцената.
- [x] 4. Ctrl+S / Ctrl+O shortcut-и работят.
- [x] 5. Зареждане на файл с нерегистриран нод тип НЕ crash-ва.
- [x] 6. Непознатите нодове се пропускат; познатите + връзките им се зареждат.
- [x] 7. Предупреждение (диалог или лог) изброява пропуснатите нод типове.
- [x] 8. Връзки, рефериращи пропуснати нодове, се премахват (не остават dangling).
- [x] 9. Qt5 + Qt6 builds PASS.

## Проследимост

- **Коммити:** `46980b6` (scene() accessor), `fad495b` (File menu + tolerant load),
  `9328afd` (test .flow files), `7d2c98c` (changelog), `d4c646b` (include fix)
- **Код:** `src/plugins/node_editor_ide/NodeEditorWidget.h/.cpp`,
  `src/plugins/node_editor_ide/NodeEditorIdeObject.h/.cpp`
- **Документация:** `CHANGELOG.md`
- **Тестове:** Qt5 + Qt6 builds + ръчен smoke (save → clear → load)
