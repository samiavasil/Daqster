#ifndef AUDIOSOURCEDATAMODEL_H
#define AUDIOSOURCEDATAMODEL_H

#include "AudioCompat.h"
#include "AudioSourceDataModelUI.h"
#include "MicCaptureWorker.h"
#include "NodeDataTypes/SampledData.h"

#include <QtCore/QThread>
#include <QtNodes/NodeDelegateModel>
#include <QtNodes/internal/Definitions.hpp>

#include <memory>

class AudioSourceDataModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
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

    QtNodes::ConnectionPolicy portConnectionPolicy(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override
    {
        Q_UNUSED(portType);
        Q_UNUSED(portIndex);
        return QtNodes::ConnectionPolicy::One;
    }

    void outputConnectionCreated(QtNodes::ConnectionId const &) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &) override;

private slots:
    void onUiStart(AudioSourceDataModelUI::StartStop start);
    void onSamplesReady(std::shared_ptr<SampledData> data);

private:
    void setCaptureEnabled(bool enabled);

    QThread *m_thread = nullptr;
    MicCaptureWorker *m_worker = nullptr; // moveToThread'ed into m_thread; freed via QThread::finished → deleteLater
    AudioSourceDataModelUI *m_Widget = nullptr;
    QAudioDeviceInfo m_DevInfo;
    QAudioFormat m_FormatAudio;
    std::shared_ptr<SampledData> m_lastData;
    int m_connectionCount = 0;
};

#endif // AUDIOSOURCEDATAMODEL_H
