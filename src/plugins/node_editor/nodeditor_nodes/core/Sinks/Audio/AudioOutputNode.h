#ifndef AUDIOOUTPUTNODE_H
#define AUDIOOUTPUTNODE_H

#include "nodeditor_nodes_core_export.h"

#include "AudioPlaybackEngine.h"
#include "capabilities/INodeConfigSchema.h"
#include "shared/IStartable.h"
#include "shared/IStoppable.h"

#include <QtNodes/NodeDelegateModel>

#include <QJsonObject>
#include <QString>

#include <memory>

/**
 * @brief Audio output node model (REQ-SW-PL-076).
 *
 * Thin `NodeDelegateModel` controller — exactly the `LlamaGenerateModel` →
 * `LlamaEngine` pattern: this class owns the config, the persistence and the
 * port wiring, `AudioPlaybackEngine` owns the device, the format and the one
 * long-lived `QAudioSink`/`QAudioOutput`.
 *
 * 1 input port ("sample", `SampledData().type()` — the same derived type as
 * every other audio node, never a hardcoded descriptor), 0 output ports. Each
 * block is handed to the engine, which converts it to the device's format and
 * appends it to the playback queue — a block is never refused for being larger
 * than the device buffer (one TTS sentence is one block of ~2 s). `nullptr`
 * (edge removed, source silent) is ignored on purpose: dropping the device
 * would click, and the sink refills on the next block.
 *
 * Lifecycle (`IStartable`/`IStoppable`, REQ-SW-PL-050):
 *   - `start()` arms playback; the engine opens the device (eagerly when the
 *     input format is already known, otherwise on the first block);
 *   - `stop()` closes and destroys the sink, idempotently.
 *   - While the graph is only being edited, `setInData()` opens nothing.
 *
 * The GUI widget (`AudioOutputWidget`) lives in `nodeditor_nodes/gui` and is
 * attached through `NodeWidgetFactory`; `embeddedWidget()` always returns
 * nullptr and this file includes no QtWidgets
 * (`check_core_widgets_free` guards that).
 */
class NODEDITOR_NODES_CORE_EXPORT AudioOutputNode : public QtNodes::NodeDelegateModel,
                                                   public Daqster::IStartable,
                                                   public Daqster::IStoppable,
                                                   public Daqster::INodeConfigSchema
{
    Q_OBJECT

public:
    /// Persisted configuration. `deviceId` is the stable string from
    /// `AudioCompat::deviceId()`, never a device index (the index shifts
    /// between sessions when a USB headset is plugged in).
    struct Config {
        QString deviceId;      ///< empty = system default output device
        double volume = 1.0;   ///< linear, 0..1
        int bufferMs = 200;    ///< sink (device) buffer, 20..2000 ms
        bool enabled = true;   ///< false = no sink, blocks are skipped
        /// Queue bound, 500..10000 ms: how much audio may wait for the device
        /// when more than one block is waiting. NOT a limit on a block — one
        /// TTS sentence is one block (measured 2.4–2.6 s), and a single queued
        /// block is never dropped for being long (hard cap: 10 s per block).
        int maxQueuedMs = AudioPlaybackEngine::kDefaultMaxQueuedMs;
    };

    AudioOutputNode();
    ~AudioOutputNode() override;

    QString caption() const override
    { return QStringLiteral("Audio Output"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("AudioOutput"); }

    /// A block only produces sound — nothing in the scene changes shape.
    /// Both opt-outs keep a 100 blocks/s audio stream from forcing a scene
    /// geometry recompute and a body repaint per block.
    bool dataArrivalChangesGeometry() const override { return false; }
    bool dataArrivalChangesWidget() const override { return false; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    /// Draft-7 fragment describing exactly the object `save()` emits (AC3):
    /// `deviceId`, `volume`, `bufferMs`, `enabled`.
    QJsonObject configSchema() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    /// Always nullptr — the widget is supplied by the GUI plugin.
    QWidget *embeddedWidget() override { return nullptr; }

    /// Arm playback (opens the device when the input format is known).
    void start() override;

    /// Close and destroy the sink. Idempotent (REQ-SW-PL-050).
    void stop() override;

    // ── Config (used by the GUI widget) ────────────────────────────────────
    const Config &config() const { return m_config; }

    /// Apply and persist a new configuration. Volume is applied live; a device
    /// or buffer-size change re-creates the sink and clears the queue (PCM
    /// converted for the old output must not go to a new one).
    void setConfig(const Config &config);

    /// Engine status for the widget ("playing · …", "no output device").
    QString statusText() const;

    /// Formats / state for tests and diagnostics.
    AudioPlaybackEngine *engine() { return &m_engine; }
    const AudioPlaybackEngine *engine() const { return &m_engine; }

signals:
    /// Forwarded from the engine; emitted only when the text changes.
    void statusChanged(const QString &status);
    /// load() restored the config — the widget should re-read config().
    void configChanged();

private:
    Config m_config;
    AudioPlaybackEngine m_engine;
};

#endif // AUDIOOUTPUTNODE_H
