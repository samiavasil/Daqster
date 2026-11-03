#pragma once

#include "FlowUiSection.h"
#include "LayoutTree.h"
#include "QBasePluginObject.h"
#include <QtNodes/Definitions>

#include <QEvent>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QVector>
#include <vector>

class LayoutDesignerPanel;
class NodeEditorWidget;
class StatusBarController;
class QAction;
class QMainWindow;
class QWidget;
namespace Daqster {
class IWidgetProvider;
}
namespace QtNodes {
class NodeDelegateModel;
}

class NodeEditorIdeObject : public Daqster::QBasePluginObject
{
    Q_OBJECT
public:
    NodeEditorIdeObject(QObject* Parent = nullptr);
    virtual ~NodeEditorIdeObject();
    void SetName(const QString& name);
    virtual bool Initialize();

    // ── Public registration + loading API (REQ-SW-PL-048) ─────────────
    // These were private before; they are now public so the GUI runtime in
    // RuntimeHostGuiPlugin (RuntimeShell) can reuse the same registration and
    // loading logic without duplication.
    //
    // REQ-SW-PL-051: the built-in node models (NumberSource, NumberDisplay,
    // Modulo, Arithmetic/Logic) are owned by nodeditor_nodes/core and
    // arrive through INodeProvider like every other plugin node. There is no
    // local registration left — see registerNodes().

    /// Registers every node provided by the loaded INodeProvider plugins.
    void registerNodes();

    /// Tolerant scene load (REQ-SW-PL-037): opens a .flow file, skips nodes
    /// whose model type is not registered in the current environment (instead
    /// of crashing on DataFlowGraphModel::loadNode's std::logic_error), removes
    /// connections referencing the skipped nodes and warns the user about the
    /// skipped types. Returns true on success (including partial loads).
    /// When `fileName` is empty a file dialog is shown; otherwise the given
    /// path is loaded directly (used by DAQSTER_AUTOSTART_FLOW, REQ-SW-PL-038).
    bool loadSceneTolerant(const QString& fileName = QString());

    /// Shared tolerant-load body: parses `fileName`, drops unregistered nodes
    /// and their connections, loads the cleaned scene and warns about skipped
    /// types. Returns true on success (including partial loads).
    bool loadSceneFromFile(const QString& fileName);

protected:
    virtual void DeInitialize();
    virtual bool eventFilter(QObject* watched, QEvent* event);

public slots:
    void MainWinDestroyed(QObject* obj);
    void ShowPlugins();

    /// REQ-SW-PL-051: re-discover and register nodes from newly loaded
    /// INodeProvider plugins (connected to QPluginManager::PluginsListChangeDetected).
    void discoverAndRegisterExternalNodes();

    /// Presentation mode toggle (REQ-SW-PL-048): F11 key handler.
    /// Hides GraphicsView + shows deembedded widgets; toggles back.
    void togglePresentationMode();

    /// Close the scene (REQ-SW-PL-078, `File → Close Scene` / Ctrl+W).
    ///
    /// Asks before discarding unsaved changes, then stops every node (sources
    /// first), notifies the plugins through `ISceneLifecycle::onSceneClosed()`
    /// and empties the canvas. Nothing is left running and nothing is left in
    /// the graph; model weights a plugin keeps warm survive by design.
    void closeScene();

protected slots:
    void nodeDoubleClicked(QtNodes::NodeId nodeId);

private:
    /// The close sequence behind `closeScene()` and behind "Load Scene…".
    ///
    /// @param confirmDiscard Ask the user before dropping unsaved changes
    ///                       (`closeScene()`), or close silently (loading
    ///                       another scene — the same teardown, no question).
    /// @return true when the scene is closed (or was already empty).
    bool closeCurrentScene(bool confirmDiscard);

    /// Watches the graph model so "unsaved changes" is a fact, not a guess
    /// (REQ-SW-PL-078 AC1). Every mutation sets the flag; load and save clear it.
    void trackSceneDirtyState();

    /// REQ-SW-PL-051: hands ALL IWidgetProvider plugins to the editor widget
    /// (each GUI plugin serves widgets for its own core models) and watches
    /// their plugin objects, re-discovering if any of them dies.
    void adoptWidgetProviders(const QList<Daqster::IWidgetProvider*>& providers);

    /// REQ-SW-PL-079: hands ALL IStatusBarProvider plugins to the status bar
    /// controller. Called on start and whenever a plugin is loaded/unloaded,
    /// so a provider that appears later still gets its row.
    void adoptStatusBarProviders();

    /// Dev driver (DAQSTER_AUTOSTART_VIDEO=1): builds a video source ->
    /// VideoOutput graph, connects it, starts playback from DAQSTER_VIDEO_FILE /
    /// DAQSTER_STREAM_URL and enables the "Perf" checkbox — no GUI interaction
    /// needed (used by the PERF measurement harness).
    void autoStartVideo();

    /// Dev driver (DAQSTER_AUTOSTART_VIDEO=1 / DAQSTER_AUTOSTART_FLOW +
    /// DAQSTER_VIDEO_FILE, REQ-SW-PL-038): finds the VideoFileSource and
    /// VideoOutput nodes in the current graph by model-name, configures the
    /// source with DAQSTER_VIDEO_FILE, presses its "Play" button and enables
    /// the "Perf" checkbox on the output (plus DAQSTER_SCENE_VIDEO handling).
    void startVideoPlayback();

    /// Saves the current scene to a .flow file (REQ-SW-PL-049): graph model
    /// JSON + groups (byte-identical to DataFlowGraphicsScene::save()) + the
    /// "ui" section captured from the current runtime layout. Opens a file
    /// dialog. Returns true on success.
    bool saveSceneToFile();

    /// Captures the current runtime UI layout (REQ-SW-PL-049): workspace
    /// geometry from m_workspaces (or the main window default) and per-node
    /// deembed state + geometry + autoStart for every node with a widget.
    FlowUi::UiSection captureUiSection() const;

    /// Restores the runtime UI layout from a loaded "ui" section
    /// (REQ-SW-PL-049): stores workspaces, records autoStart flags and
    /// deembeds nodes whose saved state says so (tolerant — missing nodes
    /// are skipped).
    void applyUiSection(const FlowUi::UiSection& ui);

    // ── Layout Designer (REQ-SW-PL-059) ────────────────────────────────────
    /// Toggles the visual layout designer (F6): hides the canvas and shows the
    /// designer panel, or captures the designed section and returns to canvas.
    void setLayoutDesignerMode(bool enabled);

    /// Deembeds every node widget so the layout engine can reparent it into the
    /// designer preview, or re-embeds them when returning to the canvas.
    void deembedAllWidgets();
    void reembedAllWidgets();

    /// Nodes that have a widget (offered in the designer's Available list).
    QVector<QPair<QtNodes::NodeId, QString>> collectAvailableNodes() const;

    /// Persists the section currently held by the designer and saves the scene.
    void saveLayoutFromDesigner();

    QMainWindow* m_Win;
    NodeEditorWidget* m_Widget;

    /// Status bar rows + plugin menu (REQ-SW-PL-079). Parented to `this`, so it
    /// survives the window; its own widget handles are QPointers and go null
    /// with the window, which turns every later refresh into a no-op.
    QPointer<StatusBarController> m_statusBar;

    /// Visual layout designer (REQ-SW-PL-059). Created in Initialize(), shown
    /// only in designer mode.
    LayoutDesignerPanel* m_designer = nullptr;
    bool m_designerMode = false;
    QAction* m_designerAction = nullptr;

    /// `File → Close Scene` (REQ-SW-PL-078). Also on the toolbar, so the close
    /// button the IDE was missing is visible without opening a menu.
    QAction* m_closeSceneAction = nullptr;

    /// True when the graph changed since the last load/save — decides whether
    /// "Close Scene" asks before discarding (REQ-SW-PL-078 AC1).
    bool m_sceneDirty = false;

    /// Schema-v2 layout section (REQ-SW-PL-053 Phase 2a / REQ-SW-PL-059): kept
    /// when a .flow carries a version>=2 "ui" section or after the designer runs.
    FlowUi::LayoutSection m_layoutSectionV2;
    bool m_hasLayoutV2 = false;

    /// Workspace layout captured at save time (REQ-SW-PL-049). Empty until a
    /// .flow with a "ui" section is loaded or a save captures the default.
    std::vector<FlowUi::WorkspaceUi> m_workspaces;

    /// Per-node autoStart flags restored from the "ui" section (REQ-SW-PL-049).
    QHash<QtNodes::NodeId, bool> m_autoStartNodes;

    /// Presentation mode state (REQ-SW-PL-048): true = canvas hidden, deembedded
    /// widgets shown; false = normal editor mode.
    bool m_presentationMode = false;
};
