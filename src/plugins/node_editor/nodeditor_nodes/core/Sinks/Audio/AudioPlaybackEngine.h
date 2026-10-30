#ifndef AUDIOPLAYBACKENGINE_H
#define AUDIOPLAYBACKENGINE_H

#include "AudioCompat.h"
#include "NodeDataTypes/SampledData.h"

#include <QAudioFormat>
#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

#include <memory>

class QIODevice;

/**
 * @brief Owns the one long-lived audio output sink of the Audio Output node
 * (REQ-SW-PL-076).
 *
 * The node model is a thin controller (the `LlamaGenerateModel` → `LlamaEngine`
 * pattern): `AudioOutputNode` only keeps the config, `save()`/`load()` and the
 * port wiring; this engine keeps the device, the format, the sink and the
 * playback queue.
 *
 * One sink for the whole life of the node — never one per block:
 *   - created lazily, on the first block that actually has to be played
 *     (a graph that is only edited never opens a device — AC2/AC7), or eagerly
 *     from `start()` when the input format is already known;
 *   - destroyed by `stop()` and re-created when the device, the buffer size or
 *     the input format changes (only for a real reason).
 *
 * Block path (`push()`) — the block size is NEVER the limit:
 *   1. `AudioFormatBridge::resolveFormat()` — ask the device whether it accepts
 *      the descriptor's format; on rejection fall back to `preferredFormat()`
 *      and resample/convert (AC4, never silent). While no sink is open yet the
 *      same resolver yields the provisional format the queue is converted to;
 *   2. `AudioFormatBridge::convert()` — one block into that format;
 *   3. the converted block is appended to a **queue bounded by time**
 *      (`maxQueuedMs`, default 2000 ms — not by the device buffer and not by
 *      the block size). A TTS sentence arrives as ONE block of ~2 s; it used to
 *      be dropped whenever `bytesFree() < size`, i.e. always with
 *      `bufferMs: 200` and even at the 2000 ms maximum. Now it is accepted and
 *      handed to the device in chunks of whatever `bytesFree()` allows.
 *
 * Draining happens on `push()`, on the sink's `stateChanged` and on a 20 ms
 * timer that runs only while the queue is non-empty **and** a device exists
 * (it is stopped when the queue empties — nothing spins forever).
 *
 * `blocksDropped()` is now only the queue bound: when the queued audio would
 * exceed `maxQueuedMs` the OLDEST block is dropped (throttled log). `bufferMs`
 * stays what it always was — the size of the device buffer the sink is created
 * with — and is not a limit of a block.
 *
 * Headless / no sound card (AC5): `defaultOutputDevice()` is null or the output
 * list is empty → nothing is opened; blocks are still accepted into the bounded
 * queue (so audio that arrives before the device/start is not lost) and the
 * status says why. Nothing throws and nothing blocks.
 *
 * Contains no `QWidget`: the widget lives in `nodeditor_nodes/gui`
 * (`check_core_widgets_free`).
 */
class AudioPlaybackEngine : public QObject
{
    Q_OBJECT

public:
    /// Default of `maxQueuedMs` — how much audio may wait for the device.
    static constexpr int kDefaultMaxQueuedMs = 2000;
    /// Bounds of `maxQueuedMs` (the queue bound is time, see push()).
    static constexpr int kMinMaxQueuedMs = 200;
    static constexpr int kMaxMaxQueuedMs = 10000;

    explicit AudioPlaybackEngine(QObject *parent = nullptr);
    ~AudioPlaybackEngine() override;

    /**
     * @brief Apply the node configuration.
     *
     * Volume is applied to the live sink immediately. A device or buffer-size
     * change destroys the sink so the next block re-creates it with the new
     * settings — and clears the queue: PCM converted for the old device must
     * not be pushed into a new one with another format (documented, not silent).
     * Disabling stops and destroys the sink.
     *
     * @param deviceId   Persisted `AudioCompat::deviceId()`; empty = system default
     * @param volume     Linear volume, clamped to [0, 1]
     * @param bufferMs   Sink buffer in milliseconds, clamped to [20, 2000].
     *                   The device buffer only — never a limit on a block.
     * @param enabled    false = disabled: no sink, blocks are skipped
     * @param maxQueuedMs Playback queue bound in milliseconds, clamped to
     *                   [200, 10000]; audio over this bound is dropped
     *                   oldest-first (one TTS sentence is one block)
     */
    void setConfig(const QString &deviceId, double volume, int bufferMs, bool enabled,
                   int maxQueuedMs = kDefaultMaxQueuedMs);

    /// Arm playback. Opens the device only when the input format is already
    /// known (a block was seen before); otherwise the first block does it.
    /// Audio queued while the node was stopped is played now.
    void start();

    /// Stop playback, destroy the sink and clear the queue. Idempotent
    /// (REQ-SW-PL-050).
    void stop();

    /// True while the sink is actually consuming audio (ActiveState).
    bool isPlaying() const;

    /// True between `start()` and `stop()`.
    bool isRunning() const { return m_running; }

    /// Human-readable state for the node widget ("playing · …", "no output
    /// device", "error: …"). Never empty.
    QString statusText() const { return m_status; }

    /// Name of the device the sink would use (empty when none).
    QString deviceLabel() const;

    /// Format the sink is open with, or an invalid format when no sink exists.
    QAudioFormat format() const { return m_format; }

    /// Queue one block. Non-blocking and never throws: the block is converted
    /// and appended to the time-bounded queue, then as much as the device
    /// accepts right now is written. A block larger than the device buffer is
    /// the normal case (one TTS sentence), not an error.
    void push(const SampledData &data);

    // ── Diagnostics (tests, status line) ───────────────────────────────────
    /// Blocks accepted into the queue (not necessarily finished playing).
    quint64 blocksPlayed() const { return m_blocksPlayed; }
    /// Blocks discarded because the queue was over `maxQueuedMs` (oldest first).
    quint64 blocksDropped() const { return m_blocksDropped; }
    /// Blocks that could not even be converted or queued (disabled, bad format).
    quint64 blocksSkipped() const { return m_blocksSkipped; }
    /// How many times a sink was actually opened. The node's whole point is
    /// "one sink for many blocks", so re-creation is observable here rather
    /// than only through a stutter in the sound.
    quint64 sinksCreated() const { return m_sinksCreated; }
    bool deviceAvailable() const;

    /// Converted PCM bytes still waiting for the device (not yet written).
    qsizetype bytesQueued() const;

    /// How much audio is still waiting for the device, in milliseconds
    /// (partially written blocks count with their remaining share).
    int queueMs() const;

signals:
    /// Emitted only when the text actually changes.
    void statusChanged(const QString &status);

private:
    /// One converted block waiting for (the rest of) its turn at the device.
    struct QueuedBlock {
        QByteArray pcm;        ///< PCM in the current sink/queue format
        qsizetype written = 0; ///< bytes already handed to the device
        double durationMs = 0.0; ///< full block duration, for the queue bound
    };

    /// Build the sink for @p descriptor on @p device. Sets the status on failure
    /// (never a silent no-op) and returns false.
    bool createSink(const SampledStreamDescriptor &descriptor);

    /// Stop and drop the sink (and the queue in the old format). Does not touch
    /// `m_running`.
    void destroySink();

    /// Drop everything still waiting for the device.
    void clearQueue();

    /// Feed as much of the queue to the device as `bytesFree()` allows. Opens
    /// the sink (throttled) when it is missing.
    void pump();

    /// Write queued PCM in chunks. @return true while data is still waiting.
    bool writeQueued();

    /// Run the 20 ms pump timer only while there is something to drain.
    void updatePumpTimer();

    /// Format the queue is converted to while no sink is open yet: what
    /// `createSink()` would pick. Never opens anything.
    QAudioFormat provisionalFormat(const SampledStreamDescriptor &descriptor) const;

    /// Recompute the status text, emit when it changed.
    void refreshStatus();

    /// Full error status: "error: <what>".
    void reportError(const QString &what);

    void onSinkStateChanged(QAudio::State state);

    /// "<device> · <rate> Hz <layout>" for the status line.
    QString streamDescription() const;

    /// Log a dropped block, rate-limited so a stalled device cannot flood the log.
    void logDrop(const QString &reason, qsizetype bytes);

    QAudioDeviceInfo m_device;

    QString m_deviceId;
    double m_volume = 1.0;
    int m_bufferMs = 200;
    int m_maxQueuedMs = kDefaultMaxQueuedMs;
    bool m_enabled = true;

    std::unique_ptr<AudioCompat::AudioOutput> m_sink;
    /// Push-mode write end, owned by the sink (never deleted here). Qt5 and Qt6
    /// both hand it out from `start()` and have no `write()` on the sink itself.
    QIODevice *m_ioDevice = nullptr;
    QAudioFormat m_format;
    /// True when the block is resampled/mixed/encoded on the way out (AC4).
    bool m_converted = false;
    /// Why the format was chosen — logged when the sink opens, so a fallback to
    /// `preferredFormat()` is never invisible.
    QString m_formatReason;
    /// Set when the last operation failed; cleared by a successful sink start.
    QString m_error;

    bool m_running = false;
    QString m_status;

    /// Blocks waiting for the device, oldest first. Bounded by `maxQueuedMs`.
    QVector<QueuedBlock> m_queue;
    /// Format every queued block is encoded in (invalid while the queue is empty).
    QAudioFormat m_queueFormat;
    /// Guards against re-entrant pumping from a synchronous `stateChanged`.
    bool m_pumping = false;
    /// Drains the queue while the device buffer frees up. Stopped when empty.
    QTimer m_pumpTimer;

    /// Format of the last block seen, even while stopped (AC7: remembering it
    /// opens nothing). `start()` uses it to open the device eagerly.
    SampledStreamDescriptor m_lastDescriptor;

    /// Throttles repeated open attempts after a failure (kCreateRetryMs).
    QElapsedTimer m_createThrottle;

    quint64 m_blocksPlayed = 0;
    quint64 m_blocksDropped = 0;
    quint64 m_blocksSkipped = 0;
    quint64 m_sinksCreated = 0;
    quint64 m_dropLogCount = 0;
};

#endif // AUDIOPLAYBACKENGINE_H
