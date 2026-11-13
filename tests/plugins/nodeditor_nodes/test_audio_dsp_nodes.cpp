#include "test_audio_dsp_nodes.h"

#include "DspTestHelpers.h"

#include "Processing/Audio/BlockResizerNode.h"
#include "Processing/Audio/SampleDecimatorNode.h"
#include "Processing/Audio/SampleResamplerNode.h"

#include "NodeDataTypes/SampledData.h"
#include "Scene/SceneVariables.h"

#include <QtNodes/NodeDelegateModel>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

constexpr double kFsAudio = 48000.0;

/// A mono float32 descriptor at @p rate — what the rate nodes publish and what
/// their inputs are expected to look like.
SampledStreamDescriptor monoDescriptor(double rate)
{
    SampledStreamDescriptor descriptor;
    descriptor.sampleRate = rate;
    descriptor.domain = QStringLiteral("audio");
    descriptor.unit = QStringLiteral("normalized");
    descriptor.channels.append(
        StreamChannelDescriptor{QStringLiteral("audio"), SampleType::FLOAT32});
    return descriptor;
}

/// One block of mono float32 samples with a known position in the stream.
std::shared_ptr<SampledData> makeBlock(const SampledStreamDescriptor &descriptor,
                                       const QVector<float> &samples,
                                       qint64 firstIndex)
{
    QByteArray buffer;
    buffer.resize(samples.size() * int(sizeof(float)));
    char *out = buffer.data();
    for (float value : samples) {
        qToLittleEndian<float>(value, out);
        out += int(sizeof(float));
    }
    return std::make_shared<SampledData>(std::move(buffer), descriptor, firstIndex);
}

QVector<float> mono(const std::shared_ptr<SampledData> &block)
{
    QVector<QVector<float>> channels;
    block->decodeToNormalizedF32(channels);
    return channels.isEmpty() ? QVector<float>() : channels.constFirst();
}

/// Capture every block a node publishes (one per `dataUpdated`).
struct Emitted {
    QVector<std::shared_ptr<SampledData>> blocks;

    QVector<float> concatenated() const
    {
        QVector<float> all;
        for (const auto &block : blocks)
            all += mono(block);
        return all;
    }
};

void attachCollector(QtNodes::NodeDelegateModel &node, Emitted &sink)
{
    QObject::connect(&node, &QtNodes::NodeDelegateModel::dataUpdated, &node,
                     [&node, &sink](QtNodes::PortIndex) {
                         auto block = std::dynamic_pointer_cast<SampledData>(node.outData(0));
                         if (block)
                             sink.blocks.append(block);
                     });
}

/// Feed @p block, then let the queued emissions (if any) unwind.
void feed(QtNodes::NodeDelegateModel &node, const QVector<float> &samples,
          const SampledStreamDescriptor &descriptor, qint64 firstIndex)
{
    node.setInData(makeBlock(descriptor, samples, firstIndex), 0);
    QCoreApplication::processEvents();
}

/// Point `SceneVariables` at a temporary table for the lifetime of the object.
///
/// The machine table wins over the environment by design (REQ-SW-PL-091), so a
/// test cannot just `qputenv("DAQSTER_DEPS")` on a box that has one — it would
/// read the real dependency root and pass or fail for the wrong reason.
class ScopedSceneTable
{
public:
    explicit ScopedSceneTable(const QString &tablePath)
        : m_oldFile(Daqster::SceneVariables::instance().filePath())
        , m_hadEnv(qEnvironmentVariableIsSet("DAQSTER_SCENE_VARIABLES"))
        , m_oldEnv(qgetenv("DAQSTER_SCENE_VARIABLES"))
    {
        qputenv("DAQSTER_SCENE_VARIABLES", QFile::encodeName(tablePath));
        Daqster::SceneVariables::instance().setFilePath(QString());
    }

    ~ScopedSceneTable()
    {
        if (m_hadEnv)
            qputenv("DAQSTER_SCENE_VARIABLES", m_oldEnv);
        else
            qunsetenv("DAQSTER_SCENE_VARIABLES");
        Daqster::SceneVariables::instance().setFilePath(m_oldFile);
    }

private:
    QString m_oldFile;
    bool m_hadEnv = false;
    QByteArray m_oldEnv;
};

/// Write a one-key scene table (`paths/DAQSTER_DEPS`) into @p directory.
bool writeDepsTable(const QString &directory, const QString &depsRoot)
{
    QJsonObject paths;
    paths.insert(QStringLiteral("DAQSTER_DEPS"), depsRoot);
    QJsonObject root;
    root.insert(QStringLiteral("paths"), paths);

    QFile file(QDir(directory).filePath(QStringLiteral("scene-variables.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

/// Reserve slots: the DSP engines need a warm-up before the first output, so a
/// short input would make "0 blocks" look like a failure.
QVector<float> twoTone(double low, double high, double amplitude, double rate, int n)
{
    QVector<float> samples(n);
    const double wLow = 2.0 * DaqsterDspTest::kPi * low / rate;
    const double wHigh = 2.0 * DaqsterDspTest::kPi * high / rate;
    for (int i = 0; i < n; ++i)
        samples[i] = float(amplitude * std::sin(wLow * i) + amplitude * std::sin(wHigh * i));
    return samples;
}

} // namespace

void TestAudioDspNodes::resizerAggregatesSplitsAndKeepsTheCarry()
{
    const SampledStreamDescriptor descriptor = monoDescriptor(kFsAudio);
    const QVector<float> source = DaqsterDspTest::makeSine(0.5, 440.0, kFsAudio, 960);

    // ── 10 blocks of 960 → 5 blocks of 1920 ────────────────────────────────
    {
        BlockResizerNode node;
        node.setBlockSamples(1920);
        Emitted emitted;
        attachCollector(node, emitted);

        for (int i = 0; i < 10; ++i)
            feed(node, source, descriptor, qint64(i) * 960);

        QCOMPARE(emitted.blocks.size(), 5);
        for (int i = 0; i < emitted.blocks.size(); ++i) {
            QCOMPARE(emitted.blocks.at(i)->blockSamples(), 1920);
            // Continuous: block i starts where block i−1 ended.
            QCOMPARE(emitted.blocks.at(i)->firstIndex(), qint64(i) * 1920);
            if (i > 0) {
                QVERIFY(emitted.blocks.at(i)->isContiguousWith(
                    *emitted.blocks.at(i - 1), emitted.blocks.at(i - 1)->blockSamples()));
            }
        }
        // The signal itself is untouched: 1920 output samples == two inputs.
        const QVector<float> out = mono(emitted.blocks.constFirst());
        QCOMPARE(out.size(), 1920);
        for (int i = 0; i < 960; ++i)
            QCOMPARE(out.at(i), source.at(i));
        for (int i = 0; i < 960; ++i)
            QCOMPARE(out.at(960 + i), source.at(i));
    }

    // ── 2 blocks of 1920 → 4 blocks of 960 (the reverse direction) ─────────
    {
        BlockResizerNode node;
        node.setBlockSamples(960);
        Emitted emitted;
        attachCollector(node, emitted);

        const QVector<float> wide = DaqsterDspTest::makeSine(0.5, 440.0, kFsAudio, 1920);
        feed(node, wide, descriptor, 0);
        feed(node, wide, descriptor, 1920);

        QCOMPARE(emitted.blocks.size(), 4);
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(emitted.blocks.at(i)->blockSamples(), 960);
            QCOMPARE(emitted.blocks.at(i)->firstIndex(), qint64(i) * 960);
        }
    }

    // ── Carry: 700-sample blocks into a 1000-sample target ────────────────
    {
        BlockResizerNode node;
        node.setBlockSamples(1000);
        Emitted emitted;
        attachCollector(node, emitted);

        const QVector<float> chunk = DaqsterDspTest::makeSine(0.5, 440.0, kFsAudio, 700);
        const QVector<float> tail = DaqsterDspTest::makeSine(0.5, 440.0, kFsAudio, 900);

        feed(node, chunk, descriptor, 0);
        QCOMPARE(emitted.blocks.size(), 0);          // 700 < 1000 — carry only
        feed(node, chunk, descriptor, 700);
        QCOMPARE(emitted.blocks.size(), 1);          // 1400 → one block, 400 carried
        QCOMPARE(emitted.blocks.constFirst()->firstIndex(), qint64(0));
        feed(node, chunk, descriptor, 1400);
        QCOMPARE(emitted.blocks.size(), 2);          // 1100 → one block, 100 carried
        QCOMPARE(emitted.blocks.at(1)->firstIndex(), qint64(1000));
        feed(node, tail, descriptor, 2100);
        QCOMPARE(emitted.blocks.size(), 3);          // 100 + 900 = exactly one
        QCOMPARE(emitted.blocks.at(2)->firstIndex(), qint64(2000));
        QCOMPARE(emitted.blocks.at(2)->blockSamples(), 1000);
    }
}

void TestAudioDspNodes::decimatorPreservesToneAndSuppressesAlias()
{
    constexpr double fsOut = 16000.0;
    constexpr double toneLow = 1000.0;
    constexpr double alias = 4000.0;   // 20 kHz folded by the 16 kHz output rate

    SampleDecimatorNode node;
    node.setFactor(3);
    Emitted emitted;
    attachCollector(node, emitted);

    const SampledStreamDescriptor descriptor = monoDescriptor(kFsAudio);
    const QVector<float> input = twoTone(toneLow, 20000.0, 0.5, kFsAudio, 4800);

    // Ten blocks of 480: the engine is exercised as a stream, not one blob.
    for (int i = 0; i < 10; ++i)
        feed(node, input.mid(i * 480, 480), descriptor, qint64(i) * 480);

    QCOMPARE(node.outputSampleRate(), fsOut);
    QVERIFY2(!emitted.blocks.isEmpty(), "дециматорът не издаде нито един блок");

    for (int i = 1; i < emitted.blocks.size(); ++i) {
        QVERIFY2(emitted.blocks.at(i)->isContiguousWith(*emitted.blocks.at(i - 1),
                                                       emitted.blocks.at(i - 1)->blockSamples()),
                 qPrintable(QStringLiteral("timebase-ът се скъса при блок %1").arg(i)));
        QCOMPARE(emitted.blocks.at(i)->sampleRate(), fsOut);
    }

    // The timebase is in OUTPUT samples: block i starts where block i−1 ended,
    // and the first block starts at 0. (`inputIndexForOutput(k) == 3k` is the
    // reverse mapping, deliberately a different unit.)
    qint64 producedSoFar = 0;
    for (const auto &block : emitted.blocks) {
        QCOMPARE(block->firstIndex(), producedSoFar);
        producedSoFar += block->blockSamples();
    }
    QCOMPARE(node.inputIndexForOutput(0), qint64(0));
    QCOMPARE(node.inputIndexForOutput(100), qint64(300));

    const QVector<float> output = emitted.concatenated();
    QVERIFY(output.size() > 1324);
    // 300 warm-up samples, then 1024 = exactly 64 periods of 1 kHz at 16 kHz.
    const QVector<float> window = output.mid(300, 1024);
    const double survivor = DaqsterDspTest::dftAmplitudeAt(window, toneLow, fsOut);
    const double folded = DaqsterDspTest::dftAmplitudeAt(window, alias, fsOut);

    QVERIFY2(std::abs(survivor - 0.5) < 0.005,
             qPrintable(QStringLiteral("1 kHz амплитуда %1 вместо 0.5").arg(survivor, 0, 'g', 8)));

    const double attenuationDb = 20.0 * std::log10(std::max(folded, 1e-12) / survivor);
    qInfo("дециматор: 1 kHz = %.4f, 4 kHz (сгънатият 20 kHz) = %.3g → %.1f dB",
          survivor, folded, attenuationDb);
    QVERIFY2(attenuationDb <= -60.0,
             qPrintable(QStringLiteral("алиасът е само %1 dB под тона").arg(attenuationDb, 0, 'f', 1)));

    // The published rate is the output rate, not the input's.
    QCOMPARE(emitted.blocks.constFirst()->sampleRate(), fsOut);
}

void TestAudioDspNodes::resamplerAppliesRatioAndDocumentsTimebase()
{
    constexpr double fsIn = 44100.0;
    constexpr double fsOut = 48000.0;

    SampleResamplerNode node;
    node.setUp(160);      // 44100 · 160/147 = 48000
    node.setDown(147);
    Emitted emitted;
    attachCollector(node, emitted);

    // The mapping is part of the contract, so it is asserted directly.
    QCOMPARE(node.inputIndexForOutput(0), qint64(0));
    QCOMPARE(node.inputIndexForOutput(1), qint64(0));
    QCOMPARE(node.inputIndexForOutput(160), qint64(147));
    QCOMPARE(node.inputIndexForOutput(320), qint64(294));
    QCOMPARE(node.inputIndexForOutput(147), qint64(135));   // ⌊147·147/160⌋

    const SampledStreamDescriptor descriptor = monoDescriptor(fsIn);
    const QVector<float> input = DaqsterDspTest::makeSine(0.5, 1000.0, fsIn, 4410);
    for (int i = 0; i < 10; ++i)
        feed(node, input.mid(i * 441, 441), descriptor, qint64(i) * 441);

    QCOMPARE(node.outputSampleRate(), fsOut);
    QVERIFY2(!emitted.blocks.isEmpty(), "ресемплерът не издаде нито един блок");

    const QVector<float> output = emitted.concatenated();
    // 4410 · 160/147 = 4800 exactly; the engine keeps a tail, so allow one
    // output block for the pending history.
    QVERIFY2(std::abs(output.size() - 4800) <= 480,
             qPrintable(QStringLiteral("изходът е %1 извадки вместо ~4800").arg(output.size())));

    // Every block's firstIndex is its position in the output stream (the unit
    // `isContiguousWith()` compares); `inputIndexForOutput()` is the reverse
    // mapping into the input timeline.
    qint64 produced = 0;
    for (const auto &block : emitted.blocks) {
        QCOMPARE(block->firstIndex(), produced);
        produced += block->blockSamples();
    }
    QCOMPARE(emitted.blocks.constFirst()->sampleRate(), fsOut);
    QVERIFY(output.size() > 960);
    float peak = 0.0F;
    for (float value : output)
        peak = std::max(peak, std::abs(value));

    // 1000 Hz at 48 kHz is exactly 48 samples per period → 960 = 20 periods,
    // so the DFT bin is leakage-free.
    const double tone = DaqsterDspTest::dftAmplitudeAt(output.mid(300, 960), 1000.0, fsOut);
    qInfo("ресемплер 160/147: %lld извадки, пик %.3f, тон %.4f",
          static_cast<long long>(output.size()), double(peak), tone);

    // A 0.5 sine must come out at 0.5 with no phase whose gain explodes — the
    // regression `antiAliasForResampling` had (peak 31 at phase 0).
    QVERIFY2(peak <= 0.55,
             qPrintable(QStringLiteral("пик %1 при вход 0.5 — фазовото усилване се разминава")
                            .arg(peak, 0, 'g', 6)));
    QVERIFY2(std::abs(tone - 0.5) < 0.02,
             qPrintable(QStringLiteral("1 kHz през ресемплера: %1").arg(tone, 0, 'g', 8)));
}

void TestAudioDspNodes::coefficientsResolveFromRefPathAndDefault()
{
    QTemporaryDir deps;
    QVERIFY(deps.isValid());
    QVERIFY(QDir(deps.path()).mkpath(QStringLiteral("filters")));

    // A valid kernel document: 64 anti-alias taps, normalised by construction.
    const FirCoefficients kernel = FirCoefficients::antiAliasForDecimation(3);
    QJsonArray taps;
    for (double tap : kernel.taps())
        taps.append(tap);
    QJsonObject document;
    document.insert(QStringLiteral("taps"), taps);
    document.insert(QStringLiteral("ref"), QStringLiteral("test/decim3"));
    document.insert(QStringLiteral("sha256"), kernel.sha256OfTaps());

    QFile file(QDir(deps.path()).filePath(QStringLiteral("filters/decim3.json")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(document).toJson(QJsonDocument::Compact));
    file.close();

    QVERIFY(writeDepsTable(deps.path(), deps.path()));
    ScopedSceneTable table(QDir(deps.path()).filePath(QStringLiteral("scene-variables.json")));

    const SampledStreamDescriptor descriptor = monoDescriptor(kFsAudio);
    const QVector<float> block = DaqsterDspTest::makeSine(0.25, 440.0, kFsAudio, 480);

    // ── 1. filterRef under <DEPS>/filters/ ─────────────────────────────────
    {
        SampleDecimatorNode node;
        node.setFactor(3);
        node.setFilterRef(QStringLiteral("decim3"));
        feed(node, block, descriptor, 0);

        QVERIFY2(node.validationState()._state == QtNodes::NodeValidationState::State::Valid,
                 qPrintable(node.coefficientStatus()));
        QVERIFY2(node.coefficientStatus().contains(QStringLiteral("filters/decim3.json")),
                 qPrintable(node.coefficientStatus()));
        QVERIFY2(node.coefficientStatus().contains(kernel.sha256OfTaps().left(8)),
                 qPrintable(node.coefficientStatus()));
    }

    // ── 2. filterPath relative to the scene directory ──────────────────────
    QTemporaryDir scene;
    QVERIFY(scene.isValid());
    QVERIFY(QDir(scene.path()).mkpath(QStringLiteral("filters")));
    QFile sceneFile(QDir(scene.path()).filePath(QStringLiteral("filters/scene-taps.json")));
    QVERIFY(sceneFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    sceneFile.write(QJsonDocument(document).toJson(QJsonDocument::Compact));
    sceneFile.close();

    Daqster::SceneVariables::instance().adoptSceneFile(
        QDir(scene.path()).filePath(QStringLiteral("demo.flow")));
    {
        SampleResamplerNode node;
        node.setUp(3);
        node.setDown(2);
        node.setFilterPath(QStringLiteral("filters/scene-taps.json"));
        feed(node, block, descriptor, 0);

        QVERIFY2(node.validationState()._state == QtNodes::NodeValidationState::State::Valid,
                 qPrintable(node.coefficientStatus()));
        QVERIFY2(node.coefficientStatus().contains(QStringLiteral("path:")),
                 qPrintable(node.coefficientStatus()));
        QVERIFY2(node.coefficientStatus().contains(QStringLiteral("scene-taps.json")),
                 qPrintable(node.coefficientStatus()));
    }

    // ── 3. Nothing configured → the built-in default, and it is reported ────
    {
        SampleDecimatorNode node;
        node.setFactor(3);
        feed(node, block, descriptor, 0);

        QVERIFY2(node.coefficientStatus().startsWith(QStringLiteral("default/anti-alias")),
                 qPrintable(node.coefficientStatus()));
        QVERIFY2(node.coefficientStatus().contains(QStringLiteral("sha256")),
                 qPrintable(node.coefficientStatus()));
    }

    // ── 4. A ref that names a missing file falls through to the default ─────
    {
        SampleDecimatorNode node;
        node.setFactor(3);
        node.setFilterRef(QStringLiteral("no-such-filter"));
        feed(node, block, descriptor, 0);

        QVERIFY2(node.coefficientStatus().contains(QStringLiteral("default/anti-alias")),
                 qPrintable(node.coefficientStatus()));
        QVERIFY2(node.coefficientStatus().contains(QStringLiteral("no-such-filter")),
                 qPrintable(node.coefficientStatus()));
    }
}

void TestAudioDspNodes::invalidCoefficientFileIsAnError()
{
    QTemporaryDir deps;
    QVERIFY(deps.isValid());
    QVERIFY(QDir(deps.path()).mkpath(QStringLiteral("filters")));

    // Two taps: shorter than a usable transition band (kMinTaps = 8).
    QJsonArray taps;
    taps.append(0.5);
    taps.append(0.5);
    QJsonObject document;
    document.insert(QStringLiteral("taps"), taps);
    document.insert(QStringLiteral("ref"), QStringLiteral("test/broken"));

    const QString path = QDir(deps.path()).filePath(QStringLiteral("filters/broken.json"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(document).toJson(QJsonDocument::Compact));
    file.close();

    QVERIFY(writeDepsTable(deps.path(), deps.path()));
    ScopedSceneTable table(QDir(deps.path()).filePath(QStringLiteral("scene-variables.json")));

    SampleDecimatorNode node;
    node.setFactor(3);
    node.setFilterRef(QStringLiteral("broken"));

    QStringList statuses;
    QObject::connect(&node, &SampleDecimatorNode::statusChanged, &node,
                     [&statuses](const QString &text) { statuses << text; });

    feed(node, DaqsterDspTest::makeSine(0.25, 440.0, kFsAudio, 480),
         monoDescriptor(kFsAudio), 0);

    const QtNodes::NodeValidationState state = node.validationState();
    QCOMPARE(state._state, QtNodes::NodeValidationState::State::Error);
    // The message names the file the scene pointed at and the value that failed.
    QVERIFY2(state._stateMessage.contains(QStringLiteral("broken.json")),
             qPrintable(state._stateMessage));
    QVERIFY2(state._stateMessage.contains(QStringLiteral("2")),
             qPrintable(state._stateMessage));
    QVERIFY2(!statuses.isEmpty(), "грешката не стигна до statusChanged");
    QVERIFY(statuses.constLast().contains(QStringLiteral("broken.json")));
}

void TestAudioDspNodes::schemasAreLiveAndCoverSaveKeys()
{
    BlockResizerNode resizer;
    SampleDecimatorNode decimator;
    SampleResamplerNode resampler;

    const QVector<QtNodes::NodeDelegateModel *> models{&resizer, &decimator, &resampler};
    for (QtNodes::NodeDelegateModel *model : models) {
        const auto *schema = dynamic_cast<const Daqster::INodeConfigSchema *>(model);
        QVERIFY2(schema != nullptr, qPrintable(model->name()));
        const QJsonObject json = schema->configSchema();
        QCOMPARE(json.value(QStringLiteral("type")).toString(), QStringLiteral("object"));

        const QJsonObject properties = json.value(QStringLiteral("properties")).toObject();
        QVERIFY2(!properties.isEmpty(), qPrintable(model->name()));

        // Schema keys == save() keys (minus the base's read-only model-name).
        QSet<QString> saved;
        const QJsonObject savedObject = model->save();
        for (auto it = savedObject.constBegin(); it != savedObject.constEnd(); ++it) {
            if (it.key() != QStringLiteral("model-name"))
                saved.insert(it.key());
        }
        QSet<QString> described;
        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it)
            described.insert(it.key());
        QCOMPARE(described, saved);

        // These nodes read their config on every block: no field needs a restart.
        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
            const QJsonObject property = it.value().toObject();
            QVERIFY2(property.contains(QStringLiteral("x-restart")),
                     qPrintable(QStringLiteral("%1.%2 без x-restart").arg(model->name(), it.key())));
            QVERIFY2(property.value(QStringLiteral("x-restart")).isBool()
                         && !property.value(QStringLiteral("x-restart")).toBool(),
                     qPrintable(QStringLiteral("%1.%2 не е живо поле").arg(model->name(), it.key())));
        }
    }
}

QTEST_GUILESS_MAIN(TestAudioDspNodes)
