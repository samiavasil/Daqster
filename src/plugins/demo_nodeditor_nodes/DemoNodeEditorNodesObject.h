#pragma once

#include "QBasePluginObject.h"
#include "INodeProvider.h"
#include <QtNodes/Definitions>

class QMainWindow;

class DemoNodeEditorNodesObject : public Daqster::QBasePluginObject, public Daqster::INodeProvider
{
    Q_OBJECT
    Q_INTERFACES(Daqster::INodeProvider)
public:
    DemoNodeEditorNodesObject(QObject* Parent = nullptr);
    virtual ~DemoNodeEditorNodesObject();
    void SetName(const QString& name);
    virtual bool Initialize();

    // INodeProvider interface
    void registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const override;

protected:
    virtual void DeInitialize();

public slots:
    void MainWinDestroyed(QObject* obj);

private:
    QMainWindow* m_Win;
};
