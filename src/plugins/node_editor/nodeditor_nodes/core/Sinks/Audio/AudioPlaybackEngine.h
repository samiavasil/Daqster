#ifndef AUDIOPLAYBACKENGINE_H
#define AUDIOPLAYBACKENGINE_H

#include "AudioCompat.h"
#include "NodeDataTypes/SampledData.h"

#include <QAudioFormat>
#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include <memory>

class QIODevice;

/**
 * @brief Owns the one long-lived audio output sink of the Audio Output node
 * (REQ-SW-PL-076).
 *
 * The node model is a thin controller (the `LlamaGenerateModel` → `LlamaEngine`
 * pattern): `AudioOutputNode` only keeps the config, `save()`/`load()` and the
 * port wiring; this engine keeps the device, the format and the sink.
 *
 * One sink for the whole life of the node — never one per block:
 *   - created lazily, on the first block that actually has to be played
 *     (a graph that is only edited never opens a device — AC2/AC7), or eagerly
 *     from `start()` when the input format is already known;
 *   - destroyed by `stop()` and re-created when the device, the buffer size or
 *     the input format changes (only for a real reason).
 *
 * Block path (`push()`):
 *   1. `AudioFormatBridge::resolveFormat()` — ask the device whether it accepts
 *      the descriptor's format; on rejection fall back to `preferredFormat()`
 *      and resample/convert (AC4, never silent);
 *   2. `AudioFormatBridge::convert()` — one block into the sink's format;
 *   3. backpressure — `bytesFree()` is checked before every write, so a slow or
 *      blocked device drops blocks instead of growing an unbounded buffer or
 *      blocking the graph. The queue is bounded by the sink's own buffer
 *      (`bufferMs`, max 2000 ms).
 *
 * Headless / no sound card (AC5): `defaultOutputDevice()` is null or the output
 * list is empty → blocks are counted and the status says so. Nothing throws,
 * nothing blocks, no device is opened.
 *
 * Contains no `QWidget`: the widget lives in `nodeditor_nodes/gui`
 * (`check_core_widgets_free`).
 */
class AudioPlaybackEngine : public QObject
{
    Q_OBJECT

public:
    explicit AudioPlaybackEngine(QObject *parent = nullptr);
    ~AudioPlaybackEngine() override;

    /**
     * @brief Apply the node configuration.
     *
     * Volume is applied to the live sink immediately. A device or buffer-size
     * change destroys the sink so the next block re-creates it with the new
     * settings (the block that triggered the change is not lost — it is the one
     * that causes the re-creation). Disabling stops and destroys the sink.
     *
     * @param deviceId Persisted `AudioCompat::deviceId()`; empty = system default
     * @param volume   Linear volume, clamped to [0, 1]
     * @param bufferMs Sink buffer in milliseconds, clamped to [20, 2000]
     * @param enabled  false = disabled: no sink, blocks are skipped
     */
    void setConfig(const QString &deviceId, double volume, int bufferMs, bool enabled);

    /// Arm playback. Opens the device only when the input format is already
    /// known (a block was seen before); otherwise the first block does it.
    void start();

    /// Stop playback and destroy the sink. Idempotent (REQ-SW-PL-050).
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

    /// Play one block. Non-blocking: never grows a queue and never throws.
    void push(const SampledData &data);

    // ── Diagnostics (tests, status line) ───────────────────────────────────
    quint64 blocksPlayed() const { return m_blocksPlayed; }
    quint64 blocksDropped() const { return m_blocksDropped; }
    quint64 blocksSkipped() const { return m_blocksSkipped; }
    /// How many times a sink was actually opened. The node's whole point is
    /// "one sink for many blocks", so re-creation is observable here rather
    /// than only through a stutter in the sound.
    quint64 sinksCreated() const { return m_sinksCreated; }
    bool deviceAvailable() const;

signals:
    /// Emitted only when the text actually changes.
    void statusChanged(const QString &status);

private:
    /// Build the sink for @p descriptor on @p device. Sets the status on failure
    /// (never a silent no-op) and returns false.
    bool createSink(const SampledStreamDescriptor &descriptor);

    /// Stop and drop the sink. Does not touch `m_running`.
    void destroySink();

    /// Recompute the status text, emit when it changed.
    void refreshStatus();

    /// Full error status: "грешка: <what>".
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
