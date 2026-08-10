#include "DaqDisplayNode.h"

#include "FftUtil.h"
#include "LogCategories.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRunnable>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <cmath>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

// Decimation budgets (REQ-SW-PL-023 §5): ≤ 2000 points per series; FFT input is
// capped at 4096 inside FftUtil → ≤ 2048 magnitude bins → stride-2 → ≤ 1024
// points; time-domain stride = ceil(n/2000) → ≤ 2000 points.
constexpr int kMaxSeriesPoints = 2000;
constexpr int kFftBinStride = 2;

} // namespace

// ── Off-GUI compute (REQ-SW-PL-023 §4, AC 4) ────────────────────────────────

class DaqDisplayNode::ComputeTask : public QRunnable
{
public:
    ComputeTask(std::shared_ptr<const SampledData> data,
                QVector<CardConfig> configs,
                DaqDisplayNode *node)
        : m_data(std::move(data))
        , m_configs(std::move(configs))
        , m_node(node)
    {
    }

    void run() override
    {
        // TEMPORARY thread-identity log for Phase 4 smoke verification
        // (REQ-SW-PL-023 AC 4). Remove after the smoke test.
        qCDebug(lcDemoNodes) << "[DaqDisplayNode][temporary] compute thread:"
                             << QThread::currentThread();

        if (m_node->m_shuttingDown.load())
            return;

        PlotResult result = DaqDisplayNode::computePlotResult(m_data, m_configs);

        // Do not post after shutdown has begun — the node may be gone.
        if (m_node->m_shuttingDown.load())
            return;

        QMetaObject::invokeMethod(m_node->m_bridge, "onComputeDone",
                                  Qt::QueuedConnection, Q_ARG(PlotResult, result));
    }

private:
    std::shared_ptr<const SampledData> m_data;
    QVector<CardConfig> m_configs;
    DaqDisplayNode *m_node;
};

PlotResult DaqDisplayNode::computePlotResult(const std::shared_ptr<const SampledData> &data,
                                             const QVector<CardConfig> &configs)
{
    PlotResult result;
    if (!data)
        return result;

    const double sampleRate = data->descriptor().sampleRate;
    result.series.reserve(configs.size());
    result.ranges.reserve(configs.size());

    for (const CardConfig &config : configs) {
        if (config.processingType == PlotCard::ProcessingType::FrequencySpectrum) {
            const QVector<float> values = spectrumSamples(*data, config.channelIndex);
            result.series.append(buildSpectrumSeries(values, sampleRate));
            result.ranges.append(spectrumRanges(values, sampleRate));
        } else {
            const QVector<float> values = channelSamples(*data, config.channelIndex);
            result.series.append(buildTimeSeries(values, sampleRate));
            result.ranges.append(timeRanges(values.size(), sampleRate));
        }
    }
    return result;
}

QVector<QPointF> DaqDisplayNode::buildTimeSeries(const QVector<float> &values, double sampleRate)
{
    // Time-domain decimation: stride = ceil(n/2000) → ≤ 2000 points (§5).
    const int n = values.size();
    const int stride = std::max(1, static_cast<int>(std::ceil(double(n) / kMaxSeriesPoints)));

    QVector<QPointF> points;
    points.reserve((n + stride - 1) / stride);
    for (int i = 0; i < n; i += stride) {
        const double x = sampleRate > 0.0 ? double(i) / sampleRate : double(i);
        points.append(QPointF(x, values.at(i)));
    }
    return points;
}

QVector<QPointF> DaqDisplayNode::buildSpectrumSeries(const QVector<float> &values, double sampleRate)
{
    // FftUtil caps input at 4096 → ≤ 2048 magnitude bins; stride-2 decimation
    // → ≤ 1024 points (REQ-SW-PL-023 §5). values.size() = bins; fftSize = 2*bins.
    const int bins = values.size();
    const double fftSize = double(bins) * 2.0;

    QVector<QPointF> points;
    points.reserve((bins + kFftBinStride - 1) / kFftBinStride);
    for (int i = 0; i < bins; i += kFftBinStride) {
        const double x = sampleRate > 0.0 ? double(i) * sampleRate / fftSize : double(i);
        points.append(QPointF(x, values.at(i)));
    }
    return points;
}

QPair<QPointF, QPointF> DaqDisplayNode::timeRanges(int sampleCount, double sampleRate)
{
    // Time axis from the descriptor: [0, (N-1)/sampleRate] (REQ-SW-PL-023 §2.6).
    const double xMax = (sampleRate > 0.0 && sampleCount > 0)
                            ? double(sampleCount - 1) / sampleRate
                            : double(sampleCount);
    // Y: normalized decoder convention [-1, 1].
    return QPair<QPointF, QPointF>(QPointF(0.0, xMax), QPointF(-1.0, 1.0));
}

QPair<QPointF, QPointF> DaqDisplayNode::spectrumRanges(const QVector<float> &values,
                                                       double sampleRate)
{
    // Frequency axis: [0, Nyquist] = sampleRate/2 (REQ-SW-PL-023 §2.6).
    const double nyquist = sampleRate > 0.0 ? sampleRate / 2.0 : double(values.size());

    double maxMag = 0.0;
    for (const float value : values) {
        if (double(value) > maxMag)
            maxMag = double(value);
    }
    return QPair<QPointF, QPointF>(QPointF(0.0, nyquist),
                                   QPointF(0.0, maxMag > 0.0 ? maxMag * 1.05 : 1.0));
}

// ── GUI-thread repaint (AC 4) ───────────────────────────────────────────────

void DaqDisplayNode::applyResult(const PlotResult &result)
{
    // TEMPORARY thread-identity log for Phase 4 smoke verification
    // (REQ-SW-PL-023 AC 4). Remove after the smoke test.
    qCDebug(lcDemoNodes) << "[DaqDisplayNode][temporary] repaint thread:"
                         << QThread::currentThread();

    // GUI thread: repaint ONLY — series->replace + axis->setRange (§4).
    const int n = qMin(result.series.size(), m_cards.size());
    for (int i = 0; i < n; ++i) {
        m_cards[i].series->replace(result.series.at(i));
        if (i < result.ranges.size()) {
            m_cards[i].axisX->setRange(result.ranges.at(i).first.x(),
                                       result.ranges.at(i).first.y());
            m_cards[i].axisY->setRange(result.ranges.at(i).second.x(),
                                       result.ranges.at(i).second.y());
        }
    }
    m_computeInFlight = false;
}

void DaqDisplayResultBridge::onComputeDone(PlotResult result)
{
    // Queued to the GUI thread (Qt::QueuedConnection) — repaint only.
    if (m_node)
        m_node->applyResult(result);
}

// ── Construction / destruction ──────────────────────────────────────────────

DaqDisplayNode::DaqDisplayNode()
{
    // Metatype for the queued result delivery (REQ-SW-PL-023 §4).
    qRegisterMetaType<PlotResult>("PlotResult");

    // Node-owned pool (maxThreadCount=1) so the destructor can clear() +
    // waitForDone() without blocking the global pool (REQ-SW-PL-023 notes).
    m_pool = new QThreadPool();
    m_pool->setMaxThreadCount(1);

    m_bridge = new DaqDisplayResultBridge(this);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000 / 30); // ~30 Hz refresh throttle
    connect(m_refreshTimer, &QTimer::timeout, this, &DaqDisplayNode::onRefreshTick);

    setupUi();

    m_refreshTimer->start();
}

DaqDisplayNode::~DaqDisplayNode()
{
    if (m_refreshTimer)
        m_refreshTimer->stop();

    m_shuttingDown = true; // worker tasks check before posting results

    if (m_pool) {
        m_pool->clear();
        m_pool->waitForDone(500);
        delete m_pool;
        m_pool = nullptr;
    }

    if (m_bridge) {
        delete m_bridge;
        m_bridge = nullptr;
    }
}

// ── Serialization (REQ-SW-PL-023 §6) ────────────────────────────────────────

QJsonObject DaqDisplayNode::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();

    // {"plots": [{"title", "processing": "time"|"fft", "channel": idx}, ...]}
    QJsonArray plots;
    for (const PlotCard &card : m_cards) {
        QJsonObject plot;
        plot["title"] = card.title;
        plot["processing"] = card.processingType == PlotCard::ProcessingType::FrequencySpectrum
                                 ? QStringLiteral("fft")
                                 : QStringLiteral("time");
        plot["channel"] = card.channelIndex;
        plots.append(plot);
    }
    modelJson["plots"] = plots;

    return modelJson;
}

void DaqDisplayNode::restore(QJsonObject const &p)
{
    // Rebuild all plot cards from saved JSON (REQ-SW-PL-023 §6).
    clearAllCards();

    const QJsonArray plots = p.value("plots").toArray();
    for (const QJsonValue &value : plots) {
        const QJsonObject plot = value.toObject();
        const QString title = plot.value("title").toString(QStringLiteral("Plot"));
        const QString processing = plot.value("processing").toString(QStringLiteral("time"));
        const int channel = plot.value("channel").toInt(0);
        const PlotCard::ProcessingType type =
            processing == QStringLiteral("fft") ? PlotCard::ProcessingType::FrequencySpectrum
                                                : PlotCard::ProcessingType::TimeDomain;
        addPlotCard(title, type, channel);
    }

    if (m_lastData)
        refresh();
}

void DaqDisplayNode::load(QJsonObject const &p)
{
    // Base Serializable API — the graph framework calls load() on scene restore.
    restore(p);
}

// ── Ports / data flow ───────────────────────────────────────────────────────

unsigned int DaqDisplayNode::nPorts(PortType portType) const
{
    return portType == PortType::In ? 1 : 0;
}

NodeDataType DaqDisplayNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    // The unified SampledData type — all sampled sources share this id; the
    // `domain` field discriminates at runtime (REQ-SW-PL-022, design note).
    return SampledData().type();
}

std::shared_ptr<NodeData> DaqDisplayNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return nullptr;
}

void DaqDisplayNode::setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex)
{
    if (portIndex != 0)
        return;

    // GUI thread: keep-latest ONLY — no decode/FFT/point-build here
    // (REQ-SW-PL-023 §4, user hard requirement). The buffer is immutable.
    m_lastData = std::dynamic_pointer_cast<SampledData>(data);

    NodeValidationState s;
    if (m_lastData) {
        s._state = NodeValidationState::State::Valid;
    } else if (data) {
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("Expected SampledData (\"sample\")");
    } else {
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = QStringLiteral("No data connected");
    }
    setValidationState(s);

    if (m_lastData) {
        refresh();          // descriptor → label + channel combos (UI only)
        m_dataDirty = true; // the 30 Hz timer submits the compute pass
    }
}

QWidget *DaqDisplayNode::embeddedWidget()
{
    return m_root;
}

// ── Built-in preprocessing functions (v1, JIT-ready slot interface) ─────────

QVector<float> DaqDisplayNode::channelSamples(const SampledData &data, int channel)
{
    // Decode straight to float — no double intermediate (REQ-SW-PL-023 §3, AC 5).
    QVector<QVector<float>> channels;
    data.decodeToNormalizedF32(channels);
    if (channel < 0 || channel >= channels.size())
        return {};
    return channels.at(channel);
}

QVector<float> DaqDisplayNode::spectrumSamples(const SampledData &data, int channel)
{
    // Shared FftUtil (cached Hann window, thread-safe) — no local FFT copy
    // remains in this node (REQ-SW-PL-023 §2, AC 3).
    return FftUtil::magnitudeSpectrum(channelSamples(data, channel));
}

// ── UI ──────────────────────────────────────────────────────────────────────

void DaqDisplayNode::setupUi()
{
    m_root = new QWidget();
    auto *rootLayout = new QVBoxLayout(m_root);
    rootLayout->setContentsMargins(2, 2, 2, 2);
    rootLayout->setSpacing(2);

    // Header: domain/rate label + Add Plot button (REQ-SW-PL-023 §1).
    auto *header = new QHBoxLayout();
    m_domainLabel = new QLabel(tr("sampled"), m_root);
    m_addPlotButton = new QPushButton(tr("Add Plot"), m_root);
    header->addWidget(m_domainLabel, 1);
    header->addWidget(m_addPlotButton);
    rootLayout->addLayout(header);

    // Scroll area hosting the configurable plot cards. No QStackedWidget — all
    // cards are visible, so the FFT view is reachable (AC 3, bug fix §2).
    m_scroll = new QScrollArea(m_root);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_cardsContainer = new QWidget(m_scroll);
    m_cardsLayout = new QVBoxLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(2, 2, 2, 2);
    m_cardsLayout->setSpacing(6);
    m_scroll->setWidget(m_cardsContainer);
    rootLayout->addWidget(m_scroll, 1);

    // Empty-state hint when no cards exist.
    m_emptyLabel = new QLabel(tr("No plots — press Add Plot"), m_cardsContainer);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_cardsLayout->addWidget(m_emptyLabel);

    connect(m_addPlotButton, &QPushButton::clicked, this, &DaqDisplayNode::onAddPlot);
}

void DaqDisplayNode::addPlotCard(const QString &title,
                                 PlotCard::ProcessingType type,
                                 int channelIndex)
{
    auto *cardWidget = new QWidget(m_cardsContainer);
    auto *cardLayout = new QVBoxLayout(cardWidget);
    cardLayout->setContentsMargins(2, 2, 2, 2);
    cardLayout->setSpacing(2);

    // Card header: title label + processing combo + channel combo + delete.
    auto *titleLabel = new QLabel(title, cardWidget);
    auto *procCombo = new QComboBox(cardWidget);
    procCombo->addItem(tr("Time Domain"), int(PlotCard::ProcessingType::TimeDomain));
    procCombo->addItem(tr("FFT"), int(PlotCard::ProcessingType::FrequencySpectrum));
    procCombo->setCurrentIndex(type == PlotCard::ProcessingType::FrequencySpectrum ? 1 : 0);

    auto *chanCombo = new QComboBox(cardWidget);
    chanCombo->setMinimumWidth(72);

    auto *deleteBtn = new QPushButton(tr("✕"), cardWidget);
    deleteBtn->setToolTip(tr("Remove plot"));
    deleteBtn->setMaximumWidth(28);

    auto *header = new QHBoxLayout();
    header->addWidget(titleLabel, 1);
    header->addWidget(procCombo);
    header->addWidget(new QLabel(tr("Ch:"), cardWidget));
    header->addWidget(chanCombo);
    header->addWidget(deleteBtn);
    cardLayout->addLayout(header);

    // Real Qt Charts graph (same construction style as the legacy slots).
    auto *chart = new QtChartsCompat::Chart();
    chart->setTitle(title);
    chart->legend()->hide();
    chart->setAnimationOptions(QtChartsCompat::Chart::NoAnimation);

    auto *axisX = new QtChartsCompat::ValueAxis();
    auto *axisY = new QtChartsCompat::ValueAxis();
    axisX->setLabelFormat(QStringLiteral("%.1f"));
    axisY->setLabelFormat(QStringLiteral("%.2f"));
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    auto *series = new QtChartsCompat::LineSeries();
    series->setName(title);
    chart->addSeries(series);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    auto *view = new QtChartsCompat::ChartView(chart);
    view->setMinimumSize(360, 180);
    view->setRenderHint(QPainter::Antialiasing);
    cardLayout->addWidget(view);

    // Populate the channel combo from the current descriptor (if any) and clamp.
    if (m_lastData) {
        const SampledStreamDescriptor &desc = m_lastData->descriptor();
        const int nChannels = desc.totalChannels();
        for (int i = 0; i < nChannels; ++i) {
            const QString name = desc.channels.at(i).name;
            chanCombo->addItem(name.isEmpty() ? QStringLiteral("Ch%1").arg(i) : name, i);
        }
        if (nChannels > 0)
            channelIndex = qBound(0, channelIndex, nChannels - 1);
        chanCombo->setCurrentIndex(channelIndex);
    }

    PlotCard card;
    card.title = title;
    card.processingType = type;
    card.channelIndex = channelIndex;
    card.widget = cardWidget;
    card.series = series;
    card.chart = chart;
    card.chartView = view;
    card.axisX = axisX;
    card.axisY = axisY;
    card.procCombo = procCombo;
    card.chanCombo = chanCombo;
    card.deleteBtn = deleteBtn;
    bindCardPreprocess(card);

    m_cards.append(card);
    m_cardsLayout->addWidget(cardWidget);

    updateEmptyState();

    // Wire the card controls. cardWidget pointers are stable across QVector
    // reallocation (only the struct copies the pointer), so the lambdas stay
    // valid and the handler locates the card by widget.
    connect(procCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this, cardWidget](int) { onCardConfigChanged(cardWidget); });
    connect(chanCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this, cardWidget](int) { onCardConfigChanged(cardWidget); });
    connect(deleteBtn, &QPushButton::clicked, this,
            [this](bool) { onRemovePlot(0); });

    if (m_lastData)
        m_dataDirty = true; // new card should render the latest data next tick
}

void DaqDisplayNode::removeCardAt(int index)
{
    if (index < 0 || index >= m_cards.size())
        return;

    const PlotCard card = m_cards.takeAt(index);
    if (card.widget)
        card.widget->deleteLater(); // view → scene → chart → series/axes cleanup

    updateEmptyState();
}

void DaqDisplayNode::clearAllCards()
{
    while (!m_cards.isEmpty())
        removeCardAt(m_cards.size() - 1);
}

void DaqDisplayNode::bindCardPreprocess(PlotCard &card)
{
    // Each card's PreprocessFn is bound to ITS OWN channelIndex
    // (REQ-SW-PL-023 §1, AC 2) — the JIT-ready slot interface.
    const int channel = card.channelIndex;
    if (card.processingType == PlotCard::ProcessingType::FrequencySpectrum) {
        card.preprocess = [channel](const SampledData &d) {
            return spectrumSamples(d, channel);
        };
    } else {
        card.preprocess = [channel](const SampledData &d) {
            return channelSamples(d, channel);
        };
    }
}

void DaqDisplayNode::refresh()
{
    if (!m_lastData)
        return;

    const SampledStreamDescriptor &desc = m_lastData->descriptor();
    const int nChannels = desc.totalChannels();

    // Domain/rate label — descriptor-driven (REQ-SW-PL-023 §2.8).
    const QString domain = desc.domain.isEmpty() ? QStringLiteral("sampled") : desc.domain;
    const QString rateText = desc.sampleRate > 0.0
                                 ? QStringLiteral("%1 Hz").arg(desc.sampleRate, 0, 'g', 6)
                                 : QStringLiteral("rate n/a");
    m_domainLabel->setText(QStringLiteral("%1 · %2").arg(domain, rateText));

    // Per-card channel combos populated from the descriptor channel count.
    for (PlotCard &card : m_cards) {
        QSignalBlocker blocker(card.chanCombo); // no dirty marks while populating
        card.chanCombo->clear();
        for (int i = 0; i < nChannels; ++i) {
            const QString name = desc.channels.at(i).name;
            card.chanCombo->addItem(name.isEmpty() ? QStringLiteral("Ch%1").arg(i) : name, i);
        }
        if (nChannels > 0) {
            card.channelIndex = qBound(0, card.channelIndex, nChannels - 1);
            card.chanCombo->setCurrentIndex(card.channelIndex);
            bindCardPreprocess(card);
        }
    }
}

void DaqDisplayNode::updateEmptyState()
{
    if (m_emptyLabel)
        m_emptyLabel->setVisible(m_cards.isEmpty());
}

void DaqDisplayNode::onCardConfigChanged(QWidget *cardWidget)
{
    for (PlotCard &card : m_cards) {
        if (card.widget != cardWidget)
            continue;

        // Processing combo changed → rebind this card's preprocess fn.
        const int proc = card.procCombo->currentData().toInt();
        card.processingType = proc == int(PlotCard::ProcessingType::FrequencySpectrum)
                                  ? PlotCard::ProcessingType::FrequencySpectrum
                                  : PlotCard::ProcessingType::TimeDomain;

        // Channel combo changed → per-plot channel, independent of other cards.
        card.channelIndex = qMax(0, card.chanCombo->currentIndex());

        bindCardPreprocess(card);
        m_dataDirty = true; // only this card's config changed; recompute next tick
        return;
    }
}

// ── Slots ───────────────────────────────────────────────────────────────────

void DaqDisplayNode::onAddPlot()
{
    // New card default: Time Domain, channel 0 (REQ-SW-PL-023 §1).
    addPlotCard(QStringLiteral("Plot %1").arg(m_cards.size() + 1),
                PlotCard::ProcessingType::TimeDomain, 0);
}

void DaqDisplayNode::onRemovePlot(int /*unused*/)
{
    auto *btn = qobject_cast<QPushButton *>(QObject::sender());
    if (!btn)
        return;
    for (int i = 0; i < m_cards.size(); ++i) {
        if (m_cards.at(i).deleteBtn == btn) {
            removeCardAt(i);
            return;
        }
    }
}

void DaqDisplayNode::onRefreshTick()
{
    if (!m_dataDirty || m_computeInFlight.load() || !m_lastData)
        return;

    // Snapshot immutable copies on the GUI thread; the worker touches only these
    // (REQ-SW-PL-023 §4) — no data mutex.
    const std::shared_ptr<const SampledData> dataCopy = m_lastData;

    QVector<CardConfig> configs;
    configs.reserve(m_cards.size());
    for (const PlotCard &card : m_cards)
        configs.append(CardConfig{card.processingType, card.channelIndex});

    m_dataDirty = false;
    m_computeInFlight = true;

    m_pool->start(new ComputeTask(dataCopy, std::move(configs), this));
}
