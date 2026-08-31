# REQ-SW-PL-014: Node Editor IDE & Demo Nodes

- **Статус:** DONE
- **Приоритет:** P2
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** REQ-SW-PL-013

## Описание

Ретроспективно изискване за node editor плъгините: `node_editor_ide`
(NodeEditorIdeObject + вградените node типове) и `demo_nodeditor_nodes`
(demo node каталог, включително LLamaSource с външна генерация).

## Acceptance Criteria

- [x] 1. `NodeEditorIdeObject::registerNodes()` регистрира вградените node типове:
       `NumberSourceDataModel`, `NumberDisplayDataModel`, `ModuloModel`,
       `ArithmeticLogicModel` — със собствена static `setStyle()` конфигурация на
       `QtNodes::ConnectionStyle` и мрежата се зарежда от идеално `flow` файл.
- [x] 2. `DemoNodeEditorNodesObject::registerNodes()` регистрира demo node типове
       в категории: AudioDisplayModel + GenericDisplayNode (Display), DemuxNode +
       MuxNode (Routing), AudioSourceDataModel (Sources), LLamaModelDataModel +
       ConsoleDataModel (LLama).
- [x] 3. LLamaSource (LLamaModelDataModel / ConsoleDataModel) ползва
       `QNetworkAccessManager` + `QProcess` + `ChatBaseWidget` за комуникация с
       външна LLama услуга — потвърдена зависимост, която по-късно се покрива
       от private AI Studio plugin (LlamaEngine/LlamaGenerateWidget в частните
       изисквания на DaqsterAiStudio).

## Проследимост

- **Коммити:** `f7aa532` (refactor #8: move shared types under src/common), `fe87d14` (feat #8: add NodeEditorIdeObject + built-in nodes), `e0f1395` (refactor #8: extract demo nodes into own plugin with INodeProvider)
- **Код:** `src/plugins/node_editor_ide/NodeEditorIdeObject.cpp`, `src/plugins/demo_nodeditor_nodes/DemoNodeEditorNodesObject.cpp`, `src/plugins/demo_nodeditor_nodes/Sources/LLamaSource/*`
- **Тестове:** Qt5 + Qt6 builds
