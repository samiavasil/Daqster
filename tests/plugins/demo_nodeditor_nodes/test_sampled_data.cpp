#include <QtTest>
#include <QByteArray>
#include <QString>
#include <QVector>

#include <cmath>
#include <cstring>

#include "NodeDataTypes/SampledData.h"
#include "test_sampled_data.h"

// SampledData decoder unit tests (REQ-SW-PL-022 AC 3, REQ-SW-PL-025 AC 1).
// Every expected value below was computed from the exact arithmetic in
// SampledData.h: the normalized int16 path divides by 32767.0 (2^15 - 1) and
// clamps to [-1, 1]; the physical path returns the raw sample (no division,
// no unsigned centering, no clamp) and applies `raw * amplitudeScale +
// amplitudeOffset`.
namespace {

constexpr int kFrameCount = 1024;
constexpr int kChannelCount = 2;
constexpr double kTwoPi = 2.0 * 3.14159265358979323846;

void appendInt16LE(QByteArray &buffer, qint16 value)
{
    const quint16 v = static_cast<quint16>(value);
    const char raw[2] = {
        static_cast<char>(v & 0xFFu),
        static_cast<char>((v >> 8) & 0xFFu),
    };
    buffer.append(raw, 2);
}

void appendUInt16LE(QByteArray &buffer, quint16 value)
{
    const char raw[2] = {
        static_cast<char>(value & 0xFFu),
        static_cast<char>((value >> 8) & 0xFFu),
    };
    buffer.append(raw, 2);
}

void appendFloat32LE(QByteArray &buffer, float value)
{
    quint32 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int i = 0; i < 4; ++i)
        buffer.append(static_cast<char>((bits >> (8 * i)) & 0xFFu));
}

SampledStreamDescriptor makeInt16StereoDescriptor(double scale, double offset)
{
    SampledStreamDescriptor descriptor;
    descriptor.sampleRate = 44100.0;
    descriptor.endianness = SampleEndian::LittleEndian;
    descriptor.unit = QStringLiteral("V");
    descriptor.amplitudeScale = scale;
    descriptor.amplitudeOffset = offset;
    descriptor.channels = {
        StreamChannelDescriptor { QStringLiteral("Ch0"), SampleType::INT16 },
        StreamChannelDescriptor { QStringLiteral("Ch1"), SampleType::INT16 },
    };
    return descriptor;
}

// Synthetic little-endian int16 2-channel SampledData: 1024 frames of a sine
// (ch0) / cosine (ch1) pair scaled to full range, interleaved per frame
// ([ch0_s0][ch1_s0][ch0_s1][ch1_s1]... - SampledStreamDescriptor.h layout).
SampledData makeInt16SineData()
{
    QByteArray buffer;
    buffer.reserve(kFrameCount * kChannelCount * 2);
    for (int i = 0; i < kFrameCount; ++i) {
        const qint16 ch0 = static_cast<qint16>(std::sin(kTwoPi * i / kFrameCount) * 32767.0);
        const qint16 ch1 = static_cast<qint16>(std::cos(kTwoPi * i / kFrameCount) * 32767.0);
        appendInt16LE(buffer, ch0);
        appendInt16LE(buffer, ch1);
    }
    return SampledData(buffer, makeInt16StereoDescriptor(0.001, 0.0));
}

} // namespace

void SampledDataTest::decodeNormalized_returnsInRange()
{
    const SampledData data = makeInt16SineData();

    QVector<QVector<double>> channels;
    data.decodeToNormalized(channels);

    QCOMPARE(static_cast<int>(channels.size()), kChannelCount);
    for (const QVector<double> &channel : channels) {
        QCOMPARE(static_cast<int>(channel.size()), kFrameCount);
        for (double v : channel) {
            QVERIFY(v >= -1.0);
            QVERIFY(v <= 1.0);
        }
    }
}

void SampledDataTest::decodeNormalizedF32_matchesNormalized()
{
    const SampledData data = makeInt16SineData();

    QVector<QVector<double>> normalized;
    data.decodeToNormalized(normalized);

    QVector<QVector<float>> f32;
    data.decodeToNormalizedF32(f32);

    QCOMPARE(static_cast<int>(f32.size()), static_cast<int>(normalized.size()));
    for (int ch = 0; ch < normalized.size(); ++ch) {
        QCOMPARE(static_cast<int>(f32.at(ch).size()), static_cast<int>(normalized.at(ch).size()));
        for (int i = 0; i < normalized.at(ch).size(); ++i) {
            QVERIFY(qAbs(static_cast<double>(f32.at(ch).at(i)) - normalized.at(ch).at(i)) < 1e-4);
        }
    }
}

void SampledDataTest::decodePhysical_int16_scaleOffset()
{
    // raw 32767 * scale 0.001 + offset 0 -> 32.767 V (full-scale int16).
    QByteArray buffer;
    appendInt16LE(buffer, 32767); // ch0
    appendInt16LE(buffer, 0);     // ch1

    const SampledData data(buffer, makeInt16StereoDescriptor(0.001, 0.0));
    QVector<QVector<float>> out;
    data.decodeToPhysical(out);

    QCOMPARE(static_cast<int>(out.size()), 2);
    QCOMPARE(static_cast<int>(out.at(0).size()), 1);
    QCOMPARE(static_cast<int>(out.at(1).size()), 1);
    QVERIFY(qAbs(static_cast<double>(out.at(0).at(0)) - 32.767) < 0.01);
    QVERIFY(qAbs(static_cast<double>(out.at(1).at(0)) - 0.0) < 0.01);

    // Descriptor offset 2.5: raw 0 -> 0 * 0.001 + 2.5 = 2.5 V.
    QByteArray zeroBuffer;
    appendInt16LE(zeroBuffer, 0);
    appendInt16LE(zeroBuffer, 0);
    const SampledData offsetData(zeroBuffer, makeInt16StereoDescriptor(0.001, 2.5));
    QVector<QVector<float>> offsetOut;
    offsetData.decodeToPhysical(offsetOut);
    QVERIFY(qAbs(static_cast<double>(offsetOut.at(0).at(0)) - 2.5) < 0.01);
}

void SampledDataTest::decodePhysical_uint16_noClamp()
{
    // UINT16 is decoded raw (no centering, no clamp): 65535 * 0.001 = 65.535,
    // well above 1.0 -> proves the physical path never clamps to [-1, 1].
    QByteArray buffer;
    appendUInt16LE(buffer, 65535);

    SampledStreamDescriptor descriptor;
    descriptor.sampleRate = 44100.0;
    descriptor.endianness = SampleEndian::LittleEndian;
    descriptor.unit = QStringLiteral("V");
    descriptor.amplitudeScale = 0.001;
    descriptor.amplitudeOffset = 0.0;
    descriptor.channels = {
        StreamChannelDescriptor { QStringLiteral("Ch0"), SampleType::UINT16 },
    };

    const SampledData data(buffer, descriptor);
    QVector<QVector<float>> out;
    data.decodeToPhysical(out);

    QCOMPARE(static_cast<int>(out.size()), 1);
    QCOMPARE(static_cast<int>(out.at(0).size()), 1);
    QVERIFY(qAbs(static_cast<double>(out.at(0).at(0)) - 65.535) < 0.01);
}

void SampledDataTest::decodePhysical_float32_passthrough()
{
    // FLOAT32 passes through raw (no clamp): 5.0 * 2.0 + (-1.0) = 9.0,
    // outside [-1, 1] -> proves floats are not clamped on the physical path.
    QByteArray buffer;
    appendFloat32LE(buffer, 5.0F);

    SampledStreamDescriptor descriptor;
    descriptor.sampleRate = 44100.0;
    descriptor.endianness = SampleEndian::LittleEndian;
    descriptor.unit = QStringLiteral("V");
    descriptor.amplitudeScale = 2.0;
    descriptor.amplitudeOffset = -1.0;
    descriptor.channels = {
        StreamChannelDescriptor { QStringLiteral("Ch0"), SampleType::FLOAT32 },
    };

    const SampledData data(buffer, descriptor);
    QVector<QVector<float>> out;
    data.decodeToPhysical(out);

    QCOMPARE(static_cast<int>(out.size()), 1);
    QCOMPARE(static_cast<int>(out.at(0).size()), 1);
    QVERIFY(qAbs(static_cast<double>(out.at(0).at(0)) - 9.0) < 1e-4);
}

void SampledDataTest::decodePhysical_emptyBuffer()
{
    // decodeToPhysical clears outChannels and returns early when the buffer is
    // empty (SampledData.h:415-419) -> the result is an empty channel vector.
    const SampledData data(QByteArray(), makeInt16StereoDescriptor(0.001, 0.0));
    QVector<QVector<float>> out;
    data.decodeToPhysical(out);
    QVERIFY(out.isEmpty());
    QCOMPARE(static_cast<int>(out.size()), 0);
}

// No QTEST_GUILESS_MAIN here: the sampled-data test class shares the
// demo_nodeditor_nodes test binary whose main lives in test_main.cpp.