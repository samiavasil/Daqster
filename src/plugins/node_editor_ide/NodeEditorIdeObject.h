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
    bool loadSceneTolerant();

    QMainWindow* m_Win;
    NodeEditorWidget* m_Widget;
};
