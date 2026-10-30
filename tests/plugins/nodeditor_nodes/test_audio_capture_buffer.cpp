#include "test_audio_capture_buffer.h"

#include "AudioCaptureBuffer.h"

#include <QAudioFormat>
#include <QtTest>

namespace {

/// A format built through AudioCompat so it is valid on Qt5 AND Qt6 (the
/// sample-size/sample-type order differs between the two).
QAudioFormat makeFormat(int rate, int channels, AudioCompat::SampleType type, int bits)
{
    QAudioFormat format;
    format.setSampleRate(rate);
    format.setChannelCount(channels);
    AudioCompat::setCodec(format, QStringLiteral("audio/pcm"));   // no-op on Qt6
    AudioCompat::setByteOrder(format, AudioCompat::LittleEndian); // no-op on Qt6
    AudioCompat::setSampleSize(format, bits);                     // order matters on Qt6
    AudioCompat::setSampleType(format, type);
    return format;
}

/// One backend period of the format, in bytes (PipeWire quantum: 1024 frames).
qint64 onePeriodBytes(const QAudioFormat &format)
{
    return 1024LL * format.channelCount() * (AudioCompat::sampleSize(format) / 8);
}

} // namespace

void TestAudioCaptureBuffer::hundredMillisecondsAtFortyEightKilo()
{
    // The format the AudioSource node really opens against a headset mic:
    // 48 kHz mono float32, 4 B per frame.
    const QAudioFormat format = makeFormat(48000, 1, AudioCompat::Float, 32);
    QCOMPARE(AudioCaptureBuffer::bytesFor(format), qint64(4800 * 4));
}

void TestAudioCaptureBuffer::requestStaysAboveOneBackendPeriod()
{
    // Every format the node can open must ask for more than the backend can
    // consume in one period, otherwise Qt6 reads only this many bytes per
    // period and the stream runs at a fraction of real time (the defect: 1000 B
    // per 21.3 ms = 24 % of real time at 48 kHz mono f32).
    const struct {
        int rate;
        int channels;
        AudioCompat::SampleType type;
        int bits;
    } cases[] = {
        {48000, 1, AudioCompat::Float, 32},     // headset mic, the measured case
        {48000, 2, AudioCompat::SignedInt, 16}, // stereo 16-bit
        {44100, 2, AudioCompat::SignedInt, 16},
        {96000, 2, AudioCompat::Float, 32},     // hi-res capture
    };

    for (const auto &c : cases) {
        const QAudioFormat format = makeFormat(c.rate, c.channels, c.type, c.bits);
        const qint64 requested = AudioCaptureBuffer::bytesFor(format);
        QVERIFY2(requested > onePeriodBytes(format),
                 qPrintable(QStringLiteral("%1 Hz %2 ch %3 bit: %4 B <= one period of %5 B")
                                .arg(c.rate)
                                .arg(c.channels)
                                .arg(c.bits)
                                .arg(requested)
                                .arg(onePeriodBytes(format))));
        // …and it stays a bounded latency, not "whatever the backend wants":
        // at most ~1.1x the 100 ms target above the frame floor.
        const qint64 hundredMs = qint64(c.rate) * (c.channels * c.bits / 8) / 10;
        QVERIFY2(requested <= hundredMs * 11 / 10 + 4 * 1024 * (c.channels * c.bits / 8),
                 qPrintable(QString::number(requested)));
    }
}

void TestAudioCaptureBuffer::lowSampleRateIsFlooredInFrames()
{
    // 8 kHz mono 16-bit: 100 ms is 800 frames = 1600 B, i.e. LESS than one
    // 1024-frame period in time (128 ms). The frame floor has to win, or the
    // starvation comes back at low rates.
    const QAudioFormat format = makeFormat(8000, 1, AudioCompat::SignedInt, 16);
    QCOMPARE(AudioCaptureBuffer::bytesFor(format), qint64(AudioCaptureBuffer::kMinFrames * 2));
    QVERIFY(AudioCaptureBuffer::bytesFor(format) > onePeriodBytes(format));
}

void TestAudioCaptureBuffer::bytesScaleWithChannelsAndSampleSize()
{
    const qint64 mono16 = AudioCaptureBuffer::bytesFor(makeFormat(48000, 1, AudioCompat::SignedInt, 16));
    const qint64 mono32 = AudioCaptureBuffer::bytesFor(makeFormat(48000, 1, AudioCompat::Float, 32));
    const qint64 stereo16 = AudioCaptureBuffer::bytesFor(makeFormat(48000, 2, AudioCompat::SignedInt, 16));

    // Same frames, double the frame size.
    QCOMPARE(mono32, mono16 * 2);
    QCOMPARE(stereo16, mono16 * 2);
}

void TestAudioCaptureBuffer::unusableFormatAsksForNothing()
{
    QAudioFormat noRate;                        // everything unset
    QVERIFY(noRate.isValid() == false);
    QCOMPARE(AudioCaptureBuffer::bytesFor(noRate), qint64(0));

    QAudioFormat noChannels = makeFormat(48000, 1, AudioCompat::Float, 32);
    noChannels.setChannelCount(0);
    QCOMPARE(AudioCaptureBuffer::bytesFor(noChannels), qint64(0));
}

QTEST_GUILESS_MAIN(TestAudioCaptureBuffer)
