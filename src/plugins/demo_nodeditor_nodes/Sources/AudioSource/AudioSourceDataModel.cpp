#include "AudioSourceDataModel.h"
#include "AudioSourceDataModelUI.h"
#include "MicCaptureWorker.h"

#include <QDebug>
#include <QThread>
#include "LogCategories.h"

using QtNodes::NodeDataType;

AudioSourceDataModel::AudioSourceDataModel()
{
    // Metatypes for the queued worker↔model connections (REQ-SW-PL-024 §3).
    qRegisterMetaType<std::shared_ptr<SampledData>>("std::shared_ptr<SampledData>");
    qRegisterMetaType<AudioSourceDataModelUI::StartStop>("AudioSourceDataModelUI::StartStop");
    qRegisterMetaType<QAudioDeviceInfo>();
    qRegisterMetaType<QAudioFormat>();

    m_DevInfo = AudioCompat::defaultInputDevice();
    m_FormatAudio = AudioCompat::preferredFormat(m_DevInfo);

    m_Widget = new AudioSourceDataModelUI(&m_DevInfo, &m_FormatAudio);
    m_Widget->setWindowFlags(Qt::Window
                             | Qt::WindowTitleHint
                             | Qt::WindowSystemMenuHint
                             | Qt::WindowMinMaxButtonsHint
                             | Qt::WindowCloseButtonHint);
    m_Widget->setWindowModality(Qt::NonModal);

    // Model-owned worker thread: ALL audio work happens there, the GUI thread
    // only keeps the latest shared_ptr and emits dataUpdated (hard requirement).
    m_thread = new QThread(this);
    m_thread->setObjectName(QStringLiteral("AudioSourceCaptureThread"));

    m_worker = new MicCaptureWorker();
    m_worker->moveToThread(m_thread);
    // Worker freed on the worker thread when the thread finishes (Qt pattern).
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    // UI → worker (queued; the worker lives in m_thread).
    // The UI has two Start() overloads (signal StartStop + private slot bool);
    // QOverload disambiguates the signal for the new-style connect.
    connect(m_Widget, QOverload<AudioSourceDataModelUI::StartStop>::of(&AudioSourceDataModelUI::Start),
            this, &AudioSourceDataModel::onUiStart);
    connect(m_Widget, &AudioSourceDataModelUI::ChangeAudioConnection,
            m_worker, &MicCaptureWorker::updateDevice);

    // worker → model (queued): SampledData crosses the thread boundary by
    // shared_ptr only; no mutex — produced fully on the worker thread.
    connect(m_worker, &MicCaptureWorker::samplesReady,
            this, &AudioSourceDataModel::onSamplesReady);

    m_thread->start();
}

AudioSourceDataModel::~AudioSourceDataModel()
{
    // Stop the capture thread first; the worker is deleted via
    // QThread::finished → deleteLater (standard Qt pattern).
    if (m_thread != nullptr) {
        m_thread->quit();
        m_thread->wait();
    }

    // Widget lifetime is owned by the node/view framework.
    // Explicit delete here causes double-free during scene teardown.
    m_Widget = nullptr;
}

QJsonObject AudioSourceDataModel::save() const
{
    QJsonObject modelJson;

    modelJson["name"] = name();
    return modelJson;
}

unsigned int AudioSourceDataModel::nPorts(QtNodes::PortType portType) const
{
    unsigned int num = 0;

    switch (portType) {
    case QtNodes::PortType::Out:
        num = 1;
        break;
    default:
        break;
    }
    return num;
}

QtNodes::NodeDataType AudioSourceDataModel::dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<QtNodes::NodeData> AudioSourceDataModel::outData(QtNodes::PortIndex const port)
{
    Q_UNUSED(port);
    return m_lastData;
}

void AudioSourceDataModel::setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const port)
{
    Q_UNUSED(data);
    Q_UNUSED(port);
    Q_ASSERT(0);
}

QWidget *AudioSourceDataModel::embeddedWidget()
{
    return m_Widget;
}

void AudioSourceDataModel::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    ++m_connectionCount;
    setCaptureEnabled(m_connectionCount > 0);
}

void AudioSourceDataModel::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    if (m_connectionCount > 0)
        --m_connectionCount;
    setCaptureEnabled(m_connectionCount > 0);
}

void AudioSourceDataModel::onUiStart(AudioSourceDataModelUI::StartStop start)
{
    // Queued dispatch to the worker thread; capture itself runs there.
    if (start == AudioSourceDataModelUI::ASDM_START
        || start == AudioSourceDataModelUI::ASDM_RELOAD) {
        QMetaObject::invokeMethod(m_worker, "startCapture", Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(m_worker, "stopCapture", Qt::QueuedConnection);
    }
}

void AudioSourceDataModel::setCaptureEnabled(bool enabled)
{
    // Queued: the worker gates wrap+emit on this flag (PL-022 §4 pattern).
    QMetaObject::invokeMethod(m_worker, "setCaptureEnabled", Qt::QueuedConnection,
                              Q_ARG(bool, enabled));
}

void AudioSourceDataModel::onSamplesReady(std::shared_ptr<SampledData> data)
{
    if (!data)
        return;

    // GUI thread: keep-latest + dataUpdated ONLY (REQ-SW-PL-024 §3).
    m_lastData = std::move(data);

    emit dataUpdated(0);
}
