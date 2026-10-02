#include "NodeWidgetFactory.h"

#include <QtNodes/NodeDelegateModel>
#include <QObject>

// Core models - include headers for static_cast
#include <Sources/AudioSource/AudioSourceDataModel.h>
#include <Sources/LLamaSource/LLamaModelDataModel.h>
#include <Sources/Video/CameraSourceNode.h>
#include <Sources/Video/StreamSourceNode.h>
#include <Sources/Video/VideoFileSourceNode.h>
#include <Sources/Video/FrameSamplerNode.h>
#include <Sources/Video/CustomShaderNode.h>
#include <Sources/Video/VideoEffectNode.h>
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
#include <Displays/NumberDisplay/NumberDisplayDataModel.h>
#include <Operators/ModuloModel.h>
#include <Operators/ArithmeticLogic/ArithmeticLogicModel.h>
#include <Sources/NumberSource/NumberSourceDataModel.h>
#include <Sources/Gamepad/GamepadModel.h>
#include <Sources/AudioSource/AudioSourceDataModel.h>
#include <Sources/AudioSource/AudioSourceDataModelObsolete.h>
#include <Sources/LLamaSource/ConsoleDataModel.h>
#include <Sources/LLamaSource/LLamaModelDataModel.h>
#include <Routing/Demux/DemuxNodeObsolete.h>
#include <Routing/Mux/MuxNodeObsolete.h>

// GUI widgets.
//
// Note: VideoGLBlitWidget and VideoPerfBadge are NOT listed here — those classes
// live in demo_nodeditor_nodes_core and are still constructed by their own
// models (deferred, see REQ-SW-PL-051).
#include "Sources/PlutoSdr/PlutoSdrWidget.h"
#include "Sources/SystemMonitor/SystemMonitorWidget.h"
#include "Sources/FilePlayback/FilePlaybackWidget.h"
#include "Sources/NetworkSource/NetworkSourceWidget.h"
#include "Sinks/NetworkSink/NetworkSinkWidget.h"
#include "Sinks/FileRecord/FileRecordWidget.h"
#include "Sources/GpuMonitor/GpuMonitorWidget.h"
#include "Sources/JackDetect/JackDetectWidget.h"
#include "Sources/Pcap/PcapWidget.h"
#include "Displays/DaqDisplay/DaqDisplayNode.h"
#include "Displays/DaqDisplay/DaqDisplayWidget.h"
#include "Displays/NumberDisplay/NumberDisplayWidget.h"
#include "Operators/ModuloWidget.h"
#include "Operators/ArithmeticLogicWidget.h"
#include "Sources/NumberSourceWidget.h"
#include "Sources/Gamepad/GamepadWidget.h"
#include "Sources/AudioSource/AudioSourceDataModelUI.h"
#include "Sources/LLamaSource/ConsoleWidget.h"
#include "Sources/LLamaSource/ChatBaseWidget.h"
#include "Sources/LLamaSource/LLamaModelWidget.h"
#include "Sources/Video/CameraSourceWidget.h"
#include "Sources/Video/StreamSourceWidget.h"
#include "Sources/Video/VideoFileSourceWidget.h"
#include "Sources/Video/FrameSamplerWidget.h"
#include "Sources/Video/CustomShaderWidget.h"
#include "Sources/Video/VideoEffectWidget.h"

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

    // DaqDisplay / GenericDisplay / AudioDisplay (historical alias) — the same
    // widget serves all three; GenericDisplay and AudioDisplay are thin legacy
    // subclasses of DaqDisplayNode (REQ-SW-PL-022 AC 8, REQ-SW-PL-025 AC 8).
    const auto createDaqDisplay = [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        // qobject_cast (not safeCast) — this creator serves several model names.
        auto* m = qobject_cast<DaqDisplayNode*>(model);
        if (!m) return nullptr;
        auto* widget = new DaqDisplayWidget(m);
        QObject::connect(widget, &DaqDisplayWidget::addPlotRequested,
                         m, &DaqDisplayNode::onAddPlotRequested);
        QObject::connect(widget, &DaqDisplayWidget::removePlotRequested,
                         m, &DaqDisplayNode::onRemovePlotRequested);
        QObject::connect(widget, &DaqDisplayWidget::cardProcessingChanged,
                         m, &DaqDisplayNode::onCardProcessingChanged);
        QObject::connect(widget, &DaqDisplayWidget::cardChannelChanged,
                         m, &DaqDisplayNode::onCardChannelChanged);
        QObject::connect(widget, &DaqDisplayWidget::ringSecondsChanged,
                         m, &DaqDisplayNode::onRingSecondsChanged);
        QObject::connect(m, &DaqDisplayNode::plotCardsChanged,
                         widget, &DaqDisplayWidget::rebuildCards);
        QObject::connect(m, &DaqDisplayNode::plotResultReady,
                         widget, &DaqDisplayWidget::applyResult);
        QObject::connect(m, &DaqDisplayNode::descriptorInfoChanged,
                         widget, &DaqDisplayWidget::setDescriptorInfo);
        QObject::connect(m, &DaqDisplayNode::ringSecondsChanged,
                         widget, &DaqDisplayWidget::setRingSeconds);
        return widget;
    };
    factory->registerWidgetCreator("DaqDisplay", createDaqDisplay);
    factory->registerWidgetCreator("GenericDisplay", createDaqDisplay);
    factory->registerWidgetCreator("AudioDisplay", createDaqDisplay);

    // CameraSource
    factory->registerWidgetCreator("CameraSource", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<CameraSourceNode>(model, "CameraSource");
        if (!m) return nullptr;
        auto* widget = new CameraSourceWidget();
        widget->setDevices(m->deviceDescriptions(), m->selectedDeviceIndex() + 1);
        widget->setRunning(m->isRunning());
        QObject::connect(widget, &CameraSourceWidget::deviceIndexChanged,
                         m, &CameraSourceNode::onDeviceIndexChanged);
        QObject::connect(widget, &CameraSourceWidget::startStopRequested,
                         m, &CameraSourceNode::onStartStopRequested);
        QObject::connect(m, &CameraSourceNode::devicesChanged,
                         widget, &CameraSourceWidget::setDevices);
        QObject::connect(m, &CameraSourceNode::statusChanged,
                         widget, &CameraSourceWidget::setStatus);
        QObject::connect(m, &CameraSourceNode::runningChanged,
                         widget, &CameraSourceWidget::setRunning);
        return widget;
    });

    // StreamSource
    factory->registerWidgetCreator("StreamSource", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<StreamSourceNode>(model, "StreamSource");
        if (!m) return nullptr;
        auto* widget = new StreamSourceWidget();
        widget->setUrl(m->url());
        widget->setPlaying(m->isPlaying());
        QObject::connect(widget, &StreamSourceWidget::connectClicked,
                         m, &StreamSourceNode::onConnectClicked);
        QObject::connect(widget, &StreamSourceWidget::urlChanged,
                         m, &StreamSourceNode::onUrlChanged);
        QObject::connect(m, &StreamSourceNode::urlChanged,
                         widget, &StreamSourceWidget::setUrl);
        QObject::connect(m, &StreamSourceNode::statusChanged,
                         widget, &StreamSourceWidget::setStatus);
        QObject::connect(m, &StreamSourceNode::playingChanged,
                         widget, &StreamSourceWidget::setPlaying);
        return widget;
    });

    // VideoFileSource
    factory->registerWidgetCreator("VideoFileSource", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<VideoFileSourceNode>(model, "VideoFileSource");
        if (!m) return nullptr;
        auto* widget = new VideoFileSourceWidget();
        widget->setFilePath(m->filePath());
        widget->setPlaying(m->isPlaying());
        QObject::connect(widget, &VideoFileSourceWidget::filePathChanged,
                         m, &VideoFileSourceNode::onFilePathChanged);
        QObject::connect(widget, &VideoFileSourceWidget::playPauseClicked,
                         m, &VideoFileSourceNode::onPlayPauseClicked);
        QObject::connect(widget, &VideoFileSourceWidget::stopClicked,
                         m, &VideoFileSourceNode::onStopClicked);
        QObject::connect(widget, &VideoFileSourceWidget::seekBackClicked,
                         m, &VideoFileSourceNode::onSeekBackClicked);
        QObject::connect(widget, &VideoFileSourceWidget::seekForwardClicked,
                         m, &VideoFileSourceNode::onSeekForwardClicked);
        QObject::connect(m, &VideoFileSourceNode::filePathChanged,
                         widget, &VideoFileSourceWidget::setFilePath);
        QObject::connect(m, &VideoFileSourceNode::statusChanged,
                         widget, &VideoFileSourceWidget::setStatus);
        QObject::connect(m, &VideoFileSourceNode::playingChanged,
                         widget, &VideoFileSourceWidget::setPlaying);
        QObject::connect(m, &VideoFileSourceNode::transportEnabledChanged,
                         widget, &VideoFileSourceWidget::setTransportEnabled);
        QObject::connect(m, &VideoFileSourceNode::positionChanged,
                         widget, &VideoFileSourceWidget::setPosition);
        return widget;
    });

    // FrameSampler
    factory->registerWidgetCreator("FrameSampler", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<FrameSamplerNode>(model, "FrameSampler");
        if (!m) return nullptr;
        auto* widget = new FrameSamplerWidget();
        widget->setParams(static_cast<int>(m->mode()), m->everyN(), m->maxFps());
        QObject::connect(widget, &FrameSamplerWidget::modeChanged,
                         m, &FrameSamplerNode::onModeChanged);
        QObject::connect(widget, &FrameSamplerWidget::everyNChanged,
                         m, &FrameSamplerNode::onEveryNChanged);
        QObject::connect(widget, &FrameSamplerWidget::maxFpsChanged,
                         m, &FrameSamplerNode::onMaxFpsChanged);
        QObject::connect(m, &FrameSamplerNode::paramsChanged,
                         widget, &FrameSamplerWidget::setParams);
        return widget;
    });

    // CustomShader
    factory->registerWidgetCreator("CustomShader", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<CustomShaderNode>(model, "CustomShader");
        if (!m) return nullptr;
        auto* widget = new CustomShaderWidget();
        const auto cfg = m->config();
        widget->setConfig(cfg.source, cfg.param, cfg.animate);
        QObject::connect(widget, &CustomShaderWidget::compileRequested,
                         m, &CustomShaderNode::onCompileRequested);
        QObject::connect(widget, &CustomShaderWidget::sourceChanged,
                         m, &CustomShaderNode::onSourceChanged);
        QObject::connect(widget, &CustomShaderWidget::paramChanged,
                         m, &CustomShaderNode::onParamChanged);
        QObject::connect(widget, &CustomShaderWidget::animateToggled,
                         m, &CustomShaderNode::onAnimateToggled);
        QObject::connect(m, &CustomShaderNode::configChanged,
                         widget, [widget](const ShaderConfig& cfg) {
                             widget->setConfig(cfg.source, cfg.param, cfg.animate);
                         });
        QObject::connect(m, &CustomShaderNode::hardwareGlMissing,
                         widget, &CustomShaderWidget::onHardwareGlMissing);
        QObject::connect(m, &CustomShaderNode::compilationError,
                         widget, &CustomShaderWidget::onCompilationError);
        QObject::connect(m, &CustomShaderNode::compilationOk,
                         widget, &CustomShaderWidget::onCompilationOk);
        return widget;
    });

    // VideoEffect
    factory->registerWidgetCreator("VideoEffect", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<VideoEffectNode>(model, "VideoEffect");
        if (!m) return nullptr;
        auto* widget = new VideoEffectWidget();
        widget->setConfig(m->effectIndex(), m->params());
        QObject::connect(widget, &VideoEffectWidget::effectIndexChanged,
                         m, &VideoEffectNode::onEffectIndexChanged);
        QObject::connect(widget, &VideoEffectWidget::brightnessChanged,
                         m, &VideoEffectNode::onBrightnessChanged);
        QObject::connect(widget, &VideoEffectWidget::contrastChanged,
                         m, &VideoEffectNode::onContrastChanged);
        QObject::connect(widget, &VideoEffectWidget::flipChanged,
                         m, &VideoEffectNode::onFlipChanged);
        QObject::connect(widget, &VideoEffectWidget::blurChanged,
                         m, &VideoEffectNode::onBlurChanged);
#ifdef HAVE_OPENCV
        QObject::connect(widget, &VideoEffectWidget::gaussianChanged,
                         m, &VideoEffectNode::onGaussianChanged);
        QObject::connect(widget, &VideoEffectWidget::cannyLowChanged,
                         m, &VideoEffectNode::onCannyLowChanged);
        QObject::connect(widget, &VideoEffectWidget::cannyHighChanged,
                         m, &VideoEffectNode::onCannyHighChanged);
        QObject::connect(widget, &VideoEffectWidget::thresholdChanged,
                         m, &VideoEffectNode::onThresholdChanged);
#endif
        QObject::connect(m, &VideoEffectNode::configChanged,
                         widget, &VideoEffectWidget::setConfig);
        QObject::connect(m, &VideoEffectNode::gpuPathUsed,
                         widget, &VideoEffectWidget::onGpuPathUsed);
        QObject::connect(m, &VideoEffectNode::cpuPathUsed,
                         widget, &VideoEffectWidget::onCpuPathUsed);
        return widget;
    });

    // NumberDisplay - uses NumberDisplayWidget from _gui plugin
    factory->registerWidgetCreator("NumberResult", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<NumberDisplayDataModel>(model, "NumberResult");
        if (!m) return nullptr;
        auto* widget = new NumberDisplayWidget();
        // Connect widget signals to model
        QObject::connect(widget->typeCombo(), QOverload<int>::of(&QComboBox::currentIndexChanged),
                         m, &NumberDisplayDataModel::onTypeChanged);
        // Model pushes the number to the widget label
        QObject::connect(m, &NumberDisplayDataModel::displayTextChanged,
                         widget, &NumberDisplayWidget::setDisplayText);
        return widget;
    });

    // AudioSource, LLama, Gamepad - TODO: add widget creators when models are updated
    // These currently use embeddedWidget() from the model (old pattern)

    // Modulo
    factory->registerWidgetCreator("Modulo", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<ModuloModel>(model, "Modulo");
        if (!m) return nullptr;
        auto* widget = new ModuloWidget();
        widget->setTypeIndex(m->typeIndex());
        QObject::connect(widget, &ModuloWidget::typeChanged,
                         m, &ModuloModel::onTypeChanged);
        return widget;
    });

    // Arithmetic/Logic
    factory->registerWidgetCreator("Arithmetic/Logic", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<ArithmeticLogicModel>(model, "Arithmetic/Logic");
        if (!m) return nullptr;
        auto* widget = new ArithmeticLogicWidget();
        widget->setTypeIndex(m->typeIndex());
        widget->setInputCount(m->inputCount());
        widget->setExpression(m->expression());
        widget->setStrobeEnabled(m->strobeEnabled());
        QObject::connect(widget, &ArithmeticLogicWidget::typeChanged,
                         m, &ArithmeticLogicModel::onTypeChanged);
        QObject::connect(widget, &ArithmeticLogicWidget::inputCountChanged,
                         m, &ArithmeticLogicModel::onInputsChanged);
        QObject::connect(widget, &ArithmeticLogicWidget::expressionChanged,
                         m, &ArithmeticLogicModel::onExpressionChanged);
        QObject::connect(widget, &ArithmeticLogicWidget::strobeToggled,
                         m, &ArithmeticLogicModel::onStrobeToggled);
        return widget;
    });

    // NumberSource
    factory->registerWidgetCreator("NumberSource", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<NumberSourceDataModel>(model, "NumberSource");
        if (!m) return nullptr;
        auto* widget = new NumberSourceWidget();
        widget->setTypeIndex(m->typeIndex());
        widget->setText(m->text());
        widget->setRandomEnabled(m->randomEnabled());
        widget->setInterval(m->interval());
        widget->setTextEditable(!m->randomEnabled());
        QObject::connect(widget, &NumberSourceWidget::typeChanged,
                         m, &NumberSourceDataModel::onTypeChanged);
        QObject::connect(widget, &NumberSourceWidget::textEdited,
                         m, &NumberSourceDataModel::onTextEdited);
        QObject::connect(widget, &NumberSourceWidget::randomToggled,
                         m, &NumberSourceDataModel::onRandomToggled);
        QObject::connect(widget, &NumberSourceWidget::intervalChanged,
                         m, &NumberSourceDataModel::onIntervalChanged);
        // Model -> widget: random mode writes new values into the line edit.
        QObject::connect(m, &NumberSourceDataModel::textChanged,
                         widget, &NumberSourceWidget::setText);
        QObject::connect(m, &NumberSourceDataModel::textEditableChanged,
                         widget, &NumberSourceWidget::setTextEditable);
        return widget;
    });

    // GamepadInput
    factory->registerWidgetCreator("GamepadInput", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<GamepadModel>(model, "GamepadInput");
        if (!m) return nullptr;
        auto* widget = new GamepadWidget();
        widget->setDevicePath(m->devicePath());
        widget->setPollRateHz(m->pollRateHz());
        QObject::connect(widget, &GamepadWidget::startRequested,
                         m, &GamepadModel::onStartRequested);
        QObject::connect(widget, &GamepadWidget::stopRequested,
                         m, &GamepadModel::onStopRequested);
        QObject::connect(widget, &GamepadWidget::devicePathChanged,
                         m, &GamepadModel::onDevicePathChanged);
        QObject::connect(widget, &GamepadWidget::pollRateChanged,
                         m, &GamepadModel::onPollRateChanged);
        // Model -> widget: the engine reports status and live axis/button state.
        QObject::connect(m, &GamepadModel::statusChanged,
                         widget, &GamepadWidget::setStatus);
        QObject::connect(m, &GamepadModel::axisValuesChanged,
                         widget, &GamepadWidget::setAxisValues);
        QObject::connect(m, &GamepadModel::buttonStatesChanged,
                         widget, &GamepadWidget::setButtonStates);
        return widget;
    });

    // AudioSource
    factory->registerWidgetCreator("AudioSource", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<AudioSourceDataModel>(model, "AudioSource");
        if (!m) return nullptr;
        // The UI mutates the model's live device/format state in place, so it
        // is handed non-owning pointers exactly as the model used to pass them.
        auto* widget = new AudioSourceDataModelUI(m->deviceInfo(), m->audioFormat());
        widget->setWindowFlags(Qt::Window
                               | Qt::WindowTitleHint
                               | Qt::WindowSystemMenuHint
                               | Qt::WindowMinMaxButtonsHint
                               | Qt::WindowCloseButtonHint);
        widget->setWindowModality(Qt::NonModal);
        QObject::connect(widget, QOverload<AudioStartStop>::of(&AudioSourceDataModelUI::Start),
                         m, &AudioSourceDataModel::onUiStart);
        QObject::connect(widget, &AudioSourceDataModelUI::ChangeAudioConnection,
                         m, &AudioSourceDataModel::onAudioConnectionChanged);
        return widget;
    });

    // AudioSourceObsolete
    factory->registerWidgetCreator("AudioSourceObsolete", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<AudioSourceDataModelObsolete>(model, "AudioSourceObsolete");
        if (!m) return nullptr;
        auto* widget = new AudioSourceDataModelUI(m->deviceInfo(), m->audioFormat());
        widget->setWindowFlags(Qt::Window
                               | Qt::WindowTitleHint
                               | Qt::WindowSystemMenuHint
                               | Qt::WindowMinMaxButtonsHint
                               | Qt::WindowCloseButtonHint);
        widget->setWindowModality(Qt::NonModal);
        QObject::connect(widget, QOverload<AudioStartStop>::of(&AudioSourceDataModelUI::Start),
                         m, &AudioSourceDataModelObsolete::StartAudio);
        QObject::connect(widget, &AudioSourceDataModelUI::ChangeAudioConnection,
                         m, &AudioSourceDataModelObsolete::ChangeAudioConnection);
        QObject::connect(m, &AudioSourceDataModelObsolete::audioStateChanged,
                         widget, &AudioSourceDataModelUI::AudioStateChanged);
        return widget;
    });

    // Console
    factory->registerWidgetCreator("Console", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<ConsoleDataModel>(model, "Console");
        if (!m) return nullptr;
        auto* widget = new ConsoleWidget();
        // Push the persisted state loaded before the widget existed.
        if (!m->chatConfig().isEmpty())
            widget->findChild<ChatBaseWidget*>()->loadConfig(m->chatConfig());
        QObject::connect(widget, &ConsoleWidget::sendRequested,
                         m, &ConsoleDataModel::onSendClicked);
        QObject::connect(widget, &ConsoleWidget::configChanged,
                         m, &ConsoleDataModel::onChatConfigChanged);
        // Model -> widget: a response arrived on the input port.
        QObject::connect(m, &ConsoleDataModel::responseReceived,
                         widget, &ConsoleWidget::addResponse);
        return widget;
    });

    // LLamaModel
    factory->registerWidgetCreator("LLamaModel", [](QtNodes::NodeDelegateModel* model) -> QWidget* {
        auto* m = safeCast<LLamaModelDataModel>(model, "LLamaModel");
        if (!m) return nullptr;
        auto* widget = new LLamaModelWidget();

        // ── Initial render from the model state (load() may have run first) ──
        widget->setHost(m->host());
        widget->setPort(m->port());
        widget->setCtxSize(m->ctxSize());
        widget->setUseGpu(m->useGpu());
        widget->setExePath(m->exePath());
        widget->setModelPath(m->modelPath());

        auto* chat = widget->chatWidget();
        if (!m->chatConfig().isEmpty())
            chat->loadConfig(m->chatConfig());

        // ── Widget -> model ──────────────────────────────────────────────
        QObject::connect(widget, &LLamaModelWidget::connectClicked,
                         m, &LLamaModelDataModel::onConnectClicked);
        QObject::connect(widget, &LLamaModelWidget::startServerClicked,
                         m, &LLamaModelDataModel::onStartServerClicked);
        QObject::connect(widget, &LLamaModelWidget::debugSendClicked,
                         m, &LLamaModelDataModel::onDebugSendClicked);
        QObject::connect(chat, &ChatBaseWidget::sendRequested,
                         m, &LLamaModelDataModel::onLocalChatSend);
        QObject::connect(widget, &LLamaModelWidget::exePathSelected,
                         m, &LLamaModelDataModel::onExePathSelected);
        QObject::connect(widget, &LLamaModelWidget::modelPathSelected,
                         m, &LLamaModelDataModel::onModelPathSelected);
        QObject::connect(widget, &LLamaModelWidget::debugPathChanged,
                         m, &LLamaModelDataModel::onDebugPathChanged);
        QObject::connect(widget, &LLamaModelWidget::debugBodyChanged,
                         m, &LLamaModelDataModel::onDebugBodyChanged);
        QObject::connect(widget, &LLamaModelWidget::chatConfigChanged,
                         m, &LLamaModelDataModel::onChatConfigChanged);

        // ── Model -> widget ──────────────────────────────────────────────
        QObject::connect(m, &LLamaModelDataModel::statusChanged,
                         widget, &LLamaModelWidget::setStatus);
        QObject::connect(m, &LLamaModelDataModel::connectButtonChanged,
                         widget, &LLamaModelWidget::setConnectButton);
        QObject::connect(m, &LLamaModelDataModel::startButtonChanged,
                         widget, &LLamaModelWidget::setStartButton);
        QObject::connect(m, &LLamaModelDataModel::debugResponseReceived,
                         widget, &LLamaModelWidget::setDebugResponse);
        QObject::connect(m, &LLamaModelDataModel::chatResponseReceived,
                         widget, &LLamaModelWidget::addChatResponse);
        return widget;
    });
}

} // namespace Daqster