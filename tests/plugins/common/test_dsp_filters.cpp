#include "test_dsp_filters.h"

#include "DspTestHelpers.h"

#include "Dsp/FirCoefficients.h"
#include "Dsp/FirDecimator.h"
#include "Dsp/PolyphaseResampler.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace {

// Signal generation and the one-bin DFT live in a header because the node
// tests ask the same questions (REQ-SW-PL-092).
using DaqsterDspTest::dftAmplitudeAt;
using DaqsterDspTest::kPi;
using DaqsterDspTest::makeConstant;
using DaqsterDspTest::makeSine;

/// Energy centroid of a signal: the "where is the impulse response" measure.
/// Equals the group delay for a symmetric kernel, and stays meaningful when the
/// response is sampled coarsely (decimator by M).
double energyCentroid(const QVector<float> &signal)
{
    double weighted = 0.0;
    double total = 0.0;
    for (int i = 0; i < signal.size(); ++i) {
        const double e = double(signal.at(i)) * double(signal.at(i));
        weighted += e * double(i);
        total += e;
    }
    if (total == 0.0)
        return 0.0;
    return weighted / total;
}

bool bitIdentical(const QVector<float> &a, const QVector<float> &b)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i) != b.at(i))
            return false;
    }
    return true;
}

/// Positive-going zero crossings in [from, to) — a frequency counter that does
/// not need a spectrum.
int countPositiveZeroCrossings(const QVector<float> &signal, int from, int to)
{
    int count = 0;
    const int end = to < 0 || to > signal.size() ? signal.size() : to;
    for (int i = std::max(from, 1); i < end; ++i) {
        if (signal.at(i - 1) < 0.0F && signal.at(i) >= 0.0F)
            ++count;
    }
    return count;
}

QJsonArray tapsToJson(const QVector<double> &taps)
{
    QJsonArray array;
    for (double tap : taps)
        array.append(tap);
    return array;
}

/// |H(f)| of a tap set in dB — the design's own frequency response, evaluated
/// directly instead of through a signal. The −6 dB point and the stopband level
/// of a kernel are properties of the numbers, not of a particular test tone.
double magnitudeDbAt(const QVector<double> &taps, double freq, double sampleRate)
{
    const double w = 2.0 * kPi * freq / sampleRate;
    double re = 0.0;
    double im = 0.0;
    for (int i = 0; i < taps.size(); ++i) {
        re += taps.at(i) * std::cos(w * double(i));
        im -= taps.at(i) * std::sin(w * double(i));
    }
    return 20.0 * std::log10(std::max(std::sqrt(re * re + im * im), 1e-12));
}

} // namespace

void TestDspFilters::defaultKernelIsNormalisedLinearPhaseAndHashed()
{
    const FirCoefficients kernel = FirCoefficients::makeDefault();
    QVERIFY2(kernel.validate(), qPrintable(QStringLiteral("default kernel invalid")));

    // The three numbers the contract names, checked against the contract.
    QCOMPARE(kernel.tapCount(), FirCoefficients::kDefaultTaps);
    QVERIFY2(std::abs(kernel.gain() - 1.0) < 1e-9,
             qPrintable(QStringLiteral("gain %1").arg(kernel.gain(), 0, 'g', 12)));
    QCOMPARE(kernel.delaySamples(), 0.5 * double(FirCoefficients::kDefaultTaps - 1));

    // Linear phase: the window is symmetric, so h[k] == h[N-1-k].
    for (int k = 0; k < kernel.tapCount() / 2; ++k) {
        QVERIFY2(std::abs(kernel.taps().at(k) - kernel.taps().at(kernel.tapCount() - 1 - k))
                     < 1e-15,
                 qPrintable(QStringLiteral("not symmetric at %1").arg(k)));
    }

    // A generated ref says where the numbers came from, and the hash is the
    // identity of the numbers themselves.
    QVERIFY(kernel.ref().startsWith(QStringLiteral("default/")));
    QCOMPARE(kernel.sha256OfTaps().size(), 64);

    // A different cutoff must not hash the same, or "same sha256" would mean
    // nothing.
    const FirCoefficients other = FirCoefficients::makeDefault(64, 0.15, 8.0);
    QVERIFY(other.sha256OfTaps() != kernel.sha256OfTaps());
}

void TestDspFilters::constantInputKeepsUnityDcGain()
{
    // A rate change is not a gain change: the kernel sums to 1 and the
    // resampler's ×up scaling restores what the inserted zeros took away.
    const QVector<float> input = makeConstant(1.0F, 1200);

    FirDecimator decimator;
    QString error;
    QVERIFY2(decimator.configure(3, FirCoefficients::antiAliasForDecimation(3), &error),
             qPrintable(error));
    QVector<float> decimated;
    decimator.process(input.constData(), input.size(), decimated);

    QVERIFY(decimated.size() >= 350);
    for (int i = 200; i < decimated.size(); ++i) {
        QVERIFY2(std::abs(double(decimated.at(i)) - 1.0) < 1e-3,
                 qPrintable(QStringLiteral("decimator DC gain %1 at %2")
                                .arg(double(decimated.at(i)), 0, 'g', 10)
                                .arg(i)));
    }

    PolyphaseResampler resampler;
    QVERIFY2(resampler.configure(3, 2, FirCoefficients::antiAliasForResampling(3, 2), &error),
             qPrintable(error));
    QVector<float> resampled;
    resampler.process(input.constData(), input.size(), resampled);

    QVERIFY(resampled.size() >= 1750);
    for (int i = 400; i < resampled.size(); ++i) {
        QVERIFY2(std::abs(double(resampled.at(i)) - 1.0) < 1e-3,
                 qPrintable(QStringLiteral("resampler DC gain %1 at %2")
                                .arg(double(resampled.at(i)), 0, 'g', 10)
                                .arg(i)));
    }
}

void TestDspFilters::decimatorSuppressesAliasAboveSixtyDb()
{
    // 48 kHz in, ÷3 → 16 kHz out. A 20 kHz tone is above the 8 kHz output
    // Nyquist, so it folds to 4 kHz; a 1 kHz tone passes untouched. Both tones
    // are chosen so the input is exactly 48-sample periodic, and the analysis
    // span (1024 outputs = 3072 inputs = 64 periods) keeps the DFT leakage-free.
    constexpr double fsIn = 48000.0;
    constexpr double fsOut = 16000.0;
    constexpr double toneLow = 1000.0;
    constexpr double toneHigh = 20000.0;
    constexpr double alias = toneHigh - fsOut;  // 4000 Hz
    constexpr double amplitude = 0.5;
    constexpr int inputLength = 4800;

    const double wLow = 2.0 * kPi * toneLow / fsIn;
    const double wHigh = 2.0 * kPi * toneHigh / fsIn;
    QVector<float> input(inputLength);
    for (int i = 0; i < inputLength; ++i) {
        input[i] = float(amplitude * std::sin(wLow * i) + amplitude * std::sin(wHigh * i));
    }

    FirDecimator decimator;
    QString error;
    QVERIFY2(decimator.configure(3, FirCoefficients::antiAliasForDecimation(3), &error),
             qPrintable(error));
    QVector<float> output;
    decimator.process(input.constData(), input.size(), output);
    QCOMPARE(output.size(), inputLength / 3);

    // Skip the kernel's transient (reported latency + a margin), then analyse an
    // exact number of periods of both the survivor and the alias.
    const int skip = int(decimator.latencySamples()) + 100;
    QVector<float> window = output.mid(skip, 1024);
    QCOMPARE(window.size(), 1024);

    const double survivor = dftAmplitudeAt(window, toneLow, fsOut);
    const double folded = dftAmplitudeAt(window, alias, fsOut);
    const double attenuationDb = 20.0 * std::log10(survivor / std::max(folded, 1e-12));

    // Sanity: the measurement itself must see the tone it is told about.
    QVERIFY2(std::abs(survivor - amplitude) < 0.01 * amplitude,
             qPrintable(QStringLiteral("1 kHz survivor amplitude %1").arg(survivor, 0, 'g', 8)));

    qInfo("alias attenuation: %.1f dB (survivor %.4f, folded %.2e)", attenuationDb, survivor,
          folded);
    QVERIFY2(attenuationDb >= 60.0,
             qPrintable(QStringLiteral("20 kHz → 4 kHz alias only %1 dB down")
                            .arg(attenuationDb, 0, 'f', 1)));
}

void TestDspFilters::antiAliasDefaultHoldsSixtyDbAtOutputNyquist()
{
    // 48 kHz → 16 kHz. The default must behave like an anti-alias filter, not a
    // telephone filter: −6 dB at 0.9 × the OUTPUT Nyquist (7.2 kHz) and the
    // stopband edge (the output Nyquist itself, 8 kHz) at least 60 dB down.
    //
    // Measured before this guard: 0.45 put the −6 dB point at 3604 Hz with
    // −20 dB @ 4477 and −60 dB @ 5400, and an A/B through whisper changed the
    // transcript (`they` → `you`) away from the reference.
    constexpr double fsIn = 48000.0;
    constexpr double fsOut = 16000.0;
    constexpr double cutoff = 0.90 * fsOut / 2.0; // 7200 Hz
    const FirCoefficients kernel = FirCoefficients::antiAliasForDecimation(3);
    const QVector<double> &taps = kernel.taps();

    const double atCutoff = magnitudeDbAt(taps, cutoff, fsIn);
    const double atNyquist = magnitudeDbAt(taps, fsOut / 2.0, fsIn);
    qInfo("anti-alias ÷3 default: %d taps, %.2f dB @ %.0f Hz, %.1f dB @ %.0f Hz",
          kernel.tapCount(), atCutoff, cutoff, atNyquist, fsOut / 2.0);

    QVERIFY2(std::abs(atCutoff + 6.0) < 0.5,
             qPrintable(QStringLiteral("−6 dB point is %1 dB at %2 Hz, not at 0.9·Nyquist")
                            .arg(atCutoff, 0, 'f', 2)
                            .arg(cutoff)));
    QVERIFY2(atNyquist <= -60.0,
             qPrintable(QStringLiteral("output Nyquist is only %1 dB down (need 60)")
                            .arg(atNyquist, 0, 'f', 1)));

    // The tap count is the Kaiser estimate, not a guess: 64 taps measure −18 dB
    // at 8 kHz for ×3, 144 taps measure −62 dB. A request below the estimate is
    // raised to it.
    QVERIFY2(kernel.tapCount() >= 154,
             qPrintable(QStringLiteral("only %1 taps").arg(kernel.tapCount())));
    QCOMPARE(FirCoefficients::antiAliasForDecimation(3, 64).tapCount(), 154);

    // The same rule on the lower of the two rates for a resampler: 44.1 → 48 k
    // keeps 0.9 × 22.05 kHz and is 60 dB down by 22.05 kHz.
    const FirCoefficients resampler = FirCoefficients::antiAliasForResampling(160, 147);
    QVERIFY2(magnitudeDbAt(resampler.taps(), 0.90 * 22050.0, 160.0 * 44100.0) < -5.5,
             "resampler 160/147: cutoff is not at 0.9 of the lower Nyquist");
    QVERIFY2(magnitudeDbAt(resampler.taps(), 22050.0, 160.0 * 44100.0) <= -60.0,
             qPrintable(QStringLiteral("resampler 160/147: lower Nyquist is %1 dB down")
                            .arg(magnitudeDbAt(resampler.taps(), 22050.0, 160.0 * 44100.0),
                                 0, 'f', 1)));
}

void TestDspFilters::decimatorPassesSixPointFiveKhz()
{
    // The 0.45 default cut everything above ~3.6 kHz, so a 6.5 kHz tone — which
    // is well inside what a 16 kHz stream can carry — came out ~20 dB down.
    // 0.90 must keep its amplitude within ±5 %.
    constexpr double fsIn = 48000.0;
    constexpr double fsOut = 16000.0;
    constexpr double freq = 6500.0;
    constexpr double amplitude = 0.5;

    const QVector<float> input = makeSine(amplitude, freq, fsIn, 4800);

    FirDecimator decimator;
    QString error;
    QVERIFY2(decimator.configure(3, FirCoefficients::antiAliasForDecimation(3), &error),
             qPrintable(error));
    QVector<float> output;
    decimator.process(input.constData(), input.size(), output);
    QCOMPARE(output.size(), input.size() / 3);

    // 1024 outputs at 16 kHz hold exactly 416 periods of 6.5 kHz → the tone is
    // on a DFT bin and the measurement needs no window.
    const int skip = int(decimator.latencySamples()) + 100;
    const QVector<float> window = output.mid(skip, 1024);
    QCOMPARE(window.size(), 1024);

    const double measured = dftAmplitudeAt(window, freq, fsOut);
    qInfo("6.5 kHz through ÷3: %.4f (input %.4f, %+.1f %%)", measured, amplitude,
          100.0 * (measured - amplitude) / amplitude);
    QVERIFY2(std::abs(measured - amplitude) < 0.05 * amplitude,
             qPrintable(QStringLiteral("6.5 kHz came out at %1, expected %2 ±5 %%")
                            .arg(measured, 0, 'g', 8)
                            .arg(amplitude, 0, 'g', 8)));
}

void TestDspFilters::decimatorPreservesSineAmplitudeAndFrequency()
{
    constexpr double fsIn = 48000.0;
    constexpr double fsOut = 16000.0;
    constexpr double freq = 1000.0;
    constexpr double amplitude = 0.5;
    constexpr int inputLength = 4800;  // 100 ms

    const QVector<float> input = makeSine(amplitude, freq, fsIn, inputLength);

    FirDecimator decimator;
    QString error;
    QVERIFY2(decimator.configure(3, FirCoefficients::antiAliasForDecimation(3), &error),
             qPrintable(error));
    QVector<float> output;
    decimator.process(input.constData(), input.size(), output);

    const int skip = int(decimator.latencySamples()) + 100;
    const int analysed = (output.size() - skip) / 16 * 16;  // whole periods
    QVERIFY(analysed > 0);

    // Amplitude by DFT, not by max|y|: 1 kHz at 16 kHz is exactly 16 samples per
    // period, so which sample sits on the crest is an accident of the phase and
    // can be up to cos(π/16) = 98 % of the amplitude. The bin value is the
    // amplitude; the sample peak is not.
    const double measured = dftAmplitudeAt(output.mid(skip), freq, fsOut);
    QVERIFY2(std::abs(measured - amplitude) < 0.01 * amplitude,
             qPrintable(QStringLiteral("amplitude %1, expected %2")
                            .arg(measured, 0, 'g', 8)
                            .arg(amplitude)));

    // 1 kHz at 16 kHz is 16 samples per period: count the positive-going
    // crossings in a whole number of periods and compare with the expectation.
    const int crossings = countPositiveZeroCrossings(output, skip, skip + analysed);
    const int expected = int(std::lround(double(analysed) * freq / fsOut));
    QVERIFY2(std::abs(crossings - expected) <= 1,
             qPrintable(QStringLiteral("crossings %1, expected %2").arg(crossings).arg(expected)));
}

void TestDspFilters::processingIsDeterministic()
{
    const QVector<float> input = makeSine(0.7, 1234.0, 48000.0, 1000);

    FirDecimator firstDecimator;
    FirDecimator secondDecimator;
    QString error;
    QVERIFY2(firstDecimator.configure(3, FirCoefficients::antiAliasForDecimation(3), &error),
             qPrintable(error));
    QVERIFY2(secondDecimator.configure(3, FirCoefficients::antiAliasForDecimation(3), &error),
             qPrintable(error));
    QVector<float> firstOut;
    QVector<float> secondOut;
    firstDecimator.process(input.constData(), input.size(), firstOut);
    secondDecimator.process(input.constData(), input.size(), secondOut);
    QVERIFY2(bitIdentical(firstOut, secondOut), "decimator output is not reproducible");

    PolyphaseResampler firstResampler;
    PolyphaseResampler secondResampler;
    QVERIFY2(firstResampler.configure(147, 160, FirCoefficients::antiAliasForResampling(147, 160),
                                      &error),
             qPrintable(error));
    QVERIFY2(secondResampler.configure(147, 160, FirCoefficients::antiAliasForResampling(147, 160),
                                       &error),
             qPrintable(error));
    firstResampler.process(input.constData(), input.size(), firstOut);
    secondResampler.process(input.constData(), input.size(), secondOut);
    QVERIFY2(bitIdentical(firstOut, secondOut), "resampler output is not reproducible");
}

void TestDspFilters::reportedLatencyMatchesMeasuredGroupDelay()
{
    // Odd tap count with an even factor keeps the sampled impulse response
    // symmetric, so the energy centroid is the group delay exactly — and a
    // latency that forgot to divide by M (the classic bug) fails by M×.
    // 161 is above every Kaiser estimate involved here (M=2 → 102, M=3 → 153,
    // 3/2 → 153), so the explicit count is what the kernel gets: the wrappers
    // only raise taps that are too few for the 0.90 transition.
    constexpr int taps = 161;
    QVector<float> impulse(1000, 0.0F);
    impulse[0] = 1.0F;

    {
        const FirCoefficients kernel = FirCoefficients::antiAliasForDecimation(2, taps);
        QCOMPARE(kernel.tapCount(), taps);

        FirDecimator decimator;
        QString error;
        QVERIFY2(decimator.configure(2, kernel, &error), qPrintable(error));
        QVector<float> output;
        decimator.process(impulse.constData(), impulse.size(), output);

        const double measured = energyCentroid(output);
        QVERIFY2(std::abs(measured - decimator.latencySamples()) <= 1.0,
                 qPrintable(QStringLiteral("decimator: reported %1, measured %2")
                                .arg(decimator.latencySamples(), 0, 'g', 8)
                                .arg(measured, 0, 'g', 8)));
        QCOMPARE(decimator.latencySamples(), double(taps - 1) / 4.0);
        QCOMPARE(decimator.latencyInputSamples(), double(taps - 1) / 2.0);
    }

    {
        PolyphaseResampler resampler;
        QString error;
        QVERIFY2(resampler.configure(3, 2, FirCoefficients::antiAliasForResampling(3, 2, taps),
                                     &error),
                 qPrintable(error));
        QVector<float> output;
        resampler.process(impulse.constData(), impulse.size(), output);

        const double measured = energyCentroid(output);
        QVERIFY2(std::abs(measured - resampler.latencySamples()) <= 1.0,
                 qPrintable(QStringLiteral("resampler: reported %1, measured %2")
                                .arg(resampler.latencySamples(), 0, 'g', 8)
                                .arg(measured, 0, 'g', 8)));
        QCOMPARE(resampler.latencySamples(), double(taps - 1) / 4.0);
    }

    // A decimator by 3 cannot keep the sampled response symmetric — the delay is
    // fractional (26.67 output samples) and the ±1 sample tolerance is what
    // covers it.
    {
        FirDecimator decimator;
        QString error;
        QVERIFY2(decimator.configure(3, FirCoefficients::antiAliasForDecimation(3, taps), &error),
                 qPrintable(error));
        QVector<float> output;
        decimator.process(impulse.constData(), impulse.size(), output);
        const double measured = energyCentroid(output);
        QVERIFY2(std::abs(measured - decimator.latencySamples()) <= 1.0,
                 qPrintable(QStringLiteral("M=3: reported %1, measured %2")
                                .arg(decimator.latencySamples(), 0, 'g', 8)
                                .arg(measured, 0, 'g', 8)));
    }
}

void TestDspFilters::coefficientFilesLoadAndValidate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString tapsPath = dir.filePath(QStringLiteral("taps.txt"));

    // 64 taps with unit DC gain — a bare-numbers file, "one per line".
    QVector<double> unitTaps(64, 0.0);
    unitTaps[32] = 1.0;
    {
        QFile file(tapsPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("# designed elsewhere\n");
        for (double tap : unitTaps)
            file.write(QStringLiteral("%1\n").arg(tap, 0, 'g', 17).toUtf8());
    }

    QString error;
    const FirCoefficients fromFile = FirCoefficients::fromTapsFile(tapsPath, &error);
    QVERIFY2(!fromFile.isEmpty(), qPrintable(error));
    QVERIFY2(fromFile.validate(&error), qPrintable(error));
    QCOMPARE(fromFile.tapCount(), 64);
    QCOMPARE(fromFile.gain(), 1.0);
    QCOMPARE(fromFile.ref(), QStringLiteral("taps.txt"));
    const QString hash = fromFile.sha256OfTaps();
    QCOMPARE(hash.size(), 64);

    // Same numbers, JSON document: metadata travels with them, and the hash is
    // stable across the two loaders — that is what "reproducible" means.
    QJsonObject document;
    document.insert(QStringLiteral("taps"), tapsToJson(unitTaps));
    document.insert(QStringLiteral("ref"), QStringLiteral("unit/impulse64"));
    document.insert(QStringLiteral("delay"), 31.5);
    document.insert(QStringLiteral("cutoff"), 0.45);
    document.insert(QStringLiteral("sha256"), hash);
    const FirCoefficients fromJson = FirCoefficients::fromJson(document, &error);
    QVERIFY2(!fromJson.isEmpty(), qPrintable(error));
    QCOMPARE(fromJson.sha256OfTaps(), hash);
    QCOMPARE(fromJson.ref(), QStringLiteral("unit/impulse64"));
    QCOMPARE(fromJson.declaredCutoff(), 0.45);
    QCOMPARE(fromJson.declaredDelay(), 31.5);

    // A recorded sha256 that does not match the numbers is a lie, and the
    // loader must say so instead of returning a kernel nobody can trust.
    QJsonObject rotten = document;
    rotten.insert(QStringLiteral("sha256"), QString(64, QLatin1Char('0')));
    QVERIFY(FirCoefficients::fromJson(rotten, &error).isEmpty());
    QVERIFY2(error.contains(QStringLiteral("sha256")), qPrintable(error));

    // Non-normalised: the message has to name the value, otherwise the user has
    // a "DC gain is wrong" and no number to act on.
    QJsonObject unnormalised;
    unnormalised.insert(QStringLiteral("taps"), tapsToJson(QVector<double>(64, 1.0)));
    QVERIFY(FirCoefficients::fromJson(unnormalised, &error).isEmpty());
    QVERIFY2(error.contains(QStringLiteral("DC gain")), qPrintable(error));
    QVERIFY2(error.contains(QStringLiteral("64")), qPrintable(error));

    // Too short / too long are a truncated design or a truncated file.
    {
        QVector<double> shortSet(7, 0.0);
        shortSet[3] = 1.0;
        QJsonObject object;
        object.insert(QStringLiteral("taps"), tapsToJson(shortSet));
        QVERIFY(FirCoefficients::fromJson(object, &error).isEmpty());
        QVERIFY2(error.contains(QStringLiteral("too few")), qPrintable(error));
    }
    {
        QVector<double> longSet(FirCoefficients::kMaxTaps + 1, 0.0);
        longSet[10] = 1.0;
        QJsonObject object;
        object.insert(QStringLiteral("taps"), tapsToJson(longSet));
        QVERIFY(FirCoefficients::fromJson(object, &error).isEmpty());
        QVERIFY2(error.contains(QStringLiteral("too many")), qPrintable(error));
    }

    // A declared delay that contradicts (taps−1)/2 describes a different filter.
    {
        QJsonObject object = document;
        object.insert(QStringLiteral("delay"), 4.0);
        QVERIFY(FirCoefficients::fromJson(object, &error).isEmpty());
        QVERIFY2(error.contains(QStringLiteral("delay")), qPrintable(error));
    }

    // Loading twice must produce the same identity, not just the same numbers.
    const FirCoefficients again = FirCoefficients::fromTapsFile(tapsPath, &error);
    QCOMPARE(again.sha256OfTaps(), hash);

    // A JSON file reached through the taps-file loader (the "I pointed the field
    // at a .json" case) must not fail for a trivial reason.
    const QString jsonPath = dir.filePath(QStringLiteral("kernel.json"));
    {
        QFile file(jsonPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(QJsonDocument(document).toJson());
    }
    const FirCoefficients fromJsonFile = FirCoefficients::fromTapsFile(jsonPath, &error);
    QVERIFY2(!fromJsonFile.isEmpty(), qPrintable(error));
    QCOMPARE(fromJsonFile.sha256OfTaps(), hash);
    QCOMPARE(fromJsonFile.tapCount(), 64);

    // A file whose numbers are not numbers reports the line, not just "failed".
    const QString brokenPath = dir.filePath(QStringLiteral("broken.txt"));
    {
        QFile file(brokenPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("0.5\n0.25\nnot-a-number\n");
    }
    QVERIFY(FirCoefficients::fromTapsFile(brokenPath, &error).isEmpty());
    QVERIFY2(error.contains(QStringLiteral(":3")), qPrintable(error));

    QVERIFY(FirCoefficients::makeDefault().validate(&error));
}

void TestDspFilters::resamplerChunkingDoesNotDrift()
{
    // The property the whole design exists for: the position in the input stream
    // is an integer division of the output index, so chunk boundaries cannot
    // change the result. 5 × 100 must equal 1 × 480, bit for bit.
    constexpr int total = 480;
    constexpr int chunk = 100;
    const QVector<float> input = makeSine(0.8, 440.0, 48000.0, total);

    PolyphaseResampler whole;
    QString error;
    QVERIFY2(whole.configure(3, 2, FirCoefficients::antiAliasForResampling(3, 2), &error),
             qPrintable(error));
    QVector<float> wholeOut;
    whole.process(input.constData(), input.size(), wholeOut);

    PolyphaseResampler chunked;
    QVERIFY2(chunked.configure(3, 2, FirCoefficients::antiAliasForResampling(3, 2), &error),
             qPrintable(error));
    QVector<float> chunkedOut;
    for (int offset = 0; offset < total; offset += chunk) {
        const int frames = std::min(chunk, total - offset);
        QVector<float> piece;
        chunked.process(input.constData() + offset, frames, piece);
        chunkedOut += piece;
    }

    // 480 · 3/2 = 720 outputs, regardless of how the input arrived.
    QCOMPARE(wholeOut.size(), total * 3 / 2);
    QVERIFY2(bitIdentical(wholeOut, chunkedOut),
             qPrintable(QStringLiteral("chunked %1 samples vs whole %2")
                            .arg(chunkedOut.size())
                            .arg(wholeOut.size())));

    // And the same for the decimator, which has its own index arithmetic.
    FirDecimator wholeDecimator;
    FirDecimator chunkedDecimator;
    QVERIFY2(wholeDecimator.configure(3, FirCoefficients::antiAliasForDecimation(3), &error),
             qPrintable(error));
    QVERIFY2(chunkedDecimator.configure(3, FirCoefficients::antiAliasForDecimation(3), &error),
             qPrintable(error));
    QVector<float> wholeDecimated;
    QVector<float> chunkedDecimated;
    wholeDecimator.process(input.constData(), input.size(), wholeDecimated);
    for (int offset = 0; offset < total; offset += chunk) {
        const int frames = std::min(chunk, total - offset);
        QVector<float> piece;
        chunkedDecimator.process(input.constData() + offset, frames, piece);
        chunkedDecimated += piece;
    }
    QVERIFY2(bitIdentical(wholeDecimated, chunkedDecimated), "decimator chunking drifted");
}

void TestDspFilters::interpolatorKeepsUnityGainAcrossPhases()
{
    // A polyphase interpolator is `up` filters in one: output sample i uses
    // branch i·down mod up, and every branch must have the same DC gain (1/up
    // before the engine's ×up scaling). The failure that motivated this test:
    // with a prototype `down×` too wide, the branch containing the kernel's
    // centre got gain ~up·h[0] while its neighbours got almost nothing, so a
    // clean 0.5 sine came out as silence punctuated by spikes of 31.
    struct Case {
        int up;
        int down;
        double inputRate;
        double tone;
    };
    const Case cases[] = {
        {160, 147, 44100.0, 1000.0},   // 44.1 kHz → 48 kHz, the real-world ratio
        {3, 2, 48000.0, 1000.0},       // the small ratio the DC test already used
    };

    for (const Case &c : cases) {
        const double outputRate = c.inputRate * double(c.up) / double(c.down);
        const int taps = std::max(FirCoefficients::kDefaultTaps, 16 * c.up);
        const QVector<float> input = makeSine(0.5, c.tone, c.inputRate, 4410);

        PolyphaseResampler resampler;
        QString error;
        QVERIFY2(resampler.configure(c.up, c.down,
                                     FirCoefficients::antiAliasForResampling(c.up, c.down, taps),
                                     &error),
                 qPrintable(error));

        QVector<float> output;
        resampler.process(input.constData(), input.size(), output);
        QVERIFY(output.size() > 1400);

        double peak = 0.0;
        for (float value : output)
            peak = std::max(peak, std::abs(double(value)));
        // 0.5 in → 0.5 out: anything near `up` means one branch dominates.
        QVERIFY2(peak <= 0.55,
                 qPrintable(QStringLiteral("%1/%2: пик %3 при вход 0.5 — фазовото усилване "
                                           "не е еднакво").arg(c.up).arg(c.down).arg(peak, 0, 'g', 6)));

        // 300 warm-up samples, then 960 = exactly 20 periods of 1 kHz at 48 kHz
        // (and 48 samples/period at 48 kHz; for 72 kHz use its own period count).
        const int periodSamples = int(std::lround(outputRate / c.tone));
        const int window = periodSamples * 20;
        const double measured = dftAmplitudeAt(output.mid(300, window), c.tone, outputRate);
        QVERIFY2(std::abs(measured - 0.5) < 0.01,
                 qPrintable(QStringLiteral("%1/%2: тонът е %3 вместо 0.5")
                                .arg(c.up).arg(c.down).arg(measured, 0, 'g', 8)));
    }
}
