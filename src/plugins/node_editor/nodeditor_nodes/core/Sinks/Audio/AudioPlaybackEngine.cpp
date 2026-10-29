#include "AudioPlaybackEngine.h"

#include "AudioFormatBridge.h"
#include "LogCategories.h"

#include <QAudioFormat>
#include <QElapsedTimer>

namespace {
/// Queue bound. The sink's own buffer is the queue — `bufferMs` is clamped to
/// this range, so a stalled device can hold at most ~2 s of audio and the graph
/// never sees an unbounded buffer or a blocking write.
constexpr int kMinBufferMs = 20;
constexpr int kMaxBufferMs = 2000;

/// Minimum interval between two attempts to open a sink that failed. Without it
/// an unusable format would retry (and log) once per block, i.e. ~100×/s.
constexpr qint64 kCreateRetryMs = 1000;

/// Layout label for the status line ("mono" / "stereo" / "N ch").
QString layoutLabel(int channels)
{
    if (channels == 1)
        return QStringLiteral("mono");
    if (channels == 2)
        return QStringLiteral("stereo");
    return QStringLiteral("%1 ch").arg(channels);
}

/// QAudio::Error → a message. Neither QAudioOutput (Qt5) nor QAudioSink (Qt6)
/// has errorString(), so the enum is the only error detail available.
QString audioErrorText(QAudio::Error error)
{
    switch (error) {
    case QAudio::NoError:
        return QStringLiteral("no error");
    case QAudio::OpenError:
        return QStringLiteral("the device cannot be opened");
    case QAudio::IOError:
        return QStringLiteral("I/O error");
    case QAudio::UnderrunError:
        return QStringLiteral("data underrun");
    case QAudio::FatalError:
        return QStringLiteral("fatal device error");
    }
    return QStringLiteral("unknown error (%1)").arg(static_cast<int>(error));
}
} // namespace

AudioPlaybackEngine::AudioPlaybackEngine(QObject *parent)
    : QObject(parent)
{
    m_status = QStringLiteral("stopped");
}

AudioPlaybackEngine::~AudioPlaybackEngine()
{
    // Single shutdown path: destroySink() stops the sink before dropping it.
    destroySink();
}

void AudioPlaybackEngine::setConfig(const QString &deviceId, double volume, int bufferMs, bool enabled)
{
    const int clampedBufferMs = qBound(kMinBufferMs, bufferMs, kMaxBufferMs);
    const bool deviceChanged = (deviceId != m_deviceId);
    // A new device or a new buffer size can only be honoured by a new sink: both
    // are constructor/buffer parameters of the QAudioSink, not live settings.
    const bool needsRecreate = deviceChanged || (clampedBufferMs != m_bufferMs);

    m_deviceId = deviceId;
    m_bufferMs = clampedBufferMs;
    m_volume = qBound(0.0, volume, 1.0);
    m_enabled = enabled;

    if (deviceChanged)
        m_device = AudioCompat::deviceFromId(m_deviceId);

    if (!m_enabled) {
        destroySink();
        m_error.clear();
        refreshStatus();
        return;
    }

    if (needsRecreate)
        destroySink();
    else if (m_sink)
        m_sink->setVolume(m_volume);   // live setting — no restart needed

    // A config change is the user's attempt to get rid of the previous error.
    m_error.clear();
    m_createThrottle.invalidate();

    // Reopen right away when the input format is already known, so a device
    // change during playback does not swallow the next block (AC6).
    if (m_running && m_sink == nullptr
        && m_lastDescriptor.sampleRate > 0.0 && !m_lastDescriptor.channels.isEmpty())
        createSink(m_lastDescriptor);

    refreshStatus();
}

void AudioPlaybackEngine::start()
{
    if (m_running) {
        refreshStatus();
        return;
    }

    m_running = true;
    m_error.clear();
    m_createThrottle.invalidate();
    // Per-run diagnostics: a stop/play cycle reports its own segment.
    m_blocksPlayed = 0;
    m_blocksDropped = 0;
    m_blocksSkipped = 0;
    m_dropLogCount = 0;

    if (!m_enabled) {
        refreshStatus();
        return;
    }

    m_device = AudioCompat::deviceFromId(m_deviceId);

    // Eager open ONLY when the input format is already known. A graph that has
    // not delivered a block yet opens the device on its first block instead —
    // start() must not invent a format the device may reject.
    if (!AudioCompat::isNull(m_device)
        && m_lastDescriptor.sampleRate > 0.0 && !m_lastDescriptor.channels.isEmpty())
        createSink(m_lastDescriptor);

    refreshStatus();
}

void AudioPlaybackEngine::stop()
{
    m_running = false;
    destroySink();
    m_error.clear();
    refreshStatus();
}

bool AudioPlaybackEngine::isPlaying() const
{
    return m_sink != nullptr && m_sink->state() == QAudio::ActiveState;
}

bool AudioPlaybackEngine::deviceAvailable() const
{
    // Resolved on demand: the default output device may appear after start()
    // (a headless run that gains a device, a USB headset plugged in mid-run).
    return !AudioCompat::isNull(AudioCompat::deviceFromId(m_deviceId));
}

QString AudioPlaybackEngine::deviceLabel() const
{
    const QAudioDeviceInfo device = AudioCompat::isNull(m_device)
        ? AudioCompat::deviceFromId(m_deviceId)
        : m_device;
    return AudioCompat::isNull(device) ? QString() : AudioCompat::deviceName(device);
}

void AudioPlaybackEngine::push(const SampledData &data)
{
    // Remember the input format even while stopped: remembering opens nothing
    // (AC7) and lets start() open the device eagerly instead of on the first block.
    m_lastDescriptor = data.descriptor();

    if (!m_enabled || !m_running)
        return;                     // graph is only being edited — no device

    if (!deviceAvailable()) {
        ++m_blocksSkipped;
        refreshStatus();
        return;
    }

    if (m_sink == nullptr) {
        // Throttled retry: a format the device cannot open must not attempt (and
        // log a warning) once per block. Config changes restart the throttle.
        if (m_createThrottle.isValid() && m_createThrottle.elapsed() < kCreateRetryMs) {
            ++m_blocksSkipped;
            return;
        }
        m_createThrottle.start();
        m_device = AudioCompat::deviceFromId(m_deviceId);
        if (!createSink(m_lastDescriptor)) {
            ++m_blocksSkipped;
            return;
        }
    }

    if (m_ioDevice == nullptr) {
        ++m_blocksSkipped;
        reportError(QStringLiteral("sink without a writable QIODevice"));
        return;
    }

    // Ask-then-convert (AC4): the sink was opened with the device's format, so
    // this resamples / mixes / re-encodes as needed. Never silent on failure.
    QString error;
    const QByteArray pcm = AudioFormatBridge::convert(data, m_format, &error);
    if (pcm.isEmpty()) {
        ++m_blocksSkipped;
        reportError(error.isEmpty() ? QStringLiteral("the block cannot be converted")
                                    : error);
        return;
    }

    // Backpressure: never block the graph and never grow a queue. The sink's
    // buffer is the queue (bufferMs), so a full buffer means drop, not wait.
    const qint64 free = static_cast<qint64>(m_sink->bytesFree());
    if (free < pcm.size()) {
        ++m_blocksDropped;
        logDrop(QStringLiteral("queue full"), pcm.size());
        return;
    }

    const qint64 written = static_cast<qint64>(m_ioDevice->write(pcm));
    if (written < 0) {
        ++m_blocksSkipped;
        reportError(audioErrorText(m_sink->error()));
        return;
    }
    if (written < pcm.size()) {
        ++m_blocksDropped;
        logDrop(QStringLiteral("partial write"), pcm.size() - written);
    }

    ++m_blocksPlayed;
    refreshStatus();
}

bool AudioPlaybackEngine::createSink(const SampledStreamDescriptor &descriptor)
{
    const AudioFormatBridge::PlaybackFormat resolved =
        AudioFormatBridge::resolveFormat(descriptor, m_device);

    // "no silent failure" (AC4): an unusable format becomes a status, not a
    // playback that quietly does nothing.
    if (!resolved.format.isValid()
        || AudioFormatBridge::bytesPerSample(resolved.format) <= 0) {
        reportError(resolved.reason.isEmpty()
                        ? QStringLiteral("unsupported audio format")
                        : resolved.reason);
        return false;
    }

    auto sink = std::unique_ptr<AudioCompat::AudioOutput>(
        new AudioCompat::AudioOutput(m_device, resolved.format));
    sink->setVolume(m_volume);

    const int bytesPerSecond = resolved.format.sampleRate()
        * resolved.format.channelCount()
        * AudioFormatBridge::bytesPerSample(resolved.format);
    const int bufferBytes = bytesPerSecond * m_bufferMs / 1000;
    if (bufferBytes > 0)
        sink->setBufferSize(bufferBytes);

    connect(sink.get(), &AudioCompat::AudioOutput::stateChanged,
            this, &AudioPlaybackEngine::onSinkStateChanged);

    // Push mode: the sink pulls from the writes into the device start() returns.
    m_ioDevice = sink->start();
    if (m_ioDevice == nullptr || sink->error() != QAudio::NoError) {
        reportError(audioErrorText(sink->error() != QAudio::NoError
                                       ? sink->error()
                                       : QAudio::OpenError));
        sink->stop();
        m_ioDevice = nullptr;
        return false;
    }

    m_sink = std::move(sink);
    m_format = resolved.format;
    m_converted = resolved.converted;
    m_formatReason = resolved.reason;
    m_error.clear();
    ++m_sinksCreated;

    qCInfo(lcDemoNodes).noquote()
        << QStringLiteral("AudioPlaybackEngine: sink opened — %1 (%2)")
               .arg(streamDescription(), m_formatReason);
    refreshStatus();
    return true;
}

void AudioPlaybackEngine::destroySink()
{
    if (m_sink != nullptr) {
        m_sink->stop();
        // m_ioDevice is owned by the sink: drop the pointer BEFORE the sink dies.
        m_ioDevice = nullptr;
        m_sink.reset();
    }
    m_ioDevice = nullptr;
    m_format = QAudioFormat();
    m_converted = false;
    m_formatReason.clear();
}

void AudioPlaybackEngine::onSinkStateChanged(QAudio::State state)
{
    if (state == QAudio::StoppedState && m_sink != nullptr
        && m_sink->error() != QAudio::NoError) {
        reportError(audioErrorText(m_sink->error()));
        return;
    }
    refreshStatus();
}

void AudioPlaybackEngine::refreshStatus()
{
    QString status;

    if (!m_enabled) {
        status = QStringLiteral("disabled");
    } else if (!m_error.isEmpty()) {
        status = QStringLiteral("error: %1").arg(m_error);
    } else if (!m_running) {
        status = QStringLiteral("stopped");
    } else if (!deviceAvailable()) {
        status = QStringLiteral("no output device");
    } else if (m_sink != nullptr) {
        status = QStringLiteral("%1 · %2")
                     .arg(m_sink->state() == QAudio::ActiveState
                              ? QStringLiteral("playing")
                              : QStringLiteral("ready"),
                          streamDescription());
    } else {
        // Running, device present, but no block has arrived yet: the format is
        // still unknown, so there is nothing to describe.
        status = QStringLiteral("ready · %1").arg(deviceLabel());
    }

    if (status != m_status) {
        m_status = status;
        Q_EMIT statusChanged(m_status);
    }
}

void AudioPlaybackEngine::reportError(const QString &what)
{
    // Log only when the message CHANGES: the same failure (an unsupported
    // encoding, a dead device) arrives with every block, and one warning per
    // block is a log flood. The counters and the status text still show that
    // blocks keep failing.
    const bool newMessage = (what != m_error);
    m_error = what;
    if (newMessage) {
        qCWarning(lcDemoNodes).noquote()
            << QStringLiteral("AudioPlaybackEngine: %1").arg(what);
    }
    refreshStatus();
}

QString AudioPlaybackEngine::streamDescription() const
{
    return QStringLiteral("%1 · %2 Hz %3%4")
        .arg(deviceLabel(),
             QString::number(m_format.sampleRate()),
             layoutLabel(m_format.channelCount()),
             m_converted ? QStringLiteral(" · converted") : QString());
}

void AudioPlaybackEngine::logDrop(const QString &reason, qsizetype bytes)
{
    ++m_dropLogCount;
    // First drop is always reported; after that only every 100th, so a device
    // that stays stalled cannot flood the log with one line per block.
    if (m_dropLogCount == 1 || (m_dropLogCount % 100) == 0) {
        qCWarning(lcDemoNodes).noquote()
            << QStringLiteral("AudioPlaybackEngine: block dropped (%1, %2 bytes) — "
                              "%3 dropped in total")
                   .arg(reason)
                   .arg(bytes)
                   .arg(m_blocksDropped);
    }
}
