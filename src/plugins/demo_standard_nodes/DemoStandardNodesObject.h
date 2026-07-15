#pragma once

#include "QBasePluginObject.h"
#include "INodeProvider.h"
#include <QtNodes/Definitions>

class QMainWindow;

using namespace Daqster;

class DemoStandardNodesObject : public QBasePluginObject, public INodeProvider
{
    Q_OBJECT
    Q_INTERFACES(Daqster::INodeProvider)
public:
    DemoStandardNodesObject(QObject* Parent = nullptr);
    virtual ~DemoStandardNodesObject();
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
