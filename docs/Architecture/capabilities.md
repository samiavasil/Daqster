# Capabilities (договори host ↔ plugin)

Родител: [Architecture](README.md) | [Индекс](../index.md)
Виж още: [Plugins](plugins/README.md) · [Core/GUI Split](core-gui-split.md) · [Node Editor](node_editor/README.md)

Capability интерфейсите са **header-only** договори в
`src/plugins/common/capabilities/`, чрез които хостът и плъгините комуникират, без
да се обвързват един с друг. Те са **plain (non-`QObject`)** интерфейси — това е
важно, защото работят и в headless режим и не изискват `QtWidgets`.

## Каталог

| Интерфейс | Роля | Discovery |
|-----------|------|-----------|
| [`INodeProvider`](#inodeprovider) | Доставя `NodeDelegateModel` типове към node editor-а | `QPluginManager::instances(INodeProvider_IID)` |
| [`IWidgetProvider`](#iwidgetprovider) | Доставя `QWidget`-а на нод, чийто модел е в widget-free core плъгин | `dynamic_cast` през `PluginRegistry` |
| [`INodeConfigSchema`](#inodeconfigschema) | Widget-free JSON Schema фрагмент за конфигурацията на нод | `dynamic_cast` върху модел инстанцията |
| [`IRuntimeHost`](#iruntimehost) | Runtime host плъгин (headless/GUI) + optional control plane | `QPluginManager::runtimeHosts()` |
| [`IStartable`](#istartable--istoppable) | Нодове с потребителско „Start" действие | `dynamic_cast` |
| [`IStoppable`](#istartable--istoppable) | Нодове с фонова работа (threads/timers), идемпотентен `stop()` | `dynamic_cast` |

> **Защо два начина за discovery?** Само `INodeProvider` и `IRuntimeHost` са Qt
> plugin interfaces (с `Q_DECLARE_INTERFACE` + IID). Останалите са plain интерфейси,
> които се откриват с `dynamic_cast` върху вече създадени обекти (модели/widget-и),
> защото са *per-object* способности, а не *per-plugin*.

## INodeProvider

Плъгин, който доставя нод типове:

```cpp
class INodeProvider {
public:
    virtual ~INodeProvider() = default;
    virtual void registerNodes(NodeDelegateModelRegistry& registry) const = 0;
};
```

Node Editor IDE (`NodeEditorIde`) пита `QPluginManager` за всички заредени
`INodeProvider` инстанции и вика `registerNodes()` върху всяка. Виж
[nodes.md](node_editor/nodes.md) за пълния каталог.

## IWidgetProvider

Позволява на плъгин да достави `QWidget` за нод, чийто **модел** живее в
widget-free core плъгин ([core/gui split](core-gui-split.md)). Editor-ът пита
`IWidgetProvider`-ите **преди** да падне към `NodeDelegateModel::embeddedWidget()`:

```cpp
virtual QWidget* widgetFor(QtNodes::NodeDelegateModel* model) = 0;  // nullptr = няма
```

Така `NodeditorNodesCore` връща `nullptr` от `embeddedWidget()` и остава без
QtWidgets, а `NodeditorNodesGui` държи всички widget-и.

## INodeConfigSchema

Опционално самоописание на конфигурацията на нод (REQ-SW-PL-063 AC10). `save()`
дава *формата* (имена на полета + JSON типове), но не и машинно четимата мета
информация, нужна на отдалечен UI — `enum`, `minimum`/`maximum`, defaults,
`visibleWhen`:

```cpp
virtual QJsonObject configSchema() const = 0;   // Draft-7 фрагмент
```

Когато моделът не го имплементира, контролната плоскост пада към best-effort схема,
изведена от `save()` — имплементацията е инкрементална и опционална. Интерфейсът е
widget-free и живее в core-а, затова работи и headless. Пилотен нод:
`VideoEffectNode`.

## IRuntimeHost

Договор за runtime host плъгин, който вдига `.flow` сцена в определен
[`RuntimeMode`](core-gui-split.md) (`Headless` или `Gui`):

- `Headless` → `HeadlessEngine`, offscreen platform, fail-fast валидация.
- `Gui` → `RuntimeShell` + MDI workspaces.

Съдържа и `ControlPlaneOptions` — конфигурацията на optional-ната remote-control
плоскост (REQ-SW-PL-054/063), която споделеният `AppBootstrap` парсва от `--rest-*`
опциите и подава чрез `configureControlPlane()`. Host, който не я поддържа, връща
`false` → bootstrap-ът третира поискана-но-неподдържана плоскост като startup
грешка (fail loudly). Виж [rest-api-remote-control](rest-api-remote-control.md).

## IStartable / IStoppable

Разделени са нарочно: дисплей/обработващи нодове остават непокътнати.

- **`IStartable`** — имплементира се **само** от source/sink нодове с
  потребителско „start" действие (напр. `SystemMonitorModel`; `NumberSource` **не**
  е `IStartable`).
- **`IStoppable`** — имплементира се от нодове с фонова работа (threads, timers,
  процеси); `stop()` трябва да е **идемпотентен** (безопасно многократно викане).

## Node Data Types

Споделените типове (в `src/plugins/common/NodeDataTypes/`) са това, което тече по
връзките между нодовете:

| Тип | Описание |
|-----|----------|
| `SampledData` | Типизирани sampled канали (audio/DAQ/сензори); носи `SampledStreamDescriptor` и `domain` |
| `SampledStreamDescriptor` | Метаданни: sample rate, канали, типове, мерни единици |
| `VideoFrameData` | Видео кадър (Qt6: zero-copy handle; Qt5: OWNED копие) + `VideoTextureHandle` |
| `VideoTextureHandle` | Дръжка към GPU текстура (zero-copy видео път) |
| `EmbeddingData` | Векторни embedding-и (AI) |
| `TextData` | Текст (LLM prompts/отговори) |
| `FloatData` | Скаларна float стойност |
| `NumericType` | Типизирано числово (generic numeric port, ADR-002) |
