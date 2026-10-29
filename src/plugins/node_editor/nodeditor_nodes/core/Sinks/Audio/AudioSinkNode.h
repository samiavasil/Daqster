#ifndef AUDIOSINKNODE_H
#define AUDIOSINKNODE_H

#include "nodeditor_nodes_core_export.h"

#include "AudioPlaybackEngine.h"
#include "capabilities/INodeConfigSchema.h"
#include "shared/IStartable.h"
#include "shared/IStoppable.h"

#include <QtNodes/NodeDelegateModel>

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <memory>

class QFile;

/**
 * @brief Audio to 16-bit PCM WAV recorder (REQ-SW-PL-077).
 *
 * Moved out of the private AI Studio plugin: the node never used a single AI
 * type, only `SampledData`, `QFile` and (for the legacy `playbackEnabled`
 * field) the audio device. Every graph that wants the audio it just produced
 * on disk used to have to load the whole AI plugin for it; now it is a public
 * `Sinks/Audio` node next to `AudioOutputNode`.
 *
 * Contract kept from the private node, on purpose:
 *   - `name()` is **still** `"AudioSinkNode"` — that string is the registration
 *     key in a `.flow` file (REQ-SW-PL-057), so existing scenes load without a
 *     migration;
 *   - `save()` emits the **same keys**: `model-name`, `outputPath`,
 *     `playbackEnabled`, `volume`.
 *
 * Recording (`outputPath` non-empty) appends every block to one file
 * (REQ-SW-PL-077 AC4). The private node opened the file with
 * `QIODevice::Truncate` on *every* block and rewrote the header, so two or more
 * blocks left only the last one on disk. Here:
 *   - the file is truncated **once**, on the first block of a recording (an
 *     existing file at start is a new recording, never appended to);
 *   - the 44-byte canonical WAV header is written first with zero sizes and the
 *     two size fields (`RIFF.ChunkSize`, `data.Subchunk2Size`) are patched in
 *     place after every append — so the file on disk is a valid WAV at any
 *     moment, not only after `stop()`;
 *   - `stop()` patches once more and closes the handle.
 *
 * The input is always mixed down to **mono** (`SampledData::decodeToNormalizedF32`
 * + average of the channels) and written as 16-bit PCM, which is the one format
 * every reader (and `whisper-cli` in the private pipeline) accepts. A block whose
 * sample rate changes mid-recording cannot share one WAV header: the recording
 * restarts instead of writing samples under a header that lies about them
 * (`lastError()` says so).
 *
 * `playbackEnabled` is kept for `.flow` compatibility but is **deprecated**: it
 * delegates to a persistent `AudioPlaybackEngine` (the same one `AudioOutputNode`
 * uses) instead of creating a `QAudioSink` per block, so an old scene that set it
 * still makes a sound. New graphs should wire an `AudioOutput` node instead.
 *
 * The GUI widget (`AudioSinkWidget`) lives in `nodeditor_nodes/gui` and is
 * attached through `NodeWidgetFactory`; `embeddedWidget()` always returns
 * nullptr and this file includes no QtWidgets (`check_core_widgets_free`).
 */
class NODEDITOR_NODES_CORE_EXPORT AudioSinkNode : public QtNodes::NodeDelegateModel,
                                                  public Daqster::IStartable,
                                                  public Daqster::IStoppable,
                                                  public Daqster::INodeConfigSchema
{
    Q_OBJECT

public:
    /// Persisted configuration (REQ-SW-PL-077 AC2). `outputPath` is empty by
    /// default: the node never invents a path that does not exist on the next
    /// machine.
    struct Config {
        QString outputPath;                  ///< WAV path; empty = no recording
        bool    playbackEnabled = false;      ///< deprecated playback delegation
        double  volume = 1.0;                ///< linear, 0..1
    };

    AudioSinkNode();
    ~AudioSinkNode() override;

    QString caption() const override
    { return QStringLiteral("Audio sink"); }

    /// Same convention as `AudioOutputNode`: the caption is not drawn on the
    /// body — the node title is the type name.
    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("AudioSinkNode"); }

    /// Writing a file produces no sound on screen and changes no port shape —
    /// both opt-outs keep a 100 blocks/s stream from repainting the scene.
    bool dataArrivalChangesGeometry() const override { return false; }
    bool dataArrivalChangesWidget() const override { return false; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    /// Draft-7 fragment describing exactly the object `save()` emits (AC2):
    /// `outputPath`, `playbackEnabled`, `volume`.
    QJsonObject configSchema() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    /// Always nullptr — the widget is supplied by the GUI plugin.
    QWidget *embeddedWidget() override { return nullptr; }

    /// Arm the (deprecated) playback delegation; recording needs no arming —
    /// it starts with the first block.
    void start() override;

    /// Finalize the WAV (sizes patched, handle closed) and stop playback.
    /// Idempotent (REQ-SW-PL-050).
    void stop() override;

    // ── Config (used by the GUI widget) ────────────────────────────────────
    const Config &config() const { return m_config; }

    /// Apply and persist a new configuration. `volume` is clamped to 0..1 —
    /// the invariant the audio device expects.
    void setConfig(const Config &config);

    QString outputPath() const { return m_config.outputPath; }
    bool playbackEnabled() const { return m_config.playbackEnabled; }
    double volume() const { return m_config.volume; }

    // ── Thin setters, kept as wrappers over Config ─────────────────────────
    void setOutputPath(const QString &path);
    void setPlaybackEnabled(bool on);
    void setVolume(qreal volume);

    // ── Diagnostics (tests, status line) ──────────────────────────────────
    /// Blocks actually written to or skipped for recording.
    int blocksReceived() const { return m_blocks; }
    /// Total recorded duration in milliseconds (all channels mixed down).
    double totalMs() const { return m_totalMs; }
    /// Last recording error; empty when the last write succeeded.
    QString lastError() const { return m_error; }
    /// Path of the file the last block went to (empty = no recording).
    QString lastWrittenPath() const { return m_lastWritten; }
    /// Bytes of sample data written so far (excludes the 44-byte header).
    quint32 dataBytesWritten() const { return m_dataBytes; }
    /// True while a WAV file is open for appending.
    bool isRecording() const { return m_file != nullptr; }

    /// The deprecated playback delegation, for tests and diagnostics.
    AudioPlaybackEngine *playbackEngine() { return &m_playback; }
    const AudioPlaybackEngine *playbackEngine() const { return &m_playback; }

signals:
    /// `load()` restored the config — the widget should re-read config().
    void configChanged();

private:
    /// Open @p path for a NEW recording: truncate, write the 44-byte header with
    /// zero sizes, remember the format. Closes a recording already in progress.
    ///
    /// @param path     WAV path to (re)create
    /// @param rate     Sample rate in Hz, written into the header
    /// @return true when the file is open and the header was written
    bool openRecording(const QString &path, quint32 rate);

    /// Append one mono block of 16-bit PCM and patch the header sizes.
    ///
    /// @param pcm16   Little-endian int16 samples
    /// @param frames  Number of samples in @p pcm16
    void appendFrames(const QByteArray &pcm16);

    /// Write the two size fields in the header, then restore the write cursor
    /// to the end of the data chunk.
    void patchHeader();

    /// Patch, close and forget the current recording. Safe to call twice.
    void finalizeRecording();

    /// Route the block to the deprecated playback delegation.
    void play(const SampledData &data);

    Config m_config;
    QString m_error;
    QString m_lastWritten;

    std::unique_ptr<QFile> m_file;   ///< open recording, nullptr when idle
    quint32 m_fileRate = 0;          ///< rate baked into the open header
    quint32 m_dataBytes = 0;         ///< sample bytes after the header

    /// The deprecated playback path: one persistent device, never one per block.
    AudioPlaybackEngine m_playback;

    int    m_blocks  = 0;
    double m_totalMs = 0.0;
};

#endif // AUDIOSINKNODE_H
