#ifndef DAQDISPLAYNODE_H
#define DAQDISPLAYNODE_H

#include "NodeDataTypes/SampledData.h"

#include "QtChartsCompat.h"

#include <QtNodes/NodeDelegateModel>

#include <QMetaType>
#include <QPair>
#include <QPointF>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QTimer;
class QThreadPool;
class QVBoxLayout;
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
 * Qt::QueuedConnection; this slot only repaints the charts (series->replace +
 * axis->setRange) — no data work on the GUI thread.
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
 * @brief DAQ Display node — multi-plot v1 (REQ-SW-PL-023).
 *
 * Evolution of the REQ-SW-PL-022 DataPlot node: instead of two fixed
 * QStackedWidget slots (the FFT slot was unreachable due to setCurrentIndex),
 * the node now hosts N configurable plot cards in a QScrollArea. Each card is
 * { title label, processing combo (Time Domain / FFT), channel combo, delete
 * button, real Qt Charts graph } — per-plot channel, independent of the other
 * cards (AC 1, AC 2).
 *
 * Threading contract (user hard requirement): decode/FFT/point-build NEVER run
 * on the GUI thread. setInData() only keeps the latest shared_ptr (immutable
 * buffer) and marks dirty. A 30 Hz GUI timer snapshots the data + per-card
 * {processingType, channelIndex} config and submits a QRunnable to a node-owned
 * QThreadPool (maxThreadCount=1). The worker touches only immutable copies plus
 * the shutdown atomic — no mutex for the data. Results are delivered back via a
 * queued invoke to the GUI-thread bridge; onComputeDone performs ONLY
 * series->replace() + axis->setRange() (AC 4).
 *
 * The std::function<QVector<float>(const SampledData&)> slot interface remains
 * the JIT-ready extension point; each card holds its own PreprocessFn bound to
 * its own channel (REQ-SW-PL-023 §1).
 */
class DaqDisplayNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

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

    QWidget *embeddedWidget() override;

    /// GUI-thread-only chart repaint of a compute result (via the bridge).
    void applyResult(const PlotResult &result);

    /// Built-in preprocessing functions (v1, JIT-ready slot interface).
    static QVector<float> channelSamples(const SampledData &data, int channel);
    static QVector<float> spectrumSamples(const SampledData &data, int channel);

private:
    using PreprocessFn = std::function<QVector<float>(const SampledData &)>;

    /// One configurable DataPlot card (REQ-SW-PL-023 §1).
    struct PlotCard {
        enum class ProcessingType { TimeDomain, FrequencySpectrum };

        QString title;
        int channelIndex = 0;
        ProcessingType processingType = ProcessingType::TimeDomain;
        PreprocessFn preprocess;          // bound to this card's channelIndex

        QtChartsCompat::LineSeries *series = nullptr;
        QtChartsCompat::Chart *chart = nullptr;
        QtChartsCompat::ChartView *chartView = nullptr;
        QtChartsCompat::ValueAxis *axisX = nullptr;
        QtChartsCompat::ValueAxis *axisY = nullptr;

        QComboBox *procCombo = nullptr;
        QComboBox *chanCombo = nullptr;
        QPushButton *deleteBtn = nullptr;
        QWidget *widget = nullptr;
    };

    /// Immutable per-card snapshot consumed by the worker task.
    struct CardConfig {
        PlotCard::ProcessingType processingType = PlotCard::ProcessingType::TimeDomain;
        int channelIndex = 0;
    };

    /// QRunnable running on the node-owned QThreadPool (off GUI thread).
    class ComputeTask;

    void setupUi();
    void addPlotCard(const QString &title, PlotCard::ProcessingType type, int channelIndex);
    void removeCardAt(int index);
    void clearAllCards();
    void bindCardPreprocess(PlotCard &card);
    void refresh();
    void updateEmptyState();
    void onCardConfigChanged(QWidget *cardWidget);

    /// Pure off-GUI computation: decode + FFT + decimation + ranges (AC 4).
    static PlotResult computePlotResult(const std::shared_ptr<const SampledData> &data,
                                        const QVector<CardConfig> &configs);
    static QVector<QPointF> buildTimeSeries(const QVector<float> &values, double sampleRate);
    static QVector<QPointF> buildSpectrumSeries(const QVector<float> &values, double sampleRate);
    static QPair<QPointF, QPointF> timeRanges(int sampleCount, double sampleRate);
    static QPair<QPointF, QPointF> spectrumRanges(const QVector<float> &values, double sampleRate);

private slots:
    void onAddPlot();
    void onRemovePlot(int /*unused*/);
    void onRefreshTick();

private:
    QWidget *m_root = nullptr;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_cardsContainer = nullptr;
    QVBoxLayout *m_cardsLayout = nullptr;
    QPushButton *m_addPlotButton = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QLabel *m_domainLabel = nullptr;
    QVector<PlotCard> m_cards;

    QThreadPool *m_pool = nullptr;      // node-owned, maxThreadCount=1
    QTimer *m_refreshTimer = nullptr;   // ~30 Hz refresh throttle (REQ-SW-PL-023 §5)
    DaqDisplayResultBridge *m_bridge = nullptr;
    std::atomic<bool> m_computeInFlight{false};
    std::atomic<bool> m_shuttingDown{false};
    bool m_dataDirty = false;

    std::shared_ptr<const SampledData> m_lastData;
};

#endif // DAQDISPLAYNODE_H
