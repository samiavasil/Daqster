#include "NodeditorNodesCoreObject.h"

#include <QtNodes/NodeDelegateModelRegistry>

// Core node models (no GUI widgets)
#include "Sources/AudioSource/AudioSourceDataModel.h"
#include "Sources/LLamaSource/LLamaModelDataModel.h"
#include "Sources/LLamaSource/ConsoleDataModel.h"
#include "Sources/Video/CameraSourceNode.h"
#include "Sources/Video/VideoFileSourceNode.h"
#include "Sources/Video/StreamSourceNode.h"
#include "Sinks/Video/VideoOutputNode.h"
#include "Processing/Video/VideoEffectNode.h"
#include "Processing/Video/CustomShaderNode.h"
#include "Processing/Video/FrameSamplerNode.h"
#include "Sources/PlutoSdr/PlutoSdrModel.h"
#include "Sources/SystemMonitor/SystemMonitorModel.h"
#include "Sources/Gamepad/GamepadModel.h"
#include "Sources/FilePlayback/FilePlaybackModel.h"
#include "Sources/NetworkSource/NetworkSourceModel.h"
#include "Sinks/NetworkSink/NetworkSinkModel.h"
#include "Sinks/FileRecord/FileRecordModel.h"
#include "Sinks/Audio/AudioOutputNode.h"
#include "Sinks/Audio/AudioSinkNode.h"
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
#include "Displays/NumberDisplay/NumberDisplayDataModel.h"
#include "Operators/ModuloModel.h"
#include "Operators/ArithmeticLogic/ArithmeticLogicModel.h"
#include "Operators/ArithmeticLogic/ExprParser.h"

namespace Daqster {

NodeditorNodesCoreObject::NodeditorNodesCoreObject(QObject* parent)
    : QBasePluginObject(parent)
{
}

NodeditorNodesCoreObject::~NodeditorNodesCoreObject()
{
}

bool NodeditorNodesCoreObject::Initialize()
{
    // Core plugin initialization - no GUI setup needed
    return true;
}

void NodeditorNodesCoreObject::DeInitialize()
{
    // Core plugin cleanup
}

void NodeditorNodesCoreObject::registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const
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
    registry.registerModel<LLamaModelDataModel>("AI/LLM");
    registry.registerModel<ConsoleDataModel>("AI/LLM");
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

    // Audio output (REQ-SW-PL-076): one persistent sink, no widget in core
    registry.registerModel<AudioOutputNode>("Sinks/Audio");
    // Audio sink / WAV recorder (REQ-SW-PL-077, moved out of the AI plugin).
    // name() is still "AudioSinkNode" — that string is the .flow key.
    registry.registerModel<AudioSinkNode>("Sinks/Audio");

    // Displays
    registry.registerModel<DaqDisplayNode>("Displays/DAQ");
    // Migrated from old plugin: generic/obsolete displays
    registry.registerModel<GenericDisplayNode>("Displays/DAQ");
    registry.registerModel<AudioDisplayModelObsolete>("Displays/DAQ");
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

void NodeditorNodesCoreObject::registerNodesHeadless(QtNodes::NodeDelegateModelRegistry& registry) const
{
    // Headless-compatible nodes ONLY - no widget dependencies
    // Built-in nodes (moved from node_editor_ide) - pure logic, no widgets
    registry.registerModel<NumberSourceDataModel>("Sources/General");
    registry.registerModel<NumberDisplayDataModel>("Displays/General");
    registry.registerModel<ModuloModel>("Operators/General");
    registry.registerModel<ArithmeticLogicModel>("Operators/General");

    // Sources - pure logic, programmatic configuration
    registry.registerModel<AudioSourceDataModel>("Sources/Audio");
    // Migrated from old plugin: obsolete audio models (REQ-SW-PL-056 AC4).
    // Registered as-is — no refactor, no removal; see the AC4 note below.
    registry.registerModel<AudioSourceDataModelObsolete>("Obsolete");
    registry.registerModel<LLamaModelDataModel>("AI/LLM");
    registry.registerModel<ConsoleDataModel>("AI/LLM");
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

    // Audio output (REQ-SW-PL-076): one persistent sink, no widget in core
    registry.registerModel<AudioOutputNode>("Sinks/Audio");
    // Audio sink / WAV recorder (REQ-SW-PL-077, moved out of the AI plugin).
    // name() is still "AudioSinkNode" — that string is the .flow key.
    registry.registerModel<AudioSinkNode>("Sinks/Audio");

    // ── Displays (REQ-SW-PL-056 AC4) ─────────────────────────────────────
    // Headless runs the flow; these render nothing, because no widget
    // provider exists in headless mode. That is the intended behaviour, not
    // an error — see the log note in HeadlessEngine.
    //
    // The _core plugin is 100% widget-include-free since 2026-10-08
    // (check_core_widgets_free.cmake, 120 TUs, EMPTY allow-list): QDevIo
    // display models are model-only (embeddedWidget() returns nullptr) and
    // the waveform view lives in the GUI plugin. Registering these pulls no
    // QtWidgets into the headless path.
    registry.registerModel<DaqDisplayNode>("Displays/DAQ");
    // Migrated from old plugin: generic/obsolete displays
    registry.registerModel<GenericDisplayNode>("Displays/DAQ");
    registry.registerModel<AudioDisplayModelObsolete>("Displays/DAQ");

    // Routing (obsolete) - no widgets
    registry.registerModel<DemuxNodeObsolete>("Routing");
    registry.registerModel<MuxNodeObsolete>("Routing");
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
    registry.registerModel<DemuxNodeObsoleteAlias>("Routing");
    registry.registerModel<MuxNodeObsoleteAlias>("Routing");
    // Alias for old saved graphs — 5 of the tracked .flow files name it
    // "AudioDisplay" (REQ-SW-PL-056 AC4).
    registry.registerModel<AudioDisplayAlias>("Displays/DAQ");
}

} // namespace Daqster