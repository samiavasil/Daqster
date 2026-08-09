#ifndef GENERICNUMERICTYPES_H
#define GENERICNUMERICTYPES_H

#include "SampledData.h"
#include "SampledStreamDescriptor.h"

#include <QByteArray>
#include <QVector>
#include <QString>
#include <QtNodes/NodeData>

/**
 * @brief Legacy shim for the pre-REQ-SW-PL-022 generic numeric types.
 *
 * The canonical sample type / channel descriptor now live in
 * src/plugins/common/NodeDataTypes/ (SampledStreamDescriptor.h):
 *   - SampleType (extended to int8..float64)
 *   - SampleEndian
 *   - StreamChannelDescriptor
 *   - SampledStreamDescriptor
 *
 * This header keeps the legacy names GenericStreamConfig / MultiplexedBuffer /
 * GenericNumericData compiling so old consumers build unchanged until they are
 * migrated to SampledData (REQ-SW-PL-022 AC 4). decodeToNormalized() now uses
 * the unified SampledDecoder convention (signed/unsigned ÷ (2^(bits-1) − 1),
 * clamped to [-1, 1]; floats clamped) — the historical 32768.0 divisor and
 * missing clamp are gone (AC 3).
 */

/**
 * @brief Legacy configuration for a generic numeric stream.
 *
 * Backed by the canonical SampledStreamDescriptor; kept for source
 * compatibility until GenericDisplayNode migrates to SampledData.
 */
struct GenericStreamConfig {
    double sampleRate = 0;                     // samples per second (Hz)
    QVector<StreamChannelDescriptor> channels; // channel layout

    int totalChannels() const { return channels.size(); }

    int bytesPerFrame() const
    {
        int total = 0;
        for (const auto& ch : channels)
            total += sampleTypeByteSize(ch.sampleType);
        return total;
    }
};

/**
 * @brief Raw multiplexed buffer with decode support.
 *
 * Layout: [ch0_sample0][ch1_sample0]...[chN_sample0][ch0_sample1]...
 * Each sample is sampleTypeByteSize(ch.sampleType) bytes.
 */
struct MultiplexedBuffer {
    QByteArray data;
    GenericStreamConfig config;

    void decodeToNormalized(QVector<QVector<double>>& outChannels) const;
};

/**
 * @brief Legacy NodeData type for generic numeric streams.
 *
 * Superseded by SampledData (src/plugins/common/NodeDataTypes/SampledData.h,
 * type id {"sample", "Sample"}); kept only for old saved graphs that still
 * reference the "generic_numeric" id.
 */
class GenericNumericData : public QtNodes::NodeData {
public:
    GenericNumericData() = default;
    GenericNumericData(MultiplexedBuffer buffer)
        : m_buffer(std::move(buffer)) {}

    QtNodes::NodeDataType type() const override {
        return {"generic_numeric", "Generic"};
    }

    MultiplexedBuffer& buffer() { return m_buffer; }
    const MultiplexedBuffer& buffer() const { return m_buffer; }

    int channels() const { return m_buffer.config.totalChannels(); }
    double sampleRate() const { return m_buffer.config.sampleRate; }
    const GenericStreamConfig& config() const { return m_buffer.config; }

private:
    MultiplexedBuffer m_buffer;
};

#endif // GENERICNUMERICTYPES_H