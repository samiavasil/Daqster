#include <QtTest>
#include <QVector>

#include <cmath>

#include "FftUtil.h"
#include "test_fft_util.h"

// FftUtil::magnitudeSpectrum() unit tests (REQ-SW-PL-023). Every expected
// value below follows from the exact arithmetic in FftUtil.h: the transform
// size is the largest power of two not exceeding both the sample count and
// maxFftSize; the Hann window is the symmetric variant
// (0.5 * (1 - cos(2*pi*i/(fftSize-1)))); magnitudes are normalized by
// 1/fftSize; the result holds bins 0 .. fftSize/2 - 1 (upper half spectrum,
// length fftSize/2, Nyquist bin excluded).
namespace {

constexpr double kTwoPi = 2.0 * 3.14159265358979323846;

// 1 kHz sine sampled at 8 kHz (REQ: peak at bin 2048 * 1000 / 8000 = 256).
QVector<float> makeSineSamples(int sampleCount)
{
    QVector<float> samples;
    samples.reserve(sampleCount);
    for (int i = 0; i < sampleCount; ++i)
        samples.append(static_cast<float>(std::sin(kTwoPi * 1000.0 * i / 8000.0)));
    return samples;
}

} // namespace

void FftUtilTest::sinePeakAtExpectedBin()
{
    // 2048 samples, default maxFftSize 4096 -> fftSize 2048 -> half 1024 bins
    // (indices 0..1023). A 1 kHz sine @ 8 kHz is exactly on-bin at index
    // 2048 * 1000 / 8000 = 256. With the symmetric Hann window the peak is
    // ~0.25 and the two immediate neighbours ~0.125 (ratio ~2).
    const QVector<float> mags = FftUtil::magnitudeSpectrum(makeSineSamples(2048));

    QCOMPARE(static_cast<int>(mags.size()), 1024);

    int peakIndex = 0;
    for (int i = 1; i < mags.size(); ++i) {
        if (mags.at(i) > mags.at(peakIndex))
            peakIndex = i;
    }

    QCOMPARE(peakIndex, 256);
    QVERIFY(qAbs(static_cast<double>(mags.at(peakIndex)) - 0.25) < 0.02);
    // Peak clearly above both immediate neighbours (expected ~0.125 each).
    const double neighborMax = qMax(static_cast<double>(mags.at(peakIndex - 1)),
                                    static_cast<double>(mags.at(peakIndex + 1)));
    QVERIFY(static_cast<double>(mags.at(peakIndex)) > 1.5 * neighborMax);
}

void FftUtilTest::emptyInput_returnsEmpty()
{
    // fftSize starts at 1 and cannot double -> returns {} (FftUtil.h:73-74).
    const QVector<float> mags = FftUtil::magnitudeSpectrum(QVector<float>());
    QVERIFY(mags.isEmpty());
    QCOMPARE(static_cast<int>(mags.size()), 0);
}

void FftUtilTest::singleSample_returnsEmpty()
{
    // 1 sample -> fftSize stays 1 -> returns {} (FftUtil.h:73-74).
    const QVector<float> mags = FftUtil::magnitudeSpectrum(QVector<float>{0.5F});
    QVERIFY(mags.isEmpty());
    QCOMPARE(static_cast<int>(mags.size()), 0);
}

void FftUtilTest::deterministic_output()
{
    const QVector<float> input = makeSineSamples(2048);
    const QVector<float> first = FftUtil::magnitudeSpectrum(input);
    const QVector<float> second = FftUtil::magnitudeSpectrum(input);

    QCOMPARE(static_cast<int>(first.size()), static_cast<int>(second.size()));
    for (int i = 0; i < first.size(); ++i)
        QCOMPARE(first.at(i), second.at(i));
}

void FftUtilTest::respectsMaxFftSize()
{
    // 10000 samples, maxFftSize 4096 -> largest power of two <= min(10000,
    // 4096) is 4096 -> output length = 4096 / 2 = 2048 (FftUtil.h:70-74, 118).
    const QVector<float> mags =
        FftUtil::magnitudeSpectrum(makeSineSamples(10000), 4096);

    QCOMPARE(static_cast<int>(mags.size()), 2048);
}

// No QTEST_GUILESS_MAIN here: the FFT test class shares the
// demo_nodeditor_nodes test binary whose main lives in test_main.cpp.
