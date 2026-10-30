#include "test_audio_resample.h"

#include "AudioResample.h"

#include <QVector>

#include <algorithm>
#include <cmath>

namespace {

/// The scenario from the defect report: a microphone stream at 44.1 kHz played
/// on a 48 kHz device. The ratio (1.0884) is NOT an integer, which is what made
/// the reset phase audible at every block boundary.
constexpr double kSourceRate = 44100.0;
constexpr double kDeviceRate = 48000.0;
/// ~5.7 ms at 44.1 kHz — the block size a live capture delivers, i.e. the
/// defect fired 172 times per second.
constexpr int kBlockFrames = 256;
constexpr int kBlocks = 300;   ///< 76 800 samples ≈ 1.74 s
constexpr double kToneHz = 997.0;   // deliberately not a whole number of output
                                    // samples per period (1000 Hz would repeat
                                    // exactly every 48 outputs and hide a
                                    // boundary pair among identical internal ones)
constexpr double kAmplitude = 0.5;
/// Samples are stored as float and interpolated in double: anything above the
/// float rounding of an interpolated value (≈1e-7) would be a difference in the
/// *phase*, not in the arithmetic.
constexpr double kSampleTolerance = 1e-6;
/// Two consecutive output samples compared against the ideal lattice: the
/// positions carry ~1e-9 of accumulated double error, a stored sample is a
/// float (≈1e-7), so anything above this is a phase difference.
constexpr double kJumpTolerance = 1e-5;
constexpr double kPi = 3.14159265358979323846;

/// @p frames samples of a 1 kHz sine at @p kSourceRate, starting at index 0.
QVector<float> tone(int frames)
{
    QVector<float> out(frames);
    for (int i = 0; i < frames; ++i) {
        out[i] = float(kAmplitude
                       * std::sin(2.0 * kPi * kToneHz * double(i) / kSourceRate));
    }
    return out;
}

/// The ideal output sample at index @p k: linear interpolation of @p source at
/// the CONTINUOUS position `k / ratio`, independent of any block split.
///
/// This is the reference the block-by-block path must match. The old
/// implementation computed `i / ratio` per block, so every boundary restarted
/// the lattice at 0 — the phase error this function makes visible.
float idealAt(const QVector<float> &source, double ratio, int k)
{
    const double pos = double(k) / ratio;
    const int i0 = int(std::floor(pos));
    if (i0 < 0 || i0 >= source.size() - 1)
        return source.constLast();
    const double frac = pos - double(i0);
    return float(double(source.at(i0)) * (1.0 - frac)
                 + double(source.at(i0 + 1)) * frac);
}

} // namespace

void AudioResampleTest::manyBlocksMatchOneBlockAndTheIdealLattice()
{
    const QVector<float> source = tone(kBlockFrames * kBlocks);
    const double ratio = kDeviceRate / kSourceRate;

    AudioResample::RateConverter converter(kSourceRate, kDeviceRate);
    QVector<float> byBlocks;
    byBlocks.reserve(int(double(source.size()) * ratio) + kBlocks);
    for (int block = 0; block < kBlocks; ++block) {
        const QVector<float> out =
            converter.process(source.mid(block * kBlockFrames, kBlockFrames));
        QVERIFY2(!out.isEmpty(), qPrintable(QStringLiteral("block %1 produced nothing")
                                                .arg(block)));
        byBlocks += out;
    }

    const QVector<float> inOneBlock =
        AudioResample::resampleMono(source, kSourceRate, kDeviceRate);

    // 1. The block split must not change the amount of audio: a whole-stream
    //    resample produces round(total * ratio) samples (±1 for the phase
    //    reference the streaming rule sits on).
    const int expected = int(std::llround(double(source.size()) * ratio));
    QVERIFY2(std::abs(byBlocks.size() - expected) <= 1,
             qPrintable(QStringLiteral("300 blocks produced %1 samples, expected %2 (±1)")
                            .arg(byBlocks.size())
                            .arg(expected)));

    // 2. …and must not change the samples themselves: 300 blocks == one block.
    QCOMPARE(byBlocks.size(), inOneBlock.size());
    double maxDifference = 0.0;
    for (int i = 0; i < byBlocks.size(); ++i) {
        const double difference = std::abs(double(byBlocks.at(i))
                                           - double(inOneBlock.at(i)));
        if (difference > maxDifference)
            maxDifference = difference;
    }
    QVERIFY2(maxDifference <= kSampleTolerance,
             qPrintable(QStringLiteral("300 blocks differ from one block of %1 samples by %2")
                            .arg(source.size())
                            .arg(maxDifference, 0, 'g', 6)));

    // 3. Both of them must sit on the ideal lattice — this is what fails when
    //    the phase restarts at every block boundary.
    double maxPhaseError = 0.0;
    int worstIndex = 0;
    for (int i = 0; i < byBlocks.size(); ++i) {
        const double error = std::abs(double(byBlocks.at(i))
                                      - double(idealAt(source, ratio, i)));
        if (error > maxPhaseError) {
            maxPhaseError = error;
            worstIndex = i;
        }
    }
    QVERIFY2(maxPhaseError <= kSampleTolerance,
             qPrintable(QStringLiteral("output sample %1 is %2 away from the ideal "
                                       "phase (block %3 of %4)")
                            .arg(worstIndex)
                            .arg(maxPhaseError, 0, 'g', 6)
                            .arg(worstIndex / (kBlockFrames * 2))
                            .arg(kBlocks)));
}

void AudioResampleTest::blockBoundaryDoesNotJump()
{
    const QVector<float> source = tone(kBlockFrames * kBlocks);
    const double ratio = kDeviceRate / kSourceRate;

    AudioResample::RateConverter converter(kSourceRate, kDeviceRate);
    double maxInsideJump = 0.0;
    double maxBoundaryJump = 0.0;
    /// Difference between the jump across a boundary and the jump the ideal
    /// lattice predicts for the same two consecutive output samples.
    double maxBoundaryError = 0.0;
    qint64 consumedOutputs = 0;
    int jumpsSeen = 0;
    float previousLast = 0.0f;
    bool havePrevious = false;

    for (int block = 0; block < kBlocks; ++block) {
        const QVector<float> out =
            converter.process(source.mid(block * kBlockFrames, kBlockFrames));
        QVERIFY(!out.isEmpty());

        if (havePrevious) {
            const double jump = std::abs(double(out.constFirst()) - double(previousLast));
            maxBoundaryJump = std::max(maxBoundaryJump, jump);
            const double idealJump =
                std::abs(double(idealAt(source, ratio, int(consumedOutputs)))
                         - double(idealAt(source, ratio, int(consumedOutputs) - 1)));
            maxBoundaryError = std::max(maxBoundaryError, std::abs(jump - idealJump));
        }
        for (int i = 1; i < out.size(); ++i) {
            maxInsideJump = std::max(maxInsideJump,
                                     std::abs(double(out.at(i)) - double(out.at(i - 1))));
            ++jumpsSeen;
        }

        consumedOutputs += out.size();
        previousLast = out.constLast();
        havePrevious = true;
    }

    QVERIFY(jumpsSeen > 1000);        // the measurement is not vacuous
    QVERIFY(maxInsideJump > 0.0);
    // A boundary is just another pair of consecutive samples: with a continuous
    // phase its jump belongs to the same distribution as the inside ones. The
    // stateless resampler repeated the first sample of the block (and
    // interpolated the last one towards itself), so the boundary advanced by a
    // whole input sample instead of 1/ratio — measured 1.0884x here, i.e. a
    // click 172 times per second.
    QVERIFY2(maxBoundaryJump <= maxInsideJump,
             qPrintable(QStringLiteral("jump across a block boundary %1 exceeds the "
                                       "largest jump inside a block %2 (x%3)")
                            .arg(maxBoundaryJump, 0, 'g', 6)
                            .arg(maxInsideJump, 0, 'g', 6)
                            .arg(maxBoundaryJump / maxInsideJump, 0, 'f', 2)));
    // …and it is not merely "smaller": the boundary pair IS the ideal pair.
    QVERIFY2(maxBoundaryError <= kJumpTolerance,
             qPrintable(QStringLiteral("a sample across a block boundary is %1 away "
                                       "from the ideal lattice")
                            .arg(maxBoundaryError, 0, 'g', 6)));
}

void AudioResampleTest::equalRatesArePassThroughAndConsumeNoState()
{
    const QVector<float> block = tone(kBlockFrames);

    // qFuzzyCompare fast path, both stateless entry points: the samples must
    // come back untouched (the 48 kHz device playback is copy-free because of it).
    QCOMPARE(AudioResample::resampleMono(block, 48000.0, 48000.0), block);
    QCOMPARE(AudioResample::resampleMono(block, kSourceRate, kSourceRate), block);

    const QVector<QVector<float>> channels{block, tone(kBlockFrames * 2)};
    QCOMPARE(AudioResample::resampleChannels(channels, 48000.0, 48000.0), channels);

    AudioResample::RateConverter converter(48000.0, 48000.0);
    QVERIFY(converter.isPassThrough());
    QCOMPARE(converter.process(block), block);
    QCOMPARE(converter.process(block), block);      // still nothing consumed
    // No state was built or advanced: an equal-rate stream cannot shift a phase.
    QVERIFY(converter.channelStates().isEmpty());
    QCOMPARE(converter.produced(), qint64(0));

    QVector<QVector<float>> converted;
    QVERIFY(converter.processChannels(channels, converted));
    QCOMPARE(converted, channels);
    QCOMPARE(converter.produced(), qint64(0));
}

void AudioResampleTest::resetRestartsThePhase()
{
    const QVector<float> source = tone(kBlockFrames * 4);

    AudioResample::RateConverter converter(kSourceRate, kDeviceRate);
    const QVector<float> first = converter.process(source.mid(0, kBlockFrames));
    const QVector<float> second = converter.process(source.mid(kBlockFrames, kBlockFrames));
    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());

    // The carried phase is real: the second block does NOT start where a fresh
    // converter would start it (that difference is the whole fix).
    AudioResample::RateConverter fresh(kSourceRate, kDeviceRate);
    const QVector<float> freshSecond = fresh.process(source.mid(kBlockFrames, kBlockFrames));
    QVERIFY2(second.constFirst() != freshSecond.constFirst(),
             "the resampler did not carry its phase into the second block");

    converter.reset();
    QCOMPARE(converter.produced(), qint64(0));
    QVERIFY(converter.channelStates().isEmpty());
    QCOMPARE(converter.process(source.mid(0, kBlockFrames)), first);

    // Re-configuring with other rates is a new stream too: the phase must not
    // survive a rate change.
    converter.process(source.mid(kBlockFrames, kBlockFrames));
    converter.configure(48000.0, 44100.0);
    QCOMPARE(converter.produced(), qint64(0));
    converter.configure(kSourceRate, kDeviceRate);
    QCOMPARE(converter.process(source.mid(0, kBlockFrames)), first);
}

void AudioResampleTest::statelessWrappersAgreeWithOneBlockConverter()
{
    const QVector<float> source = tone(kBlockFrames * 2);

    AudioResample::RateConverter oneShot(kSourceRate, kDeviceRate);
    QCOMPARE(AudioResample::resampleMono(source, kSourceRate, kDeviceRate),
             oneShot.process(source));

    // Unusable input stays unusable, never a partial block.
    QVERIFY(AudioResample::resampleMono(QVector<float>(), kSourceRate, kDeviceRate).isEmpty());
    QVERIFY(AudioResample::resampleMono(source, 0.0, kDeviceRate).isEmpty());
    QVERIFY(AudioResample::resampleMono(source, kSourceRate, 0.0).isEmpty());
    QVERIFY(AudioResample::resampleChannels({}, kSourceRate, kDeviceRate).isEmpty());

    const QVector<float> left = tone(kBlockFrames);
    const QVector<float> right = tone(kBlockFrames * 2).mid(kBlockFrames);
    const QVector<QVector<float>> channels{left, right};
    const QVector<QVector<float>> resampled =
        AudioResample::resampleChannels(channels, kSourceRate, kDeviceRate);
    QCOMPARE(resampled.size(), 2);
    QCOMPARE(resampled.at(0), AudioResample::resampleMono(left, kSourceRate, kDeviceRate));
    QCOMPARE(resampled.at(1), AudioResample::resampleMono(right, kSourceRate, kDeviceRate));

    AudioResample::RateConverter converter(kSourceRate, kDeviceRate);
    QVector<QVector<float>> viaConverter;
    QVERIFY(converter.processChannels(channels, viaConverter));
    QCOMPARE(viaConverter, resampled);
}

QTEST_GUILESS_MAIN(AudioResampleTest)
