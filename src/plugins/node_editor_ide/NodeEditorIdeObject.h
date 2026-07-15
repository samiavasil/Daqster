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

    QMainWindow* m_Win;
    NodeEditorWidget* m_Widget;
};
