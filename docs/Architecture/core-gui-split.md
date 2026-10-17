# Core/GUI Split Architecture (REQ-SW-PL-051)

## Overview

Every node is split into two halves:

- **Core model** — logic, no GUI. Lives in `nodeditor_nodes/core`
- **GUI widget** — the `QWidget`. Lives in `nodeditor_nodes/gui`

A core model has **no** `QWidget` member and returns `nullptr` from
`embeddedWidget()`. The widget is supplied at runtime by a separate plugin through
the `IWidgetProvider` capability, so the core plugin never links a widget.

The payoff is that `NodeRunner --headless --run <flow>` can load and run a
`.flow` without the GUI plugin being present at all, and the same `.flow` file
works in both modes.

> **Status:** the model split is done for 28 of 30 distinct node types
> (2 outstanding: `VideoOutput`, `AudioDisplayObsolete` — see
> `DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-051-core-gui-separation-headless.md`).
> The runner binary no longer links QtWidgets: `NodeRunner` creates its
> application object through a dlopen()ed launcher (REQ-SW-PL-072) —
> `DaqsterLauncherHeadless` (a `QGuiApplication`) for headless,
> `DaqsterLauncherGui` (a `QApplication`) for GUI — and the plugin manager no
> longer loads GUI-only plugins in headless (plugin mode gating, REQ-SW-PL-073).
> On **Qt6** the headless process is then genuinely widget-free. On **Qt5**
> `libQt5Widgets` still appears, but only via Qt5's gstreamer `mediaservice`
> plugins pulled in by `Qt5Multimedia` (see *QtWidgets in the headless run*) —
> no widget is created and no GUI plugin is loaded.

## Repository layout (actual)

The topology is *not* `src/core` + `src/gui`; the plugin infrastructure already
existed when this split was done, and the split reuses it:

```
src/
├── frame_work/
│   ├── base/                     # FrameworkCore — QtCore only
│   │   ├── src/registry/         # PluginRegistry, capability discovery
│   │   └── src/engine/           # HeadlessEngine, FlowLoader
│   └── CMakeLists.txt
├── plugins/
│   ├── common/                   # support library (NOT a plugin), capabilities
│   │   ├── capabilities/         # INodeProvider, IWidgetProvider, IStoppable, IStartable
│   │   └── GL/TexturePool.*      # single-definition GL pool (DaqsterCommonLibrary)
│   ├── nodeditor_nodes/          # node-provider plugin, split in two halves
│   │   ├── core/                 # core models, embeddedWidget() == nullptr
│   │   └── gui/                  # QWidgets + NodeWidgetFactory
│   ├── runtime_host/             # runtime hosts, one plugin per RuntimeMode
│   │   ├── headless/             # RuntimeHostHeadlessPlugin (HeadlessEngine)
│   │   └── gui/                  # RuntimeHostGuiPlugin (RuntimeShell)
│   ├── node_editor_ide/          # IDE shell
│   └── qt_coin_trader/           # GUI-only application plugin
└── apps/
    ├── Daqster/                  # GUI app
    └── NodeRunner/               # dual-mode: --run (GUI) / --headless --run
```

### Directory convention — `<plugin>/{core,gui}` (REQ-SW-PL-060)

Every plugin that provides node types lives in a single directory named after
the plugin, with two halves as subdirectories:

```
src/plugins/<plugin>/
├── <Plugin>*Interface.{h,cpp,json}   # factory/metadata, root level
├── <Plugin>*Object.{h,cpp}           # lifecycle + INodeProvider, root level
├── core/                             # headless half: models, engines — no QWidget
└── gui/                              # widget half: QWidgets, NodeWidgetFactory
```

- `core/` must not include QtWidgets and must return `nullptr` from
  `embeddedWidget()`.
- `gui/` holds the widgets and the `IWidgetProvider`; it links `core/`, never the
  other way round.
- The interface/object classes stay at the plugin root, like
  `plugins/ai_studio_plugin/` in the private repo.
- This applies to **node-provider** plugins: the two halves are the *layers*
  `core/` (models) and `gui/` (widgets).
- A plugin **family** with two variants lives under one family directory —
  parent dir = role, leaf = variant. The runtime hosts follow the same shape
  with *mode* leaves: `runtime_host/{headless,gui}`, i.e.
  `RuntimeHostHeadlessPlugin` (HeadlessEngine) and `RuntimeHostGuiPlugin`
  (RuntimeShell). They are two separate plugins, not two halves of one.
- Plugin **directories** are `snake_case`; C++ classes and CMake targets are
  `PascalCase` (e.g. dir `runtime_host/headless/` → target
  `RuntimeHostHeadlessPlugin`).
- Flat plugins that have no second variant: the IDE shell (`node_editor_ide`),
  the GUI-only app plugins (`requirements_manager`, `qt_coin_trader`).
- `src/plugins/common/` is a shared **support library**, not a plugin: its target
  name must not contain `plugin` (`DaqsterCommonLibrary`), otherwise
  `PluginDiscovery::isCandidatePluginFile()` picks it up and logs
  `Bad Plugin libDaqsterCommonLibrary.so`.

Both apps load the same plugins through `QPluginManager`. The mode only decides
which plugin *host* is created — the node models are identical either way.

## The capability: `IWidgetProvider`

`src/plugins/common/capabilities/IWidgetProvider.h` is the contract that replaces
the old "GUI factory returns nullptr in headless" idea. It is a plain
(non-`QObject`) interface, mirroring `INodeProvider`, so discovery is a
`dynamic_cast` in `PluginRegistry::widgetProviders()` rather than
`QPluginManager::instances(IWidgetProvider_IID)`.

```cpp
namespace Daqster {
class IWidgetProvider {
public:
    virtual ~IWidgetProvider() = default;
    /// Returns the widget for this model, or nullptr if this provider has none.
    virtual QWidget* createWidget(QtNodes::NodeDelegateModel* model) const = 0;
};
}
```

`NodeditorNodesGuiObject` implements it by delegating to a
`NodeWidgetFactory` whose creators are keyed on `NodeDelegateModel::name()`.

Note the inverted ownership rule that makes this work: **the core model never
holds a `QWidget`**. A model that keeps a widget pointer has not been split, and
its widget will be destroyed with the model while the scene still references it.

## Node split pattern

### Core model — `nodeditor_nodes/core`

```cpp
// Sources/Video/VideoFileSourceNode.h
class VideoFileSourceNode : public QtNodes::NodeDelegateModel,
                            public Daqster::IStartable,
                            public Daqster::IStoppable
{
    Q_OBJECT
public:
    QString name() const override { return QStringLiteral("VideoFileSource"); }

    // No QWidget member, no QtWidgets include.
    QWidget* embeddedWidget() override { return nullptr; }

    void start() override;   // IStartable
    void stop() override;    // IStoppable
};
```

### GUI widget — `nodeditor_nodes/gui`

```cpp
// Sources/Video/VideoFileSourceWidget.h
class VideoFileSourceWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoFileSourceWidget(QWidget* parent = nullptr);
    // The widget is *pulled* state + pushed commands, not constructed around
    // the model — the model and the widget never point at each other.
    void setFilePath(const QString& path);
    void setPlaying(bool playing);
Q_SIGNALS:
    void playRequested();
    void stopRequested();
    void filePathChanged(const QString& path);
};
```

The widget takes **no** model pointer. It exposes plain setters/getters and
signals; `NodeWidgetFactory` wires them to the model. That keeps the direction of
dependency one-way (gui → core) and makes the widget testable in isolation.

### Registration — `NodeWidgetFactory`

```cpp
factory->registerWidgetCreator("VideoFileSource",
    [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<VideoFileSourceNode>(model, "VideoFileSource");
        if (!m) return nullptr;
        auto* widget = new VideoFileSourceWidget();
        widget->setFilePath(m->filePath());
        widget->setPlaying(m->isPlaying());
        QObject::connect(widget, &VideoFileSourceWidget::playRequested,
                         m, &VideoFileSourceNode::start);
        return widget;
    });
```

Creators are keyed on `name()`, so an alias node that only overrides `name()`
gets its own entry (e.g. `AudioDisplay` → `DaqDisplayNode`).

The obsolete routing nodes (`DemuxNodeObsolete`, `MuxNodeObsolete` and their
aliases) deliberately have **no** creator — they have no UI and are drawn as a
plain node body.

## Where the widget is injected

`ChatGraphModel` overrides the one place QtNodes asks for an embedded widget:

```cpp
QVariant ChatGraphModel::nodeData(QtNodes::NodeId nodeId, QtNodes::NodeRole role) const
{
    if (role == QtNodes::NodeRole::Widget) {
        QWidget* widget = nodeWidget(nodeId);
        if (widget) return QVariant::fromValue(widget);
        return QVariant();
    }
    return QtNodes::DataFlowGraphModel::nodeData(nodeId, role);
}
```

`nodeWidget()` asks the provider first, falls back to
`model->embeddedWidget()` for the not-yet-split models, and **caches the result
per `NodeId`** in a `QPointer<QWidget>`.

### Why the cache is mandatory

QtNodes queries `NodeRole::Widget` four to five times per node (geometry
recompute, proxy setup, scene painting, deembed). A freshly constructed widget
has not been laid out yet, so it measures **640×480** — QtWidgets' default.
Without a cache, `recomputeSize()` measures a throwaway widget on every call and
the node box ends up sized to a widget that is immediately discarded. With the
cache, the measurement sees the real, laid-out widget and the node box matches
it.

The cache is cleared on `nodeDeleted` and whenever the provider changes.

### One widget per node — the invariant

`ChatGraphModel::nodeWidget()` is the **only** place in the tree that calls
`IWidgetProvider::createWidget()`. Everything else — `RuntimeShell`, the IDE's
deembed path, presentation mode, `captureUiSection` — goes through it:

```cpp
QWidget* w = m_editorWidget->nodeWidget(nodeId);   // not provider->createWidget()
```

Calling `createWidget()` directly is always a bug: the factory returns a **new**
instance on every call, so a second widget would be built for a model that
already has one, wired to the same signals. The first widget stays cached but
hidden and is destroyed with the model while the duplicate is still on screen.
This was a real segfault-on-quit during the migration.

## Headless mode

```bash
# GUI runtime
./build_qt5/bin/NodeRunner --run my_flow.flow

# Headless runtime
./build_qt5/bin/NodeRunner --headless --run my_flow.flow
```

The mode is decided before any plugin is instantiated: `--headless` selects
`RuntimeHostHeadlessPlugin` (headless engine), otherwise `RuntimeHostGuiPlugin`
(`RuntimeShell`, an MDI workspace shell). `NodeditorNodesCoreObject` has
both `registerNodes()` and `registerNodesHeadless()` for this reason.

In headless mode the `nodeditor_nodes/gui` plugin is simply never loaded,
so no `QWidget` is ever constructed and `ChatGraphModel::nodeWidget()` returns
`nullptr` for every node.

### Two rules that keep headless headless

These are not stylistic; breaking either one silently pulls the GUI stack into a
headless process.

**1. A capability probe must not initialize what it did not match.**
`PluginRegistry::ensureInitialized()` calls `Initialize()`, and a plugin object's
constructor can own arbitrary UI:
`RuntimeHostGuiPluginObject` builds a whole `RuntimeShell` (MDI area, QtWidgets,
OpenGL) in its constructor. So `nodeProviders()` must *not* call
`ensureInitialized()` — `registerNodes()` is `const` and works on a bare object.
Likewise `instances(iid)` has to run `qt_metacast(iid)` **before**
`ensureInitialized()`, never after. Initializing a GUI runtime host during a
node-provider probe in headless mode is what made `--headless` load
`libRuntimeHostGuiPlugin` and `libGLX`.

**2. Plugin objects are parentless on purpose.**
`capabilityInstances()` calls `createPluginObject(hash, nullptr)`, never
`createPluginObject(hash, this)`. `QPluginManager::ShutdownPluginManager()`
already deletes every instance it finds in `allPluginInstances()`, so making the
registry the QObject parent means `~QObject` deletes them a second time. That
double-free does not surface where it happens — it surfaces at process teardown,
as glibc heap corruption reported while the dynamic loader unloads `libGLX`
(`malloc_consolidate(): unaligned fastbin chunk detected`, exit 134), and only on
the paths that `return` from `main()` without reaching the shutdown call.

Ownership between creation and shutdown is still well-defined:
`Daqster/main.cpp` no longer `deleteLater()`s the objects it creates
(`CreatePluginObject(..., nullptr)`), because a cached `IWidgetProvider` pointer
in `ChatGraphModel` outlived them and every later call hit a dangling vtable.

### QtWidgets in the headless run

The runner itself is free of QtWidgets: `ldd build_qt5/bin/NodeRunner` no longer
lists `libQt5Widgets`. `NodeRunner` links `FrameworkCore` + QtCore only, and the
application object is created by a dlopen()ed launcher (REQ-SW-PL-072):

- `DaqsterLauncherHeadless` — `QGuiApplication` (links Qt::Gui, no QtWidgets)
- `DaqsterLauncherGui` — `QApplication` (links QtWidgets)

`QGuiApplication` (not `QCoreApplication`) is deliberate: the video pipeline's
hardware-GL detection (`VideoGLContextManager::hasHardwareGL`) needs a
`QGuiApplication` instance, so all GPU/GL optimisations stay available in
headless mode. Both launchers set `Qt::AA_ShareOpenGLContexts`.

`libNodeditorNodesCorePlugin` does not link QtWidgets either. The plugin manager
no longer loads GUI-only plugins in headless: **plugin mode gating**
(REQ-SW-PL-073). A plugin declares its mode in `PluginType`:

| `PluginType` | Plugins | headless |
|---|---|---|
| `gui` | `RuntimeHostGuiPlugin`, `NodeEditorIDE`, `RequirementsManager`, `QtCoinTrader` | skipped |
| `NodeGui` | `NodeditorNodesGui`, `AiStudioPluginGui` | skipped |
| `headless` | `RuntimeHostHeadlessPlugin` | loaded |
| *(absent)* | `NodeditorNodesCorePlugin`, `AiStudioPluginCore` | loaded |

`QPluginManager::SearchForPlugins()` reads `PluginType` through
`PluginDiscovery::pluginType()` — i.e. `QPluginLoader::metaData()`, which parses
the embedded JSON **without** `dlopen` — and skips `gui`/`NodeGui` plugins when
the mode is `Headless`, passed from `AppBootstrap::initializePlugins(mode)`.
A headless run therefore loads only the core/headless plugins (verified:
`RuntimeHostHeadlessPlugin`, `NodeditorNodesCorePlugin`, `libQtNodesCore`).

- **Qt6** — genuinely widget-free: no `libQt6Widgets` in the process.
- **Qt5** — `libQt5Widgets` still appears, but **not** from any plugin we load.
  `NodeditorNodesCorePlugin` links `Qt5Multimedia` (the `CameraSource`,
  `VideoFileSource`, `StreamSource` and mic nodes), and Qt5's `Qt5Multimedia`
  eagerly loads the gstreamer `mediaservice` plugins
  (`libgstcamerabin.so`, `libgstmediaplayer.so`, …), every one of which
  hard-links `libQt5MultimediaWidgets` → `libQt5Widgets`. That is a Qt5
  framework coupling, not a Daqster bug; Qt6's `Qt6Multimedia` (ffmpeg backend)
  has no such dependency. In both cases no widget is constructed and no GUI
  plugin is loaded.

## Plugin object lifetime

Capability consumers cache raw pointers to plugin objects, so ownership has to
be unambiguous. Three bugs came out of this area, all worth keeping in mind
before changing the lifetime rules:

- `main.cpp::PluginsInit()` must not destroy what it creates. It used to
  `deleteLater()` each plugin object, but the `IWidgetProvider` of one of them
  was cached in `ChatGraphModel` — every later call hit a dangling vtable and
  the IDE crashed on the next "add node".
- The `IWidgetProvider` `destroyed` re-discovery lambda must null-check the
  editor before use. Plugin teardown runs after the IDE window is gone, and
  registering nodes into a destroyed editor segfaulted on quit.
- Plugin objects are **not** parented to the `PluginRegistry`, because
  `ShutdownPluginManager()` deletes them explicitly. See rule 2 above.

## Testing

```bash
# Regression suite
cd build_qt5 && ctest    # 20/20
cd build_qt6 && ctest    # 20/20

# Headless run of a real flow
./build_qt5/bin/NodeRunner --headless --run tests/data/video_effect_chain.flow

# GUI run of the same flow
./build_qt5/bin/NodeRunner --run tests/data/video_effect_chain.flow
```

Check the **exit codes**, not just whether output appeared. A flow whose node
types are unsupported headless (any display/output node) is rejected by
`HeadlessEngine` and `main()` returns 1; that path returns before
`ShutdownPluginManager()`, so it is the one that exposes lifetime bugs. A clean
run exits 1; a run that gets further exits 0 or keeps running. Exit 134/139
means the teardown is broken.

```bash
# Should print "failed to load flow" then exit 1 — not crash
./build_qt5/bin/NodeRunner --headless --run tests/data/audio_graph.flow; echo $?

# Should NOT mention RuntimeHostGuiPlugin — that would mean the GUI runtime host
# was constructed inside a headless process
./build_qt5/bin/NodeRunner --headless --run tests/data/number_graph.flow \
    --log-console-enabled 1 --log-level Info 2>&1 | grep RuntimeHostGuiPlugin
```

Unit tests for the split itself are deferred under the standing
"NO NEW TESTS" instruction.

## Migration guide for a new node

1. **Core side** — move the model to `nodeditor_nodes/core`, delete every
   QtWidgets member and include, set
   `QWidget* embeddedWidget() override { return nullptr; }`. Add
   `IStartable`/`IStoppable` if the node has a lifecycle.
2. **GUI side** — add `*Widget.{h,cpp}` to `nodeditor_nodes/gui` with
   setters/getters and request signals. No model pointer.
3. **Register** — add a `registerWidgetCreator("<ModelName>", ...)` lambda.
   The key must match `Model::name()` exactly, including `/` in names like
   `Arithmetic/Logic`.
4. **CMake** — add the widget to `GUI_WIDGETS` in the gui plugin's
   `CMakeLists.txt`. Core-side file moves need the core plugin's list updated.
5. **Verify** — the node must be creatable through the real
   `QtNodes::CreateCommand` path in the IDE, sized to its widget, and
   deembeddable. A `createWidget()` call outside `ChatGraphModel::nodeWidget()`
   is a bug — see the invariant above.

## References

- SDRangel: `sdrangel` (GUI) vs `sdrangelsrv` (headless) — *reference only; Daqster
  uses one binary + a dlopen()ed app-object launcher (REQ-SW-PL-072), not two.*
- Runtime mode phases: `docs/Architecture/runtime-mode-architecture.md` (§4.4)
- Plugin pattern: `DevelopmentProcess/AGENT-PROCESS.md`
- REQ-SW-PL-053 — the IDE split (`RuntimeHostHeadlessPlugin` + `RuntimeHostGuiPlugin`)
  that this requirement builds on
