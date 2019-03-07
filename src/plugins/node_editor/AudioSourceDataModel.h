#ifndef SOURCEDATAMODEL_H
#define SOURCEDATAMODEL_H

#include <QtCore/QObject>
#include <nodes/NodeDataModel>
#include <QtMultimedia/QAudioDeviceInfo>
#include <QtMultimedia/QAudioInput>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDataModel;
using QtNodes::NodeValidationState;
class QAudioInput;
class AudioNodeQdevIoConnector;

class AudioSourceDataModel : public NodeDataModel
{
    Q_OBJECT

public:
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

private:
    QAudioDeviceInfo m_DeviceInfo;
    QAudioFormat     m_FormatAudio;
    std::shared_ptr<QAudioInput> m_audio_src;
    std::shared_ptr<AudioNodeQdevIoConnector> m_connector;
#if 0

protected slots:
    void ChangeTime(int t);
private Q_SLOTS:

    void
    onTextEdited(QString const &string);

private:

    std::shared_ptr<ComplexType<double>> _number;

    //NumberSourceDataUi * m_ui;
    int m_time;
#endif
};

#endif // SOURCEDATAMODEL_H
