#pragma once

#include "QBasePluginObject.h"
#include <QtNodes/Definitions>

class NodeEditorWidget;
class QMainWindow;

using namespace Daqster;

class NodeEditorAppObject : public QBasePluginObject
{
    Q_OBJECT
public:
    NodeEditorAppObject(QObject* Parent = nullptr);
    virtual ~NodeEditorAppObject();
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
    QMainWindow* m_Win;
    NodeEditorWidget* m_Widget;
};
