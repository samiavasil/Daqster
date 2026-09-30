#ifndef AUDIOSOURCEDATAMODELOBSOLETE_H
#define AUDIOSOURCEDATAMODELOBSOLETE_H

#include "AudioCompat.h"
#include "AudioStartStop.h"

#include <QtCore/QObject>
#include <QtNodes/NodeDelegateModel>
#include <QtNodes/internal/Definitions.hpp>

class AudioNodeQdevIoConnectorObsolete;
class EventThreadPullObsolete;

class AudioSourceDataModelObsolete : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:

    AudioSourceDataModelObsolete();

    virtual
    ~AudioSourceDataModelObsolete() override;

public:

    QString
    caption() const override
    { return QStringLiteral("AudioSource Source (obsolete)"); }

    bool
    captionVisible() const override
    { return false; }

    QString
    name() const override
    { return QStringLiteral("AudioSourceObsolete"); }

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

    /// Core model has no QtWidgets dependency — the config UI is created by the
    /// GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051).
    QWidget *
    embeddedWidget() override { return nullptr; }

    QAudioDeviceInfo* deviceInfo() { return &m_DevInfo; }
    QAudioFormat* audioFormat() { return &m_FormatAudio; }

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
    void StartAudio(AudioStartStop start);
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);
    /// Worker audio state, forwarded to AudioSourceDataModelUI by the GUI
    /// plugin's NodeWidgetFactory.
    void audioStateChanged(QAudio::State state);

private slots:
    void destroyedObj(QObject *obj);

private:
    std::shared_ptr<AudioNodeQdevIoConnectorObsolete> m_connector;
    QAudioDeviceInfo m_DevInfo;
    QAudioFormat m_FormatAudio;
};

#endif // AUDIOSOURCEDATAMODELOBSOLETE_H
