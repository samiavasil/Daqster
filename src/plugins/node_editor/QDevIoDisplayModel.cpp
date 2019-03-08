#include "QDevIoDisplayModel.h"
#include "AudioNodeQdevIoConnector.h"
#include"QDevioDisplayModelUi.h"
#include <QtCharts/QLineSeries>
#include "XYSeriesIODevice.h"

QT_CHARTS_USE_NAMESPACE

QDevIoDisplayModel::QDevIoDisplayModel():m_connector(nullptr)
{
    QLineSeries* series = new QLineSeries;
    m_widget = new QDevioDisplayModelUi(series);

    m_device = std::make_shared<XYSeriesIODevice>(series, this);
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
    int num = 0;
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
    return NodeDataType { "AudioNodeQdevIoConnector",
        "AudioNodeQdevIoConnector"};
}

std::shared_ptr<NodeData> QDevIoDisplayModel::outData(PortIndex port)
{
    return nullptr;
}

void QDevIoDisplayModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    auto conData = std::dynamic_pointer_cast<AudioNodeQdevIoConnector> (data);

    if (portIndex == 0)
    {
        if ( 0 )
        {
            modelValidationState = NodeValidationState::Error;
            modelValidationError = QStringLiteral("Division by zero error");
            m_connector.reset();
        }
        else if (conData)
        {
            modelValidationState = NodeValidationState::Valid;
            modelValidationError = QString();
            m_connector = conData;
            m_connector->SetDevIo( m_device );
        }
        else
        {
            modelValidationState = NodeValidationState::Warning;
            modelValidationError = QStringLiteral("Missing or incorrect inputs");
            m_connector->SetDevIo( nullptr );
            m_connector.reset();
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
