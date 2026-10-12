#include <NodeDataModelToQIODeviceConnectorObsolete.h>
#include <GenericQDevIoConnectorObsolete.h>
#include <XYSeriesIODeviceObsolete.h>
#include <AudioCompat.h>
#include <QDevIoDisplayModelObsolete.h>
#include <QDebug>
#include "LogCategories.h"

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

    // Model-only since REQ-SW-PL-056: the widget stack (audio view + config
    // panel) moved to the GUI plugin. The view is created by
    // NodeWidgetFactory ("AudioDisplayObsolete" creator) and connects to
    // device() bufferReady + audioConnectionChanged. embeddedWidget() below
    // returns nullptr, so a headless process constructs zero widgets.
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
        // No UI anymore (REQ-SW-PL-056): the GUI view shows its own empty state.
    }
}

void QDevIoDisplayModelObsolete::handleGenericConnector(std::shared_ptr<GenericQDevIoConnectorObsolete> connector)
{
    if (connector->hasStreamConfig()) {
        auto config = connector->streamConfig();
        m_currentConfig = config;
        // View routing (which page is active) is the GUI view's job now
        // (REQ-SW-PL-056 split) — the model only keeps the stream config.
    }

    // Connect the device
    m_connector->ConnectModels(this);
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

    // View part is GUI-only (REQ-SW-PL-056): the GUI view reacts to
    // audioConnectionChanged by setting its series count / switching pages.
    Q_EMIT audioConnectionChanged(formatAudio);
}

QWidget* QDevIoDisplayModelObsolete::embeddedWidget()
{
    // Model-only (REQ-SW-PL-056): the view is built by NodeWidgetFactory's
    // "AudioDisplayObsolete" creator in the GUI plugin. Headless processes
    // therefore construct zero widgets for this model.
    return nullptr;
}

std::shared_ptr<QIODevice> QDevIoDisplayModelObsolete::device() const
{
    return m_device;
}
