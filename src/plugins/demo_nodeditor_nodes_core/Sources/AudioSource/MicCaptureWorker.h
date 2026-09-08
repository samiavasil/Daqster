#ifndef MICCAPTUREWORKER_H
#define MICCAPTUREWORKER_H

#include "AudioCompat.h"
#include "AudioBufferToSampled.h"
#include "NodeDataTypes/SampledData.h"

#include <QAudioFormat>
#include <QIODevice>
#include <QObject>

#include <atomic>
#include <memory>

class MicCaptureWorker;
class CaptureDevice;

Q_DECLARE_METATYPE(std::shared_ptr<SampledData>)

/**
 * @brief Dedicated capture worker for the SampledData AudioSource node.
 *
 * Lives in a model-owned QThread (moveToThread + queued slots). All audio
 * work — QAudioSource/QAudioInput creation, readyRead draining, PCM→SampledData
 * wrapping — happens on the worker thread; the GUI thread only keeps the latest
 * shared_ptr and emits dataUpdated (REQ-SW-PL-024 §3).
 *
 * Capture policy: the audio source keeps running while the node is started,
 * but raw buffers are wrapped into SampledData and emitted ONLY while
 * `m_connected` is true (set by the model via the queued setCaptureEnabled
 * slot, mirroring the PL-022 connection-count gating). When not connected the
 * worker still drains the device so the internal audio buffer never fills.
 */
class MicCaptureWorker : public QObject
{
    Q_OBJECT

public:
    explicit MicCaptureWorker(QObject *parent = nullptr);
    ~MicCaptureWorker() override;

public slots:
    void startCapture();
    void stopCapture();
    void setCaptureEnabled(bool enabled);
    void updateDevice(const QAudioDeviceInfo &devInfo, const QAudioFormat &formatAudio);

signals:
    void samplesReady(std::shared_ptr<SampledData> data);

private slots:
    void onReadyRead();

private:
    std::unique_ptr<AudioCompat::AudioInput> m_audioSource;
    CaptureDevice *m_captureDevice = nullptr; // owned by this worker (child)
    QAudioDeviceInfo m_device;
    QAudioFormat m_format;
    QString m_sourceName;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_connected{false};
};

#endif // MICCAPTUREWORKER_H
