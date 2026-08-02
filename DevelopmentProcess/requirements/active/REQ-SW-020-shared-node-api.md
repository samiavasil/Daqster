# REQ-SW-020: Shared Node API (Capabilities & NodeDataTypes)

- **Статус:** DONE
- **Приоритет:** P1
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** —

## Описание

Ретроспективно изискване за shared plugin API в `src/plugins/common`:
интерфейс `INodeProvider` (capability за доставчици на node типове) и
`NodeDataTypes` (shared QtNodes::NodeData типове между node editor плъгините).

## Acceptance Criteria

- [x] 1. `INodeProvider` е capability интерфейс с `registerNodes()` — открива се
       от потребителите чрез `QPluginManager::instances(INodeProvider_IID)`
       (plugin loader metaData + cast към interface).
- [x] 2. `NodeDataTypes/` предоставя лек `/shared` набор от `QtNodes::NodeData`
       типове (`TextData`, `FloatData`, `EmbeddingData`, `NumericType`,
       `IntegerData`, `ImageData`), които не се сериализират сами (без
       `Q_DECLARE_METATYPE` / `QJsonObject` в header-ите) — save/load на
       `QJsonObject` остава отговорност на node моделите (например
       `GenericDisplayNode::save()`, `AudioSourceDataModel::save()`).
- [x] 3. Типовете са изнесени/консолидирани в `src/common/NodeDataTypes` и се
       включват от node editor демо плъгините (и от бъдещия private AI Studio
       plugin), вместо дублирани локални копия.

## Проследимост

- **Коммити:** `b16c409` (refactor #8: consolidate shared NodeDataTypes to src/common), `e6edd62` (refactor: move common NodeDataTypes under src/plugins/common)
- **Код:** `src/plugins/common/capabilities/INodeProvider.h`, `src/plugins/common/NodeDataTypes/*`
- **Тестове:** Qt5 + Qt6 builds
