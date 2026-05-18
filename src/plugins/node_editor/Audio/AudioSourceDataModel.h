#ifndef SOURCEDATAMODEL_H
#define SOURCEDATAMODEL_H

#include "AudioCompat.h"

#include <QtCore/QObject>
#include <QtNodes/NodeDelegateModel>
#include <QtNodes/internal/Definitions.hpp>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::NodeValidationState;
using QtNodes::ConnectionId;

class AudioNodeQdevIoConnector;
class AudioSourceDataModelUI;
class EventThreadPull;

class AudioSourceDataModel : public NodeDelegateModel
{
    Q_OBJECT

public:
    typedef enum{
        ASDM_STOP,
        ASDM_START,
        ASDM_RELOAD,
    } StartStop;

    AudioSourceDataModel();

    virtual
    ~AudioSourceDataModel() override;

public:

    QString
    caption() const override
    { return QStringLiteral("AudioSource Source"); }

    bool
    captionVisible() const override
    { return false; }

    QString
    name() const override
    { return QStringLiteral("AudioSource"); }

public:

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
    setInData(std::shared_ptr<NodeData> data, PortIndex const port) override;

    QWidget *
    embeddedWidget() override;

    void IO_connect(std::shared_ptr<QIODevice> io);

    QtNodes::ConnectionPolicy portConnectionPolicy(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override
    {
        Q_UNUSED(portType);
        Q_UNUSED(portIndex);
        return QtNodes::ConnectionPolicy::One;
    }

    void outputConnectionDeleted(ConnectionId const &) override;

signals:
    void disconnected();
    void StartAudio(AudioSourceDataModel::StartStop start);
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);

private slots:
    void destroyedObj(QObject *obj);

private:
    std::shared_ptr<AudioNodeQdevIoConnector> m_connector;
    AudioSourceDataModelUI* m_Widget;
    QAudioDeviceInfo m_DevInfo;
    QAudioFormat m_FormatAudio;
};

#endif // SOURCEDATAMODEL_H
