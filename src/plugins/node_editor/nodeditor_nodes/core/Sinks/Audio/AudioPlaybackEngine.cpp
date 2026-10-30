#include "AudioPlaybackEngine.h"

#include "AudioFormatBridge.h"
#include "LogCategories.h"

#include <QAudioFormat>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QScopedValueRollback>

#include <algorithm>
#include <cmath>

namespace {
/// Bounds of the device buffer (`bufferMs`). It is only the size of the buffer
/// the sink is created with — never a limit on how large a block may be.
constexpr int kMinBufferMs = 20;
constexpr int kMaxBufferMs = 2000;

/// Bounds of the playback queue (`maxQueuedMs`), the accumulation limit.
/// A TTS sentence arrives as one block of 2.4–2.6 s (measured), so the default
/// has to hold a sentence with reserve and the lower bound must still hold one.
/// Declared on the class (the node model clamps and describes the same range).
constexpr int kMinMaxQueuedMs = AudioPlaybackEngine::kMinMaxQueuedMs;
constexpr int kMaxMaxQueuedMs = AudioPlaybackEngine::kMaxMaxQueuedMs;
constexpr int kHardBlockCapMs = AudioPlaybackEngine::kHardBlockCapMs;

/// How often the queue is offered to the device while it drains. 20 ms is well
/// below the smallest device buffer (20 ms) so the sink never underruns because
/// of the timer, and the timer is stopped as soon as the queue is empty.
constexpr int kPumpIntervalMs = 20;

/// Minimum interval between two attempts to open a sink that failed. Without it
/// an unusable format would retry (and log) once per block, i.e. ~100×/s.
constexpr qint64 kCreateRetryMs = 1000;

/// How long a device resolution may stay in the process-wide memo before it is
/// re-checked. Qt5 resolves an id by enumerating every output device and one
/// enumeration costs 2.4–2.8 s on a machine with 71 ALSA devices, so the memo
/// must survive far longer than a single block — but a headset plugged in
/// mid-run still has to be noticed without restarting the application.
constexpr qint64 kDeviceMemoMs = 30000;

/// One memoised `AudioCompat::deviceFromId()` result.
struct DeviceMemoEntry {
    QAudioDeviceInfo device;
    QElapsedTimer resolvedAt;
};

/// Guards the memo: the engine lives on the graph thread, but several engines
/// (nodes, tests, a preview widget) resolve the same ids in one process.
QMutex &deviceMemoMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, DeviceMemoEntry> &deviceMemo()
{
    static QHash<QString, DeviceMemoEntry> memo;
    return memo;
}

/// `AudioCompat::deviceFromId()` through the process-wide memo.
///
/// Why this exists (measured, REQ-SW-PL-076): on Qt5 every resolution is a
/// `QAudioDeviceInfo::availableDevices()` call — 2.4–2.8 s with 71 ALSA
/// devices. The engine used to resolve on every status refresh, every pump tick
/// and up to four times per block, which made `audiooutput_tests` take 313 s on
/// Qt5 against 0.7 s on Qt6.
QAudioDeviceInfo resolveDeviceMemoised(const QString &id)
{
    const QMutexLocker locker(&deviceMemoMutex());
    QHash<QString, DeviceMemoEntry> &memo = deviceMemo();

    const auto it = memo.constFind(id);
    if (it != memo.constEnd() && it->resolvedAt.isValid()
        && it->resolvedAt.elapsed() < kDeviceMemoMs)
        return it->device;

    DeviceMemoEntry entry;
    entry.device = AudioCompat::deviceFromId(id);
    entry.resolvedAt.start();
    memo.insert(id, entry);
    return entry.device;
}

/// Drop every memoised resolution — the device list itself is suspect.
void clearDeviceMemo()
{
    const QMutexLocker locker(&deviceMemoMutex());
    deviceMemo().clear();
}

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

/// Frame-aligned byte count for @p ms of audio at @p format, 0 for an unusable
/// format. Used by the hard per-block cap, so a trimmed block still ends on a
/// frame boundary and plays cleanly.
qsizetype pcmBytesForMs(int ms, const QAudioFormat &format)
{
    const int bytesPerFrame =
        format.channelCount() * AudioFormatBridge::bytesPerSample(format);
    if (bytesPerFrame <= 0 || format.sampleRate() <= 0 || ms <= 0)
        return 0;
    const qint64 frames = qint64(format.sampleRate()) * ms / 1000;
    return qsizetype(frames * bytesPerFrame);
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

    // Only the id is stored here: the device is resolved lazily through the
    // cache (AC6 — a changed device is honoured by the next sink creation).
    if (deviceChanged)
        invalidateDevice();

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
    // An explicit start is the only thing that undoes an explicit stop — the
    // node's auto-arm must not resurrect a sink the user silenced on purpose.
    m_userStopped = false;
    m_error.clear();
    m_createThrottle.invalidate();
    // Per-run diagnostics: a stop/play cycle reports its own segment.
    m_blocksPlayed = 0;
    m_blocksDropped = 0;
    m_blocksSkipped = 0;
    m_blocksQueuedWhileStopped = 0;
    m_stoppedWarningLogged = false;
    m_dropLogCount = 0;
    m_underruns = 0;
    m_underrunLogCount = 0;
    m_lastSinkError = QAudio::NoError;

    // The engine is armed — whoever armed it (the node's auto-arm or a click on
    // Start). The widget needs this even when the status text does not change.
    Q_EMIT runningChanged(m_running);

    if (!m_enabled) {
        refreshStatus();
        return;
    }

    // A restart is the user asking for playback: re-check the device list (or
    // at least the memo entry) instead of trusting a resolution made an hour
    // ago — the memo keeps this at zero enumerations while it is fresh.
    invalidateDevice();

    // Eager open ONLY when the input format is already known. A graph that has
    // not delivered a block yet opens the device on its first block instead —
    // start() must not invent a format the device may reject.
    if (!AudioCompat::isNull(resolvedDevice())
        && m_lastDescriptor.sampleRate > 0.0 && !m_lastDescriptor.channels.isEmpty())
        createSink(m_lastDescriptor);

    // Blocks that arrived while the node was not started were queued instead of
    // dropped: play them now that a device may be opened.
    pump();
    refreshStatus();
}

void AudioPlaybackEngine::stop()
{
    const bool wasRunning = m_running;

    m_running = false;
    // "Stop" means "stop and stay stopped": AudioOutputNode auto-arms on the
    // first block, and only an explicit start() clears this flag again.
    m_userStopped = true;
    // A later stop must produce its own visibility report, not inherit the
    // "already warned" flag of the previous stop period.
    m_stoppedWarningLogged = false;
    // destroySink() also clears the queue (PCM in the old format).
    destroySink();
    clearQueue();
    m_error.clear();
    updatePumpTimer();
    if (wasRunning)
        Q_EMIT runningChanged(m_running);
    refreshStatus();
}

bool AudioPlaybackEngine::isPlaying() const
{
    return m_sink != nullptr && m_sink->state() == QAudio::ActiveState;
}

bool AudioPlaybackEngine::deviceAvailable() const
{
    // Served from the cache: a status refresh must never enumerate devices.
    // The cache is re-checked when the id changes, on start() and after a sink
    // error (refreshDeviceList()), which covers a device appearing or vanishing.
    return !AudioCompat::isNull(resolvedDevice());
}

const QAudioDeviceInfo &AudioPlaybackEngine::resolvedDevice() const
{
    if (m_deviceIsResolved && m_deviceResolvedForId == m_deviceId)
        return m_device;

    m_device = resolveDeviceMemoised(m_deviceId);
    m_deviceResolvedForId = m_deviceId;
    m_deviceIsResolved = true;
    ++m_deviceResolutions;
    return m_device;
}

void AudioPlaybackEngine::invalidateDevice()
{
    m_device = QAudioDeviceInfo();
    m_deviceResolvedForId.clear();
    m_deviceIsResolved = false;
}

void AudioPlaybackEngine::refreshDeviceList()
{
    invalidateDevice();
    clearDeviceMemo();
}

QString AudioPlaybackEngine::deviceLabel() const
{
    const QAudioDeviceInfo &device = resolvedDevice();
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

    // Checked BEFORE the conversion: the resampler's carried phase describes the
    // audio already in the queue, so it must be dropped in the same breath as
    // the queue itself — never after another block has been converted with it.
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

    // Ask-then-convert (AC4): the sink was opened with the device's format, so
    // this resamples / mixes / re-encodes as needed. Never silent on failure.
    // The converter is the engine's own: it continues the phase of the previous
    // block, which is the difference between seamless audio and a click at every
    // block boundary (AC9).
    QString error;
    const QByteArray pcm =
        AudioFormatBridge::convert(data, targetFormat, &m_rateConverter, &error);
    if (pcm.isEmpty()) {
        ++m_blocksSkipped;
        reportError(error.isEmpty() ? QStringLiteral("the block cannot be converted")
                                    : error);
        return;
    }

    // Stopped is NOT "no data", but it is no longer the normal state either:
    // the node arms playback on the first block, so `m_running == false` here
    // means the node was **explicitly stopped** (or is disabled, which returned
    // above). Such a block is queued and nothing plays, which in headless is
    // indistinguishable from a silent graph — the .flow "looks fine" and stays
    // mute. One warning per stop period (a live source delivers ~100 blocks/s,
    // so a line per block would flood the log), plus a counter and a status hint.
    if (!m_running) {
        ++m_blocksQueuedWhileStopped;
        if (!m_stoppedWarningLogged) {
            m_stoppedWarningLogged = true;
            qCWarning(lcDemoNodes).noquote()
                << QStringLiteral("AudioOutput was stopped explicitly — %1 block(s) "
                                  "queued, nothing plays until start()")
                       .arg(m_blocksQueuedWhileStopped);
        }
    }

    QueuedBlock block;
    block.pcm = pcm;
    block.durationMs = pcmDurationMs(pcm.size(), targetFormat);

    // Hard cap on ONE block. Silence on a runaway source is worse than a cut
    // tail: the block is accepted up to kHardBlockCapMs (frame-aligned) and the
    // trim is reported — never silent, never unbounded memory.
    if (block.durationMs > double(kHardBlockCapMs)) {
        const qsizetype keep = pcmBytesForMs(kHardBlockCapMs, targetFormat);
        if (keep > 0 && keep < block.pcm.size()) {
            const double originalMs = block.durationMs;
            block.pcm.truncate(keep);
            block.durationMs = pcmDurationMs(block.pcm.size(), targetFormat);
            logTrim(originalMs);
        }
    }

    m_queue.append(block);

    // Backpressure lives HERE, and it bounds ACCUMULATION, never one sentence:
    //  - with a single queued block (the one just appended) nothing is dropped,
    //    however long it is — a 2.4–2.6 s TTS sentence under the old 2000 ms
    //    default was dropped while it was alone in the queue, i.e. every
    //    English reply was silence and the sink was never even opened;
    //  - once a second block waits, `queueMs() > maxQueuedMs` means the
    //    producer is faster than the device: the OLDEST block goes — unless its
    //    head already reached the device. Cutting a partially played block is
    //    an audible jump inside a sentence, so the SECOND block gives up its
    //    place instead; if only the partially played block is left, the
    //    single-block rule above applies and nothing is dropped.
    while (m_queue.size() > 1 && queueMs() > m_maxQueuedMs) {
        const int dropIndex = m_queue.first().written > 0 ? 1 : 0;
        const QueuedBlock dropped = m_queue.takeAt(dropIndex);
        ++m_blocksDropped;
        logDrop(dropIndex == 0 ? QStringLiteral("queue over maxQueuedMs")
                                : QStringLiteral("queue over maxQueuedMs (head already "
                                                 "partially played — kept to avoid a jump)"),
                dropped.pcm.size() - dropped.written);
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
    // appears. Nothing is opened here, and the device comes from the cache.
    return AudioFormatBridge::resolveFormat(descriptor, resolvedDevice()).format;
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
        // Cheap re-resolve (memo hit unless it went stale): a device that was
        // missing or replaced is picked up on the next attempt without an
        // enumeration on this path.
        invalidateDevice();
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
        m_sinkReceivedData = true;   // the drain detector needs to know we fed it
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
    // The resampler's phase describes the audio that was just dropped.
    resetConversionState();
}

void AudioPlaybackEngine::resetConversionState()
{
    m_rateConverter.reset();
}

bool AudioPlaybackEngine::createSink(const SampledStreamDescriptor &descriptor)
{
    const AudioFormatBridge::PlaybackFormat resolved =
        AudioFormatBridge::resolveFormat(descriptor, resolvedDevice());

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
        new AudioCompat::AudioOutput(resolvedDevice(), resolved.format));
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
    m_lastSinkError = QAudio::NoError;
    m_sinkReceivedData = false;
    ++m_sinksCreated;

    // A new sink starts a new stream: the phase of the resampler belongs to the
    // device that just died (AC9).
    resetConversionState();

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
    m_lastSinkError = QAudio::NoError;
    m_sinkReceivedData = false;
    // The queue was converted for the sink that just died: never hand it to a
    // new device, which may (and usually does) have another format.
    clearQueue();
    updatePumpTimer();
}

void AudioPlaybackEngine::onSinkStateChanged(QAudio::State state)
{
    if (m_sink != nullptr) {
        const QAudio::Error error = m_sink->error();
        // A drain is not the only thing the device reports, and the two backends
        // spell it differently: Qt5 latches `UnderrunError`, the Qt6 sink comes
        // back with `IdleState` + `NoError`. Both mean the same thing here — the
        // device played everything it had and went silent — and the old code
        // looked only at StoppedState, so neither was reported: the status said
        // "playing" while the sound broke up (AC10).
        //
        // Counted on the transition, never per notification: a device that stays
        // dry would otherwise add thousands.
        const bool latchedUnderrun = (error == QAudio::UnderrunError
                                      && m_lastSinkError != QAudio::UnderrunError);
        const bool drained = (state == QAudio::IdleState && error != QAudio::UnderrunError
                              && m_sinkReceivedData);
        if (latchedUnderrun || drained) {
            ++m_underruns;
            logUnderrun();
        }
        m_lastSinkError = error;

        if (state == QAudio::StoppedState && error != QAudio::NoError) {
            // The one place where the device list is suspect: the sink died, so
            // the device may have been pulled out (or a headset was plugged in).
            // This is the only path that pays for a real re-enumeration.
            refreshDeviceList();
            reportError(audioErrorText(error));
            return;
        }
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
        // Never-started is a *state*, not silence (see push()). Deliberately
        // WITHOUT the live count: the status repaints the widget, and a live
        // source delivers ~100 blocks/s — the exact number is in the one-off
        // warning and in `blocksQueuedWhileStopped()`.
        status = m_blocksQueuedWhileStopped > 0
                     ? QStringLiteral("stopped · audio is queued, start to play")
                     : QStringLiteral("stopped");
    } else if (!deviceAvailable()) {
        status = QStringLiteral("no output device");
    } else if (m_sink != nullptr) {
        status = QStringLiteral("%1 · %2")
                     .arg(m_sink->state() == QAudio::ActiveState
                              ? QStringLiteral("playing")
                              : QStringLiteral("ready"),
                          streamDescription());
        // Only when it happened: a healthy stream must not grow a "0 underrun(s)"
        // suffix. The count is a hint that blocks are not keeping up, not an error.
        if (m_underruns > 0)
            status += QStringLiteral(" · %1 underrun(s)").arg(m_underruns);
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

void AudioPlaybackEngine::logTrim(double originalMs)
{
    ++m_trimLogCount;
    // Same throttling as logDrop(): a source that only emits 20 s blocks is a
    // defect worth seeing once per run, not a line per block.
    if (m_trimLogCount == 1 || (m_trimLogCount % 100) == 0) {
        qCWarning(lcDemoNodes).noquote()
            << QStringLiteral("AudioPlaybackEngine: block of %1 ms exceeds the hard "
                              "%2 ms per-block cap — the tail was cut (%3 trimmed in "
                              "total)")
                   .arg(originalMs, 0, 'f', 0)
                   .arg(kHardBlockCapMs)
                   .arg(m_trimLogCount);
    }
}

void AudioPlaybackEngine::logUnderrun()
{
    ++m_underrunLogCount;
    // Same throttling as logDrop(): a device that stays dry reports an underrun
    // on every state change (~100 per second with live blocks), so only the
    // first one and every hundredth reach the log. The counter in the status
    // line shows that the situation continues.
    if (m_underrunLogCount == 1 || (m_underrunLogCount % 100) == 0) {
        qCWarning(lcDemoNodes).noquote()
            << QStringLiteral("AudioPlaybackEngine: device underrun — the sink ran dry "
                              "(%1 underrun(s) in total, %2 ms queued)")
                   .arg(m_underruns)
                   .arg(queueMs());
    }
}
