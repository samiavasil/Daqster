#ifndef SOURCEDATAMODEL_H
#define SOURCEDATAMODEL_H

#include <QtCore/QObject>
#include <nodes/NodeDataModel>
#include <QtMultimedia/QAudioDeviceInfo>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDataModel;
using QtNodes::NodeValidationState;
class QAudioInput;
class AudioNodeQdevIoConnector;
class AudioSourceDataModelUI;
class QAudioInputThread;

class AudioSourceDataModel : public NodeDataModel
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
    ~AudioSourceDataModel();

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
    outData(PortIndex port) override;

    void
    setInData(std::shared_ptr<NodeData> data, PortIndex port) override;

    QWidget *
    embeddedWidget() override;

    void IO_connect(QSharedPointer<QIODevice> io);

    virtual
    ConnectionPolicy
    portOutConnectionPolicy(PortIndex) const
    {
      return ConnectionPolicy::One;
    }
signals:
    void ModChanged();
public slots:
    virtual void outputConnectionDeleted(QtNodes::Connection const&con);

private slots:
    void ChangeAudioConnection(QAudioDeviceInfo &devInfo, QAudioFormat &formatAudio);
    void StartAudio(AudioSourceDataModel::StartStop start);
    void destroyedObj(QObject *obj);
private:
    std::shared_ptr<AudioNodeQdevIoConnector> m_connector;
    QSharedPointer<QIODevice>   m_devio;
    std::shared_ptr<QAudioInputThread> m_audio_src;
    QAudioFormat     m_FormatAudio;
    QAudioDeviceInfo m_DevInfo;
    AudioSourceDataModelUI* m_Widget;
};

#endif // SOURCEDATAMODEL_H
