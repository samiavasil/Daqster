#include "MicCaptureWorker.h"

#include "LogCategories.h"

#include <QMutex>

#include <cstring>

/**
 * @brief Thread-safe QIODevice sink used as the QAudioSource/QAudioInput target.
 *
 * QAudioSource/QAudioInput call writeData() from their internal audio thread;
 * the worker's onReadyRead() drains the buffered PCM on the worker thread.
 * The mutex guards the shared buffer between the two threads. readyRead is
 * emitted after every write so the queued connection to the worker fires.
 */
class CaptureDevice : public QIODevice
{
public:
    explicit CaptureDevice(QObject *parent = nullptr)
        : QIODevice(parent)
    {
        // QAudioSource/QAudioInput requires the device opened in WriteOnly or
        // ReadWrite before start(); the worker reads back via readAll().
        open(QIODevice::ReadWrite);
    }

    qint64 bytesAvailable() const override
    {
        QMutexLocker locker(&m_mutex);
        return m_buffer.size();
    }

    qint64 readData(char *data, qint64 maxlen) override
    {
        if (data == nullptr || maxlen <= 0)
            return 0;

        QMutexLocker locker(&m_mutex);
        const qint64 n = qMin<qint64>(maxlen, m_buffer.size());
        if (n > 0) {
            std::memcpy(data, m_buffer.constData(), static_cast<size_t>(n));
            m_buffer.remove(0, static_cast<int>(n));
        }
        return n;
    }

    qint64 writeData(const char *data, qint64 len) override
    {
        if (data == nullptr || len <= 0)
            return 0;

        {
            QMutexLocker locker(&m_mutex);
            m_buffer.append(data, static_cast<int>(len));
        }
        emit readyRead();
        return len;
    }

    void clear()
    {
        QMutexLocker locker(&m_mutex);
        m_buffer.clear();
    }

private:
    mutable QMutex m_mutex;
    QByteArray m_buffer;
};

MicCaptureWorker::MicCaptureWorker(QObject *parent)
    : QObject(parent)
    , m_captureDevice(new CaptureDevice(this))
    , m_device(AudioCompat::defaultInputDevice())
    , m_format(AudioCompat::preferredFormat(m_device))
    , m_sourceName(AudioCompat::isNull(m_device)
                       ? QStringLiteral("AudioSource")
                       : AudioCompat::deviceName(m_device))
{
}

MicCaptureWorker::~MicCaptureWorker()
{
    // Runs on the worker thread (QThread::finished → deleteLater); the
    // QAudioSource destructor stops capture cleanly.
}

void MicCaptureWorker::startCapture()
{
    if (m_running.load())
        return;

    if (AudioCompat::isNull(m_device)) {
        qCWarning(lcDemoNodes) << "MicCaptureWorker: no default audio input device — capture not started";
        return;
    }

    if (!AudioCompat::isFormatSupported(m_device, m_format)) {
        qCWarning(lcDemoNodes) << "MicCaptureWorker: format not supported, using device preferred format";
        m_format = AudioCompat::preferredFormat(m_device);
    }

    m_captureDevice->clear();

    m_audioSource = std::make_unique<AudioCompat::AudioInput>(m_device, m_format);
    m_audioSource->setBufferSize(1000);

    connect(m_captureDevice, &QIODevice::readyRead,
            this, &MicCaptureWorker::onReadyRead, Qt::UniqueConnection);
    m_audioSource->start(m_captureDevice);

    if (m_audioSource->state() == QAudio::StoppedState) {
        qCWarning(lcDemoNodes) << "MicCaptureWorker: audio input failed to start (StoppedState)";
        m_audioSource.reset();
        m_running.store(false);
        return;
    }

    m_running.store(true);
}

void MicCaptureWorker::stopCapture()
{
    if (m_audioSource) {
        m_audioSource->stop();
        m_audioSource.reset();
    }
    if (m_captureDevice)
        m_captureDevice->clear();
    m_running.store(false);
}

void MicCaptureWorker::setCaptureEnabled(bool enabled)
{
    m_connected.store(enabled);
}

void MicCaptureWorker::updateDevice(const QAudioDeviceInfo &devInfo, const QAudioFormat &formatAudio)
{
    const bool wasRunning = m_running.load();

    stopCapture();
    m_device = devInfo;
    m_format = formatAudio;
    m_sourceName = AudioCompat::isNull(m_device)
                       ? QStringLiteral("AudioSource")
                       : AudioCompat::deviceName(m_device);

    if (wasRunning)
        startCapture();
}

void MicCaptureWorker::onReadyRead()
{
    // Drain unconditionally (the audio thread keeps writing); wrap + emit only
    // while a downstream consumer is connected (REQ-SW-PL-024 §3).
    const QByteArray raw = m_captureDevice->readAll();
    if (raw.isEmpty() || !m_connected.load())
        return;

    auto data = std::make_shared<SampledData>(
        raw, AudioBufferToSampled::descriptorFromFormat(m_format, m_sourceName));
    emit samplesReady(data);
}
