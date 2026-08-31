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

    QMainWindow* m_Win;
    NodeEditorWidget* m_Widget;
};
