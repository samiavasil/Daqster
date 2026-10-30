#include "AudioPlaybackEngine.h"

#include "AudioFormatBridge.h"
#include "LogCategories.h"

#include <QAudioFormat>
#include <QElapsedTimer>
#include <QScopedValueRollback>

#include <algorithm>
#include <cmath>

namespace {
/// Bounds of the device buffer (`bufferMs`). It is only the size of the buffer
/// the sink is created with — never a limit on how large a block may be.
constexpr int kMinBufferMs = 20;
constexpr int kMaxBufferMs = 2000;

/// Bounds of the playback queue (`maxQueuedMs`), the actual backpressure limit.
/// A TTS sentence arrives as one block of up to ~2 s, so the lower bound has to
/// be able to hold a sentence, not a 20 ms frame. Declared on the class (the
/// node model clamps and describes the same range).
constexpr int kMinMaxQueuedMs = AudioPlaybackEngine::kMinMaxQueuedMs;
constexpr int kMaxMaxQueuedMs = AudioPlaybackEngine::kMaxMaxQueuedMs;

/// How often the queue is offered to the device while it drains. 20 ms is well
/// below the smallest device buffer (20 ms) so the sink never underruns because
/// of the timer, and the timer is stopped as soon as the queue is empty.
constexpr int kPumpIntervalMs = 20;

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

/// Duration of @p pcm in milliseconds at @p format, 0 for an unusable format.
double pcmDurationMs(qsizetype bytes, const QAudioFormat &format)
{
    const int bytesPerFrame =
        format.channelCount() * AudioFormatBridge::bytesPerSample(format);
    if (bytesPerFrame <= 0 || format.sampleRate() <= 0)
        return 0.0;
    return double(bytes / bytesPerFrame) * 1000.0 / double(format.sampleRate());
}
} // namespace

AudioPlaybackEngine::AudioPlaybackEngine(QObject *parent)
    : QObject(parent)
{
    m_status = QStringLiteral("stopped");
    m_pumpTimer.setInterval(kPumpIntervalMs);
    connect(&m_pumpTimer, &QTimer::timeout, this, &AudioPlaybackEngine::pump);
}

AudioPlaybackEngine::~AudioPlaybackEngine()
{
    // Single shutdown path: destroySink() stops the sink before dropping it.
    destroySink();
}

void AudioPlaybackEngine::setConfig(const QString &deviceId, double volume, int bufferMs,
                                    bool enabled, int maxQueuedMs)
{
    const int clampedBufferMs = qBound(kMinBufferMs, bufferMs, kMaxBufferMs);
    const bool deviceChanged = (deviceId != m_deviceId);
    // A new device or a new buffer size can only be honoured by a new sink: both
    // are constructor/buffer parameters of the QAudioSink, not live settings.
    const bool needsRecreate = deviceChanged || (clampedBufferMs != m_bufferMs);

    m_deviceId = deviceId;
    m_bufferMs = clampedBufferMs;
    m_maxQueuedMs = qBound(kMinMaxQueuedMs, maxQueuedMs, kMaxMaxQueuedMs);
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

    if (needsRecreate) {
        // The queue is in the OLD format/device: it is dropped with the sink
        // instead of being pushed into a device it was not converted for.
        destroySink();
    } else if (m_sink) {
        m_sink->setVolume(m_volume);   // live setting — no restart needed
    }

    // A config change is the user's attempt to get rid of the previous error.
    m_error.clear();
    m_createThrottle.invalidate();

    // Reopen right away when the input format is already known, so a device
    // change during playback does not swallow the next block (AC6).
    if (m_running && m_sink == nullptr
        && m_lastDescriptor.sampleRate > 0.0 && !m_lastDescriptor.channels.isEmpty())
        createSink(m_lastDescriptor);

    pump();
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

    // Blocks that arrived while the node was not started were queued instead of
    // dropped: play them now that a device may be opened.
    pump();
    refreshStatus();
}

void AudioPlaybackEngine::stop()
{
    m_running = false;
    // destroySink() also clears the queue (PCM in the old format).
    destroySink();
    clearQueue();
    m_error.clear();
    updatePumpTimer();
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
    // (AC7) and lets start()/pump() open the device eagerly.
    m_lastDescriptor = data.descriptor();

    if (!m_enabled) {
        ++m_blocksSkipped;
        refreshStatus();
        return;
    }

    // The queue is single-format: while the sink is open that is the sink's
    // format, before that the format the sink WOULD open with (resolved
    // without touching the device). A block is never refused because it is
    // larger than the device buffer — only the queue bound matters.
    const QAudioFormat targetFormat =
        (m_sink != nullptr) ? m_format : provisionalFormat(m_lastDescriptor);
    if (!targetFormat.isValid()) {
        ++m_blocksSkipped;
        reportError(QStringLiteral("unsupported audio format"));
        return;
    }

    // Ask-then-convert (AC4): the sink was opened with the device's format, so
    // this resamples / mixes / re-encodes as needed. Never silent on failure.
    QString error;
    const QByteArray pcm = AudioFormatBridge::convert(data, targetFormat, &error);
    if (pcm.isEmpty()) {
        ++m_blocksSkipped;
        reportError(error.isEmpty() ? QStringLiteral("the block cannot be converted")
                                    : error);
        return;
    }

    if (!m_queue.isEmpty() && m_queueFormat != targetFormat) {
        // The output format changed under the queued audio (a device appeared
        // or was replaced). Playing old-format PCM into the new sink would be
        // garbage, so the queue is dropped explicitly — never silently.
        qCInfo(lcDemoNodes).noquote()
            << QStringLiteral("AudioPlaybackEngine: queued audio cleared — "
                              "output format changed");
        clearQueue();
    }
    m_queueFormat = targetFormat;

    QueuedBlock block;
    block.pcm = pcm;
    block.durationMs = pcmDurationMs(pcm.size(), targetFormat);
    m_queue.append(block);

    // Backpressure lives HERE, and it is a TIME bound, not a block-size check:
    // when the queued audio would exceed maxQueuedMs the OLDEST block goes. A
    // 2 s TTS sentence in a 200 ms device buffer is the normal case now.
    while (!m_queue.isEmpty() && queueMs() > m_maxQueuedMs) {
        const QueuedBlock oldest = m_queue.takeFirst();
        ++m_blocksDropped;
        logDrop(QStringLiteral("queue over maxQueuedMs"),
                oldest.pcm.size() - oldest.written);
    }

    // Accepted into the queue — whether it is written now or by the next pump.
    ++m_blocksPlayed;

    pump();
    refreshStatus();
}

qsizetype AudioPlaybackEngine::bytesQueued() const
{
    qsizetype bytes = 0;
    for (const QueuedBlock &block : m_queue)
        bytes += block.pcm.size() - block.written;
    return bytes;
}

int AudioPlaybackEngine::queueMs() const
{
    double ms = 0.0;
    for (const QueuedBlock &block : m_queue) {
        const qsizetype total = block.pcm.size();
        const qsizetype remaining = total - block.written;
        if (total <= 0 || remaining <= 0)
            continue;
        ms += block.durationMs * double(remaining) / double(total);
    }
    return int(std::lround(ms));
}

QAudioFormat AudioPlaybackEngine::provisionalFormat(
    const SampledStreamDescriptor &descriptor) const
{
    // A null device makes resolveFormat() return the descriptor's own format —
    // exactly what we want while no device exists: queue it, play it when one
    // appears. Nothing is opened here.
    const QAudioDeviceInfo device = AudioCompat::isNull(m_device)
        ? AudioCompat::deviceFromId(m_deviceId)
        : m_device;
    return AudioFormatBridge::resolveFormat(descriptor, device).format;
}

void AudioPlaybackEngine::pump()
{
    if (m_pumping)
        return;   // re-entered from a synchronous stateChanged during write()
    const QScopedValueRollback<bool> guard(m_pumping, true);

    if (!m_enabled || !m_running || m_queue.isEmpty()) {
        updatePumpTimer();
        return;
    }

    if (m_sink == nullptr) {
        // Throttled retry: a format the device cannot open must not attempt (and
        // log a warning) once per block. Config changes restart the throttle.
        if (!deviceAvailable()
            || (m_createThrottle.isValid() && m_createThrottle.elapsed() < kCreateRetryMs)) {
            updatePumpTimer();
            return;
        }
        m_createThrottle.start();
        m_device = AudioCompat::deviceFromId(m_deviceId);
        if (!createSink(m_lastDescriptor)) {
            updatePumpTimer();
            return;   // the queued block survives — the bound keeps memory finite
        }
    }

    if (m_ioDevice == nullptr) {
        updatePumpTimer();
        return;
    }

    writeQueued();
    updatePumpTimer();
    refreshStatus();
}

bool AudioPlaybackEngine::writeQueued()
{
    if (m_sink == nullptr || m_ioDevice == nullptr)
        return !m_queue.isEmpty();

    const int frameBytes =
        m_format.channelCount() * AudioFormatBridge::bytesPerSample(m_format);

    while (!m_queue.isEmpty()) {
        QueuedBlock &head = m_queue.first();
        const qsizetype remaining = head.pcm.size() - head.written;
        if (remaining <= 0) {
            m_queue.removeFirst();
            continue;
        }

        const qint64 free = static_cast<qint64>(m_sink->bytesFree());
        if (free <= 0)
            return true;   // device buffer full — the pump timer comes back

        // Write at most what fits RIGHT NOW: the block size is irrelevant, the
        // device buffer is the only thing that decides a chunk.
        qint64 chunk = std::min<qint64>(free, static_cast<qint64>(remaining));
        if (frameBytes > 0)
            chunk -= chunk % frameBytes;
        if (chunk <= 0)
            return true;

        const qint64 written =
            static_cast<qint64>(m_ioDevice->write(head.pcm.constData() + head.written, chunk));
        if (written < 0) {
            reportError(audioErrorText(m_sink->error()));
            return true;   // keep the data: a retry may succeed after a state change
        }
        if (written == 0)
            return true;   // no space yet, not an error

        head.written += written;
        if (head.written >= head.pcm.size())
            m_queue.removeFirst();
        else
            return true;   // the device buffer is full again
    }

    return false;
}

void AudioPlaybackEngine::updatePumpTimer()
{
    const bool needed = m_enabled && m_running && !m_queue.isEmpty()
                        && (m_ioDevice != nullptr || deviceAvailable());
    if (needed && !m_pumpTimer.isActive())
        m_pumpTimer.start();
    else if (!needed && m_pumpTimer.isActive())
        m_pumpTimer.stop();   // never spin on an empty queue / absent device
}

void AudioPlaybackEngine::clearQueue()
{
    m_queue.clear();
    m_queueFormat = QAudioFormat();
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

    // PCM queued before the device opened is in `m_queueFormat`. If the device
    // picked another format, that audio belongs to a different output and is
    // dropped instead of being played as noise.
    if (!m_queue.isEmpty() && m_queueFormat != m_format) {
        qCInfo(lcDemoNodes).noquote()
            << QStringLiteral("AudioPlaybackEngine: queued audio cleared — "
                              "output format changed on sink open");
        clearQueue();
    }

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
    // The queue was converted for the sink that just died: never hand it to a
    // new device, which may (and usually does) have another format.
    clearQueue();
    updatePumpTimer();
}

void AudioPlaybackEngine::onSinkStateChanged(QAudio::State state)
{
    if (state == QAudio::StoppedState && m_sink != nullptr
        && m_sink->error() != QAudio::NoError) {
        reportError(audioErrorText(m_sink->error()));
        return;
    }
    // Idle/Active means the device buffer just drained or reopened: feed the
    // queue now instead of waiting for the next 20 ms tick.
    pump();
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
