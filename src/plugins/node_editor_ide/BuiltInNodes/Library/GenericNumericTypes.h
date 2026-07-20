#ifndef GENERICNUMERICTYPES_H
#define GENERICNUMERICTYPES_H

#include <QByteArray>
#include <QVector>
#include <QString>
#include <QtNodes/NodeData>

/**
 * @brief Sample type enumeration for generic numeric data.
 */
enum class SampleType : uint8_t {
    INT16   = 0,   // signed 16-bit integer
    UINT16  = 1,   // unsigned 16-bit integer
    INT32   = 2,   // signed 32-bit integer
    UINT32  = 3,   // unsigned 32-bit integer
    FLOAT32 = 4,   // 32-bit IEEE 754 float
    FLOAT64 = 5,   // 64-bit IEEE 754 double
};

inline int sampleTypeByteSize(SampleType t) {
    switch (t) {
        case SampleType::INT16:   return 2;
        case SampleType::UINT16:  return 2;
        case SampleType::INT32:   return 4;
        case SampleType::UINT32:  return 4;
        case SampleType::FLOAT32: return 4;
        case SampleType::FLOAT64: return 8;
    }
    return 0;
}

inline QString sampleTypeName(SampleType t) {
    switch (t) {
        case SampleType::INT16:   return "int16";
        case SampleType::UINT16:  return "uint16";
        case SampleType::INT32:   return "int32";
        case SampleType::UINT32:  return "uint32";
        case SampleType::FLOAT32: return "float32";
        case SampleType::FLOAT64: return "float64";
    }
    return "unknown";
}

/**
 * @brief Descriptor for a single channel in a generic numeric stream.
 */
struct ChannelDescriptor {
    QString name;           // e.g., "Left", "Right", "R", "G", "B"
    SampleType sampleType;  // numeric type of each sample
};

/**
 * @brief Configuration for a generic numeric stream.
 */
struct GenericStreamConfig {
    int sampleRate = 0;                     // samples per second
    QVector<ChannelDescriptor> channels;    // channel layout

    int totalChannels() const { return channels.size(); }

    int bytesPerFrame() const {
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
 * @brief NodeData type for generic numeric streams.
 *
 * Carries N multiplexed channels with typed samples through the node graph.
 * Display nodes use decodeToNormalized() to get plot-ready data.
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
    int sampleRate() const { return m_buffer.config.sampleRate; }
    const GenericStreamConfig& config() const { return m_buffer.config; }

private:
    MultiplexedBuffer m_buffer;
};

#endif // GENERICNUMERICTYPES_H
