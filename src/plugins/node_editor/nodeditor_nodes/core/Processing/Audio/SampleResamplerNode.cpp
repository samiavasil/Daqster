// SPDX-License-Identifier: MIT

#include "Processing/Audio/SampleResamplerNode.h"

#include "Processing/Audio/FilterResolver.h"

#include <LogCategories.h>

#include <QJsonValue>

#include <algorithm>
#include <cmath>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

constexpr int kFactorMin = 1;
constexpr int kFactorMax = 1024;

int readBoundedInt(const QJsonObject &object, const QString &key, int current,
                   int lo, int hi)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
        return current;
    const int number = value.toInt(current);
    if (number < lo || number > hi)
        return current;
    return number;
}

QString readString(const QJsonObject &object, const QString &key, const QString &current)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString().trimmed() : current;
}

} // namespace

SampleResamplerNode::SampleResamplerNode() = default;

SampleResamplerNode::~SampleResamplerNode() = default;

QJsonObject SampleResamplerNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();
    obj[QStringLiteral("up")] = m_up;
    obj[QStringLiteral("down")] = m_down;
    obj[QStringLiteral("filterRef")] = m_filterRef;
    obj[QStringLiteral("filterPath")] = m_filterPath;
    return obj;
}

void SampleResamplerNode::load(QJsonObject const &p)
{
    setUp(readBoundedInt(p, QStringLiteral("up"), m_up, kFactorMin, kFactorMax));
    setDown(readBoundedInt(p, QStringLiteral("down"), m_down, kFactorMin, kFactorMax));
    setFilterRef(readString(p, QStringLiteral("filterRef"), m_filterRef));
    setFilterPath(readString(p, QStringLiteral("filterPath"), m_filterPath));
}

QJsonObject SampleResamplerNode::configSchema() const
{
    QJsonObject properties;

    properties.insert(QStringLiteral("up"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("integer")},
        {QStringLiteral("minimum"), kFactorMin},
        {QStringLiteral("maximum"), kFactorMax},
        {QStringLiteral("default"), 160},
        {QStringLiteral("x-restart"), false},
        {QStringLiteral("description"),
         QStringLiteral("Interpolation factor L: output rate = input rate × L/M "
                        "(44.1 → 48 kHz is 160/147)")}});

    properties.insert(QStringLiteral("down"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("integer")},
        {QStringLiteral("minimum"), kFactorMin},
        {QStringLiteral("maximum"), kFactorMax},
        {QStringLiteral("default"), 147},
        {QStringLiteral("x-restart"), false},
        {QStringLiteral("description"),
         QStringLiteral("Decimation factor M of the ratio L/M. A change restarts "
                        "the filter and the timebase mapping")}});

    properties.insert(QStringLiteral("filterRef"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("default"), QString()},
        {QStringLiteral("x-restart"), false},
        {QStringLiteral("description"),
         QStringLiteral("Filter name under <DAQSTER_DEPS>/filters (<ref>.json). "
                        "Empty falls through to filterPath, then the built-in kernel")}});

    properties.insert(QStringLiteral("filterPath"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("default"), QString()},
        {QStringLiteral("x-restart"), false},
        {QStringLiteral("description"),
         QStringLiteral("Filter file path (JSON with taps, or bare taps); may be "
                        "scene-relative or carry ${VAR}")}});

    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), properties},
        {QStringLiteral("additionalProperties"), false}};
}

unsigned int SampleResamplerNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType SampleResamplerNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<NodeData> SampleResamplerNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

QtNodes::NodeValidationState SampleResamplerNode::validationState() const
{
    return m_validation;
}

qint64 SampleResamplerNode::inputIndexForOutput(qint64 outputIndex) const
{
    if (outputIndex < 0 || m_up <= 0)
        return SampledData::kUnknownIndex;
    // Integer division of the global output index — exactly the position the
    // polyphase engine evaluates (`q = ⌊i·down/up⌋`), so the timebase cannot
    // drift away from the samples.
    return (outputIndex * qint64(m_down)) / qint64(m_up);
}

void SampleResamplerNode::setUp(int up)
{
    const int clamped = std::max(kFactorMin, std::min(kFactorMax, up));
    if (clamped == m_up)
        return;
    m_up = clamped;
    m_configured = false;
    m_engines.clear();
    restartMapping();
    Q_EMIT configChanged();
}

void SampleResamplerNode::setDown(int down)
{
    const int clamped = std::max(kFactorMin, std::min(kFactorMax, down));
    if (clamped == m_down)
        return;
    m_down = clamped;
    m_configured = false;
    m_engines.clear();
    restartMapping();
    Q_EMIT configChanged();
}

void SampleResamplerNode::setFilterRef(const QString &filterRef)
{
    const QString trimmed = filterRef.trimmed();
    if (trimmed == m_filterRef)
        return;
    m_filterRef = trimmed;
    m_configured = false;
    m_engines.clear();
    Q_EMIT configChanged();
}

void SampleResamplerNode::setFilterPath(const QString &filterPath)
{
    const QString trimmed = filterPath.trimmed();
    if (trimmed == m_filterPath)
        return;
    m_filterPath = trimmed;
    m_configured = false;
    m_engines.clear();
    Q_EMIT configChanged();
}

void SampleResamplerNode::restartMapping()
{
    // A ratio change is a new mapping: counting from the old origin would put
    // every later index in the wrong place. The next block re-anchors it.
    m_origin = SampledData::kUnknownIndex;
    m_outBase = SampledData::kUnknownIndex;
    m_outEmitted = 0;
}

bool SampleResamplerNode::ensureConfigured(int channels)
{
    if (m_configured && m_engines.size() == channels)
        return true;

    const AudioFilterResolution resolution = resolveAudioFilter(
        AudioFilterKind::Resampling, m_up, m_down, m_filterRef, m_filterPath);
    if (!resolution.ok) {
        setError(resolution.describe());
        return false;
    }

    m_engines.clear();
    m_engines.resize(channels);
    QString reason;
    for (PolyphaseResampler &engine : m_engines) {
        if (!engine.configure(m_up, m_down, resolution.coefficients, &reason)) {
            setError(QStringLiteral("%1/%2: %3").arg(m_up).arg(m_down).arg(reason));
            return false;
        }
    }

    m_coefficientStatus = resolution.describe();
    m_configured = true;
    m_validation._state = QtNodes::NodeValidationState::State::Valid;
    m_validation._stateMessage.clear();
    Q_EMIT statusChanged(m_coefficientStatus);
    return true;
}

void SampleResamplerNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(portIndex);

    const auto input = std::dynamic_pointer_cast<SampledData>(data);
    if (!input || input->buffer().isEmpty() || input->blockSamples() <= 0) {
        invalidate();
        return;
    }

    const SampledStreamDescriptor &descriptor = input->descriptor();
    const QString mismatch = m_fingerprint.mismatch(descriptor);
    if (!mismatch.isEmpty()) {
        setError(mismatch);
        invalidate();
        return;
    }
    if (!m_fingerprint.set) {
        m_fingerprint.adopt(descriptor);
        m_origin = input->firstIndex();
        // Output-index base of the stream start: the inverse of the rational
        // mapping (input origin ↔ output `origin·up/down`).
        m_outBase = m_origin >= 0
            ? (m_origin * qint64(m_up)) / qint64(std::max(1, m_down))
            : SampledData::kUnknownIndex;
        m_outEmitted = 0;
    }

    m_outputRate = descriptor.sampleRate * double(m_up) / double(m_down);

    if (!ensureConfigured(descriptor.totalChannels())) {
        invalidate();
        return;
    }

    const QVector<QVector<float>> channels = AudioDspSupport::decodeChannels(*input);
    const int frames = AudioDspSupport::frameCount(channels);
    if (frames <= 0) {
        invalidate();
        return;
    }

    QVector<QVector<float>> resampled(channels.size());
    for (int channel = 0; channel < channels.size(); ++channel) {
        m_engines[channel].process(channels.at(channel).constData(), frames,
                                   resampled[channel]);
    }

    const int produced = AudioDspSupport::frameCount(resampled);
    if (produced <= 0)
        return;   // still filling the kernel — not an error, not a clear

    const qint64 firstIndex = m_outBase >= 0
        ? m_outBase + m_outEmitted
        : SampledData::kUnknownIndex;
    m_outEmitted += produced;

    m_output = AudioDspSupport::makeFloat32Block(*input, m_outputRate, firstIndex, resampled);
    if (!m_output) {
        invalidate();
        return;
    }
    Q_EMIT dataUpdated(0);
}

void SampleResamplerNode::setError(const QString &message)
{
    m_validation._state = QtNodes::NodeValidationState::State::Error;
    m_validation._stateMessage = message;
    qCWarning(lcDemoNodes).noquote() << "SampleResamplerNode:" << message;
    Q_EMIT statusChanged(message);
}

void SampleResamplerNode::invalidate()
{
    m_output.reset();
    Q_EMIT dataInvalidated(0);
}
