#ifndef QDEVIODISPLAY_H
#define QDEVIODISPLAY_H

#include <QtCore/QObject>
#include <nodes/NodeDataModel>


using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDataModel;
using QtNodes::NodeValidationState;
class AudioNodeQdevIoConnector;
class XYSeriesIODevice;

class QDevIoDisplayModel : public NodeDataModel
{
    Q_OBJECT

public:
    QDevIoDisplayModel();

    virtual
    ~QDevIoDisplayModel() override;

public:

    QString
    caption() const override
    { return QStringLiteral("QDevIo Display"); }

    bool
    captionVisible() const override
    { return false; }

    QString
    name() const override
    { return QStringLiteral("QDevIoDisplay"); }

    QJsonObject
    save() const override;

    /*void
restore(QJsonObject const &p) override;
*/

public:

    unsigned int
    nPorts(PortType portType) const override;

    NodeDataType
    dataType(PortType portType, PortIndex portIndex) const override;

    std::shared_ptr<NodeData>
    outData(PortIndex port) override;

    void
    setInData(std::shared_ptr<NodeData> data, PortIndex portIndex) override;

    QWidget *
    embeddedWidget() override;

    NodeValidationState
    validationState() const override;

    QString
    validationMessage() const override;

    bool UpdateModel(int chan_num);

protected:
     std::shared_ptr<AudioNodeQdevIoConnector> m_connector;
     NodeValidationState modelValidationState = NodeValidationState::Warning;
     QString modelValidationError = QStringLiteral("Missing or incorrect inputs");
     QWidget* m_widget;
     QSharedPointer<QIODevice> m_device;
};

#endif // QDEVIODISPLAY_H
