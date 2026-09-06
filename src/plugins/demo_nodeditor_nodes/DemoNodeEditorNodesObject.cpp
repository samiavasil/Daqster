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
#include "Sources/Video/VideoEffectNode.h"
#include "Sources/Video/CustomShaderNode.h"
#include "Sources/Video/FrameSamplerNode.h"
#ifdef HAVE_LIBIIO
#include "Sources/PlutoSdr/PlutoSdrModel.h"
#endif
#ifdef HAVE_SYSTEM_MONITOR
#include "Sources/SystemMonitor/SystemMonitorModel.h"
#endif
#ifdef HAVE_GAMEPAD
#include "Sources/Gamepad/GamepadModel.h"
#endif
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
// The "AudioDisplay" key is consolidated onto the SampledData display world:
// the alias now derives from DaqDisplayNode (canonical SampledData display),
// NOT the QDevIO obsolete node — old saved graphs resolve to the real
// multi-plot/FFT/ring-buffer display. The QDevIO world stays alive under
// "AudioDisplayObsolete" for old QDevIO graphs.
class AudioDisplayAlias : public DaqDisplayNode
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
    registry.registerModel<AudioDisplayModelObsolete>("Obsolete");
    registry.registerModel<AudioDisplayAlias>("Daq/Display"); // old key "AudioDisplay" -> SampledData display
    registry.registerModel<GenericDisplayNode>("Daq/Display");
    registry.registerModel<DaqDisplayNode>("Daq/Display");
    registry.registerModel<QDevIoDisplayModelObsolete>("Obsolete");
    registry.registerModel<QDevIoDisplayModelObsoleteAlias>("Obsolete"); // old key "QDevIoDisplay"

    // Stream routing nodes
    registry.registerModel<DemuxNodeObsolete>("Obsolete");
    registry.registerModel<DemuxNodeObsoleteAlias>("Obsolete"); // old key "DemuxNode"
    registry.registerModel<MuxNodeObsolete>("Obsolete");
    registry.registerModel<MuxNodeObsoleteAlias>("Obsolete"); // old key "MuxNode"

    // Audio source + LLama source (moved from node_editor_ide)
    // REQ-SW-PL-024: the SampledData AudioSource takes the "AudioSource" key;
    // the old QDevIO mic is registered as "AudioSourceObsolete" (no alias
    // under "AudioSource" — old saved graphs instantiate the new node and
    // QDevIO edges drop, documented consequence).
    registry.registerModel<AudioSourceDataModel>("Audio/Sources");
    registry.registerModel<AudioSourceDataModelObsolete>("Obsolete");
#ifdef HAVE_LIBIIO
    // PlutoSDR RX DAQ node (REQ-SW-PL-040) — compiled only when libiio is found.
    registry.registerModel<PlutoSdrModel>("Daq/Sources");
#endif
#ifdef HAVE_SYSTEM_MONITOR
    // System Monitor source node (REQ-SW-PL-041) — Linux /proc + /sys telemetry.
    registry.registerModel<SystemMonitorModel>("Daq/Sources");
#endif
#ifdef HAVE_GAMEPAD
    // Gamepad input source node (REQ-SW-PL-042) — Linux joystick API.
    registry.registerModel<GamepadModel>("Daq/Sources");
#endif
    registry.registerModel<LLamaModelDataModel>("AI/LLM");
    registry.registerModel<ConsoleDataModel>("General/Display");

    // Video nodes (VideoFrameData / "video-frame" flow)
    registry.registerModel<CameraSourceNode>("Video/Sources");
    registry.registerModel<VideoFileSourceNode>("Video/Sources");
    registry.registerModel<StreamSourceNode>("Video/Sources");
    registry.registerModel<VideoOutputNode>("Video/Display");

    // Video effect nodes (VideoFrameData flow, REQ-SW-PL-028) — ONE node with
    // an effect combo (REQ-SW-PL-028 AC 4) + the frame resampler (REQ-SW-PL-030).
    // The 7 per-effect aliases (VideoEffectBrightnessNode, ...) were removed on
    // 2026-08-26 (user decision) — old saved graphs referencing those registry
    // keys no longer load; "VideoEffect" is the only registered effect node.
    registry.registerModel<VideoEffectNode>("Video/Processing");
    registry.registerModel<CustomShaderNode>("Video/Processing");
    registry.registerModel<FrameSamplerNode>("Video/Processing");
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
