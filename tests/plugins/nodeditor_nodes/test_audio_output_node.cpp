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

/// One block of @p frames silence at @p rate, mono FLOAT32. 45 600 frames at
/// 24 kHz is the 1.9 s block `TtsNode` emits for a spoken sentence — the block
/// whose size used to be compared against `bytesFree()`.
std::shared_ptr<SampledData> makeFloatBlock(int frames, double rate)
{
    SampledStreamDescriptor descriptor;
    descriptor.sampleRate = rate;
    descriptor.domain = QStringLiteral("audio");
    StreamChannelDescriptor channel;
    channel.name = QStringLiteral("Mono");
    channel.sampleType = SampleType::FLOAT32;
    descriptor.channels.append(channel);

    QByteArray pcm(frames * 4, '\0');
    return std::make_shared<SampledData>(pcm, descriptor);
}

/// 1.9 s of silence at 24 kHz, mono float32 — one TTS sentence as one block.
std::shared_ptr<SampledData> makeSentenceBlock()
{
    return makeFloatBlock(45600, 24000.0);
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

void TestAudioOutputNode::saveLoadRoundTripsAllFields()
{
    AudioOutputNode source;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    config.volume = 0.35;
    config.bufferMs = 240;
    config.enabled = false;
    config.maxQueuedMs = 3200;
    source.setConfig(config);

    const QJsonObject saved = source.save();
    QCOMPARE(saved.value(QStringLiteral("deviceId")).toString(), config.deviceId);
    QCOMPARE(saved.value(QStringLiteral("volume")).toDouble(), config.volume);
    QCOMPARE(saved.value(QStringLiteral("bufferMs")).toInt(), config.bufferMs);
    QCOMPARE(saved.value(QStringLiteral("enabled")).toBool(), config.enabled);
    QCOMPARE(saved.value(QStringLiteral("maxQueuedMs")).toInt(), config.maxQueuedMs);

    AudioOutputNode restored;
    restored.load(saved);

    QCOMPARE(restored.config().deviceId, config.deviceId);
    QCOMPARE(restored.config().volume, config.volume);
    QCOMPARE(restored.config().bufferMs, config.bufferMs);
    QCOMPARE(restored.config().enabled, config.enabled);
    QCOMPARE(restored.config().maxQueuedMs, config.maxQueuedMs);
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

    // The queue bound guards against ACCUMULATION: it is only consulted when
    // more than one block waits, so the minimum is a sentence (a measured
    // English reply is 2.4-2.6 s) and the default has to cover one with reserve.
    QCOMPARE(properties.value(QStringLiteral("maxQueuedMs")).toObject()
                 .value(QStringLiteral("minimum")).toInt(), 500);
    QCOMPARE(properties.value(QStringLiteral("maxQueuedMs")).toObject()
                 .value(QStringLiteral("maximum")).toInt(), 10000);
    QCOMPARE(properties.value(QStringLiteral("maxQueuedMs")).toObject()
                 .value(QStringLiteral("default")).toInt(), 3000);
}

void TestAudioOutputNode::volumeAndBufferAreClamped()
{
    AudioOutputNode node;

    AudioOutputNode::Config config;
    config.volume = 2.5;
    config.bufferMs = 5000;
    config.maxQueuedMs = 60000;
    node.setConfig(config);
    QCOMPARE(node.config().volume, 1.0);
    QCOMPARE(node.config().bufferMs, 2000);
    QCOMPARE(node.config().maxQueuedMs, 10000);

    config.volume = -3.0;
    config.bufferMs = 1;
    config.maxQueuedMs = 1;
    node.setConfig(config);
    QCOMPARE(node.config().volume, 0.0);
    QCOMPARE(node.config().bufferMs, 20);
    QCOMPARE(node.config().maxQueuedMs, 500);
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
    json[QStringLiteral("maxQueuedMs")] = -5;

    AudioOutputNode node;
    node.load(json);

    QCOMPARE(node.config().volume, 1.0);
    QCOMPARE(node.config().bufferMs, 2000);
    QCOMPARE(node.config().maxQueuedMs, 500);
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

void TestAudioOutputNode::unknownDeviceQueuesBlocksAndOpensNothing()
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

    // Blocks are ACCEPTED while no device exists (a device may appear later —
    // a USB headset, a headless run that gains a sink) and wait in the bounded
    // queue. Nothing is opened, nothing is dropped, nothing blocks.
    QCOMPARE(node.engine()->blocksPlayed(), quint64(3));
    QCOMPARE(node.engine()->blocksDropped(), quint64(0));
    QCOMPARE(node.engine()->blocksSkipped(), quint64(0));
    QCOMPARE(node.engine()->sinksCreated(), quint64(0));
    QVERIFY(node.engine()->bytesQueued() > 0);
    QCOMPARE(node.engine()->queueMs(), 30);
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
        // Documented no-device path: nothing is opened, blocks are queued.
        for (int i = 0; i < 3; ++i)
            node.setInData(makeBlock(), 0);
        QCOMPARE(node.engine()->sinksCreated(), quint64(0));
        QCOMPARE(node.engine()->blocksPlayed(), quint64(3));
        QVERIFY(node.engine()->bytesQueued() > 0);
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

// ── Queue semantics: the block size is not the limit ─────────────────────────

void TestAudioOutputNode::deviceIsResolvedOnceAndCached()
{
    // Measured on Qt5: `QAudioDeviceInfo::availableDevices()` takes 2.4–2.8 s
    // with 71 ALSA devices, and the engine used to resolve the device up to
    // four times per block plus once per 20 ms pump tick — the audio output
    // suite took 250 s on Qt5 against 0.7 s on Qt6. Resolution is now cached
    // per device id and re-checked only on start() or after a sink error.
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    node.setConfig(config);
    node.start();

    const quint64 resolutions = node.engine()->deviceResolutions();
    QVERIFY(resolutions >= 1);

    // The hot path — status refresh, pump timer, per-block format resolution —
    // must not touch the device list at all.
    for (int i = 0; i < 5; ++i) {
        QVERIFY(!node.engine()->deviceAvailable());
        node.setInData(makeBlock(), 0);
        QVERIFY(node.statusText().startsWith(QStringLiteral("no output device")));
    }

    QCOMPARE(node.engine()->deviceResolutions(), resolutions);
    QCOMPARE(node.engine()->blocksPlayed(), quint64(5));
}

void TestAudioOutputNode::blocksBeforeStartAreQueuedNotLost()
{
    // A block that arrives before the graph is started used to be dropped on
    // the floor (`!m_running` → return) — exactly what a TTS sentence that
    // arrives a millisecond early used to be.
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();   // deterministic: never opens a device
    node.setConfig(config);

    node.setInData(makeSentenceBlock(), 0);

    QCOMPARE(node.engine()->sinksCreated(), quint64(0));
    QCOMPARE(node.engine()->blocksSkipped(), quint64(0));
    QVERIFY(node.engine()->bytesQueued() > 0);
    QCOMPARE(node.engine()->queueMs(), 1900);

    // start() must not throw the queue away: it is played once a device exists.
    node.start();
    QVERIFY(node.engine()->bytesQueued() > 0);
    QCOMPARE(node.engine()->sinksCreated(), quint64(0));
}

void TestAudioOutputNode::largeBlockIsQueuedNotDropped()
{
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.bufferMs = 200;       // ~38 KB device buffer against a ~183 KB block
    config.maxQueuedMs = 2000;   // holds one sentence
    node.setConfig(config);
    node.start();

    // One TTS sentence as ONE block. The old code compared its size with
    // bytesFree() and dropped it — every sentence was silent at bufferMs 200,
    // and everything over 2 s was silent even at the 2000 ms maximum.
    node.setInData(makeSentenceBlock(), 0);

    QCOMPARE(node.engine()->blocksDropped(), quint64(0));
    QCOMPARE(node.engine()->blocksPlayed(), quint64(1));

    if (!node.engine()->deviceAvailable()) {
        QCOMPARE(node.engine()->sinksCreated(), quint64(0));
        QCOMPARE(node.engine()->queueMs(), 1900);
        QVERIFY(node.engine()->bytesQueued() > 0);
        QSKIP("no audio output device on this machine — queue-only path checked");
    }

    // The device buffer is a fraction of the block: the rest has to drain in
    // chunks instead of being dropped.
    const qsizetype queued = node.engine()->bytesQueued();
    QVERIFY(queued > 0);
    QTRY_VERIFY_WITH_TIMEOUT(node.engine()->bytesQueued() < queued, 5000);
    QCOMPARE(node.engine()->blocksDropped(), quint64(0));

    node.stop();
}

void TestAudioOutputNode::blockOverQueueBoundDropsOldest()
{
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();   // the bound is a queue property only
    config.bufferMs = 200;
    config.maxQueuedMs = 500;              // 1.9 s blocks are far over the bound
    node.setConfig(config);
    node.start();

    // One sentence alone is NEVER dropped for being longer than the bound: the
    // bound guards against accumulation, and 1900 > 500 used to mean silence
    // for every English TTS reply (measured 2.4–2.6 s > the old 2000 ms).
    node.setInData(makeSentenceBlock(), 0);
    QCOMPARE(node.engine()->blocksDropped(), quint64(0));
    QCOMPARE(node.engine()->queueMs(), 1900);

    // A second block changes the situation: two blocks over the bound mean the
    // producer is faster than the device, so the OLDEST one goes.
    node.setInData(makeSentenceBlock(), 0);
    QCOMPARE(node.engine()->blocksPlayed(), quint64(2));
    QCOMPARE(node.engine()->blocksDropped(), quint64(1));

    // What is left is the newer block, and only it: the queue cannot grow with
    // the stream, and the drop stops at one block instead of emptying the queue.
    QCOMPARE(node.engine()->queueMs(), 1900);
    QCOMPARE(node.engine()->bytesQueued(), qsizetype(45600 * 4));
}

void TestAudioOutputNode::singleSentenceUnderTheOldBoundIsNotDropped()
{
    // The regression this rule exists for: an English reply measured at
    // 2.4–2.6 s (236 800 / 249 600 B @ 24 kHz mono f32) was ONE block, the old
    // default bound was 2000 ms, so the block was alone in the queue and
    // `queueMs() > maxQueuedMs` threw away the only thing there was to play —
    // silence, and a sink that was therefore never opened.
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    config.maxQueuedMs = 2000;             // the OLD default, deliberately
    node.setConfig(config);
    node.start();

    node.setInData(makeFloatBlock(62400, 24000.0), 0);   // 2.6 s, 249 600 B

    QCOMPARE(node.engine()->blocksPlayed(), quint64(1));
    QCOMPARE(node.engine()->blocksDropped(), quint64(0));
    QCOMPARE(node.engine()->queueMs(), 2600);
    QCOMPARE(node.engine()->bytesQueued(), qsizetype(249600));
}

void TestAudioOutputNode::blockOverHardCapIsTrimmedNotSilent()
{
    // The single-block exemption must not become an unbounded queue: one block
    // is capped at kHardBlockCapMs (10 s), and the trim is visible in the log
    // (not in status — the audio still plays).
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    config.maxQueuedMs = 3000;             // the default, far below the block
    node.setConfig(config);
    node.start();

    node.setInData(makeFloatBlock(24000 * 12, 24000.0), 0);   // 12 s, 1 152 000 B

    QCOMPARE(node.engine()->blocksPlayed(), quint64(1));
    QCOMPARE(node.engine()->blocksDropped(), quint64(0));
    QCOMPARE(node.engine()->queueMs(), 10000);
    QCOMPARE(node.engine()->bytesQueued(), qsizetype(24000 * 10 * 4));
}

void TestAudioOutputNode::queueStaysBoundedByTime()
{
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    config.maxQueuedMs = 500;
    node.setConfig(config);
    node.start();

    // Ten 300 ms blocks into a 500 ms queue: accepted one by one, the oldest is
    // dropped as the bound is crossed, so memory cannot grow with the stream.
    for (int i = 0; i < 10; ++i)
        node.setInData(makeFloatBlock(7200, 24000.0), 0);

    QCOMPARE(node.engine()->blocksPlayed(), quint64(10));
    QVERIFY(node.engine()->blocksDropped() > 0);
    QVERIFY(node.engine()->queueMs() <= config.maxQueuedMs);
    // 10 blocks would be 288 000 bytes at the source format; the bound is what
    // is still waiting, not the sum of everything ever pushed.
    QVERIFY(node.engine()->bytesQueued() <= 28800 * 2);
}

void TestAudioOutputNode::stopClearsTheQueue()
{
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    node.setConfig(config);
    node.start();
    node.setInData(makeSentenceBlock(), 0);
    QVERIFY(node.engine()->bytesQueued() > 0);

    // REQ-SW-PL-050: stop() closes the sink and the queue dies with it — a
    // later start() must not play audio from a previous run into the device.
    node.stop();
    QCOMPARE(node.engine()->bytesQueued(), qsizetype(0));
    QCOMPARE(node.engine()->queueMs(), 0);
    QVERIFY(!node.engine()->isPlaying());
}

QTEST_GUILESS_MAIN(TestAudioOutputNode)
