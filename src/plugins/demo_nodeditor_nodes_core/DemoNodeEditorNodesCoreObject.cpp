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
    // Sources
    registry.registerModel<AudioSourceDataModel>("Sources/Audio");
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

    // Routing (obsolete)
    registry.registerModel<DemuxNodeObsolete>("Routing");
    registry.registerModel<MuxNodeObsolete>("Routing");
}

} // namespace Daqster