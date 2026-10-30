#ifndef AUDIOPLAYBACKENGINE_H
#define AUDIOPLAYBACKENGINE_H

#include "AudioCompat.h"
#include "AudioResample.h"
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
 *      the descriptor's format; then try the SAME rate and channels with
 *      another encoding (F32/S16/S32), which needs no resampling at all; only
 *      if the device refuses the rate as well fall back to `preferredFormat()`
 *      + resampling (AC4, never silent). While no sink is open yet the same
 *      resolver yields the provisional format the queue is converted to;
 *   2. `AudioFormatBridge::convert()` — one block into that format, resampled
 *      through this engine's `RateConverter` (state carried ACROSS blocks: a
 *      stateless per-block resampler clicks at every block boundary, see the
 *      class comment of `AudioResample::RateConverter`);
 *   3. the converted block is appended to a **queue bounded by time**
 *      (`maxQueuedMs`, default 3000 ms — not by the device buffer and not by
 *      the block size). A TTS sentence arrives as ONE block of 2.4–2.6 s
 *      (measured); it used to be dropped whenever `bytesFree() < size`, i.e.
 *      always with `bufferMs: 200`, and the time bound then dropped it again
 *      because one sentence was longer than the 2000 ms default. Now it is
 *      accepted and handed to the device in chunks of whatever `bytesFree()`
 *      allows.
 *
 * Draining happens on `push()`, on the sink's `stateChanged` and on a 20 ms
 * timer that runs only while the queue is non-empty **and** a device exists
 * (it is stopped when the queue empties — nothing spins forever).
 *
 * `maxQueuedMs` guards against **accumulation**, not against one sentence:
 *   - a queue holding only the block that was just appended never drops it,
 *     however long it is, up to the hard per-block cap `kHardBlockCapMs`
 *     (20 s). A longer block is trimmed to the cap and reported in the log —
 *     silence would hide a runaway source, a trimmed tail is audible as the
 *     defect it is;
 *   - with two or more queued blocks, `queueMs() > maxQueuedMs` drops the
 *     OLDEST block (throttled log) — the producer is faster than the device,
 *     which is exactly what the bound is for. **A partially written block is
 *     never dropped**: cutting a block whose head already went to the device
 *     is an audible jump inside a sentence. The next block gives up its place
 *     instead, and a lone (partially written) block is never dropped at all.
 *
 * `bufferMs` stays what it always was — the size of the device buffer the sink
 * is created with — and is not a limit of a block.
 *
 * Device resolution is CACHED (`m_device`, `m_deviceResolvedForId`): on Qt5
 * one `QAudioDeviceInfo::availableDevices()` call costs 2.4–2.8 s on a machine
 * with 71 ALSA devices, and the status/pump path used to resolve the device up
 * to four times per block plus once per 20 ms tick (313 s for the audio output
 * test suite, 0.7 s on Qt6). Enumeration now happens only when the configured
 * id changes, when the node is (re)started after the process memo went stale,
 * or when the sink itself reports an error.
 *
 * Headless / no sound card (AC5): `defaultOutputDevice()` is null or the output
 * list is empty → nothing is opened; blocks are still accepted into the bounded
 * queue (so audio that arrives before the device/start is not lost) and the
 * status says why. Nothing throws and nothing blocks.
 *
 * **Playback arms itself** (REQ-SW-PL-076, defect seen in a real run): the IDE
 * never calls `start()` for an `IStartable` node, so a sink that waited for an
 * explicit start was mute no matter what the `.flow` contained — and that is
 * what forced a Start button into the widget. `AudioOutputNode::setInData()`
 * now arms the engine on the first block it receives, the rule `AudioSinkNode`
 * has always followed. An explicit `stop()` is still respected: it sets
 * `userStopped()`, which suppresses auto-arm until the next explicit `start()`
 * — Stop means "stop and stay stopped", not "start by hand".
 *
 * Blocks accepted while the node is **explicitly stopped** are queued, not
 * dropped, so a stopped sink is never mistaken for "no data": the first such
 * block logs one `qCWarning` (`AudioOutput was stopped explicitly — N block(s)
 * queued, nothing plays until start()`), every one of them increments
 * `blocksQueuedWhileStopped()` (reset by `start()`), and the status line says
 * `stopped · audio is queued, start to play`. Never one line per block: a live
 * source delivers ~100 blocks/s.
 *
 * Contains no `QWidget`: the widget lives in `nodeditor_nodes/gui`
 * (`check_core_widgets_free`).
 */
class AudioPlaybackEngine : public QObject
{
    Q_OBJECT

public:
    /// Default of `maxQueuedMs` — how much audio may wait for the device.
    /// Measured English TTS sentence: 2.4–2.6 s (236 800 / 249 600 B @ 24 kHz
    /// mono f32), so the old 2000 ms default dropped every real sentence.
    static constexpr int kDefaultMaxQueuedMs = 3000;
    /// Bounds of `maxQueuedMs` (the queue bound is time, see push()).
    static constexpr int kMinMaxQueuedMs = 500;
    static constexpr int kMaxMaxQueuedMs = 10000;
    /// Hard cap on ONE block, independent of `maxQueuedMs`: one sentence must
    /// never be dropped for being long, but a runaway source must not make the
    /// queue unbounded either. A longer block is trimmed to the cap and logged.
    ///
    /// **Why 20 s** (was 10 s, REQ-SW-PL-076 defect): a VAD segment arrives as
    /// ONE block and `VadNode`'s default `maxSpeechMs` is 15 000 ms, so the old
    /// 10 s cap cut the tail of every long utterance — audible, and at the same
    /// time invisible in the status. 20 s sits above the VAD ceiling with 5 s of
    /// reserve (a segment can be emitted a silence window late) and still only
    /// fires on a runaway source: it is ~8× a measured TTS sentence (2.4–2.6 s).
    static constexpr int kHardBlockCapMs = 20000;

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
     * @param maxQueuedMs Queue bound in milliseconds, clamped to
     *                   [500, 10000]; with more than one queued block, audio
     *                   over this bound is dropped oldest-first. A single
     *                   block is never dropped for being long (one TTS
     *                   sentence is one block)
     */
    void setConfig(const QString &deviceId, double volume, int bufferMs, bool enabled,
                   int maxQueuedMs = kDefaultMaxQueuedMs);

    /// Arm playback. Opens the device only when the input format is already
    /// known (a block was seen before); otherwise the first block does it.
    /// Audio queued while the node was stopped is played now.
    /// This is the one call that clears `userStopped()`: after an explicit
    /// `stop()` only an explicit start (or the node's Start button) re-arms.
    void start();

    /// Stop playback, destroy the sink and clear the queue. Idempotent
    /// (REQ-SW-PL-050). Marks the stop as **explicit** (`userStopped()`): the
    /// node's auto-arm then leaves the engine alone until the next `start()`.
    void stop();

    /// True while the sink is actually consuming audio (ActiveState).
    bool isPlaying() const;

    /// True between `start()` and `stop()`.
    bool isRunning() const { return m_running; }

    /// True after an explicit `stop()` and before the next `start()`: the
    /// auto-arm in `AudioOutputNode::setInData()` is skipped, so "Stop" means
    /// "stop and stay stopped". Cleared by `start()`.
    bool userStopped() const { return m_userStopped; }

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
    /// Blocks discarded because the queue was over `maxQueuedMs` while more
    /// than one block was waiting (oldest first).
    quint64 blocksDropped() const { return m_blocksDropped; }
    /// Blocks that could not even be converted or queued (disabled, bad format).
    quint64 blocksSkipped() const { return m_blocksSkipped; }
    /// Device underruns seen since the last `start()` — the sink ran dry after
    /// having played data. Qt5 latches `QAudio::UnderrunError` for it, the Qt6
    /// sink reports `IdleState` + `NoError`; in push mode neither is
    /// StoppedState, so before AC10 the status showed a healthy "playing" line
    /// while the sound clicked. Reset per run, like the other counters, and
    /// also shown in `statusText()`.
    quint64 underruns() const { return m_underruns; }
    /// Blocks accepted while the node was **not started** (queued, nothing
    /// written) — a subset of `blocksPlayed()`. A stopped node used to be
    /// indistinguishable from "no data" in headless; the counter and the
    /// one-off warning in `push()` make the difference visible
    /// (REQ-SW-PL-076).
    quint64 blocksQueuedWhileStopped() const { return m_blocksQueuedWhileStopped; }
    /// How many times a sink was actually opened. The node's whole point is
    /// "one sink for many blocks", so re-creation is observable here rather
    /// than only through a stutter in the sound.
    quint64 sinksCreated() const { return m_sinksCreated; }
    /// How many times the configured device was actually resolved (i.e. the
    /// device list was enumerated). The hot path must never change this: on Qt5
    /// one resolution costs 2.4–2.8 s with 71 ALSA devices.
    quint64 deviceResolutions() const { return m_deviceResolutions; }
    /// True when the configured device resolves to a real output device.
    /// Served from the cache — the device list is never enumerated here.
    bool deviceAvailable() const;

    /// Converted PCM bytes still waiting for the device (not yet written).
    qsizetype bytesQueued() const;

    /// How much audio is still waiting for the device, in milliseconds
    /// (partially written blocks count with their remaining share).
    int queueMs() const;

signals:
    /// Emitted only when the text actually changes.
    void statusChanged(const QString &status);
    /// Emitted when `isRunning()` changes — start()/stop(), including the
    /// automatic arming from the first block. A widget cannot infer this from
    /// `statusChanged()`: the status text also carries errors, and a stopped
    /// engine with an error keeps the same text when it starts.
    void runningChanged(bool running);

private:
    /// One converted block waiting for (the rest of) its turn at the device.
    struct QueuedBlock {
        QByteArray pcm;        ///< PCM in the current sink/queue format
        qsizetype written = 0; ///< bytes already handed to the device
        double durationMs = 0.0; ///< full block duration, for the queue bound
    };

    /// Build the sink for @p descriptor on the resolved device. Sets the status
    /// on failure (never a silent no-op) and returns false.
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

    /// Log a block trimmed by `kHardBlockCapMs`, rate-limited like logDrop().
    void logTrim(double originalMs);

    /// Log a device underrun, rate-limited like logDrop() (a device that stays
    /// dry reports one on every state change, i.e. ~100×/s).
    void logUnderrun();

    /// Reset everything that describes the audio already handed to the device:
    /// the resampler phase and the queue format. Called whenever that audio is
    /// discarded (sink re-created, output format changed) — a carried phase
    /// would belong to sound nobody will hear, and the next block would be
    /// offset by up to one input sample (REQ-SW-PL-076 AC9).
    void resetConversionState();

    /// The configured device, resolved ONCE per `deviceId` (and re-resolved
    /// only after invalidateDevice()/refreshDeviceList()). Never enumerates on
    /// its own beyond the process-wide memo — see the class comment.
    const QAudioDeviceInfo &resolvedDevice() const;

    /// Drop the engine's cached resolution. Cheap: the next resolve normally
    /// hits the process-wide memo (used when a node is (re)started).
    void invalidateDevice();

    /// Drop the cached resolution AND the process-wide memo — the device list
    /// itself is suspect (the sink reported an error, a device may have been
    /// plugged in or pulled out).
    void refreshDeviceList();

    /// Cached device; `m_deviceResolvedForId` records which id it belongs to.
    mutable QAudioDeviceInfo m_device;
    mutable QString m_deviceResolvedForId;
    mutable bool m_deviceIsResolved = false;
    /// Counts real resolutions (device-list enumerations) for the regression
    /// test that pins the Qt5 hot-path fix — not reset by start().
    mutable quint64 m_deviceResolutions = 0;

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
    /// Streaming resampler state (phase, last sample and accumulated count, per
    /// channel). Owned here because the sink is: a block boundary must continue
    /// the previous block's phase, and that phase must be dropped when the audio
    /// it refers to is dropped (`resetConversionState()`).
    AudioResample::RateConverter m_rateConverter;
    /// Set by `stop()`, cleared by `start()`: the difference between "nothing
    /// started me yet" (auto-arm may do it) and "the user stopped me" (only an
    /// explicit start() may resume).
    bool m_userStopped = false;
    QString m_status;

    /// Blocks waiting for the device, oldest first. Bounded by `maxQueuedMs`
    /// when two or more blocks wait; a single block is bounded by
    /// `kHardBlockCapMs` only.
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
    /// Blocks accepted while `m_running` was false (queued, not played).
    /// Counted separately from `m_blocksPlayed` so "nothing plays because the
    /// node was never started" has a number in the diagnostics.
    quint64 m_blocksQueuedWhileStopped = 0;
    /// True once the "not started" warning was logged for the current stop
    /// period — a stopped node receives ~100 blocks/s and must not flood the
    /// log with one line per block.
    bool m_stoppedWarningLogged = false;
    quint64 m_sinksCreated = 0;
    quint64 m_dropLogCount = 0;
    quint64 m_trimLogCount = 0;
    /// Device underruns since the last `start()`, and the throttling counter for
    /// their log line. `m_lastSinkError` keeps a dry device from being counted
    /// once per state change while the same `UnderrunError` is still latched.
    quint64 m_underruns = 0;
    quint64 m_underrunLogCount = 0;
    QAudio::Error m_lastSinkError = QAudio::NoError;
    /// True once audio was actually written to the CURRENT sink. A drain is only
    /// a drain if the device had something to play — the IdleState a sink emits
    /// right after `start()` is not an underrun.
    bool m_sinkReceivedData = false;
};

#endif // AUDIOPLAYBACKENGINE_H
