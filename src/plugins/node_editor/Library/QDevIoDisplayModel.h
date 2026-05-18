#ifndef QDEVIODISPLAY_H
#define QDEVIODISPLAY_H

#include "AudioCompat.h"

#include <QtCore/QObject>
#include <QtNodes/NodeDelegateModel>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::NodeValidationState;

class NodeDataModelToQIODeviceConnector;

//TODO change QDevIoDisplayModel to QAudioDevIoDisplayModel:public QDevIoDisplayModel

class QDevIoDisplayModel : public NodeDelegateModel
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
    outData(PortIndex const port) override;

    void
    setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex) override;

    QWidget *
    embeddedWidget() override;

    std::shared_ptr<QIODevice> device() const;

public slots:
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);

protected:
     std::shared_ptr<NodeDataModelToQIODeviceConnector> m_connector;
     QWidget* m_widget;
     std::shared_ptr<QIODevice> m_device;
     friend class AudioXYSeriesIODevice;
};

#endif // QDEVIODISPLAY_H
