// SPDX-License-Identifier: MIT

#include "Processing/Audio/SampleDecimatorNode.h"

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

SampleDecimatorNode::SampleDecimatorNode() = default;

SampleDecimatorNode::~SampleDecimatorNode() = default;

QJsonObject SampleDecimatorNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();
    obj[QStringLiteral("factor")] = m_factor;
    obj[QStringLiteral("filterRef")] = m_filterRef;
    obj[QStringLiteral("filterPath")] = m_filterPath;
    obj[QStringLiteral("bypassFilter")] = m_bypassFilter;
    return obj;
}

void SampleDecimatorNode::load(QJsonObject const &p)
{
    setFactor(readBoundedInt(p, QStringLiteral("factor"), m_factor, kFactorMin, kFactorMax));
    setFilterRef(readString(p, QStringLiteral("filterRef"), m_filterRef));
    setFilterPath(readString(p, QStringLiteral("filterPath"), m_filterPath));

    const QJsonValue bypass = p.value(QStringLiteral("bypassFilter"));
    if (bypass.isBool())
        setBypassFilter(bypass.toBool());
}

QJsonObject SampleDecimatorNode::configSchema() const
{
    QJsonObject properties;

    properties.insert(QStringLiteral("factor"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("integer")},
        {QStringLiteral("minimum"), kFactorMin},
        {QStringLiteral("maximum"), kFactorMax},
        {QStringLiteral("default"), 3},
        {QStringLiteral("x-restart"), false},
        {QStringLiteral("description"),
         QStringLiteral("Integer decimation factor M: output rate = input rate / M. "
                        "A change restarts the filter and the timebase mapping")}});

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

    properties.insert(QStringLiteral("bypassFilter"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("boolean")},
        {QStringLiteral("default"), false},
        {QStringLiteral("x-restart"), false},
        {QStringLiteral("description"),
         QStringLiteral("Skip the anti-alias filter (testing only — aliasing is "
                        "then audible/visible on purpose)")}});

    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), properties},
        {QStringLiteral("additionalProperties"), false}};
}

unsigned int SampleDecimatorNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType SampleDecimatorNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<NodeData> SampleDecimatorNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

QtNodes::NodeValidationState SampleDecimatorNode::validationState() const
{
    return m_validation;
}

qint64 SampleDecimatorNode::inputIndexForOutput(qint64 outputIndex) const
{
    if (outputIndex < 0)
        return SampledData::kUnknownIndex;
    return outputIndex * qint64(std::max(1, m_factor));
}

void SampleDecimatorNode::setFactor(int factor)
{
    const int clamped = std::max(kFactorMin, std::min(kFactorMax, factor));
    if (clamped == m_factor)
        return;
    m_factor = clamped;
    // A new factor is a new filter and a new timebase mapping: pretending the
    // old phase still applies would put every later index in the wrong place.
    m_configured = false;
    m_engines.clear();
    m_origin = SampledData::kUnknownIndex;
    m_outBase = SampledData::kUnknownIndex;
    m_outEmitted = 0;
    m_inSeen = 0;
    Q_EMIT configChanged();
}

void SampleDecimatorNode::setFilterRef(const QString &filterRef)
{
    const QString trimmed = filterRef.trimmed();
    if (trimmed == m_filterRef)
        return;
    m_filterRef = trimmed;
    m_configured = false;
    m_engines.clear();
    Q_EMIT configChanged();
}

void SampleDecimatorNode::setFilterPath(const QString &filterPath)
{
    const QString trimmed = filterPath.trimmed();
    if (trimmed == m_filterPath)
        return;
    m_filterPath = trimmed;
    m_configured = false;
    m_engines.clear();
    Q_EMIT configChanged();
}

void SampleDecimatorNode::setBypassFilter(bool bypass)
{
    if (bypass == m_bypassFilter)
        return;
    m_bypassFilter = bypass;
    m_configured = false;
    m_engines.clear();
    Q_EMIT configChanged();
}

bool SampleDecimatorNode::ensureConfigured(int channels)
{
    if (m_configured && m_engines.size() == channels)
        return true;

    const AudioFilterResolution resolution = resolveAudioFilter(
        AudioFilterKind::Decimation, 1, m_factor, m_filterRef, m_filterPath);
    if (!resolution.ok) {
        setError(resolution.describe());
        return false;
    }

    m_engines.clear();
    m_engines.resize(channels);
    QString reason;
    for (FirDecimator &engine : m_engines) {
        if (!engine.configure(m_factor, resolution.coefficients, &reason)) {
            setError(QStringLiteral("factor=%1: %2").arg(m_factor).arg(reason));
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

QVector<QVector<float>> SampleDecimatorNode::decimate(const QVector<QVector<float>> &channels)
{
    const int frames = AudioDspSupport::frameCount(channels);
    QVector<QVector<float>> output(channels.size());

    // M == 1 is not "filter at cutoff 0.45" — that would remove half the band
    // for no reason. It is a passthrough (keep every sample, filtered or not).
    const bool keepEverySample = m_bypassFilter || m_factor <= 1;
    if (keepEverySample) {
        for (int channel = 0; channel < channels.size(); ++channel)
            output[channel].reserve(frames / m_factor + 1);

        for (int i = 0; i < frames; ++i) {
            const qint64 globalIndex = m_inSeen + qint64(i);
            if (m_factor > 1 && globalIndex % m_factor != 0)
                continue;
            for (int channel = 0; channel < channels.size(); ++channel)
                output[channel].append(channels.at(channel).at(i));
        }
        m_inSeen += frames;
        return output;
    }

    for (int channel = 0; channel < channels.size(); ++channel) {
        m_engines[channel].process(channels.at(channel).constData(), frames,
                                   output[channel]);
    }
    return output;
}

void SampleDecimatorNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
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
        // The output is a new stream at its own rate: its start is the input
        // start re-anchored, so the first block keeps the absolute position and
        // later blocks stay continuous in output units.
        m_outBase = m_origin >= 0 ? m_origin / qint64(std::max(1, m_factor))
                                  : SampledData::kUnknownIndex;
        m_outEmitted = 0;
        m_inSeen = 0;
    }

    m_outputRate = descriptor.sampleRate / double(m_factor);

    if (!ensureConfigured(descriptor.totalChannels())) {
        invalidate();
        return;
    }

    const QVector<QVector<float>> channels = AudioDspSupport::decodeChannels(*input);
    if (AudioDspSupport::frameCount(channels) <= 0) {
        invalidate();
        return;
    }

    const QVector<QVector<float>> decimated = decimate(channels);
    const int produced = AudioDspSupport::frameCount(decimated);
    if (produced <= 0) {
        // The filter needs its history before the first output: silence here is
        // not an error and must not clear a downstream node's input.
        return;
    }

    const qint64 firstIndex = m_outBase >= 0
        ? m_outBase + m_outEmitted
        : SampledData::kUnknownIndex;
    m_outEmitted += produced;

    m_output = AudioDspSupport::makeFloat32Block(*input, m_outputRate, firstIndex, decimated);
    if (!m_output) {
        invalidate();
        return;
    }
    Q_EMIT dataUpdated(0);
}

void SampleDecimatorNode::setError(const QString &message)
{
    m_validation._state = QtNodes::NodeValidationState::State::Error;
    m_validation._stateMessage = message;
    qCWarning(lcDemoNodes).noquote() << "SampleDecimatorNode:" << message;
    Q_EMIT statusChanged(message);
}

void SampleDecimatorNode::invalidate()
{
    m_output.reset();
    Q_EMIT dataInvalidated(0);
}
