#include "DemoNodeEditorNodesObject.h"
#include "QPluginManager.h"
#include "debug.h"

#include <QMainWindow>

#include <QtNodes/NodeDelegateModelRegistry>

#include "Displays/GenericDisplay/GenericDisplayNode.h"
#include "Displays/AudioDisplay/AudioDisplayModelObsolete.h"
#include "Displays/DaqDisplay/DaqDisplayNode.h"
#include "Routing/Demux/DemuxNodeObsolete.h"
#include "Routing/Mux/MuxNodeObsolete.h"
#include "Sources/AudioSource/AudioSourceDataModel.h"
#include "Sources/AudioSource/AudioSourceDataModelObsolete.h"
#include "Sources/LLamaSource/LLamaModelDataModel.h"
#include "Sources/LLamaSource/ConsoleDataModel.h"
#include "Sources/Video/CameraSourceNode.h"
#include "Sources/Video/VideoFileSourceNode.h"
#include "Sources/Video/StreamSourceNode.h"
#include "Sources/Video/VideoOutputNode.h"
#include "Sources/Video/VideoTransformNode.h"
#include <QDevIoDisplayModelObsolete.h>

// ── Saved-graph alias registration (REQ-SW-PL-023 §7) ────────────────
// The bundled QtNodes registry (external_libs/nodeeditor) keys models by their
// (virtual) name() — NodeDelegateModelRegistry::computeName() in
// NodeDelegateModelRegistry.hpp. There is NO explicit-name registration
// overload, so old-graph compatibility is implemented with thin subclasses
// that override only name() to return the pre-rename registry key. The
// obsolete node is then registered under BOTH its new `*Obsolete` name and
// the historical key; old saved graphs resolve to the same working model.
// No Q_OBJECT here: the aliases add no signals/slots — they only override the
// virtual name() (their QObject meta-object is inherited from the base class).
class AudioDisplayModelObsoleteAlias : public AudioDisplayModelObsolete
{
public:
    QString name() const override
    { return QStringLiteral("AudioDisplay"); }
};

class DemuxNodeObsoleteAlias : public DemuxNodeObsolete
{
public:
    QString name() const override
    { return QStringLiteral("DemuxNode"); }
};

class MuxNodeObsoleteAlias : public MuxNodeObsolete
{
public:
    QString name() const override
    { return QStringLiteral("MuxNode"); }
};

class QDevIoDisplayModelObsoleteAlias : public QDevIoDisplayModelObsolete
{
public:
    QString name() const override
    { return QStringLiteral("QDevIoDisplay"); }
};

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
    registry.registerModel<AudioDisplayModelObsolete>("Displays");
    registry.registerModel<AudioDisplayModelObsoleteAlias>("Displays"); // old key "AudioDisplay"
    registry.registerModel<GenericDisplayNode>("Displays");
    registry.registerModel<DaqDisplayNode>("Displays");
    registry.registerModel<QDevIoDisplayModelObsolete>("Displays");
    registry.registerModel<QDevIoDisplayModelObsoleteAlias>("Displays"); // old key "QDevIoDisplay"

    // Stream routing nodes
    registry.registerModel<DemuxNodeObsolete>("Routing");
    registry.registerModel<DemuxNodeObsoleteAlias>("Routing"); // old key "DemuxNode"
    registry.registerModel<MuxNodeObsolete>("Routing");
    registry.registerModel<MuxNodeObsoleteAlias>("Routing"); // old key "MuxNode"

    // Audio source + LLama source (moved from node_editor_ide)
    // REQ-SW-PL-024: the SampledData AudioSource takes the "AudioSource" key;
    // the old QDevIO mic is registered as "AudioSourceObsolete" (no alias
    // under "AudioSource" — old saved graphs instantiate the new node and
    // QDevIO edges drop, documented consequence).
    registry.registerModel<AudioSourceDataModel>("Sources");
    registry.registerModel<AudioSourceDataModelObsolete>("Sources");
    registry.registerModel<LLamaModelDataModel>("LLama");
    registry.registerModel<ConsoleDataModel>("LLama");

    // Video nodes (ImageData / "image" flow)
    registry.registerModel<CameraSourceNode>("Video");
    registry.registerModel<VideoFileSourceNode>("Video");
    registry.registerModel<StreamSourceNode>("Video");
    registry.registerModel<VideoOutputNode>("Video");
    registry.registerModel<VideoTransformNode>("Video");
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
