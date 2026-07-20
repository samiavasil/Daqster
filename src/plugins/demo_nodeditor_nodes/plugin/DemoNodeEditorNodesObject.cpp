#include "DemoNodeEditorNodesObject.h"
#include "QPluginManager.h"
#include "debug.h"

#include <QMainWindow>

#include <QtNodes/NodeDelegateModelRegistry>

#include "../Sources/NumberSourceDataModel.h"
#include "../Displays/NumberDisplayDataModel.h"
#include "../Operators/ModuloModel.h"
#include "../Displays/AudioDisplayModel.h"
#include "../Displays/GenericDisplayNode.h"
#include "../Routing/DemuxNode.h"
#include "../Routing/MuxNode.h"

DemoNodeEditorNodesObject::DemoNodeEditorNodesObject(QObject* Parent)
    : Daqster::QBasePluginObject(Parent)
    , m_Win(nullptr)
{
}

DemoNodeEditorNodesObject::~DemoNodeEditorNodesObject()
{
    DeInitialize();
}

void DemoNodeEditorNodesObject::SetName(const QString& name)
{
    Q_UNUSED(name);
}

bool DemoNodeEditorNodesObject::Initialize()
{
    // This plugin has no GUI — it only provides nodes to the node editor.
    // The node_editor_ide plugin discovers us via INodeProvider and calls registerNodes().
    return true;
}

void DemoNodeEditorNodesObject::registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const
{
    // Original number nodes
    registry.registerModel<NumberSourceDataModel>("Sources");
    registry.registerModel<NumberDisplayDataModel>("Displays");
    registry.registerModel<ModuloModel<int>>("Operators");
    registry.registerModel<ModuloModel<double>>("Operators");

    // New display nodes
    registry.registerModel<AudioDisplayModel>("Displays");
    registry.registerModel<GenericDisplayNode>("Displays");

    // Stream routing nodes
    registry.registerModel<DemuxNode>("Routing");
    registry.registerModel<MuxNode>("Routing");
}

void DemoNodeEditorNodesObject::DeInitialize()
{
    DEBUG_V << "DemoNodeEditorNodesObject destroyed";
}

void DemoNodeEditorNodesObject::MainWinDestroyed(QObject* obj)
{
    m_Win = nullptr;
    deleteLater();
    if (nullptr == obj)
        DEBUG << "Strange::!!!";
}
