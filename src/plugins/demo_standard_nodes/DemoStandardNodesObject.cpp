#include "DemoStandardNodesObject.h"
#include "QPluginManager.h"
#include "debug.h"

#include <QMainWindow>

#include <QtNodes/NodeDelegateModelRegistry>

#include "NumberSourceDataModel.h"
#include "NumberDisplayDataModel.h"
#include "ModuloModel.h"

DemoStandardNodesObject::DemoStandardNodesObject(QObject* Parent)
    : QBasePluginObject(Parent)
    , m_Win(nullptr)
{
}

DemoStandardNodesObject::~DemoStandardNodesObject()
{
    DeInitialize();
}

void DemoStandardNodesObject::SetName(const QString& name)
{
    Q_UNUSED(name);
}

bool DemoStandardNodesObject::Initialize()
{
    // This plugin has no GUI — it only provides nodes to the node editor.
    // The node_editor_ide plugin discovers us via INodeProvider and calls registerNodes().
    return true;
}

void DemoStandardNodesObject::registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const
{
    registry.registerModel<NumberSourceDataModel>("Sources");
    registry.registerModel<NumberDisplayDataModel>("Displays");
    registry.registerModel<ModuloModel<int>>("Operators");
    registry.registerModel<ModuloModel<double>>("Operators");
}

void DemoStandardNodesObject::DeInitialize()
{
    DEBUG_V << "DemoStandardNodesObject destroyed";
}

void DemoStandardNodesObject::MainWinDestroyed(QObject* obj)
{
    m_Win = nullptr;
    deleteLater();
    if (nullptr == obj)
        DEBUG << "Strange::!!!";
}
