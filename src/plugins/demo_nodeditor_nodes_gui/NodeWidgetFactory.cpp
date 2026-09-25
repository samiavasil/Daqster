#include "NodeWidgetFactory.h"

#include <QtNodes/NodeDelegateModel>
#include <QObject>

// Core models - include headers for static_cast
#include <Sources/AudioSource/AudioSourceDataModel.h>
#include <Sources/LLamaSource/LLamaModelDataModel.h>
#include <Sources/LLamaSource/ConsoleDataModel.h>
#include <Sources/Video/CameraSourceNode.h>
#include <Sources/Video/VideoFileSourceNode.h>
#include <Sources/Video/StreamSourceNode.h>
#include <Sources/Video/VideoOutputNode.h>
#include <Sources/Video/VideoEffectNode.h>
#include <Sources/Video/CustomShaderNode.h>
#include <Sources/Video/FrameSamplerNode.h>
#include <Sources/PlutoSdr/PlutoSdrModel.h>
#include <Sources/SystemMonitor/SystemMonitorModel.h>
#include <Sources/Gamepad/GamepadModel.h>
#include <Sources/FilePlayback/FilePlaybackModel.h>
#include <Sinks/FileRecord/FileRecordModel.h>
#include <Sources/NetworkSource/NetworkSourceModel.h>
#include <Sinks/NetworkSink/NetworkSinkModel.h>
#include <Sources/GpuMonitor/GpuMonitorModel.h>
#include <Sources/JackDetect/JackDetectModel.h>
#include <Sources/Pcap/PcapModel.h>
#include <Displays/DaqDisplay/DaqDisplayNode.h>
#include <Routing/Demux/DemuxNodeObsolete.h>
#include <Routing/Mux/MuxNodeObsolete.h>

// GUI widgets
#include "Sources/AudioSource/AudioSourceDataModelUI.h"
#include "Sources/LLamaSource/ChatBaseWidget.h"
#include "Sources/Video/VideoGLBlitWidget.h"
#include "Sources/Video/VideoPerfBadge.h"
#include "Sources/PlutoSdr/PlutoSdrWidget.h"
#include "Sources/SystemMonitor/SystemMonitorWidget.h"
#include "Sources/Gamepad/GamepadWidget.h"
#include "Sources/FilePlayback/FilePlaybackWidget.h"
#include "Sources/NetworkSource/NetworkSourceWidget.h"
#include "Sinks/NetworkSink/NetworkSinkWidget.h"
#include "Sinks/FileRecord/FileRecordWidget.h"
#include "Sources/GpuMonitor/GpuMonitorWidget.h"
#include "Sources/JackDetect/JackDetectWidget.h"
#include "Sources/Pcap/PcapWidget.h"
#include "Displays/DaqDisplay/DaqDisplayNode.h"

// Core engines (from core plugin)
#include <Sources/SystemMonitor/SystemMonitorEngine.h>
#include <Sources/GpuMonitor/GpuMonitorEngine.h>
#include <Sources/JackDetect/JackDetectEngine.h>
#include <Sources/Pcap/PcapEngine.h>
#include <Sources/PlutoSdr/PlutoSdrEngine.h>

namespace Daqster {

NodeWidgetFactory::NodeWidgetFactory(QObject* parent)
    : QObject(parent)
{
}

NodeWidgetFactory::~NodeWidgetFactory()
{
}

void NodeWidgetFactory::registerWidgetCreator(const QString& modelName,
                                              std::function<QWidget*(QtNodes::NodeDelegateModel*)> creator)
{
    m_creators[modelName] = std::move(creator);
}

QWidget* NodeWidgetFactory::createWidget(QtNodes::NodeDelegateModel* model) const
{
    if (!model)
        return nullptr;

    const QString modelName = model->name();
    auto it = m_creators.constFind(modelName);
    if (it != m_creators.constEnd()) {
        return it.value()(model);
    }
    return nullptr;
}

bool NodeWidgetFactory::hasWidgetCreator(const QString& modelName) const
{
    return m_creators.contains(modelName);
}

// Helper: safe static cast after verifying model name
template<typename T>
T* safeCast(QtNodes::NodeDelegateModel* model, const QString& expectedName)
{
    if (!model || model->name() != expectedName)
        return nullptr;
    return static_cast<T*>(model);
}

// Static function to register all default widget creators
void registerDefaultWidgetCreators(NodeWidgetFactory* factory)
{
    if (!factory)
        return;

    // SystemMonitor
    factory->registerWidgetCreator("SystemMonitor", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<SystemMonitorModel>(model, "SystemMonitor");
        if (!m) return nullptr;
        auto* widget = new SystemMonitorWidget();
        widget->setEngine(m->engine());
        QObject::connect(widget, &SystemMonitorWidget::startRequested, m, &SystemMonitorModel::onStartRequested);
        QObject::connect(widget, &SystemMonitorWidget::stopRequested, m, &SystemMonitorModel::onStopRequested);
        QObject::connect(widget, &SystemMonitorWidget::intervalChanged, m, &SystemMonitorModel::onIntervalChanged);
        QObject::connect(widget, &SystemMonitorWidget::metricsChanged, m, &SystemMonitorModel::onMetricsChanged);
        return widget;
    });

    // FilePlayback
    factory->registerWidgetCreator("FilePlayback", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<FilePlaybackModel>(model, "FilePlayback");
        if (!m) return nullptr;
        auto* widget = new FilePlaybackWidget();
        QObject::connect(widget, &FilePlaybackWidget::playRequested, m, &FilePlaybackModel::onPlayRequested);
        QObject::connect(widget, &FilePlaybackWidget::stopRequested, m, &FilePlaybackModel::onStopRequested);
        QObject::connect(widget, &FilePlaybackWidget::pathChanged, m, &FilePlaybackModel::onPathChanged);
        QObject::connect(m, &FilePlaybackModel::statusChanged, widget, &FilePlaybackWidget::setStatus);
        return widget;
    });

    // NetworkSource
    factory->registerWidgetCreator("NetworkSource", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<NetworkSourceModel>(model, "NetworkSource");
        if (!m) return nullptr;
        auto* widget = new NetworkSourceWidget();
        QObject::connect(widget, &NetworkSourceWidget::startRequested, m, &NetworkSourceModel::onStartRequested);
        QObject::connect(widget, &NetworkSourceWidget::stopRequested, m, &NetworkSourceModel::onStopRequested);
        QObject::connect(m, &NetworkSourceModel::statusChanged, widget, &NetworkSourceWidget::setStatus);
        return widget;
    });

    // NetworkSink
    factory->registerWidgetCreator("NetworkSink", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<NetworkSinkModel>(model, "NetworkSink");
        if (!m) return nullptr;
        auto* widget = new NetworkSinkWidget();
        QObject::connect(widget, &NetworkSinkWidget::startRequested, m, &NetworkSinkModel::onStartRequested);
        QObject::connect(widget, &NetworkSinkWidget::stopRequested, m, &NetworkSinkModel::onStopRequested);
        QObject::connect(m, &NetworkSinkModel::statusChanged, widget, &NetworkSinkWidget::setStatus);
        return widget;
    });

    // FileRecord
    factory->registerWidgetCreator("FileRecord", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<FileRecordModel>(model, "FileRecord");
        if (!m) return nullptr;
        auto* widget = new FileRecordWidget();
        QObject::connect(widget, &FileRecordWidget::startRequested, m, &FileRecordModel::onStartRequested);
        QObject::connect(widget, &FileRecordWidget::stopRequested, m, &FileRecordModel::onStopRequested);
        QObject::connect(widget, &FileRecordWidget::pathChanged, m, &FileRecordModel::onPathChanged);
        QObject::connect(m, &FileRecordModel::statusChanged, widget, &FileRecordWidget::setStatus);
        return widget;
    });

    // GpuMonitor
    factory->registerWidgetCreator("GpuMonitor", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<GpuMonitorModel>(model, "GpuMonitor");
        if (!m) return nullptr;
        auto* widget = new GpuMonitorWidget();
        widget->setEngine(m->engine());
        QObject::connect(widget, &GpuMonitorWidget::startRequested, m, &GpuMonitorModel::onStartRequested);
        QObject::connect(widget, &GpuMonitorWidget::stopRequested, m, &GpuMonitorModel::onStopRequested);
        QObject::connect(widget, &GpuMonitorWidget::intervalChanged, m, &GpuMonitorModel::onIntervalChanged);
        return widget;
    });

    // JackDetect
    factory->registerWidgetCreator("JackDetect", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<JackDetectModel>(model, "JackDetect");
        if (!m) return nullptr;
        auto* widget = new JackDetectWidget();
        widget->setEngine(m->engine());
        QObject::connect(widget, &JackDetectWidget::startRequested, m, &JackDetectModel::onStartRequested);
        QObject::connect(widget, &JackDetectWidget::stopRequested, m, &JackDetectModel::onStopRequested);
        QObject::connect(widget, &JackDetectWidget::intervalChanged, m, &JackDetectModel::onIntervalChanged);
        return widget;
    });

    // PcapCapture
    factory->registerWidgetCreator("PcapCapture", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<PcapModel>(model, "PcapCapture");
        if (!m) return nullptr;
        auto* widget = new PcapWidget();
        widget->setEngine(m->engine());
        QObject::connect(widget, &PcapWidget::startRequested, m, &PcapModel::onStartRequested);
        QObject::connect(widget, &PcapWidget::stopRequested, m, &PcapModel::onStopRequested);
        QObject::connect(widget, &PcapWidget::interfaceChanged, m, &PcapModel::onInterfaceChanged);
        QObject::connect(widget, &PcapWidget::filterChanged, m, &PcapModel::onFilterChanged);
        QObject::connect(widget, &PcapWidget::snaplenChanged, m, &PcapModel::onSnaplenChanged);
        QObject::connect(widget, &PcapWidget::promiscuousChanged, m, &PcapModel::onPromiscuousChanged);
        return widget;
    });

    // PlutoSdr
    factory->registerWidgetCreator("PlutoSdr", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<PlutoSdrModel>(model, "PlutoSdr");
        if (!m) return nullptr;
        auto* widget = new PlutoSdrWidget();
        widget->setEngine(m->engine());
        QObject::connect(widget, &PlutoSdrWidget::startRequested, m, &PlutoSdrModel::onStartRequested);
        QObject::connect(widget, &PlutoSdrWidget::stopRequested, m, &PlutoSdrModel::onStopRequested);
        QObject::connect(widget, &PlutoSdrWidget::configChanged, m, &PlutoSdrModel::onConfigChanged);
        QObject::connect(m, &PlutoSdrModel::statusChanged, widget, &PlutoSdrWidget::setStatus);
        return widget;
    });

    // DaqDisplay - uses embedded widget from model
    factory->registerWidgetCreator("DaqDisplay", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<DaqDisplayNode>(model, "DaqDisplay");
        if (!m) return nullptr;
        return m->embeddedWidget();
    });

    // GenericDisplay - same as DaqDisplay
    factory->registerWidgetCreator("GenericDisplay", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<DaqDisplayNode>(model, "GenericDisplay");
        if (!m) return nullptr;
        return m->embeddedWidget();
    });

    // AudioSource, LLama, Gamepad - TODO: add widget creators when models are updated
    // These currently use embeddedWidget() from the model (old pattern)
}

} // namespace Daqster