#include "test_audio_output_node.h"

#include "AudioCompat.h"
#include "NodeDataTypes/SampledData.h"
#include "Sinks/Audio/AudioOutputNode.h"
#include "Sinks/Audio/AudioPlaybackEngine.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>

#include <memory>

using QtNodes::NodeData;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

/// A device id that can never resolve: the tests must not depend on the box
/// having a sound card (CI, headless runs). An unknown non-empty id makes
/// `AudioCompat::deviceFromId()` return a null device on both Qt versions —
/// Qt6 by opaque id, Qt5 by device name.
QString missingDeviceId()
{
    return QStringLiteral("no-such-output-device/REQ-SW-PL-076");
}

/// One 10 ms block of silence: 48 frames @ 48 kHz, mono int16. Small enough to
/// fit in any real device buffer, so the device path exercises a write rather
/// than a backpressure drop.
std::shared_ptr<SampledData> makeBlock()
{
    SampledStreamDescriptor descriptor;
    descriptor.sampleRate = 48000.0;
    descriptor.domain = QStringLiteral("audio");
    StreamChannelDescriptor channel;
    channel.name = QStringLiteral("Mono");
    channel.sampleType = SampleType::INT16;
    descriptor.channels.append(channel);

    QByteArray pcm(480 * 2, '\0');   // 480 frames, 16-bit, one channel
    return std::make_shared<SampledData>(pcm, descriptor);
}

QStringList sortedKeys(const QJsonObject &object)
{
    QStringList keys = object.keys();
    keys.sort();
    return keys;
}

} // namespace

// ── Contract with the graph model ─────────────────────────────────────────────

void TestAudioOutputNode::nameAndCaptionMatchTheSinkConvention()
{
    AudioOutputNode node;

    // `name()` is the registration key and what a .flow file stores.
    QCOMPARE(node.name(), QStringLiteral("AudioOutput"));

    // Sink nodes follow VideoOutput/FileRecord: a caption is defined for menus
    // and tooltips, but the node body does not draw it (AC1).
    QCOMPARE(node.caption(), QStringLiteral("Audio Output"));
    QCOMPARE(node.captionVisible(), false);

    // A block only produces sound — no geometry or widget recompute per block,
    // otherwise a 100 blocks/s stream repaints the scene 100 times a second.
    QCOMPARE(node.dataArrivalChangesGeometry(), false);
    QCOMPARE(node.dataArrivalChangesWidget(), false);
}

void TestAudioOutputNode::portsAndDataTypeAreSampledData()
{
    AudioOutputNode node;

    QCOMPARE(node.nPorts(PortType::In), 1u);
    QCOMPARE(node.nPorts(PortType::Out), 0u);

    const QtNodes::NodeDataType expected = SampledData().type();
    const QtNodes::NodeDataType actual = node.dataType(PortType::In, 0);
    QCOMPARE(actual.id, expected.id);
    QCOMPARE(actual.name, expected.name);

    // A sink emits nothing.
    QVERIFY(node.outData(0) == nullptr);
}

// ── Persistence (AC3) ─────────────────────────────────────────────────────────

void TestAudioOutputNode::saveLoadRoundTripsAllFourFields()
{
    AudioOutputNode source;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    config.volume = 0.35;
    config.bufferMs = 240;
    config.enabled = false;
    source.setConfig(config);

    const QJsonObject saved = source.save();
    QCOMPARE(saved.value(QStringLiteral("deviceId")).toString(), config.deviceId);
    QCOMPARE(saved.value(QStringLiteral("volume")).toDouble(), config.volume);
    QCOMPARE(saved.value(QStringLiteral("bufferMs")).toInt(), config.bufferMs);
    QCOMPARE(saved.value(QStringLiteral("enabled")).toBool(), config.enabled);

    AudioOutputNode restored;
    restored.load(saved);

    QCOMPARE(restored.config().deviceId, config.deviceId);
    QCOMPARE(restored.config().volume, config.volume);
    QCOMPARE(restored.config().bufferMs, config.bufferMs);
    QCOMPARE(restored.config().enabled, config.enabled);
}

void TestAudioOutputNode::saveCarriesModelName()
{
    AudioOutputNode node;
    const QJsonObject saved = node.save();

    // Written by the BASE save() (REQ-SW-PL-057): a flow file that loses this
    // key loads the node as "<unnamed>" and drops every connection to it.
    QCOMPARE(saved.value(QStringLiteral("model-name")).toString(),
             QStringLiteral("AudioOutput"));
}

void TestAudioOutputNode::configSchemaDescribesExactlyWhatSaveEmits()
{
    AudioOutputNode node;
    const QJsonObject schema = node.configSchema();

    QCOMPARE(schema.value(QStringLiteral("type")).toString(),
             QStringLiteral("object"));
    const QJsonObject properties = schema.value(QStringLiteral("properties")).toObject();

    // The schema is the contract behind GET /api/nodes/{id}/schema and the
    // REST config form: a key that save() writes but the schema omits is
    // invisible to both, and a documented key save() drops cannot be restored.
    QCOMPARE(sortedKeys(properties), sortedKeys(node.save()));

    QCOMPARE(properties.value(QStringLiteral("volume")).toObject()
                 .value(QStringLiteral("minimum")).toDouble(), 0.0);
    QCOMPARE(properties.value(QStringLiteral("volume")).toObject()
                 .value(QStringLiteral("maximum")).toDouble(), 1.0);
    QCOMPARE(properties.value(QStringLiteral("bufferMs")).toObject()
                 .value(QStringLiteral("minimum")).toInt(), 20);
    QCOMPARE(properties.value(QStringLiteral("bufferMs")).toObject()
                 .value(QStringLiteral("maximum")).toInt(), 2000);
}

void TestAudioOutputNode::volumeAndBufferAreClamped()
{
    AudioOutputNode node;

    AudioOutputNode::Config config;
    config.volume = 2.5;
    config.bufferMs = 5000;
    node.setConfig(config);
    QCOMPARE(node.config().volume, 1.0);
    QCOMPARE(node.config().bufferMs, 2000);

    config.volume = -3.0;
    config.bufferMs = 1;
    node.setConfig(config);
    QCOMPARE(node.config().volume, 0.0);
    QCOMPARE(node.config().bufferMs, 20);
}

void TestAudioOutputNode::loadClampsOutOfRangeValues()
{
    // A hand-edited (or older) flow file must not smuggle an out-of-range value
    // past the engine and into save() — the engine would clamp it silently and
    // the file would disagree with the running node.
    QJsonObject json;
    json[QStringLiteral("deviceId")] = QStringLiteral("hand-edited");
    json[QStringLiteral("volume")] = 7.5;
    json[QStringLiteral("bufferMs")] = 9000;
    json[QStringLiteral("enabled")] = true;

    AudioOutputNode node;
    node.load(json);

    QCOMPARE(node.config().volume, 1.0);
    QCOMPARE(node.config().bufferMs, 2000);
    QCOMPARE(node.save().value(QStringLiteral("volume")).toDouble(), 1.0);
    QCOMPARE(node.save().value(QStringLiteral("bufferMs")).toInt(), 2000);
}

// ── Status plumbing ───────────────────────────────────────────────────────────

void TestAudioOutputNode::statusIsNeverEmpty()
{
    AudioOutputNode node;

    // A fresh node is stopped, and it says so: an empty status line would look
    // like "fine" in the widget while nothing was armed.
    QVERIFY(!node.statusText().isEmpty());
    QCOMPARE(node.statusText(), QStringLiteral("stopped"));
}

void TestAudioOutputNode::statusChangesAreForwarded()
{
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    node.setConfig(config);

    QSignalSpy spy(&node, &AudioOutputNode::statusChanged);
    node.start();

    QVERIFY(spy.count() >= 1);
    const QString status = node.statusText();
    QVERIFY(status.startsWith(QStringLiteral("no output device")));
    QCOMPARE(spy.last().at(0).toString(), status);
}

// ── No output device (AC5) ────────────────────────────────────────────────────

void TestAudioOutputNode::unknownDeviceIsANoOp()
{
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    node.setConfig(config);
    node.start();

    // Outcome depends on the box: with a sound card the id still does not
    // resolve, so this is the no-device path either way.
    QVERIFY(!node.engine()->deviceAvailable());
    QCOMPARE(node.engine()->sinksCreated(), quint64(0));

    for (int i = 0; i < 3; ++i)
        node.setInData(makeBlock(), 0);

    QCOMPARE(node.engine()->blocksSkipped(), quint64(3));
    QCOMPARE(node.engine()->blocksPlayed(), quint64(0));
    QCOMPARE(node.engine()->blocksDropped(), quint64(0));
    QCOMPARE(node.engine()->sinksCreated(), quint64(0));
}

void TestAudioOutputNode::noDeviceStatusExplainsWhy()
{
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    node.setConfig(config);
    node.start();

    QCOMPARE(node.statusText(), QStringLiteral("no output device"));

    // Disabled is a different state with a different text — "no device" while
    // the user switched the node off would send them looking for hardware.
    config.enabled = false;
    node.setConfig(config);
    QCOMPARE(node.statusText(), QStringLiteral("disabled"));
}

void TestAudioOutputNode::stopIsSafeBeforeStartAndIdempotent()
{
    AudioOutputNode node;

    // REQ-SW-PL-050: HeadlessEngine::stopAllNodes() casts every model in the
    // graph and calls stop(), including nodes that were never started.
    node.stop();
    node.stop();

    node.start();
    node.stop();
    node.stop();

    QCOMPARE(node.engine()->isRunning(), false);
    QCOMPARE(node.engine()->isPlaying(), false);
    QCOMPARE(node.statusText(), QStringLiteral("stopped"));
    QCOMPARE(node.engine()->sinksCreated(), quint64(0));
}

// ── One persistent sink for N blocks (AC2) ────────────────────────────────────

void TestAudioOutputNode::manyBlocksUseAtMostOneSink()
{
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.bufferMs = 200;
    node.setConfig(config);
    node.start();

    const bool hasDevice = node.engine()->deviceAvailable();
    if (!hasDevice) {
        // Documented no-device path: nothing is opened, blocks are counted.
        for (int i = 0; i < 3; ++i)
            node.setInData(makeBlock(), 0);
        QCOMPARE(node.engine()->sinksCreated(), quint64(0));
        QCOMPARE(node.engine()->blocksSkipped(), quint64(3));
        QSKIP("no audio output device on this machine — no-device path checked");
    }

    node.setInData(makeBlock(), 0);
    const quint64 sinksAfterFirst = node.engine()->sinksCreated();
    QCOMPARE(sinksAfterFirst, quint64(1));

    node.setInData(makeBlock(), 0);
    node.setInData(makeBlock(), 0);

    // The whole point of the node: the same sink plays every block. A second
    // creation here is the private AudioSinkNode's defect (a sink per block,
    // destroyed immediately, nothing audible) coming back.
    QCOMPARE(node.engine()->sinksCreated(), sinksAfterFirst);
    QVERIFY(node.engine()->blocksPlayed() >= 1);
    QVERIFY(!node.statusText().isEmpty());
}

void TestAudioOutputNode::nullInputChangesNothing()
{
    AudioOutputNode node;
    node.setConfig(AudioOutputNode::Config{});
    node.start();

    node.setInData(makeBlock(), 0);
    const quint64 played = node.engine()->blocksPlayed();
    const quint64 skipped = node.engine()->blocksSkipped();
    const quint64 dropped = node.engine()->blocksDropped();
    const quint64 sinks = node.engine()->sinksCreated();

    // Edge removed / source went silent: the model ignores nullptr on purpose
    // (tearing the sink down on a transient gap is exactly the click a
    // persistent sink avoids), and a foreign port index is not this node's.
    node.setInData(nullptr, 0);
    node.setInData(nullptr, 0);
    node.setInData(makeBlock(), 1);

    QCOMPARE(node.engine()->blocksPlayed(), played);
    QCOMPARE(node.engine()->blocksSkipped(), skipped);
    QCOMPARE(node.engine()->blocksDropped(), dropped);
    QCOMPARE(node.engine()->sinksCreated(), sinks);
}

QTEST_GUILESS_MAIN(TestAudioOutputNode)

#include "test_audio_output_node.moc"
