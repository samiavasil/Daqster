#ifndef SOURCEDATAMODEL_H
#define SOURCEDATAMODEL_H

#include "AudioCompat.h"

#include <QtCore/QObject>
#include <QtNodes/NodeDelegateModel>
#include <QtNodes/internal/Definitions.hpp>

class AudioNodeQdevIoConnector;
class AudioSourceDataModelUI;
class EventThreadPull;

class AudioSourceDataModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    enum StartStop {
        ASDM_STOP,
        ASDM_START,
        ASDM_RELOAD,
    };

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
    nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType
    dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData>
    outData(QtNodes::PortIndex const port) override;

    void
    setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const port) override;

    QWidget *
    embeddedWidget() override;

    void IO_connect(std::shared_ptr<QIODevice> io);

    QtNodes::ConnectionPolicy portConnectionPolicy(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override
    {
        Q_UNUSED(portType);
        Q_UNUSED(portIndex);
        return QtNodes::ConnectionPolicy::One;
    }

    void outputConnectionDeleted(QtNodes::ConnectionId const &) override;

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
