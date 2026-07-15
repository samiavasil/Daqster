#include <NodeDataModelToQIODeviceConnector.h>
#include <XYSeriesIODevice.h>
#include <AudioCompat.h>
#include <QDevIoDisplayModel.h>
#include <QDevioDisplayModelUi.h>
#include <QDebug>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;

QDevIoDisplayModel::QDevIoDisplayModel():m_connector(nullptr)
{
    m_widget = new QDevioDisplayModelUi();
    m_device = std::shared_ptr<XYSeriesIODevice>(new XYSeriesIODevice(this));
    connect(m_device.get(), SIGNAL(bufferReady(QVector<QPointF>&, int)),
            m_widget, SLOT(bufferReady(QVector<QPointF>&, int)));
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
        /*case QtNodes::PortType::Out:
        num = 1;
        break;*/
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
    auto conData = std::dynamic_pointer_cast<NodeDataModelToQIODeviceConnector> (data);

    if (portIndex == 0)
    {
        if (conData)
        {
            NodeValidationState s;
            s._state = NodeValidationState::State::Valid;
            setValidationState(s);
            m_connector = conData;
            m_connector->ConnectModels(this);
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
        }
    }
}

QWidget *QDevIoDisplayModel::embeddedWidget()
{
    return m_widget;
}

void QDevIoDisplayModel::ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio)
{
    std::shared_ptr<XYSeriesIODevice> device = std::dynamic_pointer_cast<XYSeriesIODevice>(m_device);
    QDevioDisplayModelUi *displayUi = dynamic_cast<QDevioDisplayModelUi*>(m_widget);

    qDebug() <<   "Changed: " << formatAudio << AudioCompat::deviceName(devInfo);

    // Audio backends may report an invalid/unknown format when no input device
    // is available. Avoid forwarding invalid channel/sample values to chart UI.
    const bool validChannels = formatAudio.channelCount() > 0;
    const bool validSampleSize = AudioCompat::sampleSize(formatAudio) > 0;
    const bool knownSampleType = AudioCompat::sampleType(formatAudio) != AudioCompat::Unknown;
    if (!device.get() || displayUi == nullptr || !validChannels || !validSampleSize || !knownSampleType) {
        qWarning() << "Ignore invalid audio format update:" << formatAudio;
        return;
    }

    device->ReinitDevice(formatAudio);
    displayUi->SetSeries(0, formatAudio.channelCount());
}

std::shared_ptr<QIODevice> QDevIoDisplayModel::device() const
{
    return m_device;
}
