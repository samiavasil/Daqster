#pragma once

#include "QBasePluginObject.h"
#include <QtNodes/Definitions>

class NodeEditorWidget;
class QMainWindow;

class NodeEditorIdeObject : public Daqster::QBasePluginObject
{
    Q_OBJECT
public:
    NodeEditorIdeObject(QObject* Parent = nullptr);
    virtual ~NodeEditorIdeObject();
    void SetName(const QString& name);
    virtual bool Initialize();

protected:
    virtual void DeInitialize();

public slots:
    void MainWinDestroyed(QObject* obj);
    void ShowPlugins();

protected slots:
    void nodeDoubleClicked(QtNodes::NodeId nodeId);

private:
    void registerBuiltInNodes();
    void discoverAndRegisterExternalNodes();

    /// Dev driver (DAQSTER_AUTOSTART_VIDEO=1): builds a video source ->
    /// VideoOutput graph, connects it, starts playback from DAQSTER_VIDEO_FILE /
    /// DAQSTER_STREAM_URL and enables the "Perf" checkbox — no GUI interaction
    /// needed (used by the PERF measurement harness).
    void autoStartVideo();

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

    /// Dev driver (DAQSTER_AUTOSTART_VIDEO=1 / DAQSTER_AUTOSTART_FLOW +
    /// DAQSTER_VIDEO_FILE, REQ-SW-PL-038): finds the VideoFileSource and
    /// VideoOutput nodes in the current graph by model-name, configures the
    /// source with DAQSTER_VIDEO_FILE, presses its "Play" button and enables
    /// the "Perf" checkbox on the output (plus DAQSTER_SCENE_VIDEO handling).
    void startVideoPlayback();

    QMainWindow* m_Win;
    NodeEditorWidget* m_Widget;
};
