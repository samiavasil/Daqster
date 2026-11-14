// SPDX-License-Identifier: MIT

#include "Processing/Audio/BlockResizerNode.h"

#include "NodeDataTypes/SampledData.h"

#include <LogCategories.h>

#include <QJsonValue>

#include <algorithm>
#include <cmath>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

/// A persisted integer; missing / non-numeric / out-of-range keeps @p current.
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

/// A persisted finite double inside [lo, hi]; otherwise @p current.
double readBoundedDouble(const QJsonObject &object, const QString &key, double current,
                         double lo, double hi)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
        return current;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < lo || number > hi)
        return current;
    return number;
}

} // namespace

BlockResizerNode::BlockResizerNode() = default;

BlockResizerNode::~BlockResizerNode() = default;

QJsonObject BlockResizerNode::save() const
{
    QJsonObject obj = QtNodes::NodeDelegateModel::save();
    obj[QStringLiteral("blockSamples")] = m_blockSamples;
    obj[QStringLiteral("blockMs")] = m_blockMs;
    return obj;
}

void BlockResizerNode::load(QJsonObject const &p)
{
    // A partial / damaged document leaves the previous configuration intact
    // (same contract as every other public node, REQ-SW-PL-077 AC2).
    setBlockSamples(readBoundedInt(p, QStringLiteral("blockSamples"), m_blockSamples,
                                   0, 1 << 20));
    setBlockMs(readBoundedDouble(p, QStringLiteral("blockMs"), m_blockMs, 0.0, 1.0e6));
}

QJsonObject BlockResizerNode::configSchema() const
{
    QJsonObject properties;

    properties.insert(QStringLiteral("blockSamples"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("integer")},
        {QStringLiteral("minimum"), 0},
        {QStringLiteral("maximum"), 1 << 20},
        {QStringLiteral("default"), 0},
        {QStringLiteral("x-restart"), false},
        {QStringLiteral("description"),
         QStringLiteral("Frames per output block; 0 = passthrough. Wins over "
                        "blockMs when both are set")}});

    properties.insert(QStringLiteral("blockMs"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("number")},
        {QStringLiteral("minimum"), 0.0},
        {QStringLiteral("maximum"), 1.0e6},
        {QStringLiteral("default"), 0.0},
        {QStringLiteral("x-restart"), false},
        {QStringLiteral("description"),
         QStringLiteral("Milliseconds per output block, converted to frames once "
                        "when the input rate is known; 0 = not configured")}});

    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), properties},
        {QStringLiteral("additionalProperties"), false}};
}

unsigned int BlockResizerNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType BlockResizerNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<NodeData> BlockResizerNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void BlockResizerNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(portIndex);

    const auto input = std::dynamic_pointer_cast<SampledData>(data);
    if (!input || input->buffer().isEmpty() || input->blockSamples() <= 0) {
        m_output.reset();
        Q_EMIT dataInvalidated(0);
        return;
    }

    const SampledStreamDescriptor &descriptor = input->descriptor();
    const QString mismatch = m_fingerprint.mismatch(descriptor);
    if (!mismatch.isEmpty()) {
        setError(mismatch);
        m_output.reset();
        Q_EMIT dataInvalidated(0);
        return;
    }
    if (!m_fingerprint.set)
        m_fingerprint.adopt(descriptor);

    if (!resolveTarget(descriptor))
        return;

    if (m_targetFrames <= 0) {
        // Passthrough: hand the same block on (ref-count bump, no copy).
        m_validation._state = QtNodes::NodeValidationState::State::Valid;
        m_validation._stateMessage.clear();
        m_output = input;
        Q_EMIT dataUpdated(0);
        return;
    }

    // The validated descriptor is what every emitted block carries: the
    // stream cannot change under this node (checked above), so it is resolved
    // once instead of per block.
    m_descriptor = descriptor;
    appendAndEmit(*input);
}

QtNodes::NodeValidationState BlockResizerNode::validationState() const
{
    return m_validation;
}

void BlockResizerNode::setBlockSamples(int blockSamples)
{
    m_blockSamples = std::max(0, blockSamples);
    resetState();
    Q_EMIT configChanged(m_blockSamples, m_blockMs);
}

void BlockResizerNode::setBlockMs(double blockMs)
{
    m_blockMs = (std::isfinite(blockMs) && blockMs > 0.0) ? blockMs : 0.0;
    resetState();
    Q_EMIT configChanged(m_blockSamples, m_blockMs);
}

void BlockResizerNode::resetState()
{
    m_targetResolved = false;
    m_targetFrames = 0;
    m_warnedBothSizes = false;
    m_pending.clear();
    m_pendingFirstIndex = -1;
    m_validation._state = QtNodes::NodeValidationState::State::Valid;
    m_validation._stateMessage.clear();
}

bool BlockResizerNode::resolveTarget(const SampledStreamDescriptor &descriptor)
{
    if (m_targetResolved)
        return true;

    if (m_blockSamples > 0 && m_blockMs > 0.0 && !m_warnedBothSizes) {
        m_warnedBothSizes = true;
        const QString note = QStringLiteral(
            "BlockResizerNode: зададени са и blockSamples=%1, и blockMs=%2 — "
            "blockSamples печели (милисекундите се игнорират)")
                                 .arg(m_blockSamples)
                                 .arg(m_blockMs, 0, 'g', 10);
        qCWarning(lcDemoNodes).noquote() << note;
        Q_EMIT statusChanged(note);
    }

    m_targetResolved = true;
    m_targetFrames = m_blockSamples;

    if (m_targetFrames <= 0 && m_blockMs > 0.0) {
        if (descriptor.sampleRate <= 0.0) {
            setError(QStringLiteral("blockMs=%1 изисква известна честота, а входът не я носи")
                         .arg(m_blockMs, 0, 'g', 10));
            return false;
        }
        // The only place milliseconds become frames (ADR-AI-016 §2).
        const double frames = std::llround(m_blockMs / 1000.0 * descriptor.sampleRate);
        m_targetFrames = int(std::max<qint64>(1, qint64(frames)));
    }

    m_frameBytes = descriptor.bytesPerFrame();
    m_validation._state = QtNodes::NodeValidationState::State::Valid;
    m_validation._stateMessage.clear();
    return true;
}

int BlockResizerNode::pendingFrames() const
{
    return m_frameBytes > 0 ? m_pending.size() / m_frameBytes : 0;
}

void BlockResizerNode::appendAndEmit(const SampledData &input)
{
    const int frameBytes = input.descriptor().bytesPerFrame();
    const int frames = input.blockSamples();
    const qint64 inputFirst = input.firstIndex();

    if (!m_pending.isEmpty() && inputFirst >= 0 && m_pendingFirstIndex >= 0
        && inputFirst != m_pendingFirstIndex + qint64(pendingFrames())) {
        // A hole in the timebase: the carry belongs to a stream that no longer
        // continues here. Gluing across the hole would hide it.
        const QString note = QStringLiteral(
            "BlockResizerNode: дупка в timebase-а (%1 → %2) — захвърлям %3 извадки от carry-то")
                                 .arg(m_pendingFirstIndex + qint64(pendingFrames()))
                                 .arg(inputFirst)
                                 .arg(pendingFrames());
        qCWarning(lcDemoNodes).noquote() << note;
        Q_EMIT statusChanged(note);
        m_pending.clear();
    }

    if (m_pending.isEmpty())
        m_pendingFirstIndex = inputFirst;

    m_pending.append(input.buffer().constData(), frames * frameBytes);

    while (pendingFrames() >= m_targetFrames) {
        const int chunkBytes = m_targetFrames * frameBytes;
        const QByteArray chunk = m_pending.left(chunkBytes);
        m_pending.remove(0, chunkBytes);

        m_output = std::make_shared<SampledData>(chunk, m_descriptor, m_pendingFirstIndex);
        if (m_pendingFirstIndex >= 0)
            m_pendingFirstIndex += m_targetFrames;

        m_validation._state = QtNodes::NodeValidationState::State::Valid;
        m_validation._stateMessage.clear();
        // One emit per produced block: the graph pulls outData() for each
        // dataUpdated, so the loop delivers every block this input completed.
        Q_EMIT dataUpdated(0);
    }
}

void BlockResizerNode::setError(const QString &message)
{
    m_validation._state = QtNodes::NodeValidationState::State::Error;
    m_validation._stateMessage = message;
    qCWarning(lcDemoNodes).noquote() << "BlockResizerNode:" << message;
    Q_EMIT statusChanged(message);
}
