#include <NodeDataModelToQIODeviceConnectorObsolete.h>
#include <GenericQDevIoConnectorObsolete.h>
#include <XYSeriesIODeviceObsolete.h>
#include <AudioCompat.h>
#include <QDevIoDisplayModelObsolete.h>
#include <QDevioDisplayModelUiObsolete.h>
#include <QDebug>
#include "LogCategories.h"

#include <QApplication>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QFormLayout>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;

QDevIoDisplayModelObsolete::QDevIoDisplayModelObsolete()
    : m_connector(nullptr)
    , m_device(std::shared_ptr<XYSeriesIODeviceObsolete>(new XYSeriesIODeviceObsolete()))
{
    // Display nodes must never get a graphics effect (perf): the shadow blur
    // runs per repaint and costs ~46% CPU during video playback.
    QtNodes::NodeStyle s = this->nodeStyle();
    s.ShadowEnabled = false;
    this->setNodeStyle(s);

    // The widget stack (audio view + config panel) is NOT built here anymore —
    // it is created lazily by ensureUi() only when a QApplication exists
    // (REQ-SW-PL-056: a headless QGuiApplication process never constructs
    // widgets). Data routing and the audio device work without any UI.
}

void QDevIoDisplayModelObsolete::ensureUi()
{
    if (m_stack)
        return;

    // Headless (QGuiApplication only, no QApplication): never build widgets.
    if (QApplication::instance() == nullptr)
        return;

    m_stack = std::make_unique<QStackedWidget>();

    // Page 0: Audio waveform view (existing UI)
    auto* audioView = new QDevioDisplayModelUiObsolete();
    m_audioViewIndex = m_stack->addWidget(audioView);
    m_typeToWidget["audio"] = m_audioViewIndex;

    // Connect audio device buffer to audio view
    connect(m_device.get(), SIGNAL(bufferReady(QVector<QPointF>&, int)),
            audioView, SLOT(bufferReady(QVector<QPointF>&, int)));

    // Page 1: Manual config panel
    auto* configPanel = new QWidget();
    auto* configLayout = new QFormLayout(configPanel);

    auto* typeCombo = new QComboBox();
    typeCombo->addItem("Audio", "audio");
    typeCombo->addItem("Sensor", "sensor");
    typeCombo->addItem("Video (future)", "video");
    typeCombo->addItem("Generic", "generic");
    configLayout->addRow("Stream Type:", typeCombo);

    auto* applyBtn = new QPushButton("Apply");
    configLayout->addRow(applyBtn);

    m_configPanelIndex = m_stack->addWidget(configPanel);

    // Start on config panel (no data yet)
    m_stack->setCurrentIndex(m_configPanelIndex);
}

QDevIoDisplayModelObsolete::~QDevIoDisplayModelObsolete()
{
}

QJsonObject QDevIoDisplayModelObsolete::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    return modelJson;
}

unsigned int QDevIoDisplayModelObsolete::nPorts(PortType portType) const
{
    unsigned int num = 0;
    switch (portType) {
    case QtNodes::PortType::In:
        num = 1;
        break;
    default:
        break;
    }
    return num;
}

NodeDataType QDevIoDisplayModelObsolete::dataType(PortType portType, PortIndex portIndex) const
{
    return NodeDataType {"QDevIO", "IO"};
}

std::shared_ptr<NodeData> QDevIoDisplayModelObsolete::outData(PortIndex port)
{
    return nullptr;
}

void QDevIoDisplayModelObsolete::setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex)
{
    if (portIndex != 0) return;

    // Build the UI lazily if a QApplication exists; no-op headless.
    ensureUi();

    auto conData = std::dynamic_pointer_cast<NodeDataModelToQIODeviceConnectorObsolete>(data);

    if (conData)
    {
        NodeValidationState s;
        s._state = NodeValidationState::State::Valid;
        setValidationState(s);
        m_connector = conData;

        // Try GenericQDevIoConnectorObsolete first (new path with metadata)
        auto genericConn = std::dynamic_pointer_cast<GenericQDevIoConnectorObsolete>(data);
        if (genericConn) {
            handleGenericConnector(genericConn);
        } else {
            // Legacy path (AudioNodeQdevIoConnector)
            m_connector->ConnectModels(this);
        }
    }
    else
    {
        NodeValidationState s;
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("Missing or incorrect inputs");
        setValidationState(s);
        if(m_connector) {
            m_connector.reset();
        }
        // Show config panel when disconnected
        if (m_stack)
            m_stack->setCurrentIndex(m_configPanelIndex);
    }
}

void QDevIoDisplayModelObsolete::handleGenericConnector(std::shared_ptr<GenericQDevIoConnectorObsolete> connector)
{
    if (connector->hasStreamConfig()) {
        auto config = connector->streamConfig();
        m_currentConfig = config;

        // Auto-route by type
        int idx = m_typeToWidget.value(config.type, -1);
        if (idx >= 0) {
            if (m_stack)
                m_stack->setCurrentIndex(idx);
        } else {
            // Unknown type — show config panel
            showConfigPanel();
        }

        // For mixed streams, activate all matching views
        if (connector->isMixed()) {
            // Future: iterate payload channels and activate each matching view
        }
    } else {
        // No metadata — show config panel for manual configuration
        showConfigPanel();
    }

    // Connect the device
    m_connector->ConnectModels(this);
}

void QDevIoDisplayModelObsolete::showConfigPanel()
{
    if (m_stack)
        m_stack->setCurrentIndex(m_configPanelIndex);
}

void QDevIoDisplayModelObsolete::registerView(const QString& type, QWidget* widget)
{
    // Headless never has a stack; registration is a GUI-only view concern.
    if (!m_stack)
        return;
    int idx = m_stack->addWidget(widget);
    m_typeToWidget[type] = idx;
}

int QDevIoDisplayModelObsolete::viewIndex(const QString& type) const
{
    return m_typeToWidget.value(type, -1);
}

void QDevIoDisplayModelObsolete::ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio)
{
    std::shared_ptr<XYSeriesIODeviceObsolete> device = std::dynamic_pointer_cast<XYSeriesIODeviceObsolete>(m_device);

    qCInfo(lcNodeEditor) << "Changed: " << formatAudio << AudioCompat::deviceName(devInfo);

    const bool validChannels = formatAudio.channelCount() > 0;
    const bool validSampleSize = AudioCompat::sampleSize(formatAudio) > 0;
    const bool knownSampleType = AudioCompat::sampleType(formatAudio) != AudioCompat::Unknown;
    if (!device.get() || !validChannels || !validSampleSize || !knownSampleType) {
        qCWarning(lcNodeEditor) << "Ignore invalid audio format update:" << formatAudio;
        return;
    }

    device->ReinitDevice(formatAudio);

    // Widget part is GUI-only (REQ-SW-PL-056): headless has no UI stack.
    if (!m_stack)
        return;

    QDevioDisplayModelUiObsolete *displayUi = dynamic_cast<QDevioDisplayModelUiObsolete*>(m_stack->widget(m_audioViewIndex));
    if (displayUi == nullptr) {
        qCWarning(lcNodeEditor) << "Ignore invalid audio format update (no audio view):" << formatAudio;
        return;
    }

    displayUi->SetSeries(0, formatAudio.channelCount());

    // Auto-switch to audio view when format changes
    m_stack->setCurrentIndex(m_audioViewIndex);
}

QWidget* QDevIoDisplayModelObsolete::embeddedWidget()
{
    // Build the UI lazily when a QApplication exists; headless returns nullptr.
    ensureUi();
    return m_stack.get();
}

std::shared_ptr<QIODevice> QDevIoDisplayModelObsolete::device() const
{
    return m_device;
}
