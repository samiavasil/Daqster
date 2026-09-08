# Daqster Runtime Mode — Архитектурно предложение

## 1. Визия

"Node diagram = application". Два режима:

- **Editor режим** (текущ): node editor, редактиране, дебъгване
- **Runtime режим**: `Daqster --run <flow.flow>` — editor-ът е скрит, деембеднатите виджети на нодовете формират UI-то на приложението

Деембедингът е ключовият механизъм: той е мостът между editor и приложение (моделът на LabVIEW front panel / Pure Data presentation mode / TouchDesigner perform mode).

## 2. Текущо състояние (одит)

- .flow формат: JSON {nodes, connections, groups}; per-node: id, internal-data (model save()), label, position. Източници: DataFlowGraphModel.cpp:546-694, DataFlowGraphicsScene.cpp:206-289
- Per-node widget state СЕ записва чрез model save()/load() (GamepadModel.cpp:79-87, PcapModel.cpp:66-75)
- Deembed: setWidgetEmbedded(false) → top-level прозорец с Qt::WindowStaysOnTopHint (NodeGraphicsObject.cpp:220-308); геометрията НЕ се персистира
- CLI: няма --run флаг; само DAQSTER_AUTOSTART_FLOW env (NodeEditorIdeObject.cpp:145-150)
- Thread lifecycle: per-model destructor cleanup; НЯМА unified stop()/wait() на NodeDelegateModel
- Shutdown: ShutdownHandler → quit(); async deleteLater верига (QPluginManager.cpp:379-386); нишките може да работят при exit
- 8 празнини: (1) няма --run, (2) деембедната геометрия не се персистира, (3) няма unified stop/wait, (4) .flow няма runtime metadata, (5) няма editor hiding, (6) auto-start е video-specific, (7) WindowStaysOnTopHint без window management, (8) async shutdown

## 3. Уроци от SDRangel

- Workspace = QDockWidget обвиващ QMdiArea; plugin GUIs = QMdiSubWindow (sdrgui/gui/workspace.h:29, channelgui.h:36)
- Configuration persistence: версионирани per-widget blobs; settings отделно от geometry (sdrbase/settings/configuration.h:25)
- Custom geometry blob {x,y,w,h,maximized} — QWidget::saveGeometry е ненадежден за MDI (sdrgui/gui/mdiutils.cpp)
- Core/GUI split: същият PluginInterface, GUI factory-тата връщат nullptr headless; два бинарника (sdrangel/sdrangelsrv)
- Message/MessageQueue за между-нишкова комуникация (sdrbase/util/message.h)
- Auto-stack на sub-windows при добавяне (workspace.cpp eventFilter)
- ДА НЕ копираме: бинарни blobs (qCompress+base64), FSM sprawl в MainWindow, singleton MainWindow, magic-number geometry fixes, QSettings като единствен storage

## 4. Предложена архитектура по фази

### Фаза 1 — Runtime режим (REQ-SW-PL-048, REQ-SW-PL-049, REQ-SW-PL-050)

**4.1 Flow schema extension (REQ-SW-PL-049)**

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

- Опционална секция → backward compatible
- Custom geometry формат (SDRangel урок)
- autoStart: generic node auto-start (разширение на startVideoPlayback модела)

**4.2 Runtime shell (REQ-SW-PL-048)**

- CLI: `--run <flow>` (+ `--headless` във Фаза 2)
- v1: управлявани top-level прозорци (разширение на съществуващия deembed; геометрия от ui секцията)
- v2 (опционално): QMainWindow shell — QMdiArea workspace (SDRangel модел) или QDockWidget docks; QML shell като бъдеща опция
- Presentation toggle в editor-а (Pure Data модел)
- Generic auto-start механизъм

**4.3 Node lifecycle протокол (REQ-SW-PL-050)**

- virtual stop() на NodeDelegateModel (+ wait() където е приложимо)
- Всички нодове го имплементират (таблица: PlutoSdr QThread, Pcap QThread, AudioSource QThread, VideoEffect ComputePool, LLama QProcess, Gamepad/SystemMonitor/GpuMonitor/JackDetect QTimer)
- deleteNode() вика stop() преди унищожаване
- ShutdownHandler верига: stop() → wait() → deleteLater → exit

### Фаза 2 — Headless/Server (REQ-SW-PL-051)

**4.4 Core/GUI разделение (REQ-SW-PL-051)**

- **Библиотеки:**
  - `DaqsterCore` — QtCore only: plugin infrastructure, platform, logging, process, perf, capabilities (INodeProvider, IStoppable, IStartable), ALL NodeData types, HeadlessEngine, FlowLoader
  - `DaqsterGui` — QtWidgets: QPluginManagerGui, NodeEditorWidget, RuntimeShell, built-in nodes, merged NodeEditorLibrary
  
- **Плъгини:**
  - `demo_nodeditor_nodes_core` — Core node models (24+), NO widgets, `embeddedWidget() = nullptr`, implements IStoppable/IStartable
  - `demo_nodeditor_nodes_gui` — GUI widgets + NodeWidgetFactory, registers `modelName → widget creator` lambdas
  - `node_editor_ide` — IDE shell, depends on DaqsterGui

- **NodeWidgetFactory контракт:**
  ```cpp
  // In demo_nodeditor_nodes_gui plugin
  factory->registerWidgetCreator("AudioSource", [](NodeDelegateModel* m) {
      return new AudioSourceDataModelUI(qobject_cast<AudioSourceDataModel*>(m));
  });
  factory->registerWidgetCreator("DaqDisplay", [](NodeDelegateModel* m) {
      return new DaqDisplayNodeUI(qobject_cast<DaqDisplayNode*>(m));
  });
  // ... all 24+ models
  ```

- **Editor/Runtime интеграция:**
  ```cpp
  // NodeEditorWidget / RuntimeShell uses factory instead of model->embeddedWidget()
  QWidget* widget = factory ? factory->createWidget(model) : model->embeddedWidget();
  // In headless: factory is null → embeddedWidget() returns nullptr → no widgets created
  ```

**4.5 Headless исполнение**

- **DaqsterHeadless** executable: QtCore only, links DaqsterCore
- **CLI:** `DaqsterHeadless --run <flow.flow>` (or `Daqster --headless --run <flow.flow>`)
- **HeadlessEngine:** QCoreApplication event loop, loads .flow via FlowLoader, instantiates core models, connects via DataFlowGraphModel, auto-starts via IStartable, stops via IStoppable on shutdown
- **FlowLoader:** Tolerant loading (skips unregistered node types, logs warning), parses UI section for autoStart

**Verification:**
```bash
# Headless binary - no QtWidgets/QtGui
ldd build_qt5/bin/DaqsterHeadless | grep -E "Qt5Widgets|Qt5Gui"  # EMPTY

# Core library - no QtWidgets/QtGui  
ldd build_qt5/bin/libDaqsterCore.so | grep -E "Qt5Widgets|Qt5Gui"  # EMPTY

# Headless smoke test
./build_qt5/bin/DaqsterHeadless --run test.flow  # runs, data flows, clean exit
```

- (Опционално) REST API за remote control — документирано като разширение

### Фаза 3 — Дистрибуция (REQ-SW-PL-052)

- Flatpak (SDRangel манифест модел) или AppImage
- Решава Qt version проблема (6.4.2 vs 6.8.3)

## 5. Компонентна диаграма (текстова)

[Editor режим]

```
Daqster (QMainWindow) → NodeEditorWidget → GraphicsView → Scene → NodeGraphicsObject → QGraphicsProxyWidget → Node widget
```

[Runtime режим — v1]

```
Daqster --run → RuntimeShell (QMainWindow, скрит editor) → деембеднати виджети (top-level, геометрия от ui секцията) → NodeDelegateModel::stop() при exit
```

[Runtime режим — v2 (опционално)]

```
RuntimeShell → QMdiArea/Workspace → QMdiSubWindow(деембеднат виджет) → cascade/tile/tab
```

## 6. Рискове и отворени въпроси

- QML shell vs QWidget shell (QML дискусията: QQuickWidget е QWidget, но findChildren автоматизацията се чупи)
- Backward compatibility на .flow
- MDI geometry quirks (SDRangel magic numbers — да се избегнат)
- WindowStaysOnTopHint в runtime режим (вероятно трябва да се махне)
- Thread-safe stop() при активни нишки

## 7. REQ обобщение

| REQ | Фаза | Описание |
|-----|------|----------|
| REQ-SW-PL-048 | 1 | Runtime режим (--run, presentation toggle) |
| REQ-SW-PL-049 | 1 | .flow ui секция (deembedded, geometry, autoStart) |
| REQ-SW-PL-050 | 1 | Thread lifecycle протокол (stop/wait) |
| REQ-SW-PL-051 | 2 | Core/GUI разделение + headless |
| REQ-SW-PL-052 | 3 | Пакетиране (Flatpak/AppImage) |