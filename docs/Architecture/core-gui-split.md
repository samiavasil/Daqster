# Core/GUI Split Architecture (REQ-SW-PL-051)

## Overview

Every node is split into two halves:

- **Core model** — logic, no GUI. Lives in `demo_nodeditor_nodes_core`
- **GUI widget** — the `QWidget`. Lives in `demo_nodeditor_nodes_gui`

A core model has **no** `QWidget` member and returns `nullptr` from
`embeddedWidget()`. The widget is supplied at runtime by a separate plugin through
the `IWidgetProvider` capability, so the core plugin never links a widget.

The payoff is that `NodeRunner --headless --run <flow>` can load and run a
`.flow` without the GUI plugin being present at all, and the same `.flow` file
works in both modes.

> **Status:** the model split is done for 28 of 30 distinct node types
> (2 outstanding: `VideoOutput`, `AudioDisplayObsolete` — see
> `DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-051-core-gui-separation-headless.md`).
> A binary that links *no* QtWidgets is **not** achieved yet — `NodeRunner` still
> needs `QApplication`, and the core plugin still compiles a few QtWidgets
> translation units for `AudioDisplayObsolete`. The same document records the
> blocker.

## Repository layout (actual)

The topology is *not* `src/core` + `src/gui`; the plugin infrastructure already
existed when this split was done, and the split reuses it:

```
src/
├── frame_work/
│   ├── base/                     # FrameworkCore — QtCore only
│   │   └── src/registry/         # PluginRegistry, capability discovery
│   │   └── src/engine/           # HeadlessEngine, FlowLoader
│   └── CMakeLists.txt
├── plugins/
│   ├── common/capabilities/      # INodeProvider, IWidgetProvider, IStoppable, IStartable
│   ├── demo_nodeditor_nodes_core/# core models, embeddedWidget() == nullptr
│   ├── demo_nodeditor_nodes_gui/ # QWidgets + NodeWidgetFactory
│   ├── FrameworkCorePlugin/      # headless runtime host
│   ├── FrameworkGuiPlugin/       # GUI runtime host (RuntimeShell)
│   └── node_editor_ide/          # IDE shell
└── apps/
    ├── Daqster/                  # GUI app
    └── NodeRunner/               # dual-mode: --run (GUI) / --headless --run
```

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

`DemoNodeEditorNodesGuiObject` implements it by delegating to a
`NodeWidgetFactory` whose creators are keyed on `NodeDelegateModel::name()`.

Note the inverted ownership rule that makes this work: **the core model never
holds a `QWidget`**. A model that keeps a widget pointer has not been split, and
its widget will be destroyed with the model while the scene still references it.

## Node split pattern

### Core model — `demo_nodeditor_nodes_core`

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

### GUI widget — `demo_nodeditor_nodes_gui`

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
`FrameworkCorePlugin` (headless engine), otherwise `FrameworkGuiPlugin`
(`RuntimeShell`, an MDI workspace shell). `DemoNodeEditorNodesCoreObject` has
both `registerNodes()` and `registerNodesHeadless()` for this reason.

In headless mode the `demo_nodeditor_nodes_gui` plugin is simply never loaded,
so no `QWidget` is ever constructed and `ChatGraphModel::nodeWidget()` returns
`nullptr` for every node.

### Two rules that keep headless headless

These are not stylistic; breaking either one silently pulls the GUI stack into a
headless process.

**1. A capability probe must not initialize what it did not match.**
`PluginRegistry::ensureInitialized()` calls `Initialize()`, and a plugin object's
constructor can own arbitrary UI:
`FrameworkGuiPluginObject` builds a whole `RuntimeShell` (MDI area, QtWidgets,
OpenGL) in its constructor. So `nodeProviders()` must *not* call
`ensureInitialized()` — `registerNodes()` is `const` and works on a bare object.
Likewise `instances(iid)` has to run `qt_metacast(iid)` **before**
`ensureInitialized()`, never after. Initializing a GUI runtime host during a
node-provider probe in headless mode is what made `--headless` load
`libFrameworkGuiPlugin` and `libGLX`.

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

### What headless does not yet give you

`ldd build_qt5/bin/NodeRunner | grep -E "Qt5Widgets|Qt5Gui"` is **not** empty
today. Two reasons, both tracked in the REQ file:

1. `NodeRunner` uses `QApplication` (Qt5: part of `QtWidgets`) because QtNodes
   and `QTimer` need an event loop object. Switching the headless path to
   `QCoreApplication` is a separate change.
2. `demo_nodeditor_nodes_core/CMakeLists.txt` still compiles seven QtWidgets
   translation units out of `node_editor_ide/BuiltInNodes/Library` because
   `AudioDisplayModelObsolete` inherits a widget-building display model.

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
cd build_qt5 && ctest    # 11/11
cd build_qt6 && ctest    # 11/11

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

# Should NOT mention FrameworkGuiPlugin — that would mean the GUI runtime host
# was constructed inside a headless process
./build_qt5/bin/NodeRunner --headless --run tests/data/number_graph.flow \
    --log-console-enabled 1 --log-level Info 2>&1 | grep FrameworkGuiPlugin
```

Unit tests for the split itself are deferred under the standing
"NO NEW TESTS" instruction.

## Migration guide for a new node

1. **Core side** — move the model to `demo_nodeditor_nodes_core`, delete every
   QtWidgets member and include, set
   `QWidget* embeddedWidget() override { return nullptr; }`. Add
   `IStartable`/`IStoppable` if the node has a lifecycle.
2. **GUI side** — add `*Widget.{h,cpp}` to `demo_nodeditor_nodes_gui` with
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

- SDRangel: `sdrangel` (GUI) vs `sdrangelsrv` (headless)
- Runtime mode phases: `docs/Architecture/runtime-mode-architecture.md` (§4.4)
- Plugin pattern: `DevelopmentProcess/AGENT-PROCESS.md`
- REQ-SW-PL-053 — the IDE split (`FrameworkCorePlugin` + `FrameworkGuiPlugin`)
  that this requirement builds on
