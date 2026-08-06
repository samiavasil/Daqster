#include "DemoNodeEditorNodesObject.h"
#include "QPluginManager.h"
#include "debug.h"

#include <QMainWindow>

#include <QtNodes/NodeDelegateModelRegistry>

#include "Displays/GenericDisplay/GenericDisplayNode.h"
#include "Displays/AudioDisplay/AudioDisplayModel.h"
#include "Routing/Demux/DemuxNode.h"
#include "Routing/Mux/MuxNode.h"
#include "Sources/AudioSource/AudioSourceDataModel.h"
#include "Sources/LLamaSource/LLamaModelDataModel.h"
#include "Sources/LLamaSource/ConsoleDataModel.h"
#include "Sources/Video/CameraSourceNode.h"
#include "Sources/Video/VideoFileSourceNode.h"
#include "Sources/Video/StreamSourceNode.h"
#include "Sources/Video/VideoOutputNode.h"
#include "Sources/Video/VideoModifierNode.h"

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
    // Display nodes
    registry.registerModel<AudioDisplayModel>("Displays");
    registry.registerModel<GenericDisplayNode>("Displays");

    // Stream routing nodes
    registry.registerModel<DemuxNode>("Routing");
    registry.registerModel<MuxNode>("Routing");

    // Audio source + LLama source (moved from node_editor_ide)
    registry.registerModel<AudioSourceDataModel>("Sources");
    registry.registerModel<LLamaModelDataModel>("LLama");
    registry.registerModel<ConsoleDataModel>("LLama");

    // Video nodes (ImageData / "image" flow)
    registry.registerModel<CameraSourceNode>("Video");
    registry.registerModel<VideoFileSourceNode>("Video");
    registry.registerModel<StreamSourceNode>("Video");
    registry.registerModel<VideoOutputNode>("Video");
    registry.registerModel<VideoModifierNode>("Video");
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
