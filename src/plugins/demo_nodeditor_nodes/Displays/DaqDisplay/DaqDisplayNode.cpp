#include "DaqDisplayNode.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <cmath>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

DaqDisplayNode::DaqDisplayNode()
{
    setupUi();
}

DaqDisplayNode::~DaqDisplayNode()
{
}

QJsonObject DaqDisplayNode::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["channel"] = m_currentChannel;
    return modelJson;
}

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

    if (m_lastData)
        refresh();
}

QWidget *DaqDisplayNode::embeddedWidget()
{
    return m_root;
}

// ── Built-in preprocessing functions (v1) ────────────────────────────────

QVector<float> DaqDisplayNode::channelSamples(const SampledData &data, int channel)
{
    QVector<QVector<double>> channels;
    data.decodeToNormalized(channels);
    if (channel < 0 || channel >= channels.size())
        return {};

    QVector<float> out;
    out.reserve(channels.at(channel).size());
    for (double value : channels.at(channel))
        out.append(static_cast<float>(value));
    return out;
}

QVector<float> DaqDisplayNode::spectrumSamples(const SampledData &data, int channel)
{
    const QVector<float> samples = channelSamples(data, channel);
    const int n = samples.size();

    int fftSize = 1;
    while (fftSize * 2 <= n)
        fftSize <<= 1;
    if (fftSize < 2)
        return {};

    // Hann window, then Cooley-Tukey radix-2 DIT (pattern: QDevioDisplayModelUi).
    QVector<double> re(fftSize);
    QVector<double> im(fftSize, 0.0);
    for (int i = 0; i < fftSize; ++i) {
        const double w = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (fftSize - 1)));
        re[i] = samples.at(i) * w;
    }

    // Bit reversal.
    for (int i = 1, j = 0; i < fftSize; ++i) {
        int bit = fftSize >> 1;
        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    // Radix-2 DIT butterflies.
    for (int len = 2; len <= fftSize; len <<= 1) {
        const double ang = 2.0 * M_PI / len;
        const double wRe = std::cos(ang);
        const double wIm = -std::sin(ang);
        for (int i = 0; i < fftSize; i += len) {
            double curRe = 1.0, curIm = 0.0;
            const int half = len / 2;
            for (int j = 0; j < half; ++j) {
                const double tRe = curRe * re[i + j + half] - curIm * im[i + j + half];
                const double tIm = curRe * im[i + j + half] + curIm * re[i + j + half];
                re[i + j + half] = re[i + j] - tRe;
                im[i + j + half] = im[i + j] - tIm;
                re[i + j] += tRe;
                im[i + j] += tIm;
                const double tmp = curRe * wRe - curIm * wIm;
                curIm = curRe * wIm + curIm * wRe;
                curRe = tmp;
            }
        }
    }

    // Magnitude spectrum, upper half (0 .. Nyquist).
    const int half = fftSize / 2;
    QVector<float> out;
    out.reserve(half);
    for (int i = 0; i < half; ++i)
        out.append(static_cast<float>(std::sqrt(re[i] * re[i] + im[i] * im[i])
                                      / double(fftSize)));
    return out;
}

// ── UI ───────────────────────────────────────────────────────────────────

void DaqDisplayNode::setupUi()
{
    m_root = new QWidget();
    auto *rootLayout = new QVBoxLayout(m_root);
    rootLayout->setContentsMargins(2, 2, 2, 2);
    rootLayout->setSpacing(2);

    // Header: domain/rate label + channel selector.
    auto *header = new QHBoxLayout();
    m_domainLabel = new QLabel(tr("sampled"), m_root);
    m_channelCombo = new QComboBox(m_root);
    m_channelCombo->setMinimumWidth(72);
    header->addWidget(m_domainLabel, 1);
    header->addWidget(new QLabel(tr("Ch:"), m_root));
    header->addWidget(m_channelCombo);
    rootLayout->addLayout(header);

    // Two DataPlot slots: Time Domain (identity) + Frequency Spectrum (FFT).
    m_stack = new QStackedWidget(m_root);

    addSlot(tr("Time Domain"), /*frequencyAxis=*/false, 0,
            [this](const SampledData &d) {
                return channelSamples(d, m_currentChannel);
            });
    addSlot(tr("Frequency Spectrum"), /*frequencyAxis=*/true, 0,
            [this](const SampledData &d) {
                return spectrumSamples(d, m_currentChannel);
            });

    rootLayout->addWidget(m_stack, 1);

    connect(m_channelCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &DaqDisplayNode::onChannelChanged);
}

void DaqDisplayNode::addSlot(const QString &title, bool frequencyAxis,
                             int channelIndex, PreprocessFn preprocess)
{
    PlotSlot slot;
    slot.title = title;
    slot.frequencyAxis = frequencyAxis;
    slot.channelIndex = channelIndex;
    slot.preprocess = std::move(preprocess);

    slot.chart = new QtChartsCompat::Chart();
    slot.chart->setTitle(title);
    slot.chart->legend()->hide();
    slot.chart->setAnimationOptions(QtChartsCompat::Chart::NoAnimation);

    slot.axisX = new QtChartsCompat::ValueAxis();
    slot.axisY = new QtChartsCompat::ValueAxis();
    slot.axisX->setLabelFormat(QStringLiteral("%.1f"));
    slot.axisY->setLabelFormat(QStringLiteral("%.2f"));
    slot.chart->addAxis(slot.axisX, Qt::AlignBottom);
    slot.chart->addAxis(slot.axisY, Qt::AlignLeft);

    slot.series = new QtChartsCompat::LineSeries();
    slot.series->setName(title);
    slot.chart->addSeries(slot.series);
    slot.series->attachAxis(slot.axisX);
    slot.series->attachAxis(slot.axisY);

    auto *view = new QtChartsCompat::ChartView(slot.chart);
    view->setMinimumSize(360, 180);
    view->setRenderHint(QPainter::Antialiasing);

    if (slot.frequencyAxis)
        m_fftChartIndex = m_stack->addWidget(view);
    else
        m_timeChartIndex = m_stack->addWidget(view);
    m_slots.append(std::move(slot));

    m_stack->setCurrentIndex(m_timeChartIndex);
}

void DaqDisplayNode::refresh()
{
    if (!m_lastData || m_updating)
        return;

    m_updating = true;

    const SampledStreamDescriptor &desc = m_lastData->descriptor();

    // (Re)populate the channel selector from the stream descriptor.
    QSignalBlocker blocker(m_channelCombo);
    m_channelCombo->clear();
    for (int i = 0; i < desc.totalChannels(); ++i) {
        const QString name = desc.channels.at(i).name;
        m_channelCombo->addItem(name.isEmpty() ? QStringLiteral("Ch%1").arg(i) : name, i);
    }
    if (m_channelCombo->count() == 0) {
        m_updating = false;
        return;
    }
    m_channelCombo->setCurrentIndex(qBound(0, m_currentChannel, m_channelCombo->count() - 1));
    m_currentChannel = m_channelCombo->currentIndex();

    const QString domain = desc.domain.isEmpty() ? QStringLiteral("sampled") : desc.domain;
    const QString rateText = desc.sampleRate > 0.0
                                 ? QStringLiteral("%1 Hz").arg(desc.sampleRate, 0, 'g', 6)
                                 : QStringLiteral("rate n/a");
    m_domainLabel->setText(QStringLiteral("%1 · %2").arg(domain, rateText));

    for (int i = 0; i < m_slots.size(); ++i)
        updateSlot(i);

    m_updating = false;
}

void DaqDisplayNode::updateSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= m_slots.size() || !m_lastData)
        return;

    const PlotSlot &slot = m_slots.at(slotIndex);
    const QVector<float> values = slot.preprocess(*m_lastData);
    const double sampleRate = m_lastData->descriptor().sampleRate;

    QVector<QPointF> points;
    points.reserve(values.size());

    if (slot.frequencyAxis) {
        // Bin i → i * sampleRate / fftSize Hz; fftSize = 2 * values.size().
        const double fftSize = double(values.size()) * 2.0;
        double maxMag = 0.0;
        for (int i = 0; i < values.size(); ++i) {
            const double x = sampleRate > 0.0 ? double(i) * sampleRate / fftSize
                                              : double(i);
            const double y = values.at(i);
            if (y > maxMag) maxMag = y;
            points.append(QPointF(x, y));
        }
        slot.series->replace(points);
        slot.axisX->setRange(0.0, sampleRate > 0.0 ? sampleRate / 2.0
                                                   : double(values.size()));
        slot.axisY->setRange(0.0, maxMag > 0.0 ? maxMag * 1.05 : 1.0);
    } else {
        // Time-domain waveform: x = sample index / sampleRate (seconds).
        for (int i = 0; i < values.size(); ++i) {
            const double x = sampleRate > 0.0 ? double(i) / sampleRate : double(i);
            points.append(QPointF(x, values.at(i)));
        }
        slot.series->replace(points);
        slot.axisX->setRange(0.0, sampleRate > 0.0
                                      ? double(values.size() - 1) / sampleRate
                                      : double(values.size()));
        slot.axisY->setRange(-1.0, 1.0); // unified decoder normalizes to [-1, 1]
    }
}

void DaqDisplayNode::onChannelChanged(int index)
{
    if (index < 0 || m_updating)
        return;
    m_currentChannel = index;
    if (m_lastData)
        refresh();
}
