#include "AudioSourceDataModel.h"
#include "MicCaptureWorker.h"

#include <QDebug>
#include <QThread>
#include "LogCategories.h"

using QtNodes::NodeDataType;

AudioSourceDataModel::AudioSourceDataModel()
{
    // Metatypes for the queued worker↔model connections (REQ-SW-PL-024 §3).
    qRegisterMetaType<std::shared_ptr<SampledData>>("std::shared_ptr<SampledData>");
    qRegisterMetaType<AudioStartStop>("AudioStartStop");
    qRegisterMetaType<QAudioDeviceInfo>();
    qRegisterMetaType<QAudioFormat>();

    m_DevInfo = AudioCompat::defaultInputDevice();
    m_FormatAudio = AudioCompat::preferredFormat(m_DevInfo);

    // Model-owned worker thread: ALL audio work happens there, the GUI thread
    // only keeps the latest shared_ptr and emits dataUpdated (hard requirement).
    m_thread = new QThread(this);
    m_thread->setObjectName(QStringLiteral("AudioSourceCaptureThread"));

    m_worker = new MicCaptureWorker();
    m_worker->moveToThread(m_thread);
    // Worker freed on the worker thread when the thread finishes (Qt pattern).
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    // worker → model (queued): SampledData crosses the thread boundary by
    // shared_ptr only; no mutex — produced fully on the worker thread.
    connect(m_worker, &MicCaptureWorker::samplesReady,
            this, &AudioSourceDataModel::onSamplesReady);

    m_thread->start();
}

AudioSourceDataModel::~AudioSourceDataModel()
{
    // Single shutdown path: stop() quits + waits the capture thread
    // (REQ-SW-PL-050).
    stop();
}

void AudioSourceDataModel::stop()
{
    // Idempotent: quit()/wait() on an already-stopped thread is a no-op.
    if (m_thread != nullptr) {
        m_thread->quit();
        m_thread->wait();
    }
}

/// Start capture programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onUiStart(ASDM_START).
void AudioSourceDataModel::start()
{
    QMetaObject::invokeMethod(m_worker, "startCapture", Qt::QueuedConnection);
}

QJsonObject AudioSourceDataModel::save() const
{
    QJsonObject modelJson = NodeDelegateModel::save();
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

void AudioSourceDataModel::onUiStart(AudioStartStop start)
{
    // Queued dispatch to the worker thread; capture itself runs there.
    if (start == ASDM_START
        || start == ASDM_RELOAD) {
        QMetaObject::invokeMethod(m_worker, "startCapture", Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(m_worker, "stopCapture", Qt::QueuedConnection);
    }
}

/// A device/format choice made in the UI must be remembered here: the UI holds
/// non-owning pointers to m_DevInfo / m_FormatAudio, so those must track the
/// selection. Forwarding to the worker is wired in NodeWidgetFactory.
void AudioSourceDataModel::onAudioConnectionChanged(QAudioDeviceInfo devInfo,
                                                    QAudioFormat formatAudio)
{
    m_DevInfo = devInfo;
    m_FormatAudio = formatAudio;

    // The capture worker lives in m_thread; updateDevice is a plain slot so a
    // direct call from the GUI thread is safe (it only re-creates the QIODevice
    // wrapper) — this mirrors the old direct widget→worker connection.
    QMetaObject::invokeMethod(m_worker, "updateDevice", Qt::QueuedConnection,
                              Q_ARG(QAudioDeviceInfo, devInfo),
                              Q_ARG(QAudioFormat, formatAudio));
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
