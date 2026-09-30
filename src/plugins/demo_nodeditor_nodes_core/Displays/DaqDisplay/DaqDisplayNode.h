#ifndef DAQDISPLAYNODE_H
#define DAQDISPLAYNODE_H

#include "demo_nodeditor_nodes_core_export.h"

#include "NodeDataTypes/SampledData.h"

#include "shared/IStoppable.h"

#include <QtNodes/NodeDelegateModel>

#include <QByteArray>
#include <QMetaType>
#include <QPair>
#include <QPointF>
#include <QStringList>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

class QTimer;
class QWidget;

class DaqDisplayNode;

/// Result of one off-GUI compute pass: per-card decimated series + axis ranges.
struct PlotResult
{
    QVector<QVector<QPointF>> series;        // one decimated series per card
    QVector<QPair<QPointF, QPointF>> ranges; // per card: (xRange, yRange)
};
Q_DECLARE_METATYPE(PlotResult)

/**
 * @brief GUI-thread bridge for queued compute results (REQ-SW-PL-023 §4).
 *
 * Lives on the GUI thread. The worker task posts PlotResult here with
 * Qt::QueuedConnection; this slot only hands the result to the widget for
 * repainting (series->replace + axis->setRange) — no data work on the GUI
 * thread.
 */
class DaqDisplayResultBridge : public QObject
{
    Q_OBJECT

public:
    explicit DaqDisplayResultBridge(DaqDisplayNode *node)
        : m_node(node)
    {
    }

public slots:
    void onComputeDone(PlotResult result);

private:
    DaqDisplayNode *m_node;
};

/**
 * @brief DAQ Display node — multi-plot v2 (REQ-SW-PL-023 + REQ-SW-PL-025).
 *
 * Evolution of the REQ-SW-PL-022 DataPlot node: instead of two fixed
 * QStackedWidget slots (the FFT slot was unreachable due to setCurrentIndex),
 * the node now hosts N configurable plot cards. Each card is
 * { title label, processing combo (Time Domain / FFT), channel combo, delete
 * button, real Qt Charts graph } — per-plot channel, independent of the other
 * cards (AC 1, AC 2).
 *
 * v2 (REQ-SW-PL-025) adds three features on top of v1:
 *  - per-card decode mode: physical (`raw × amplitudeScale + amplitudeOffset`,
 *    no normalization/clamp) vs normalized (v1 convention); Y axes get unit
 *    titles from SampledStreamDescriptor ("Time (s)", "Frequency (Hz)",
 *    descriptor.unit / "Magnitude") and physical cards get data-driven Y
 *    ranges with ~5% padding instead of the fixed [-1, 1];
 *  - a worker-owned N-second ring buffer (default 10 s, `ringSeconds`): each
 *    new block is appended on the compute pass, old samples drop from the
 *    front, the descriptor-change reset clears the ring on
 *    sampleRate/channel-count/bytesPerFrame changes, and the FFT reads the
 *    most recent samples from the ring tail (≤4096);
 *  - save()/restore() round-trips `ringSeconds`, per-card `mode` and
 *    `unitAxes` — old v1 files load unchanged (backward compatible).
 *
 * Threading contract (user hard requirement): decode/FFT/point-build/min-max
 * NEVER run on the GUI thread. setInData() only keeps the latest shared_ptr
 * (immutable buffer) and marks dirty. A 30 Hz GUI timer snapshots the data +
 * per-card config and submits a task to the shared ComputePool (REQ-SW-PL-039)
 * under a per-node key — the pool's per-key "latest-wins" submission + per-key
 * serialization preserves the worker-only ring contract. The ring buffer is
 * owned by the worker side — only the submitted task touches it, never the GUI
 * thread — so no data mutex is required (same immutable-copy model as v1).
 * Results are delivered back via a queued invoke to the GUI-thread bridge;
 * onComputeDone emits plotResultReady(), which the widget turns into
 * series->replace() + axis->setRange() (AC 4).
 *
 * REQ-SW-PL-051: this model owns NO widgets. PlotCard carries the per-card
 * *configuration* only; the Qt Charts card widgets live in the GUI plugin
 * (DaqDisplayWidget), created through NodeWidgetFactory. The model reports
 * state changes through the signals below and receives user edits through the
 * public slots.
 *
 * The std::function<QVector<float>(const SampledData&)> slot interface remains
 * the JIT-ready extension point; each card holds its own PreprocessFn bound to
 * its own channel (REQ-SW-PL-023 §1).
 */
class DEMO_NODEDITOR_NODES_CORE_EXPORT DaqDisplayNode : public QtNodes::NodeDelegateModel, public Daqster::IStoppable
{
    Q_OBJECT

    friend class DaqDisplayNodeTest;

public:
    DaqDisplayNode();
    ~DaqDisplayNode() override;

    QString caption() const override
    { return QStringLiteral("DAQ Display"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("DaqDisplay"); }

    QJsonObject save() const override;

    /// Rebuild all plot cards from saved JSON (REQ-SW-PL-023 §6). Must round-trip.
    void restore(QJsonObject const &p);

    /// Base Serializable API — the graph framework calls load() on restore.
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    /// Core model has no QtWidgets dependency — the plot cards are created by
    /// the GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051).
    QWidget *embeddedWidget() override { return nullptr; }

    /// Stop the refresh timer and cancel queued compute tasks. Idempotent —
    /// safe to call multiple times (REQ-SW-PL-050).
    void stop() override;

    /// The node BODY (boundary, caption, ports) does not depend on data —
    /// widget content self-repaints via Qt. The validation border self-repaints
    /// via setValidationState(). Opts out of the per-frame body repaint.
    bool dataArrivalChangesWidget() const override { return false; }

    /// Built-in preprocessing functions (v1, JIT-ready slot interface).
    static QVector<float> channelSamples(const SampledData &data, int channel);
    static QVector<float> spectrumSamples(const SampledData &data, int channel);

    // ── Per-card axis title helper, used by the widget (REQ-SW-PL-025 §2) ──
    static QString unitAxisTitleY(const QString &unit, bool physicalMode, bool isSpectrum);

    /// Per-card configuration. Since REQ-SW-PL-051 this holds no widget
    /// pointers — the GUI widget keeps its own card objects in the same order.
    struct PlotCard {
        enum class ProcessingType { TimeDomain, FrequencySpectrum };

        /// Decode semantics for this card (REQ-SW-PL-025 AC 1/AC 2).
        enum class DecodeMode { Normalized, Physical };

        QString title;
        int channelIndex = 0;
        ProcessingType processingType = ProcessingType::TimeDomain;
        DecodeMode mode = DecodeMode::Normalized; // default normalized = v1 behavior
        bool unitAxes = true;                     // descriptor unit axis titles
        /// Descriptor-derived axis titles, computed by the model and consumed by
        /// the GUI widget's chart (REQ-SW-PL-025 §2, AC 2 / AC 3). Empty when
        /// unitAxes == false (the v1 no-title look, §4).
        QString axisTitleX;
        QString axisTitleY;
        std::function<QVector<float>(const SampledData &)> preprocess; // bound to this card's channelIndex
    };

    /// Immutable per-card snapshot consumed by the worker task.
    struct CardConfig {
        PlotCard::ProcessingType processingType = PlotCard::ProcessingType::TimeDomain;
        int channelIndex = 0;
        PlotCard::DecodeMode mode = PlotCard::DecodeMode::Normalized;
        bool unitAxes = true;
    };

    /// Add a configured plot card to the display (also used by restore()).
    /// Appends to the configuration list and asks the widget to rebuild.
    void addPlotCard(const QString &title, PlotCard::ProcessingType type, int channelIndex,
                     PlotCard::DecodeMode mode, bool unitAxes);

    /// Remove the card at `index` (widget removes its card object too).
    void removeCardAt(int index);

    /// Number of configured cards — the widget builds exactly this many.
    int cardCount() const { return m_cards.size(); }

    /// Read-only access to a card's configuration (for the widget).
    PlotCard const &cardAt(int index) const { return m_cards.at(index); }

    /// Ring-buffer duration, in seconds.
    double ringSeconds() const { return m_ringSeconds; }

signals:
    /// The card list changed (add / remove / restore) — rebuild the cards.
    void plotCardsChanged();

    /// A compute pass finished on the worker thread and was delivered to the
    /// GUI thread. The widget repaints series + axis ranges (AC 4).
    void plotResultReady(const PlotResult &result);

    /// The input descriptor changed: header text + per-card channel combo
    /// entries. The channel count may have changed.
    void descriptorInfoChanged(const QString &headerText, const QStringList &channelNames);

    /// The descriptor recommended a ring-buffer duration (REQ-SW-PL-023 §7).
    void ringSecondsChanged(double seconds);

public slots:
    /// "Add Plot" button in the widget.
    void onAddPlotRequested();
    /// "✕" button on card `index`.
    void onRemovePlotRequested(int index);
    /// Processing combo of card `index` changed.
    void onCardProcessingChanged(int index, int processingType);
    /// Channel combo of card `index` changed.
    void onCardChannelChanged(int index, int channelIndex);
    /// Ring-buffer spin box changed.
    void onRingSecondsChanged(double seconds);

private:
    using PreprocessFn = std::function<QVector<float>(const SampledData &)>;

    /// Worker-owned rolling history (REQ-SW-PL-025 §3). Touched ONLY by the
    /// worker thread (shared ComputePool, per-key serialization) — never by the
    /// GUI thread, so no data mutex is required (v1 immutable-copy model).
    struct RingChannel {
        QByteArray bytes; // rolling raw sample bytes for one channel
    };

    /// Worker-owned ring state + descriptor signature for format-change reset.
    struct ComputeState {
        QVector<RingChannel> channels;            // one rolling ring per channel
        QVector<SampleType> sampleTypes;          // per-channel type of stored bytes
        SampleEndian endianness = SampleEndian::LittleEndian;
        double sampleRate = 0.0;                  // descriptor signature (AC 4)
        int bytesPerFrame = 0;                    // descriptor signature (AC 4)
        quint64 lastAppendedGeneration = 0;       // identity of last appended block
    };

    void clearAllCards();
    void bindCardPreprocess(PlotCard &card);
    void refresh();
    void applyAxisTitlesFor(PlotCard &card, const SampledStreamDescriptor &desc);

    /// Default decode mode for a NEW card: physical unless the descriptor unit
    /// is "normalized" (REQ-SW-PL-025 §2). Restored cards keep their saved mode.
    PlotCard::DecodeMode defaultDecodeMode() const;

    /// Pure off-GUI computation: ring append + decode + FFT + decimation +
    /// ranges (AC 4). Worker thread only — touches the worker-owned m_ring.
    PlotResult computePlotResult(const std::shared_ptr<const SampledData> &data,
                                 const QVector<CardConfig> &configs,
                                 double ringSeconds,
                                 quint64 dataGeneration);
    /// Append one block to the worker-owned ring; descriptor-change reset (AC 4).
    static void appendRingBlock(ComputeState &ring, const SampledData &data,
                                double ringSeconds, quint64 dataGeneration);
    /// Decode one channel's rolling raw bytes; tailSamples > 0 → only the most
    /// recent N samples (FFT tail, AC 5).
    static QVector<float> decodeRingChannel(const RingChannel &ring, SampleType type,
                                            SampleEndian endian,
                                            PlotCard::DecodeMode mode,
                                            double scale, double offset,
                                            int tailSamples = -1);
    static QVector<QPointF> buildTimeSeries(const QVector<float> &values, double sampleRate);
    static QVector<QPointF> buildSpectrumSeries(const QVector<float> &values, double sampleRate);
    static QPair<QPointF, QPointF> timeRanges(int sampleCount, double sampleRate,
                                              const QVector<float> &values,
                                              PlotCard::DecodeMode mode);
    static QPair<QPointF, QPointF> spectrumRanges(const QVector<float> &values, double sampleRate);

private slots:
    void onRefreshTick();

private:
    QVector<PlotCard> m_cards;

    QTimer *m_refreshTimer = nullptr;   // ~30 Hz refresh throttle (REQ-SW-PL-023 §5)
    DaqDisplayResultBridge *m_bridge = nullptr;
    QByteArray m_poolKey;               // per-node key into the shared ComputePool
    std::atomic<bool> m_shuttingDown{false};
    bool m_dataDirty = false;

    std::shared_ptr<const SampledData> m_lastData;

    // ── v2 (REQ-SW-PL-025) ──────────────────────────────────────────────────
    double m_ringSeconds = 10.0;          // N-second rolling window (config)
    quint64 m_dataGeneration = 0;         // identity of the current m_lastData block
    ComputeState m_ring;                  // worker-owned rolling history (worker ONLY)
};

#endif // DAQDISPLAYNODE_H
