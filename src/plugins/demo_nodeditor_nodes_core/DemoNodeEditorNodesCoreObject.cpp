#include "DemoNodeEditorNodesCoreObject.h"

#include <QtNodes/NodeDelegateModelRegistry>

// Core node models (no GUI widgets)
#include "Sources/AudioSource/AudioSourceDataModel.h"
#include "Sources/LLamaSource/LLamaModelDataModel.h"
#include "Sources/LLamaSource/ConsoleDataModel.h"
#include "Sources/Video/CameraSourceNode.h"
#include "Sources/Video/VideoFileSourceNode.h"
#include "Sources/Video/StreamSourceNode.h"
#include "Sources/Video/VideoOutputNode.h"
#include "Sources/Video/VideoEffectNode.h"
#include "Sources/Video/CustomShaderNode.h"
#include "Sources/Video/FrameSamplerNode.h"
#include "Sources/PlutoSdr/PlutoSdrModel.h"
#include "Sources/SystemMonitor/SystemMonitorModel.h"
#include "Sources/Gamepad/GamepadModel.h"
#include "Sources/FilePlayback/FilePlaybackModel.h"
#include "Sources/NetworkSource/NetworkSourceModel.h"
#include "Sinks/NetworkSink/NetworkSinkModel.h"
#include "Sinks/FileRecord/FileRecordModel.h"
#include "Sources/GpuMonitor/GpuMonitorModel.h"
#include "Sources/JackDetect/JackDetectModel.h"
#include "Sources/Pcap/PcapModel.h"
#include "Displays/DaqDisplay/DaqDisplayNode.h"
#include "Routing/Demux/DemuxNodeObsolete.h"
#include "Routing/Mux/MuxNodeObsolete.h"
#include "Sources/AudioSource/AudioSourceDataModelObsolete.h"
#include "Sources/AudioSource/AudioWorkerObsolete.h"
#include "Displays/GenericDisplay/GenericDisplayNode.h"
#include "Displays/AudioDisplay/AudioDisplayModelObsolete.h"
#include "GenericQDevIoConnectorObsolete.h"

// Built-in nodes (moved from node_editor_ide)
#include "Sources/NumberSource/NumberSourceDataModel.h"
#include "Sources/NumberSource/NumberSourceDataUi.h"
#include "Displays/NumberDisplay/NumberDisplayDataModel.h"
#include "Operators/ModuloModel.h"
#include "Operators/ArithmeticLogic/ArithmeticLogicModel.h"
#include "Operators/ArithmeticLogic/ExprParser.h"

namespace Daqster {

DemoNodeEditorNodesCoreObject::DemoNodeEditorNodesCoreObject(QObject* parent)
    : QBasePluginObject(parent)
{
}

DemoNodeEditorNodesCoreObject::~DemoNodeEditorNodesCoreObject()
{
}

bool DemoNodeEditorNodesCoreObject::Initialize()
{
    // Core plugin initialization - no GUI setup needed
    return true;
}

void DemoNodeEditorNodesCoreObject::DeInitialize()
{
    // Core plugin cleanup
}

void DemoNodeEditorNodesCoreObject::registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const
{
    // Register core node models (no embedded widgets)
    // Built-in nodes (moved from node_editor_ide)
    registry.registerModel<NumberSourceDataModel>("Sources/General");
    registry.registerModel<NumberDisplayDataModel>("Displays/General");
    registry.registerModel<ModuloModel>("Operators/General");
    registry.registerModel<ArithmeticLogicModel>("Operators/General");

    // Sources
    registry.registerModel<AudioSourceDataModel>("Sources/Audio");
    // Migrated from old plugin: obsolete audio models
    registry.registerModel<AudioSourceDataModelObsolete>("Obsolete");
    registry.registerModel<LLamaModelDataModel>("Sources/LLM");
    registry.registerModel<ConsoleDataModel>("Sources/LLM");
    registry.registerModel<CameraSourceNode>("Sources/Video");
    registry.registerModel<VideoFileSourceNode>("Sources/Video");
    registry.registerModel<StreamSourceNode>("Sources/Video");
    registry.registerModel<VideoOutputNode>("Sinks/Video");
    registry.registerModel<VideoEffectNode>("Processing/Video");
    registry.registerModel<CustomShaderNode>("Processing/Video");
    registry.registerModel<FrameSamplerNode>("Processing/Video");
    
    // Conditional nodes (guarded by compile definitions)
#ifdef HAVE_LIBIIO
    registry.registerModel<PlutoSdrModel>("Sources/SDR");
#endif
#ifdef HAVE_SYSTEM_MONITOR
    registry.registerModel<SystemMonitorModel>("Sources/System");
#endif
#ifdef HAVE_GAMEPAD
    registry.registerModel<GamepadModel>("Sources/Gamepad");
#endif
#ifdef HAVE_NVML
    registry.registerModel<GpuMonitorModel>("Sources/GPU");
#endif
#ifdef HAVE_JACK_DETECT
    registry.registerModel<JackDetectModel>("Sources/Audio");
#endif
#ifdef HAVE_PCAP
    registry.registerModel<PcapModel>("Sources/Network");
#endif

    // File I/O
    registry.registerModel<FilePlaybackModel>("Sources/File");
    registry.registerModel<FileRecordModel>("Sinks/File");

    // Network
    registry.registerModel<NetworkSourceModel>("Sources/Network");
    registry.registerModel<NetworkSinkModel>("Sinks/Network");

    // Displays
    registry.registerModel<DaqDisplayNode>("Displays/DAQ");
    // Migrated from old plugin: generic/obsolete displays
    registry.registerModel<GenericDisplayNode>("Displays/DAQ");
    registry.registerModel<AudioDisplayModelObsolete>("Displays/DAQ");
    // QDevIoDisplayModelObsolete is provided by node_editor_ide/BuiltInNodes plugin,
    // not by this core plugin. Available when GUI plugin loads.
    // Aliases for old saved graphs (name() override only)
    class DemuxNodeObsoleteAlias : public DemuxNodeObsolete {
    public:
        QString name() const override { return QStringLiteral("DemuxNode"); }
    };
    class MuxNodeObsoleteAlias : public MuxNodeObsolete {
    public:
        QString name() const override { return QStringLiteral("MuxNode"); }
    };
    class AudioDisplayAlias : public DaqDisplayNode {
    public:
        QString name() const override { return QStringLiteral("AudioDisplay"); }
    };
    registry.registerModel<DemuxNodeObsolete>("Routing");
    registry.registerModel<MuxNodeObsolete>("Routing");
    registry.registerModel<DemuxNodeObsoleteAlias>("Routing");
    registry.registerModel<MuxNodeObsoleteAlias>("Routing");
    registry.registerModel<AudioDisplayAlias>("Displays/DAQ");
}

} // namespace Daqster