#include "AudioSourceDataModel.h"
#include "MicCaptureWorker.h"

#include <QDebug>
#include "LogCategories.h"

using QtNodes::NodeDataType;

namespace {
/// How long stop() waits for the capture thread (REQ-SW-PL-050). The worker
/// only drains a device, so a bounded join is enough; ThreadOwner bounds it.
constexpr int kCaptureJoinMs = 3000;
} // namespace

AudioSourceDataModel::AudioSourceDataModel()
{
    // Metatypes for the queued worker↔model connections (REQ-SW-PL-024 §3).
    qRegisterMetaType<std::shared_ptr<SampledData>>("std::shared_ptr<SampledData>");
    qRegisterMetaType<AudioStartStop>("AudioStartStop");
    qRegisterMetaType<QAudioDeviceInfo>();
    qRegisterMetaType<QAudioFormat>();

    m_DevInfo = AudioCompat::defaultInputDevice();
    m_FormatAudio = AudioCompat::preferredFormat(m_DevInfo);

    // Model-owned capture thread: ALL audio work happens there, the GUI thread
    // only keeps the latest shared_ptr and emits dataUpdated (hard requirement).
    // ThreadOwner owns the thread, the moveToThread() and the worker's lifetime
    // (REQ-SW-PL-050).
    m_capture.start([] { return new MicCaptureWorker(); });

    // worker → model (queued): SampledData crosses the thread boundary by
    // shared_ptr only; no mutex — produced fully on the worker thread.
    if (MicCaptureWorker *worker = m_capture.alive()) {
        connect(worker, &MicCaptureWorker::samplesReady,
                this, &AudioSourceDataModel::onSamplesReady);
    }
}

AudioSourceDataModel::~AudioSourceDataModel()
{
    // Single shutdown path: stop() quits + joins the capture thread
    // (REQ-SW-PL-050).
    stop();
}

void AudioSourceDataModel::stop()
{
    // Idempotent (REQ-SW-PL-050): ThreadOwner raises its latch BEFORE quit(),
    // joins with a deadline and drops the worker; a second call is NotRunning.
    // The latch closes the window in which a queued setCaptureEnabled() would
    // otherwise be posted to a worker that is being freed (REQ-SW-PL-078).
    m_capture.stop(kCaptureJoinMs);
}

bool AudioSourceDataModel::onCaptureThread(std::function<void(MicCaptureWorker *)> call)
{
    const QPointer<MicCaptureWorker> worker = m_capture.alive();
    if (!worker)
        return false;
    return m_capture.invoke([worker, call = std::move(call)] {
        if (worker)
            call(worker.data());
    });
}

/// Start capture programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onUiStart(ASDM_START).
void AudioSourceDataModel::start()
{
    onCaptureThread([](MicCaptureWorker *worker) { worker->startCapture(); });
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
    // Stopped: the capture thread is gone, nothing to command (ThreadOwner's
    // latch makes invoke() refuse and returns false).
    if (start == ASDM_START
        || start == ASDM_RELOAD) {
        onCaptureThread([](MicCaptureWorker *worker) { worker->startCapture(); });
    } else {
        onCaptureThread([](MicCaptureWorker *worker) { worker->stopCapture(); });
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

    // Stopped: keep the selection — it is model state, not capture.
    // updateDevice re-creates the QIODevice wrapper, so it must run on the
    // capture thread like every other worker-bound call.
    onCaptureThread([devInfo, formatAudio](MicCaptureWorker *worker) {
        worker->updateDevice(devInfo, formatAudio);
    });
}

void AudioSourceDataModel::setCaptureEnabled(bool enabled)
{
    // Reached from outputConnectionDeleted(), i.e. from the scene close AFTER
    // stop() freed the worker (REQ-SW-PL-078): onCaptureThread() checks the
    // owner's latch/worker, so the call is a no-op instead of a queued invoke
    // on a dangling pointer.
    onCaptureThread([enabled](MicCaptureWorker *worker) {
        worker->setCaptureEnabled(enabled);
    });
}

void AudioSourceDataModel::onSamplesReady(std::shared_ptr<SampledData> data)
{
    if (!data)
        return;

    // GUI thread: keep-latest + dataUpdated ONLY (REQ-SW-PL-024 §3).
    m_lastData = std::move(data);

    emit dataUpdated(0);
}
