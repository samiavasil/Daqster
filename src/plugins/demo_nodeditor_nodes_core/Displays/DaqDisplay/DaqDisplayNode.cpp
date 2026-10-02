#include "DaqDisplayNode.h"

#include "FftUtil.h"
#include "Threading/ComputePool.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

#include <cmath>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;
using QtNodes::PortIndex;
using QtNodes::PortType;

using Daqster::ComputePool;

namespace {

// Decimation budgets (REQ-SW-PL-023 §5): ≤ 2000 points per series; FFT input is
// capped at 4096 inside FftUtil → ≤ 2048 magnitude bins → stride-2 → ≤ 1024
// points; time-domain stride = ceil(n/2000) → ≤ 2000 points.
constexpr int kMaxSeriesPoints = 2000;
constexpr int kFftBinStride = 2;
// v2 FFT input window: the most recent samples from the ring tail, capped at
// FftUtil's own 4096 limit (REQ-SW-PL-025 §3, AC 5).
constexpr int kMaxFftSamples = 4096;

} // namespace

// ── Off-GUI compute (REQ-SW-PL-023 §4, AC 4) ────────────────────────────────
//
// The compute pass runs on the shared ComputePool (REQ-SW-PL-039) under a
// per-node key. The pool's per-key "latest-wins" submission + per-key
// serialization guarantee at most one task per node runs at a time, so the
// worker-owned ring buffer (m_ring) is never touched concurrently.

PlotResult DaqDisplayNode::computePlotResult(const std::shared_ptr<const SampledData> &data,
                                             const QVector<CardConfig> &configs,
                                             double ringSeconds,
                                             quint64 dataGeneration)
{
    PlotResult result;
    if (!data)
        return result;

    const SampledStreamDescriptor &desc = data->descriptor();
    const double sampleRate = desc.sampleRate;

    // 1. Worker-owned ring append (REQ-SW-PL-025 §3, AC 3/AC 4). Runs on the
    //    worker thread; m_ring is never touched by the GUI thread (AC 6).
    appendRingBlock(m_ring, *data, ringSeconds, dataGeneration);

    // 2. Per-card series + ranges from the rolling history.
    result.series.reserve(configs.size());
    result.ranges.reserve(configs.size());
    for (const CardConfig &config : configs) {
        const int channel = config.channelIndex;
        if (channel < 0 || channel >= m_ring.channels.size()
            || channel >= desc.channels.size()) {
            result.series.append(QVector<QPointF>());
            result.ranges.append(
                QPair<QPointF, QPointF>(QPointF(0.0, 1.0), QPointF(-1.0, 1.0)));
            continue;
        }

        const RingChannel &ringCh = m_ring.channels.at(channel);
        const StreamChannelDescriptor &chDesc = desc.channels.at(channel);
        const double scale = desc.amplitudeScale;
        const double offset = desc.amplitudeOffset;

        if (config.processingType == PlotCard::ProcessingType::FrequencySpectrum) {
            // FFT from the ring TAIL: most recent ≤4096 samples (AC 5), never
            // the whole window.
            const QVector<float> tail = decodeRingChannel(
                ringCh, chDesc.sampleType, desc.endianness, config.mode,
                scale, offset, kMaxFftSamples);
            const QVector<float> mags = FftUtil::magnitudeSpectrum(tail);
            result.series.append(buildSpectrumSeries(mags, sampleRate));
            result.ranges.append(spectrumRanges(mags, sampleRate));
        } else {
            // Time Domain: the whole rolling window (AC 3), decimated in
            // buildTimeSeries (≤ 2000 points/series, v1 budget).
            const QVector<float> values = decodeRingChannel(
                ringCh, chDesc.sampleType, desc.endianness, config.mode,
                scale, offset);
            result.series.append(buildTimeSeries(values, sampleRate));
            result.ranges.append(timeRanges(values.size(), sampleRate, values, config.mode));
        }
    }
    return result;
}

void DaqDisplayNode::appendRingBlock(ComputeState &ring, const SampledData &data,
                                     double ringSeconds, quint64 dataGeneration)
{
    // Identity guard: the SAME data block must not be appended twice (config
    // changes re-submit the current m_lastData; only a NEW block appends).
    if (dataGeneration == ring.lastAppendedGeneration)
        return;

    const SampledStreamDescriptor &desc = data.descriptor();
    const int nChannels = desc.totalChannels();
    const int frameBytes = desc.bytesPerFrame();
    if (nChannels <= 0 || frameBytes <= 0 || data.buffer().isEmpty())
        return;

    // Descriptor-change reset (AC 4): sampleRate / channel count / bytesPerFrame
    // (plus endianness + per-channel sample types — no mixing formats).
    bool typesMatch = ring.sampleTypes.size() == nChannels;
    for (int ch = 0; typesMatch && ch < nChannels; ++ch)
        typesMatch = ring.sampleTypes.at(ch) == desc.channels.at(ch).sampleType;
    const bool formatChanged = ring.channels.size() != nChannels
                               || ring.sampleRate != desc.sampleRate
                               || ring.bytesPerFrame != frameBytes
                               || ring.endianness != desc.endianness
                               || !typesMatch;
    if (formatChanged) {
        ring.channels.clear();
        ring.channels.resize(nChannels);
        ring.sampleTypes.clear();
        ring.sampleTypes.reserve(nChannels);
        for (const StreamChannelDescriptor &ch : desc.channels)
            ring.sampleTypes.append(ch.sampleType);
        ring.sampleRate = desc.sampleRate;
        ring.bytesPerFrame = frameBytes;
        ring.endianness = desc.endianness;
    }
    ring.lastAppendedGeneration = dataGeneration;

    // Per-channel byte offsets inside one interleaved frame.
    QVector<int> chOffsets(nChannels);
    int offset = 0;
    for (int ch = 0; ch < nChannels; ++ch) {
        chOffsets[ch] = offset;
        offset += sampleTypeByteSize(desc.channels.at(ch).sampleType);
    }

    const int frameCount = data.buffer().size() / frameBytes;
    const char *src = data.buffer().constData();
    for (int ch = 0; ch < nChannels; ++ch) {
        const int chBytes = sampleTypeByteSize(desc.channels.at(ch).sampleType);
        QByteArray &ringBytes = ring.channels[ch].bytes;
        if (frameCount > 0) {
            ringBytes.reserve(ringBytes.size() + frameCount * chBytes);
            for (int frame = 0; frame < frameCount; ++frame)
                ringBytes.append(src + frame * frameBytes + chOffsets.at(ch), chBytes);
        }

        // Rolling window: drop old samples from the front (capacity = N seconds
        // of this channel's samples; total = N × sampleRate × bytesPerFrame).
        const qsizetype capacity = (ringSeconds > 0.0 && desc.sampleRate > 0.0)
                                       ? qsizetype(ringSeconds * desc.sampleRate) * chBytes
                                       : 0;
        if (capacity > 0 && ringBytes.size() > capacity)
            ringBytes.remove(0, static_cast<int>(ringBytes.size() - capacity));
    }
}

QVector<float> DaqDisplayNode::decodeRingChannel(const RingChannel &ring, SampleType type,
                                                 SampleEndian endian,
                                                 PlotCard::DecodeMode mode,
                                                 double scale, double offset,
                                                 int tailSamples)
{
    const int byteSize = sampleTypeByteSize(type);
    if (byteSize <= 0)
        return {};

    const int totalSamples = ring.bytes.size() / byteSize;
    const int start = tailSamples > 0 ? std::max(0, totalSamples - tailSamples) : 0;
    const int count = totalSamples - start;
    QVector<float> out(count);
    if (count <= 0)
        return out;

    const char *ptr = ring.bytes.constData() + start * byteSize;
    if (mode == PlotCard::DecodeMode::Physical) {
        // Physical: raw × amplitudeScale + amplitudeOffset, no clamp (AC 1).
        for (int i = 0; i < count; ++i) {
            out[i] = static_cast<float>(
                SampledDecoder::decodeRawSample(ptr, type, endian) * scale + offset);
            ptr += byteSize;
        }
    } else {
        // Normalized: v1 convention (32767 + clamp to [-1, 1]).
        for (int i = 0; i < count; ++i) {
            out[i] = static_cast<float>(
                SampledDecoder::decodeNormalizedSample(ptr, type, endian));
            ptr += byteSize;
        }
    }
    return out;
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

QPair<QPointF, QPointF> DaqDisplayNode::timeRanges(int sampleCount, double sampleRate,
                                                   const QVector<float> &values,
                                                   PlotCard::DecodeMode mode)
{
    // Time axis from the descriptor: [0, (N-1)/sampleRate] (REQ-SW-PL-023 §2.6).
    const double xMax = (sampleRate > 0.0 && sampleCount > 0)
                            ? double(sampleCount - 1) / sampleRate
                            : double(sampleCount);

    if (mode == PlotCard::DecodeMode::Normalized)
        // Y: normalized decoder convention [-1, 1].
        return QPair<QPointF, QPointF>(QPointF(0.0, xMax), QPointF(-1.0, 1.0));

    // Y (physical): min/max of the decoded values with ~5% padding — replaces
    // the fixed [-1, 1] ONLY for physical cards (REQ-SW-PL-025 §2, AC 2).
    double min = 0.0;
    double max = 0.0;
    bool first = true;
    for (const float value : values) {
        const double v = double(value);
        if (first || v < min)
            min = v;
        if (first || v > max)
            max = v;
        first = false;
    }
    if (first) { // empty window — fall back to a neutral range
        min = -1.0;
        max = 1.0;
    } else {
        double pad = (max - min) * 0.05;
        if (pad <= 0.0)
            pad = (max != 0.0) ? std::abs(max) * 0.05 : 1.0; // flat line / all-zero
        min -= pad;
        max += pad;
    }
    return QPair<QPointF, QPointF>(QPointF(0.0, xMax), QPointF(min, max));
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

void DaqDisplayResultBridge::onComputeDone(PlotResult result)
{
    // Queued to the GUI thread (Qt::QueuedConnection) — repaint only. The
    // actual series->replace + axis->setRange happens in DaqDisplayWidget
    // (REQ-SW-PL-051), which listens to plotResultReady().
    if (m_node)
        Q_EMIT m_node->plotResultReady(result);
}

// ── Construction / destruction ──────────────────────────────────────────────

DaqDisplayNode::DaqDisplayNode()
{
    // Display nodes must never get a graphics effect (perf): the shadow blur
    // runs per repaint and costs ~46% CPU during video playback.
    QtNodes::NodeStyle s = this->nodeStyle();
    s.ShadowEnabled = false;
    this->setNodeStyle(s);

    // Metatype for the queued result delivery (REQ-SW-PL-023 §4).
    qRegisterMetaType<PlotResult>("PlotResult");

    // Per-node key into the shared ComputePool (REQ-SW-PL-039): the pool's
    // per-key latest-wins submission + serialization replaces the old
    // node-owned QThreadPool(maxThreadCount=1).
    m_poolKey = QByteArray::number(reinterpret_cast<quintptr>(this));

    m_bridge = new DaqDisplayResultBridge(this);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000 / 30); // ~30 Hz refresh throttle
    connect(m_refreshTimer, &QTimer::timeout, this, &DaqDisplayNode::onRefreshTick);


    m_refreshTimer->start();
}

DaqDisplayNode::~DaqDisplayNode()
{
    // Single shutdown path: stop() stops the timer + cancels compute tasks
    // (REQ-SW-PL-050).
    stop();

    if (m_bridge) {
        delete m_bridge;
        m_bridge = nullptr;
    }
}

void DaqDisplayNode::stop()
{
    // Idempotent: stopping an already-stopped timer / null key is a no-op.
    if (m_refreshTimer)
        m_refreshTimer->stop();

    m_shuttingDown = true; // worker tasks check before posting results

    // Cancel queued/pending tasks for this key and wait for the running one
    // (≤ 500 ms) — replaces the old m_pool->clear() + waitForDone().
    ComputePool::instance().cancel(m_poolKey);
}

// ── Serialization (REQ-SW-PL-023 §6) ────────────────────────────────────────

QJsonObject DaqDisplayNode::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();

    // {"ringSeconds": double, "plots": [{"title", "processing": "time"|"fft",
    //  "channel": idx, "mode": "normalized"|"physical", "unitAxes": bool}, ...]}
    // New v2 fields are optional; v1 files load with defaults (AC 7).
    QJsonArray plots;
    for (const PlotCard &card : m_cards) {
        QJsonObject plot;
        plot["title"] = card.title;
        plot["processing"] = card.processingType == PlotCard::ProcessingType::FrequencySpectrum
                                 ? QStringLiteral("fft")
                                 : QStringLiteral("time");
        plot["channel"] = card.channelIndex;
        plot["mode"] = card.mode == PlotCard::DecodeMode::Physical
                           ? QStringLiteral("physical")
                           : QStringLiteral("normalized");
        plot["unitAxes"] = card.unitAxes;
        plots.append(plot);
    }
    modelJson["plots"] = plots;
    modelJson["ringSeconds"] = m_ringSeconds;

    return modelJson;
}

void DaqDisplayNode::restore(QJsonObject const &p)
{
    // Rebuild all plot cards from saved JSON (REQ-SW-PL-023 §6). v2 reads the
    // new fields with defaults when missing — old v1 files load unchanged (AC 7).
    clearAllCards();

    const double ringSeconds = p.value("ringSeconds").toDouble(10.0);
    m_ringSeconds = ringSeconds > 0.0 ? ringSeconds : 10.0;
    // Notify the widget even when no plot card exists (so the spin box shows
    // the restored value on an empty display).
    Q_EMIT ringSecondsChanged(m_ringSeconds);

    const QJsonArray plots = p.value("plots").toArray();
    for (const QJsonValue &value : plots) {
        const QJsonObject plot = value.toObject();
        const QString title = plot.value("title").toString(QStringLiteral("Plot"));
        const QString processing = plot.value("processing").toString(QStringLiteral("time"));
        const int channel = plot.value("channel").toInt(0);
        const PlotCard::ProcessingType type =
            processing == QStringLiteral("fft") ? PlotCard::ProcessingType::FrequencySpectrum
                                                : PlotCard::ProcessingType::TimeDomain;
        const QString modeStr = plot.value("mode").toString(QStringLiteral("normalized"));
        const PlotCard::DecodeMode mode =
            modeStr == QStringLiteral("physical") ? PlotCard::DecodeMode::Physical
                                                  : PlotCard::DecodeMode::Normalized;
        const bool unitAxes = plot.value("unitAxes").toBool(true);
        addPlotCard(title, type, channel, mode, unitAxes);
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
    std::shared_ptr<SampledData> sampled = std::dynamic_pointer_cast<SampledData>(data);

    // v2 ring identity: a NEW block (different object) gets a fresh generation
    // so the worker appends it exactly once; re-triggers of the same block
    // (config change, same pointer) are skipped in appendRingBlock.
    if (sampled && sampled.get() != m_lastData.get())
        ++m_dataGeneration;

    m_lastData = sampled;

    // Descriptor-override: source can recommend a ring-buffer duration; only
    // applies when the descriptor explicitly provides a value > 0
    // (REQ-SW-PL-023 §7 — expectedBufferSeconds).
    if (sampled) {
        const SampledStreamDescriptor &desc = sampled->descriptor();
        if (desc.expectedBufferSeconds > 0.0) {
            m_ringSeconds = desc.expectedBufferSeconds;
            Q_EMIT ringSecondsChanged(m_ringSeconds);
        }
    }

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

// ── Card configuration (REQ-SW-PL-051: no widgets here) ────────────────────

void DaqDisplayNode::addPlotCard(const QString &title,
                                 PlotCard::ProcessingType type,
                                 int channelIndex,
                                 PlotCard::DecodeMode mode,
                                 bool unitAxes)
{
    PlotCard card;
    card.title = title;
    card.processingType = type;
    card.channelIndex = channelIndex;
    card.mode = mode;
    card.unitAxes = unitAxes;
    bindCardPreprocess(card);

    m_cards.append(card);

    // The GUI widget rebuilds its card objects from the new configuration.
    Q_EMIT plotCardsChanged();

    if (m_lastData) {
        applyAxisTitlesFor(m_cards.last(), m_lastData->descriptor());
        m_dataDirty = true; // new card should render the latest data next tick
    }
}

void DaqDisplayNode::removeCardAt(int index)
{
    if (index < 0 || index >= m_cards.size())
        return;

    m_cards.removeAt(index);

    // The GUI widget drops its card object in the same position.
    Q_EMIT plotCardsChanged();
}

void DaqDisplayNode::clearAllCards()
{
    if (m_cards.isEmpty())
        return;

    m_cards.clear();
    Q_EMIT plotCardsChanged();
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

DaqDisplayNode::PlotCard::DecodeMode DaqDisplayNode::defaultDecodeMode() const
{
    // Minimal v2 default for NEW cards: physical unless the descriptor unit is
    // "normalized" (REQ-SW-PL-025 §2). Restored cards keep their saved mode.
    if (m_lastData && m_lastData->descriptor().unit != QStringLiteral("normalized"))
        return PlotCard::DecodeMode::Physical;
    return PlotCard::DecodeMode::Normalized;
}

QString DaqDisplayNode::unitAxisTitleY(const QString &unit, bool physicalMode,
                                       bool isSpectrum)
{
    // Time Domain: descriptor unit (fallback "normalized" / "amplitude").
    // Frequency: descriptor unit in physical mode, "Magnitude" in normalized.
    if (isSpectrum) {
        if (physicalMode)
            return unit.isEmpty() ? QStringLiteral("Magnitude") : unit;
        return QStringLiteral("Magnitude");
    }
    if (!unit.isEmpty())
        return unit;
    return physicalMode ? QStringLiteral("amplitude") : QStringLiteral("normalized");
}

void DaqDisplayNode::applyAxisTitlesFor(PlotCard &card,
                                        const SampledStreamDescriptor &desc)
{
    // The card's chart axes live in the GUI plugin, but the titles are derived
    // from the descriptor here so the descriptor stays the single source of
    // truth (REQ-SW-PL-025 §2, AC 2). The widget reads them via cardAt().
    const bool isSpectrum =
        card.processingType == PlotCard::ProcessingType::FrequencySpectrum;
    const QString xTitle = isSpectrum ? QStringLiteral("Frequency (Hz)")
                                      : QStringLiteral("Time (s)");
    const QString yTitle = unitAxisTitleY(desc.unit,
                                          card.mode == PlotCard::DecodeMode::Physical,
                                          isSpectrum);
    // unitAxes=false keeps the v1 no-title look (REQ-SW-PL-025 §4).
    card.axisTitleX = card.unitAxes ? xTitle : QString();
    card.axisTitleY = card.unitAxes ? yTitle : QString();
}

void DaqDisplayNode::refresh()
{
    if (!m_lastData)
        return;

    const SampledStreamDescriptor &desc = m_lastData->descriptor();
    const int nChannels = desc.totalChannels();

    // Header text — descriptor-driven (REQ-SW-PL-023 §2.8).
    const QString domain = desc.domain.isEmpty() ? QStringLiteral("sampled") : desc.domain;
    const QString rateText = desc.sampleRate > 0.0
                                 ? QStringLiteral("%1 Hz").arg(desc.sampleRate, 0, 'g', 6)
                                 : QStringLiteral("rate n/a");

    QStringList channelNames;
    channelNames.reserve(nChannels);
    for (int i = 0; i < nChannels; ++i) {
        const QString name = desc.channels.at(i).name;
        channelNames.append(name.isEmpty() ? QStringLiteral("Ch%1").arg(i) : name);
    }

    // Clamp the per-card channel to the new channel count and rebind.
    for (PlotCard &card : m_cards) {
        if (nChannels > 0) {
            const int clamped = qBound(0, card.channelIndex, nChannels - 1);
            if (clamped != card.channelIndex) {
                card.channelIndex = clamped;
                bindCardPreprocess(card);
            }
        }
        applyAxisTitlesFor(card, desc);
    }

    const QString header = QStringLiteral("%1 · %2").arg(domain, rateText);

    // refresh() runs on every incoming sample, but plotCardsChanged() makes the
    // widget destroy and rebuild its entire chart tree. Rebuilding that per
    // audio buffer saturated the GUI thread and froze the app. Only broadcast
    // when the descriptor really changed — which is what "channel combos need
    // repopulating" actually means.
    if (m_descriptorBroadcast && header == m_lastDescriptorHeader
        && channelNames == m_lastChannelNames)
        return;

    m_descriptorBroadcast = true;
    m_lastDescriptorHeader = header;
    m_lastChannelNames = channelNames;

    Q_EMIT descriptorInfoChanged(header, channelNames);
    Q_EMIT plotCardsChanged(); // channel combos need repopulating
}

// ── Slots (called by DaqDisplayWidget through NodeWidgetFactory) ───────────

void DaqDisplayNode::onAddPlotRequested()
{
    // New card default: Time Domain, channel 0 (REQ-SW-PL-023 §1); decode mode
    // defaults to physical unless the descriptor unit is "normalized" (v2 §2).
    addPlotCard(QStringLiteral("Plot %1").arg(m_cards.size() + 1),
                PlotCard::ProcessingType::TimeDomain, 0,
                defaultDecodeMode(), /*unitAxes=*/true);
}

void DaqDisplayNode::onRemovePlotRequested(int index)
{
    removeCardAt(index);
}

void DaqDisplayNode::onCardProcessingChanged(int index, int processingType)
{
    if (index < 0 || index >= m_cards.size())
        return;

    PlotCard &card = m_cards[index];
    card.processingType = processingType == int(PlotCard::ProcessingType::FrequencySpectrum)
                              ? PlotCard::ProcessingType::FrequencySpectrum
                              : PlotCard::ProcessingType::TimeDomain;

    bindCardPreprocess(card);

    // Time↔FFT switch changes the unit axis titles (AC 2).
    if (m_lastData)
        applyAxisTitlesFor(card, m_lastData->descriptor());

    m_dataDirty = true; // only this card's config changed; recompute next tick
}

void DaqDisplayNode::onCardChannelChanged(int index, int channelIndex)
{
    if (index < 0 || index >= m_cards.size())
        return;

    PlotCard &card = m_cards[index];
    // Channel combo changed → per-plot channel, independent of other cards.
    card.channelIndex = qMax(0, channelIndex);
    bindCardPreprocess(card);

    m_dataDirty = true;
}

void DaqDisplayNode::onRingSecondsChanged(double seconds)
{
    if (seconds > 0.0)
        m_ringSeconds = seconds;
}

void DaqDisplayNode::onRefreshTick()
{
    if (!m_dataDirty || !m_lastData)
        return;

    // Snapshot immutable copies on the GUI thread; the worker touches only these
    // (REQ-SW-PL-023 §4) — no data mutex.
    const std::shared_ptr<const SampledData> dataCopy = m_lastData;

    QVector<CardConfig> configs;
    configs.reserve(m_cards.size());
    for (const PlotCard &card : m_cards)
        configs.append(CardConfig{card.processingType, card.channelIndex,
                                  card.mode, card.unitAxes});

    m_dataDirty = false;

    // Shared ComputePool (REQ-SW-PL-039): per-key latest-wins submission +
    // per-key serialization. The pool tracks busy-ness per key, so a new block
    // arriving while a task runs becomes pending (skipped) and runs after —
    // the ring buffer (m_ring) stays worker-only.
    const double ringSeconds = m_ringSeconds;
    const quint64 dataGeneration = m_dataGeneration;
    ComputePool::instance().submitLatest(
        m_poolKey,
        [this, dataCopy, configs, ringSeconds, dataGeneration]() {
            if (m_shuttingDown.load())
                return;

            // Ring append + decode + FFT + point build + min/max — ALL on this
            // worker thread (REQ-SW-PL-025 §3/§4, AC 6); m_ring is worker-owned.
            PlotResult result = computePlotResult(dataCopy, configs,
                                                  ringSeconds, dataGeneration);

            // Do not post after shutdown has begun — the node may be gone.
            if (m_shuttingDown.load())
                return;

            QMetaObject::invokeMethod(m_bridge, "onComputeDone",
                                      Qt::QueuedConnection, Q_ARG(PlotResult, result));
        });
}
