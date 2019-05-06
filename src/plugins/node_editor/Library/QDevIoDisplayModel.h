#ifndef QDEVIODISPLAY_H
#define QDEVIODISPLAY_H

#include <QtCore/QObject>
#include <nodes/NodeDataModel>
#include <QtMultimedia/QAudioDeviceInfo>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDataModel;
using QtNodes::NodeValidationState;

class NodeDataModelToQIODeviceConnector;

//TODO change QDevIoDisplayModel to QAudioDevIoDisplayModel:public QDevIoDisplayModel

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

    std::shared_ptr<QIODevice> device() const;

public slots:
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);

protected:
     std::shared_ptr<NodeDataModelToQIODeviceConnector> m_connector;
     NodeValidationState modelValidationState = NodeValidationState::Warning;
     QString modelValidationError = QStringLiteral("Missing or incorrect inputs");
     QWidget* m_widget;
     std::shared_ptr<QIODevice> m_device;
     friend class AudioXYSeriesIODevice;
};

#endif // QDEVIODISPLAY_H
