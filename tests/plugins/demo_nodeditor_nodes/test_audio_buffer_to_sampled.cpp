#include <QtTest>
#include <QAudioBuffer>
#include <QAudioFormat>
#include <QByteArray>
#include <QString>
#include <QVector>

#include "AudioBufferToSampled.h"
#include "NodeDataTypes/SampledData.h"
#include "test_audio_buffer_to_sampled.h"

// QAudioBuffer -> SampledData capture glue unit tests (REQ-SW-PL-022 §4,
// REQ-SW-PL-024). Expected values are computed from the exact arithmetic in
// AudioBufferToSampled.h and SampledData.h: descriptorFromFormat sets
// domain "audio", unit "normalized", amplitudeScale 1.0 / amplitudeOffset 0.0,
// and decodeToNormalized divides int16 by 32767.0 (2^15 - 1) and clamps to
// [-1, 1] (SampledDecoder::decodeS16).
namespace {

// Int16 / 44100 Hz / 2 channels / little-endian. Qt5 additionally requires
// the "audio/pcm" codec for QAudioFormat::isValid() (and therefore
// QAudioBuffer::isValid(), which wrapBuffer() checks) to return true; Qt6
// removed codec()/sampleType()/byteOrder() and its isValid() only checks
// sampleFormat/sampleRate/channelCount.
QAudioFormat makeInt16StereoFormat()
{
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    format.setSampleFormat(QAudioFormat::Int16);
#else
    format.setCodec(QStringLiteral("audio/pcm"));
    format.setSampleType(QAudioFormat::SignedInt);
    format.setSampleSize(16);
    format.setByteOrder(QAudioFormat::LittleEndian);
#endif
    return format;
}

// Append one little-endian int16 sample (SampledStreamDescriptor.h layout).
void appendInt16LE(QByteArray &buffer, qint16 value)
{
    const quint16 v = static_cast<quint16>(value);
    const char raw[2] = {
        static_cast<char>(v & 0xFFu),
        static_cast<char>((v >> 8) & 0xFFu),
    };
    buffer.append(raw, 2);
}

} // namespace

void AudioBufferToSampledTest::descriptorFromFormat_int16Stereo()
{
    const QAudioFormat format = makeInt16StereoFormat();
    const SampledStreamDescriptor desc =
        AudioBufferToSampled::descriptorFromFormat(format, QStringLiteral("test"));

    QCOMPARE(desc.sampleRate, 44100.0);
    QCOMPARE(desc.totalChannels(), 2);
    QCOMPARE(static_cast<int>(desc.channels.at(0).sampleType),
             static_cast<int>(SampleType::INT16));
    QCOMPARE(static_cast<int>(desc.channels.at(1).sampleType),
             static_cast<int>(SampleType::INT16));
    QCOMPARE(desc.unit, QStringLiteral("normalized"));
    QCOMPARE(desc.domain, QStringLiteral("audio"));
    QCOMPARE(static_cast<int>(desc.endianness),
             static_cast<int>(SampleEndian::LittleEndian));
    QCOMPARE(desc.amplitudeScale, 1.0);
    QCOMPARE(desc.amplitudeOffset, 0.0);
    QCOMPARE(desc.sourceName, QStringLiteral("test"));
}

void AudioBufferToSampledTest::wrapBuffer_producesExpectedFrames()
{
    constexpr int kFrameCount = 100;
    // 100 frames x 2 channels x 2 bytes (int16) = 400 bytes.
    QByteArray raw;
    raw.fill(0, kFrameCount * 2 * 2);

    const QAudioBuffer buffer(raw, makeInt16StereoFormat());
    QVERIFY(buffer.isValid());

    const auto data = AudioBufferToSampled::wrapBuffer(buffer);
    QVERIFY(data != nullptr);
    QCOMPARE(data->buffer().size(), kFrameCount * 2 * 2);

    QVector<QVector<double>> channels;
    data->decodeToNormalized(channels);

    QCOMPARE(static_cast<int>(channels.size()), 2);
    QCOMPARE(static_cast<int>(channels.at(0).size()), kFrameCount);
    QCOMPARE(static_cast<int>(channels.at(1).size()), kFrameCount);
}

void AudioBufferToSampledTest::wrapBuffer_decodeMatchesInput()
{
    // Known int16 values interleaved across two channels (5 frames): frame i
    // carries [ch0 = values[i]] [ch1 = values[(i + 1) % 5]]. The -32768
    // sample decodes to -32768/32767 = -1.00003... which SampledDecoder
    // clamps to -1.0 - within the 1e-4 tolerance of raw/32767.0.
    const qint16 values[5] = {0, 16384, 32767, -32768, -16384};

    QByteArray raw;
    raw.reserve(5 * 2 * 2);
    for (int i = 0; i < 5; ++i) {
        appendInt16LE(raw, values[i]);
        appendInt16LE(raw, values[(i + 1) % 5]);
    }

    const QAudioBuffer buffer(raw, makeInt16StereoFormat());
    const auto data = AudioBufferToSampled::wrapBuffer(buffer);
    QVERIFY(data != nullptr);

    QVector<QVector<double>> channels;
    data->decodeToNormalized(channels);

    QCOMPARE(static_cast<int>(channels.size()), 2);
    QCOMPARE(static_cast<int>(channels.at(0).size()), 5);
    QCOMPARE(static_cast<int>(channels.at(1).size()), 5);
    for (int i = 0; i < 5; ++i) {
        const double expectedCh0 = static_cast<double>(values[i]) / 32767.0;
        const double expectedCh1 =
            static_cast<double>(values[(i + 1) % 5]) / 32767.0;
        QVERIFY(qAbs(channels.at(0).at(i) - expectedCh0) < 1e-4);
        QVERIFY(qAbs(channels.at(1).at(i) - expectedCh1) < 1e-4);
    }
}

// No QTEST_GUILESS_MAIN here: the audio-buffer test class shares the
// demo_nodeditor_nodes test binary whose main lives in test_main.cpp.
