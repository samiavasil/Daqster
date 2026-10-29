#include "test_audio_sink_node.h"

#include "NodeDataTypes/SampledData.h"
#include "Sinks/Audio/AudioSinkNode.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>
#include <cstring>
#include <memory>

using QtNodes::NodeData;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

/// One mono FLOAT32 block of a 220 Hz tone (amplitude 0 = digital silence).
std::shared_ptr<SampledData> makeBlock(int ms, double rate, double amplitude)
{
    const int frames = int(rate * double(ms) / 1000.0);

    QByteArray raw;
    raw.resize(frames * int(sizeof(float)));
    float *samples = reinterpret_cast<float *>(raw.data());
    for (int i = 0; i < frames; ++i) {
        samples[i] = float(amplitude * qSin(2.0 * M_PI * 220.0 * double(i) / rate));
    }

    SampledStreamDescriptor descriptor;
    descriptor.sampleRate = rate;
    descriptor.domain = QStringLiteral("audio");
    StreamChannelDescriptor channel;
    channel.name = QStringLiteral("Mono");
    channel.sampleType = SampleType::FLOAT32;
    descriptor.channels.append(channel);

    return std::make_shared<SampledData>(raw, descriptor);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

/// Little-endian readers — a WAV header is LE regardless of the host.
quint16 le16(const QByteArray &data, int offset)
{
    const auto *p = reinterpret_cast<const uchar *>(data.constData() + offset);
    return quint16(p[0] | (p[1] << 8));
}

quint32 le32(const QByteArray &data, int offset)
{
    const auto *p = reinterpret_cast<const uchar *>(data.constData() + offset);
    return quint32(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

/// One 16-bit sample of the data chunk.
qint16 sampleAt(const QByteArray &wav, int index)
{
    const int offset = 44 + index * 2;
    return qint16(le16(wav, offset));
}

QStringList sortedKeys(const QJsonObject &object)
{
    QStringList keys = object.keys();
    keys.sort();
    return keys;
}

} // namespace

// ── Contract with the graph model (AC1) ──────────────────────────────────────

void TestAudioSinkNode::nameAndCaptionFollowTheSinkConvention()
{
    AudioSinkNode node;

    // The string a .flow file stores (REQ-SW-PL-057). Changing it is breaking
    // and is explicitly out of scope for this requirement.
    QCOMPARE(node.name(), QStringLiteral("AudioSinkNode"));
    QCOMPARE(node.caption(), QStringLiteral("Audio sink"));

    // Same convention as AudioOutputNode: a caption exists for menus and
    // tooltips, the node body does not draw it.
    QCOMPARE(node.captionVisible(), false);

    // Nothing was recorded yet.
    QCOMPARE(node.blocksReceived(), 0);
    QVERIFY(!node.isRecording());
    QVERIFY(node.lastWrittenPath().isEmpty());
    QVERIFY(node.lastError().isEmpty());
}

void TestAudioSinkNode::portsAndDataTypeAreSampledData()
{
    AudioSinkNode node;

    QCOMPARE(node.nPorts(PortType::In), 1u);
    QCOMPARE(node.nPorts(PortType::Out), 0u);

    // Derived, never a hardcoded {"sample","Sample"} literal: if the audio type
    // ever changes, a hardcoded descriptor would silently stop connecting.
    const QtNodes::NodeDataType expected = SampledData().type();
    const QtNodes::NodeDataType actual = node.dataType(PortType::In, 0);
    QCOMPARE(actual.id, expected.id);
    QCOMPARE(actual.name, expected.name);

    QVERIFY(node.outData(0) == nullptr);
}

void TestAudioSinkNode::dataArrivalChangesNothingVisible()
{
    AudioSinkNode node;

    QCOMPARE(node.dataArrivalChangesGeometry(), false);
    QCOMPARE(node.dataArrivalChangesWidget(), false);
}

// ── Persistence (AC2) ────────────────────────────────────────────────────────

void TestAudioSinkNode::saveCarriesModelName()
{
    AudioSinkNode node;
    const QJsonObject saved = node.save();

    // Written by the BASE save() (REQ-SW-PL-057): a flow file that loses this
    // key loads the node as "<unnamed>" and drops every connection to it.
    QCOMPARE(saved.value(QStringLiteral("model-name")).toString(),
             QStringLiteral("AudioSinkNode"));
}

void TestAudioSinkNode::saveLoadRoundTripsAllThreeFields()
{
    AudioSinkNode source;
    AudioSinkNode::Config config;
    config.outputPath = QStringLiteral("/tmp/daqster-sink-round-trip.wav");
    config.playbackEnabled = true;
    config.volume = 0.35;
    source.setConfig(config);

    const QJsonObject saved = source.save();
    QCOMPARE(sortedKeys(saved),
             (QStringList{QStringLiteral("model-name"), QStringLiteral("outputPath"),
                          QStringLiteral("playbackEnabled"), QStringLiteral("volume")}));

    AudioSinkNode restored;
    restored.load(saved);

    QCOMPARE(restored.outputPath(), config.outputPath);
    QCOMPARE(restored.playbackEnabled(), config.playbackEnabled);
    QCOMPARE(restored.volume(), config.volume);
}

void TestAudioSinkNode::loadPartialKeepsDefaults()
{
    const AudioSinkNode::Config defaults;
    AudioSinkNode node;

    node.load(QJsonObject{{QStringLiteral("model-name"), QStringLiteral("AudioSinkNode")},
                          {QStringLiteral("playbackEnabled"), true}});

    QCOMPARE(node.playbackEnabled(), true);
    QCOMPARE(node.outputPath(), defaults.outputPath);
    QCOMPARE(node.volume(), defaults.volume);

    // No invented path: a node that guesses "out.wav" writes into the working
    // directory of whatever process loaded the scene.
    QCOMPARE(node.outputPath(), QString());
}

void TestAudioSinkNode::loadInvalidValueIsIgnored()
{
    AudioSinkNode node;
    AudioSinkNode::Config config;
    config.outputPath = QStringLiteral("/tmp/kept.wav");
    config.playbackEnabled = true;
    config.volume = 0.50;
    node.setConfig(config);

    // volume out of [0,1]; outputPath not a string; playbackEnabled not a bool
    // (1/0 is a number, not a bool).
    node.load(QJsonObject{{QStringLiteral("volume"), 5.0},
                          {QStringLiteral("outputPath"), 42},
                          {QStringLiteral("playbackEnabled"), 1}});

    QCOMPARE(node.volume(), 0.50);
    QCOMPARE(node.outputPath(), QStringLiteral("/tmp/kept.wav"));
    QCOMPARE(node.playbackEnabled(), true);
}

void TestAudioSinkNode::configSchemaDescribesExactlyWhatSaveEmits()
{
    AudioSinkNode node;
    const QJsonObject schema = node.configSchema();

    QCOMPARE(schema.value(QStringLiteral("type")).toString(), QStringLiteral("object"));

    const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();
    QCOMPARE(sortedKeys(properties), sortedKeys(node.save()));

    // A key save() writes but the schema omits is invisible to the REST config
    // form; a documented key save() drops cannot be restored.
    QVERIFY(!schema.value(QStringLiteral("additionalProperties")).toBool(true));

    QCOMPARE(properties.value(QStringLiteral("volume")).toObject()
                 .value(QStringLiteral("minimum")).toDouble(), 0.0);
    QCOMPARE(properties.value(QStringLiteral("volume")).toObject()
                 .value(QStringLiteral("maximum")).toDouble(), 1.0);
    QCOMPARE(properties.value(QStringLiteral("outputPath")).toObject()
                 .value(QStringLiteral("type")).toString(), QStringLiteral("string"));

    // The deprecated field is marked as such — a remote UI can grey it out.
    QCOMPARE(properties.value(QStringLiteral("playbackEnabled")).toObject()
                 .value(QStringLiteral("deprecated")).toBool(), true);

    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        const QJsonObject field = it.value().toObject();
        QVERIFY2(!field.value(QStringLiteral("type")).toString().isEmpty(),
                 qPrintable(it.key()));
        QVERIFY2(field.contains(QStringLiteral("default"))
                     || field.contains(QStringLiteral("const")),
                 qPrintable(it.key()));
    }
}

void TestAudioSinkNode::loadExpandsEnvironmentVariables()
{
    const QByteArray name = QByteArrayLiteral("DAQSTER_TEST_SINK_DEPS");
    const bool wasSet = qEnvironmentVariableIsSet(name.constData());
    const QByteArray previous = qgetenv(name.constData());
    qputenv(name.constData(), QByteArrayLiteral("/opt/deps"));

    AudioSinkNode node;

    // A set variable is expanded: the .flow stays portable, the node holds a
    // real path.
    node.load(QJsonObject{{QStringLiteral("outputPath"),
                           QStringLiteral("${DAQSTER_TEST_SINK_DEPS}/out.wav")}});
    QCOMPARE(node.outputPath(), QStringLiteral("/opt/deps/out.wav"));

    // save() does not re-expand — it returns what the node holds.
    QCOMPARE(node.save().value(QStringLiteral("outputPath")).toString(),
             QStringLiteral("/opt/deps/out.wav"));

    if (wasSet)
        qputenv(name.constData(), previous);
    else
        qunsetenv(name.constData());

    // `${VAR:-default}` when the variable is missing.
    node.load(QJsonObject{{QStringLiteral("outputPath"),
                           QStringLiteral("${DAQSTER_TEST_SINK_MISSING:-/tmp/fallback.wav}")}});
    QCOMPARE(node.outputPath(), QStringLiteral("/tmp/fallback.wav"));

    // An unknown variable without a default stays literal on purpose: the broken
    // path has to be visible instead of becoming an empty path (= no recording).
    node.load(QJsonObject{{QStringLiteral("outputPath"),
                           QStringLiteral("${DAQSTER_TEST_SINK_MISSING}/x.wav")}});
    QCOMPARE(node.outputPath(), QStringLiteral("${DAQSTER_TEST_SINK_MISSING}/x.wav"));

    // `${VAR:?message}` without a value keeps the whole expression, message
    // included, so the GUI shows which path is broken.
    node.load(QJsonObject{{QStringLiteral("outputPath"),
                           QStringLiteral("${DAQSTER_TEST_SINK_MISSING:?set it}/y.wav")}});
    QCOMPARE(node.outputPath(), QStringLiteral("${DAQSTER_TEST_SINK_MISSING:?set it}/y.wav"));

    // Values are trimmed: a path copied from another machine often carries spaces.
    node.load(QJsonObject{{QStringLiteral("outputPath"),
                           QStringLiteral("  /tmp/spaced.wav  ")}});
    QCOMPARE(node.outputPath(), QStringLiteral("/tmp/spaced.wav"));
}

// ── Recording (AC4) ──────────────────────────────────────────────────────────

void TestAudioSinkNode::oneBlockWritesAValidWav()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    AudioSinkNode sink;
    const QString path = dir.filePath(QStringLiteral("one.wav"));
    sink.setOutputPath(path);

    sink.setInData(makeBlock(100, 24000.0, 0.5), 0);
    sink.stop();

    const QByteArray wav = readFile(path);
    QCOMPARE(wav.size(), 44 + 2400 * 2);   // 100 ms @ 24 kHz, mono, 16 bit

    QCOMPARE(wav.mid(0, 4), QByteArrayLiteral("RIFF"));
    QCOMPARE(wav.mid(8, 4), QByteArrayLiteral("WAVE"));
    QCOMPARE(wav.mid(12, 4), QByteArrayLiteral("fmt "));
    QCOMPARE(wav.mid(36, 4), QByteArrayLiteral("data"));

    QCOMPARE(le16(wav, 22), quint16(1));         // mono
    QCOMPARE(le32(wav, 24), quint32(24000));     // sample rate
    QCOMPARE(le16(wav, 34), quint16(16));        // bits per sample
    QCOMPARE(le32(wav, 4), quint32(36 + 4800));  // RIFF.ChunkSize
    QCOMPARE(le32(wav, 40), quint32(4800));      // data.Subchunk2Size

    QCOMPARE(sink.blocksReceived(), 1);
    QCOMPARE(sink.totalMs(), 100.0);
    QCOMPARE(sink.lastWrittenPath(), path);
    QCOMPARE(sink.dataBytesWritten(), quint32(4800));
}

void TestAudioSinkNode::threeBlocksAreAllInOneValidWav()
{
    // The defect REQ-SW-PL-077 closes: the private node opened the file with
    // Truncate on every block, so a 3-block stream left only block 3 on disk.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    AudioSinkNode sink;
    const QString path = dir.filePath(QStringLiteral("three.wav"));
    sink.setOutputPath(path);

    sink.setInData(makeBlock(100, 24000.0, 0.5), 0);    // 2400 frames, audible
    sink.setInData(makeBlock(50, 24000.0, 0.0), 0);     // 1200 frames, silence
    sink.setInData(makeBlock(25, 24000.0, 0.25), 0);    // 600 frames, audible
    sink.stop();                                        // a second stop is a no-op

    const int totalFrames = 2400 + 1200 + 600;
    const quint32 dataBytes = quint32(totalFrames * 2);

    const QByteArray wav = readFile(path);
    QCOMPARE(wav.size(), int(44 + dataBytes));
    QCOMPARE(le32(wav, 4), quint32(36) + dataBytes);
    QCOMPARE(le32(wav, 40), dataBytes);

    QCOMPARE(sink.blocksReceived(), 3);
    QCOMPARE(sink.totalMs(), 175.0);
    QCOMPARE(sink.dataBytesWritten(), dataBytes);
    QVERIFY(!sink.isRecording());   // stop() closed the file

    // Block 1 is on disk (not overwritten by block 3)...
    QVERIFY(qAbs(sampleAt(wav, 10)) > 0);
    // ...block 2 (the silence) kept its place in the middle...
    QCOMPARE(sampleAt(wav, 2400 + 10), qint16(0));
    // ...and block 3 is the tail.
    QVERIFY(qAbs(sampleAt(wav, 2400 + 1200 + 10)) > 0);
}

void TestAudioSinkNode::existingFileIsReplacedNotAppendedTo()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("old.wav"));
    {
        // Something from an earlier run — a real WAV or garbage, either way it
        // must not be appended to: that would produce one file with two headers.
        QFile stale(path);
        QVERIFY(stale.open(QIODevice::WriteOnly));
        stale.write(QByteArray(1000, 'x'));
    }

    AudioSinkNode sink;
    sink.setOutputPath(path);
    sink.setInData(makeBlock(10, 24000.0, 0.5), 0);   // 240 frames → 480 bytes
    sink.stop();

    const QByteArray wav = readFile(path);
    QCOMPARE(wav.size(), 44 + 480);
    QCOMPARE(wav.mid(0, 4), QByteArrayLiteral("RIFF"));
    QCOMPARE(le32(wav, 40), quint32(480));
}

void TestAudioSinkNode::noPathMeansNoFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    AudioSinkNode sink;   // outputPath empty by default
    QVERIFY(sink.outputPath().isEmpty());

    sink.setInData(makeBlock(10, 24000.0, 0.5), 0);

    QCOMPARE(sink.blocksReceived(), 1);
    QVERIFY(!sink.isRecording());
    QVERIFY(sink.lastWrittenPath().isEmpty());
    QVERIFY(sink.lastError().isEmpty());   // "no recording" is not an error
    QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
}

void TestAudioSinkNode::emptyInputWritesNothing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    AudioSinkNode sink;
    const QString path = dir.filePath(QStringLiteral("empty.wav"));
    sink.setOutputPath(path);

    sink.setInData(nullptr, 0);                            // edge removed
    sink.setInData(std::make_shared<SampledData>(), 0);    // no buffer

    QCOMPARE(sink.blocksReceived(), 0);
    QVERIFY(!QFileInfo::exists(path));   // 0 blocks → no file, not a broken one
}

void TestAudioSinkNode::rateChangeRestartsTheRecording()
{
    // A WAV header describes one rate and one channel count. Two rates in one
    // file would play the first block at the wrong pitch, so the recording
    // restarts instead (documented in the header of the node).
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    AudioSinkNode sink;
    const QString path = dir.filePath(QStringLiteral("rate-change.wav"));
    sink.setOutputPath(path);

    sink.setInData(makeBlock(100, 24000.0, 0.5), 0);
    sink.setInData(makeBlock(100, 16000.0, 0.25), 0);   // 1600 frames → 3200 bytes
    sink.stop();

    const QByteArray wav = readFile(path);
    QCOMPARE(wav.size(), 44 + 3200);
    QCOMPARE(le32(wav, 24), quint32(16000));
    QCOMPARE(le32(wav, 40), quint32(3200));
}

// ── Deprecated playback delegation ───────────────────────────────────────────

void TestAudioSinkNode::enablingPlaybackDoesNotOpenADeviceOnLoad()
{
    // `playbackEnabled` is kept for .flow compatibility, but loading a scene is
    // not a reason to grab the output device (a loaded graph is not a running
    // one) — the same rule AudioOutputNode follows.
    AudioSinkNode node;
    node.load(QJsonObject{{QStringLiteral("outputPath"), QStringLiteral("/tmp/x.wav")},
                          {QStringLiteral("playbackEnabled"), true},
                          {QStringLiteral("volume"), 0.5}});

    QCOMPARE(node.playbackEnabled(), true);
    QCOMPARE(node.playbackEngine()->sinksCreated(), quint64(0));
    QVERIFY(!node.playbackEngine()->isRunning());
}

void TestAudioSinkNode::recordingWorksWithPlaybackEnabledOnADeviceLessBox()
{
    // CI and headless runs have no sound card: playback must degrade to a status,
    // while the WAV — the reason the node exists — is still written.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    AudioSinkNode sink;
    AudioSinkNode::Config config;
    config.outputPath = dir.filePath(QStringLiteral("both.wav"));
    config.playbackEnabled = true;
    sink.setConfig(config);

    sink.setInData(makeBlock(20, 24000.0, 0.4), 0);   // 480 frames → 960 bytes
    sink.stop();

    QCOMPARE(sink.blocksReceived(), 1);
    QCOMPARE(sink.lastError(), QString());
    QCOMPARE(readFile(config.outputPath).size(), 44 + 960);

    // The device path never throws and never blocks, with or without a device.
    QVERIFY(!sink.playbackEngine()->statusText().isEmpty());
}

// ── Lifecycle ────────────────────────────────────────────────────────────────
void TestAudioSinkNode::stopFinalizesAndIsIdempotent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    AudioSinkNode sink;
    const QString path = dir.filePath(QStringLiteral("lifecycle.wav"));
    sink.setOutputPath(path);

    sink.setInData(makeBlock(20, 24000.0, 0.4), 0);
    sink.setInData(makeBlock(20, 24000.0, 0.4), 0);
    QVERIFY(sink.isRecording());

    sink.stop();
    QVERIFY(!sink.isRecording());

    // The header is written while recording, so a second stop() neither reopens
    // nor corrupts the file (REQ-SW-PL-050: stop() must be callable twice).
    const QByteArray once = readFile(path);
    sink.stop();
    const QByteArray twice = readFile(path);
    QCOMPARE(twice, once);
    QCOMPARE(le32(twice, 40), quint32(960 * 2));
}

QTEST_GUILESS_MAIN(TestAudioSinkNode)
