#pragma once

#include "FlowUiSection.h"
#include <DaqsterCore/plugin/QBasePluginObject.h>
#include <QtNodes/Definitions>

#include <QEvent>
#include <vector>

class NodeEditorWidget;
class QMainWindow;
class RuntimeShell;

class NodeEditorIdeObject : public Daqster::QBasePluginObject
{
    Q_OBJECT
public:
    NodeEditorIdeObject(QObject* Parent = nullptr);
    virtual ~NodeEditorIdeObject();
    void SetName(const QString& name);
    virtual bool Initialize();

    /// Runtime mode entry point (REQ-SW-PL-048): loads `flowPath` and shows
    /// deembedded widgets as the application UI (editor canvas hidden).
    /// Returns true on success, false on error (error message already shown).
    Q_INVOKABLE bool RunRuntime(const QString& flowPath);

    // ── Public registration + loading API (REQ-SW-PL-048) ─────────────
    // These were private before; they are now public so RuntimeShell can
    /// reuse the same registration and loading logic without duplication.
    void registerBuiltInNodes();
    void discoverAndRegisterExternalNodes();

    /// Consolidated registration: calls registerBuiltInNodes() then
    /// discoverAndRegisterExternalNodes() — mirrors the sequence in
    /// Initialize().
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

    /// Presentation mode toggle (REQ-SW-PL-048): F11 key handler.
    /// Hides GraphicsView + shows deembedded widgets; toggles back.
    void togglePresentationMode();

protected slots:
    void nodeDoubleClicked(QtNodes::NodeId nodeId);

private:
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

    QMainWindow* m_Win;
    NodeEditorWidget* m_Widget;

    /// RuntimeShell instance for runtime mode (REQ-SW-PL-048). Owned by this
    /// object; created lazily in RunRuntime() and stays alive for the duration
    /// of the event loop.
    RuntimeShell* m_runtimeShell = nullptr;

    /// Workspace layout captured at save time (REQ-SW-PL-049). Empty until a
    /// .flow with a "ui" section is loaded or a save captures the default.
    std::vector<FlowUi::WorkspaceUi> m_workspaces;

    /// Per-node autoStart flags restored from the "ui" section (REQ-SW-PL-049).
    QHash<QtNodes::NodeId, bool> m_autoStartNodes;

    /// Presentation mode state (REQ-SW-PL-048): true = canvas hidden, deembedded
    /// widgets shown; false = normal editor mode.
    bool m_presentationMode = false;
};