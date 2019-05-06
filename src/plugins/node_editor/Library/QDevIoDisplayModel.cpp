#include <NodeDataModelToQIODeviceConnector.h>
#include <XYSeriesIODevice.h>
#include <QDevIoDisplayModel.h>
#include <QDevioDisplayModelUi.h>
#include <QtCharts/QLineSeries>
#include <QDebug>

QT_CHARTS_USE_NAMESPACE

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

void QDevIoDisplayModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    auto conData = std::dynamic_pointer_cast<NodeDataModelToQIODeviceConnector> (data);

    if (portIndex == 0)
    {

        if (conData)
        {
            modelValidationState = NodeValidationState::Valid;
            modelValidationError = QString();
            m_connector = conData;
            m_connector->ConnectModels(this);
        }
        else
        {
            modelValidationState = NodeValidationState::Warning;
            modelValidationError = QStringLiteral("Missing or incorrect inputs");
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

QtNodes::NodeValidationState QDevIoDisplayModel::validationState() const
{
    return modelValidationState;
}

QString QDevIoDisplayModel::validationMessage() const
{
    return modelValidationError;
}

void QDevIoDisplayModel::ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio)
{
    qDebug() <<   "Change: " << formatAudio << devInfo.deviceName();
}

std::shared_ptr<QIODevice> QDevIoDisplayModel::device() const
{
    return m_device;
}
