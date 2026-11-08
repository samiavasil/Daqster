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
| [`IStatusBarProvider`](#istatusbarprovider) | Плъгинът показва глобалното си състояние в статус бара + действия в меню | `QPluginManager::statusBarProviders()` |
| [`IMenuProvider`](#imenuprovider) | Плъгинът добавя действия на произволно място в менюто на хоста (`Tools → AI Setup`) | `QPluginManager::menuProviders()` |
| [`INodeConfigSchema`](#inodeconfigschema) | Widget-free JSON Schema фрагмент за конфигурацията на нод | `dynamic_cast` върху модел инстанцията |
| [`IRuntimeHost`](#iruntimehost) | Runtime host плъгин (headless/GUI) + optional control plane | `QPluginManager::runtimeHosts()` |
| [`IStartable`](#istartable--istoppable) | Нодове с потребителско „Start" действие | `dynamic_cast` |
| [`IStoppable`](#istartable--istoppable) | Нодове с фонова работа (threads/timers), идемпотентен `stop()` | `dynamic_cast` |
| [`ISceneLifecycle`](#iscenelifecycle) | Плъгин: „сцената е спряна, приложи политиката си за теглата" | `dynamic_cast` върху `INodeProvider` |
| [`ISceneOpening`](#isceneopening) | Плъгин: „тази сцена не може да се отвори тук" — **преди** `load()` | `dynamic_cast` върху `INodeProvider` |

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

## IStatusBarProvider

Плъгинът държи състояние, което **не е на нито един нод** — кеширани тегла,
заета VRAM, топъл сървър, фонова задача. Никой node widget не може да го
покаже, защото то е на плъгина. Capability-то дава на плъгина ред в статус бара
на хоста плюс действия в меню (REQ-SW-PL-079):

```cpp
virtual QString statusId() const = 0;        // "ai-studio.models", уникален
virtual QString statusText() const = 0;      // кратък ред; празен = скрит ред
virtual QString statusTooltip() const = 0;   // подробности (може многоредов)
virtual int statusOrder() const { return 100; }
struct Action { QString id; QString text; QString tooltip; };
virtual QList<Action> statusActions() const { return {}; }
virtual void triggerStatusAction(const QString& id) = 0;
```

Интерфейсът е plain (non-`QObject`) и **без QtWidgets** — widget-ите прави
хостът. Открива се с `dynamic_cast` през
`QPluginManager::statusBarProviders()`/`PluginRegistry::statusBarProviders()` и
се връща **инициализиран** (редът показва живото състояние на онова, което
`Initialize()` е построил). IDE-то рендира по един permanent `QLabel` на
провайдър (подредени по `statusOrder()`), а действията — в `Tools → Plugins`.

`statusText()` се вика **на всеки 2 s** — трябва да е евтин (кеширано число).
Как се добавя провайдър, кога се обновява и какво не бива:
[plugins-status-bar.md](plugins-status-bar.md).

## IMenuProvider

Плъгинът добавя **действия на точното им място** в менюто на хоста, без хостът
да знае за плъгина (REQ-SW-PL-084). `IStatusBarProvider` стига, докато
единственото меню на плъгина е `Tools → Plugins`; „`Tools → AI Setup`" е
действие на Tools, не ред в статус бара.

```cpp
struct PluginMenuAction {
    QString menuPath;   // "Tools", "Tools/Plugins" — "/" = подменю
    QString text;       // текст на действието (преведен от плъгина)
    QString id;         // идентификатор, който провайдърът разпознава
    bool    enabled = true;
};
class IMenuProvider {
public:
    virtual QVector<PluginMenuAction> menuActions() const = 0;
    virtual void triggerMenuAction(const QString& id) = 0;
};
```

Интерфейсът е plain (non-`QObject`) и **без QtWidgets** — `QAction`/`QMenu`
прави хостът. Открива се с `dynamic_cast` през
`QPluginManager::menuProviders()`/`PluginRegistry::menuProviders()` и се връща
**инициализиран** (действието често работи с онова, което `Initialize()` е
построило). IDE-то инсталира действията през `PluginMenuController`
(`src/plugins/node_editor/node_editor_ide/PluginMenuController.{h,cpp}`):

- сегментите на `menuPath` се резолвват по **заглавие** (`&` се игнорира) —
  съществуващо меню/подменю (напр. `Tools → Plugins`) се **преизползва**, липсващото
  се създава;
- `setProviders()` е **идемпотентен**: премахва предишните си действия и само
  създадените от него менюта, затова повторна инсталация не дава дубликат и не
  изпразва чуждо меню;
- празен `id`/`menuPath` се пропуска с предупреждение, `enabled=false` се показва
  сиво, изключение от `menuActions()`/`triggerMenuAction()` се логва, не се
  разпространява.

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

## ISceneLifecycle

`src/plugins/common/capabilities/ISceneLifecycle.h` — **optional** hook за
затваряне на сцена. Имплементира се от plugin object-а до `INodeProvider`
(двата интерфейса са независими: доставчик без hook просто се пропуска).

```cpp
class ISceneLifecycle {
public:
    virtual void onSceneClosed() = 0;
    // Информация, не заповед: Replace (hot-reload) или ExplicitClose (Ctrl+W).
    virtual void onSceneClosed(Daqster::SceneCloseReason reason)
    {
        static_cast<void>(reason);
        onSceneClosed();          // старият hook продължава да работи
    }
};
```

Host-ът вика `onSceneClosed()` **след** като всеки нод е получил
`IStoppable::stop()` и **преди** моделите да бъдат унищожени. Тогава плъгинът
вижда „нула живи контексти, още живи обекти" — единственият момент, в който може
да реши какво да прави с ресурсите, които нарочно преживяват нодовете (кеширани
тегла, топъл сървър). Hook-ът е идемпотентен: затварянето може да се извика
повече от веднъж.

`SceneCloseReason` (`Replace` / `ExplicitClose`) казва **защо** сцената изчезва,
но не налага политика: хостът казва *защо*, плъгинът решава *какво остава*, а
изричната му настройка печели пред причината. Затова двата слота са отделни —
стар плъгин (само `onSceneClosed()`) получава hook-а си непроменен.

Целият договор (ред на спиране, какво остава по дизайн, REST-ът):
[node_editor/scene-lifecycle.md](node_editor/scene-lifecycle.md).

## ISceneOpening

`src/plugins/common/capabilities/ISceneOpening.h` — **optional** hook за
**отваряне** на сцена (REQ-SW-PL-082). Огледалото на `ISceneLifecycle`, но за
другия край: плъгинът казва дали сцената може да се отвори в **този** процес,
преди хостът да е спрял и унищожил работещата.

```cpp
struct SceneOpenProblem {                 // ISceneOpening.h:39
    QString nodeId;                       // nodes[].id; може празен (проблем за цялата сцена)
    QString nodeName;                     // модел/заглавие за човека
    QString message;                      // какво не е наред
    QString hint;                         // какво да направи човекът; може празен
};
struct SceneOpenVerdict {                 // ISceneOpening.h:64
    QVector<SceneOpenProblem> problems;   // празен == приема; ВСИЧКИ, не първата
    bool accepted() const { return problems.isEmpty(); }
};
class ISceneOpening {                     // ISceneOpening.h:115
public:
    virtual SceneOpenVerdict onSceneOpening(const QJsonObject& scene) = 0;  // :129
    // нов default: знае и от кой файл идва сцената (REQ-SW-PL-082)
    virtual SceneOpenVerdict onSceneOpening(const QJsonObject& scene,
                                            const QString& scenePath);      // :152
};
```

`scenePath` е пътят на `.flow` файла, от който сцената е прочетена, или **празен**
низ, когато сцената е само в паметта (`reloadFlow(QJsonObject)`, REST тяло на
`POST /api/flow`) — плъгинът го ползва, за да намери `project.json` до сцената,
и трябва да тълкува празния низ като „неизвестен", не като текущата директория.
Default-имплементацията препраща към едноаргументната, така че плъгин, компилиран
срещу стария header, продължава да работи без промяна (vtable-ът на интерфейса
не се пипа).

Проблемите са **структурирани**, не низове: всеки носи id на нода (`nodes[].id`),
име за човека, съобщение и подсказка, така че хостът да маркира визирания нод, а
не само да отпечата списък. `SceneOpenReport::problemsJson()` дава същите полета
(`nodeId`, `nodeName`, `message`, `hint`) за REST тяло, а
`sceneOpeningMessage()` рендира по един ред на проблем.

Хостът пита **след** като е решил да зарежда (или преди `load()`, ако пита за
съвет) и **присъдата никога не спира зареждането сама по себе си**: сцена с
проблеми се зарежда, а само нодовете ѝ с `autoStart` остават спрени
(„зарежда, маркира, не стартира" — `REQ-SW-PL-082`). Реализацията е една:
`FrameworkCore::SceneOpen` (`src/frame_work/base/src/engine/SceneOpen.{h,cpp}`) —
`checkSceneOpening()` (`SceneOpen.h:74`/`:90` — с път; `:105`/`:117` — без, с празен път), `SceneOpenReport` (`SceneOpen.h:30`)
(`problemsJson()` — `SceneOpen.h:43`) и `sceneOpeningMessage()` (`SceneOpen.h:94`).

**Защо е отделен интерфейс, а не слот в `ISceneLifecycle`.** `ISceneLifecycle`
не декларира `Q_DECLARE_INTERFACE` IID и версия (`capabilities/ISceneLifecycle.h`
— нито един ред за IID), тоест няма договор за съвместимост: хостът знае само
подредбата на vtable-а, срещу който плъгинът е компилиран. Нов virtual там значи
плъгин, компилиран със стария header, да бъде викан през слот, който в неговия
vtable не съществува — индиректен call в паметта след последния слот (класът
дефекти INC-014; същата причина `INodeProvider.h` да остава непокътнат). Нов
интерфейс, откриван с `dynamic_cast`, е невидим за плъгините без него: те просто
не се питат. Затова и `DAQSTER_PLUGIN_INTERFACE_*` **не** се вдига
(`src/frame_work/base/src/include/QPluginInterface.h:254-258`) — той е
all-or-nothing и би счупил всеки несвързан плъгин.

**Какво hook-ът не бива.** Не стартира процеси, нишки и не зарежда модели: вика
се, докато потребителят чака, и преди каквото и да съществува. Отговаря от вече
известното (настройки, наличие на файл, конфигурация на билда); истинската
подготовка е в `start()` на нодовете. **И трябва да отговаря без `Initialize()`.**
`PluginRegistry::nodeProviders()` умишлено **не** вика `ensureInitialized()`
(`src/frame_work/base/src/registry/PluginRegistry.cpp:232-239`): инициализация
тук би построила runtime host-ове (цял `RuntimeShell` с MDI, widget-и и OpenGL)
само за да зададе един въпрос. Затова настройките се четат лениво.

Кой пита и какво прави с отказа: [node_editor/scene-lifecycle.md](node_editor/scene-lifecycle.md)
→ „Отварянето е договор".

## Node Data Types

Споделените типове (в `src/plugins/common/NodeDataTypes/`) са това, което тече по
връзките между нодовете:

| Тип | Описание |
|-----|----------|
| `SampledData` | Типизирани sampled канали (audio/DAQ/сензори); носи `SampledStreamDescriptor` и `domain` |
| `SampledStreamDescriptor` | Метаданни: sample rate, канали, типове, мерни единици |
| `VideoFrameData` | Видео кадър (Qt6: zero-copy handle; Qt5: OWNED копие) + `VideoTextureHandle` |
| `VideoTextureHandle` | Дръжка към GPU текстура (zero-copy видео път) |
| `EmbeddingData` | Векторни embedding-и |
| `TextData` | Текст |
| `FloatData` | Скаларна float стойност |
| `NumericType` | Типизирано числово (generic numeric port, ADR-002) |
