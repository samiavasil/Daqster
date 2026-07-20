#include <NodeDataModelToQIODeviceConnector.h>
#include <GenericQDevIoConnector.h>
#include <XYSeriesIODevice.h>
#include <AudioCompat.h>
#include <QDevIoDisplayModel.h>
#include <QDevioDisplayModelUi.h>
#include <QDebug>

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

QDevIoDisplayModel::QDevIoDisplayModel()
    : m_connector(nullptr)
    , m_widget(nullptr)
    , m_stack(new QStackedWidget())
    , m_device(std::shared_ptr<XYSeriesIODevice>(new XYSeriesIODevice()))
{
    // Page 0: Audio waveform view (existing UI)
    auto* audioView = new QDevioDisplayModelUi();
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

    m_widget = m_stack;
}

QDevIoDisplayModel::~QDevIoDisplayModel()
{
}

QJsonObject QDevIoDisplayModel::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    return modelJson;
}

unsigned int QDevIoDisplayModel::nPorts(PortType portType) const
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

NodeDataType QDevIoDisplayModel::dataType(PortType portType, PortIndex portIndex) const
{
    return NodeDataType {"QDevIO", "IO"};
}

std::shared_ptr<NodeData> QDevIoDisplayModel::outData(PortIndex port)
{
    return nullptr;
}

void QDevIoDisplayModel::setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex)
{
    if (portIndex != 0) return;

    auto conData = std::dynamic_pointer_cast<NodeDataModelToQIODeviceConnector>(data);

    if (conData)
    {
        NodeValidationState s;
        s._state = NodeValidationState::State::Valid;
        setValidationState(s);
        m_connector = conData;

        // Try GenericQDevIoConnector first (new path with metadata)
        auto genericConn = std::dynamic_pointer_cast<GenericQDevIoConnector>(data);
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
        m_stack->setCurrentIndex(m_configPanelIndex);
    }
}

void QDevIoDisplayModel::handleGenericConnector(std::shared_ptr<GenericQDevIoConnector> connector)
{
    if (connector->hasStreamConfig()) {
        auto config = connector->streamConfig();
        m_currentConfig = config;

        // Auto-route by type
        int idx = m_typeToWidget.value(config.type, -1);
        if (idx >= 0) {
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

void QDevIoDisplayModel::showConfigPanel()
{
    m_stack->setCurrentIndex(m_configPanelIndex);
}

void QDevIoDisplayModel::registerView(const QString& type, QWidget* widget)
{
    int idx = m_stack->addWidget(widget);
    m_typeToWidget[type] = idx;
}

int QDevIoDisplayModel::viewIndex(const QString& type) const
{
    return m_typeToWidget.value(type, -1);
}

void QDevIoDisplayModel::ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio)
{
    std::shared_ptr<XYSeriesIODevice> device = std::dynamic_pointer_cast<XYSeriesIODevice>(m_device);
    QDevioDisplayModelUi *displayUi = dynamic_cast<QDevioDisplayModelUi*>(m_stack->widget(m_audioViewIndex));

    qDebug() << "Changed: " << formatAudio << AudioCompat::deviceName(devInfo);

    const bool validChannels = formatAudio.channelCount() > 0;
    const bool validSampleSize = AudioCompat::sampleSize(formatAudio) > 0;
    const bool knownSampleType = AudioCompat::sampleType(formatAudio) != AudioCompat::Unknown;
    if (!device.get() || displayUi == nullptr || !validChannels || !validSampleSize || !knownSampleType) {
        qWarning() << "Ignore invalid audio format update:" << formatAudio;
        return;
    }

    device->ReinitDevice(formatAudio);
    displayUi->SetSeries(0, formatAudio.channelCount());

    // Auto-switch to audio view when format changes
    m_stack->setCurrentIndex(m_audioViewIndex);
}

std::shared_ptr<QIODevice> QDevIoDisplayModel::device() const
{
    return m_device;
}
