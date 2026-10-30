#include "test_audio_output_node.h"

#include "AudioCompat.h"
#include "NodeDataTypes/SampledData.h"
#include "Sinks/Audio/AudioFormatBridge.h"
#include "Sinks/Audio/AudioOutputNode.h"
#include "Sinks/Audio/AudioPlaybackEngine.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>

#include <algorithm>
#include <cmath>
#include <cstring>
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

void TestAudioOutputNode::encodingLadderBuildsEveryCandidate()
{
    // resolveFormat() tries the SOURCE RATE with another encoding before falling
    // back to preferredFormat()+resampling (AC9): a sink on the source clock has
    // no block boundary to get wrong. That ladder only works if each candidate is
    // really the requested encoding — on Qt5 codec/byteOrder/sampleSize/
    // sampleType are derived from each other, so building the format in the
    // wrong order silently hands back the previous encoding.
    const QAudioFormat f32 =
        AudioFormatBridge::formatWithEncoding(44100, 2, AudioCompat::Float, 32);
    QVERIFY(f32.isValid());
    QCOMPARE(f32.sampleRate(), 44100);
    QCOMPARE(f32.channelCount(), 2);
    QCOMPARE(AudioFormatBridge::encodingLabel(f32), QStringLiteral("F32"));
    QCOMPARE(AudioFormatBridge::bytesPerSample(f32), 4);

    const QAudioFormat s16 =
        AudioFormatBridge::formatWithEncoding(44100, 2, AudioCompat::SignedInt, 16);
    QVERIFY(s16.isValid());
    QCOMPARE(AudioFormatBridge::encodingLabel(s16), QStringLiteral("S16"));
    QCOMPARE(AudioFormatBridge::bytesPerSample(s16), 2);

    const QAudioFormat s32 =
        AudioFormatBridge::formatWithEncoding(44100, 2, AudioCompat::SignedInt, 32);
    QVERIFY(s32.isValid());
    QCOMPARE(AudioFormatBridge::encodingLabel(s32), QStringLiteral("S32"));
    QCOMPARE(AudioFormatBridge::bytesPerSample(s32), 4);

    // The rate and the channel count are the point of the ladder: they are kept.
    QCOMPARE(s16.sampleRate(), f32.sampleRate());
    QCOMPARE(s16.channelCount(), f32.channelCount());

    // An unusable description builds nothing instead of a half-configured format.
    QVERIFY(!AudioFormatBridge::formatWithEncoding(0, 2, AudioCompat::Float, 32).isValid());
    QVERIFY(!AudioFormatBridge::formatWithEncoding(44100, 0, AudioCompat::Float, 32).isValid());
    QVERIFY(!AudioFormatBridge::formatWithEncoding(44100, 2, AudioCompat::Unknown, 32).isValid());
}

void TestAudioOutputNode::conversionCarriesThePhaseAcrossBlocks()
{
    // The engine hands ITS converter to every block. Converted block-by-block
    // with carried state, two blocks must produce what one block of the joined
    // input produces — the stateless path cannot, because every block restarts
    // the phase at 0 (172 clicks/s on a live microphone).
    constexpr double sourceRate = 44100.0;
    const QAudioFormat target =
        AudioFormatBridge::formatWithEncoding(48000, 1, AudioCompat::Float, 32);

    SampledStreamDescriptor descriptor;
    descriptor.sampleRate = sourceRate;
    descriptor.domain = QStringLiteral("audio");
    StreamChannelDescriptor channel;
    channel.name = QStringLiteral("Mono");
    channel.sampleType = SampleType::FLOAT32;
    descriptor.channels.append(channel);

    constexpr int frames = 4410;                 // 100 ms
    constexpr int split = 1024;
    QByteArray pcm(frames * 4, Qt::Uninitialized);
    for (int i = 0; i < frames; ++i) {
        const float value = float(0.4 * std::sin(2.0 * 3.14159265358979323846 * 997.0
                                                 * double(i) / sourceRate));
        std::memcpy(pcm.data() + i * 4, &value, 4);
    }
    QByteArray head = pcm.left(split * 4);

    QString error;
    AudioResample::RateConverter converter(sourceRate, target.sampleRate());
    const QByteArray firstHalf = AudioFormatBridge::convert(SampledData(head, descriptor),
                                                            target, &converter, &error);
    const QByteArray secondHalf =
        AudioFormatBridge::convert(SampledData(pcm.mid(split * 4), descriptor), target,
                                   &converter, &error);
    QVERIFY2(!firstHalf.isEmpty() && !secondHalf.isEmpty(), qPrintable(error));

    // The reference: the same signal as ONE block, i.e. one uninterrupted phase.
    const QByteArray joined =
        AudioFormatBridge::convert(SampledData(pcm, descriptor), target, nullptr, &error);
    QVERIFY2(!joined.isEmpty(), qPrintable(error));

    const QByteArray byBlocks = firstHalf + secondHalf;
    QCOMPARE(byBlocks.size(), joined.size());
    QCOMPARE(byBlocks.size() % 4, qsizetype(0));

    double maxDifference = 0.0;
    for (qsizetype offset = 0; offset + 4 <= byBlocks.size(); offset += 4) {
        float a = 0.0f;
        float b = 0.0f;
        std::memcpy(&a, byBlocks.constData() + offset, 4);
        std::memcpy(&b, joined.constData() + offset, 4);
        maxDifference = std::max(maxDifference, std::abs(double(a) - double(b)));
    }
    QVERIFY2(maxDifference <= 1e-6,
             qPrintable(QStringLiteral("block-by-block conversion differs from the joined "
                                       "block by %1 — the phase is not carried")
                            .arg(maxDifference, 0, 'g', 6)));

    // A stateless caller cannot match it: that difference IS the boundary phase.
    const QByteArray stateless =
        AudioFormatBridge::convert(SampledData(head, descriptor), target, &error)
        + AudioFormatBridge::convert(SampledData(pcm.mid(split * 4), descriptor), target,
                                     &error);
    QCOMPARE(stateless.size(), joined.size());
    QVERIFY(stateless != joined);
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

void TestAudioOutputNode::firstBlockAutoArmsPlayback()
{
    // The IDE never calls start() for an IStartable node, so the node waited
    // forever and a graph opened in the editor was mute — that is the defect the
    // auto-arm fixes. A block is the only thing that opens the device, exactly
    // as in AudioSinkNode::play().
    AudioOutputNode node;   // default device: "" (system default)
    QVERIFY(!node.engine()->isRunning());
    QVERIFY(!node.engine()->userStopped());

    node.setInData(makeBlock(), 0);

    // Armed by the block itself, nothing was queued "while stopped".
    QVERIFY(node.engine()->isRunning());
    QCOMPARE(node.engine()->blocksQueuedWhileStopped(), quint64(0));
    QCOMPARE(node.engine()->blocksPlayed(), quint64(1));
    QVERIFY(!node.statusText().isEmpty());

    if (!node.engine()->deviceAvailable()) {
        // No sound card (CI, headless): the engine is armed, the block waits in
        // the queue and the status says why — never a crash, never a throw.
        QCOMPARE(node.engine()->sinksCreated(), quint64(0));
        QVERIFY(node.engine()->bytesQueued() > 0);
        QSKIP("no audio output device on this machine — armed, queue-only path checked");
    }

    // A real device: the first block opens the one long-lived sink.
    QCOMPARE(node.engine()->sinksCreated(), quint64(1));
    QTRY_VERIFY_WITH_TIMEOUT(node.engine()->bytesQueued() < 960, 5000);

    // …and the second block must not re-arm (no second sink, no counter reset).
    node.setInData(makeBlock(), 0);
    QCOMPARE(node.engine()->sinksCreated(), quint64(1));
    QCOMPARE(node.engine()->blocksPlayed(), quint64(2));

    node.stop();
}

void TestAudioOutputNode::explicitStopIsNotUndoneByAutoArm()
{
    // "Stop" means "stop and stay stopped": the button is not "start by hand",
    // it is the way to silence a node whose upstream keeps producing.
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    node.setConfig(config);

    node.start();
    QVERIFY(node.engine()->isRunning());
    node.stop();
    QVERIFY(!node.engine()->isRunning());
    QVERIFY(node.engine()->userStopped());

    // A block after an explicit stop is queued, NOT re-armed — and it is
    // visible as such instead of vanishing into a silent sink.
    node.setInData(makeSentenceBlock(), 0);
    QVERIFY(!node.engine()->isRunning());
    QCOMPARE(node.engine()->blocksQueuedWhileStopped(), quint64(1));
    QCOMPARE(node.engine()->queueMs(), 1900);
    QVERIFY2(node.statusText().contains(QStringLiteral("queued")),
             qPrintable(node.statusText()));

    // Only an explicit start re-arms, and it does not throw the queue away.
    node.start();
    QVERIFY(node.engine()->isRunning());
    QVERIFY(!node.engine()->userStopped());
    QCOMPARE(node.engine()->blocksQueuedWhileStopped(), quint64(0));
    QVERIFY(node.engine()->bytesQueued() > 0);
}

void TestAudioOutputNode::disabledNodeDoesNotAutoArm()
{
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.enabled = false;
    node.setConfig(config);

    node.setInData(makeSentenceBlock(), 0);

    // Disabled means "no device, blocks are skipped" — arming it would only
    // produce a "ready" status for a node that plays nothing.
    QVERIFY(!node.engine()->isRunning());
    QCOMPARE(node.engine()->blocksSkipped(), quint64(1));
    QCOMPARE(node.engine()->blocksQueuedWhileStopped(), quint64(0));
    QCOMPARE(node.engine()->sinksCreated(), quint64(0));
}

void TestAudioOutputNode::blocksBeforeStartAreQueuedNotLost()
{
    // A block that arrives before the graph is started used to be dropped on
    // the floor (`!m_running` → return) — exactly what a TTS sentence that
    // arrives a millisecond early used to be. The node arms itself now, but an
    // explicitly stopped node still queues instead of losing the block.
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();   // deterministic: never opens a device
    node.setConfig(config);
    node.start();
    node.stop();   // explicit: auto-arm must stay out of the way

    node.setInData(makeSentenceBlock(), 0);

    QCOMPARE(node.engine()->sinksCreated(), quint64(0));
    QCOMPARE(node.engine()->blocksSkipped(), quint64(0));
    QVERIFY(node.engine()->bytesQueued() > 0);
    QCOMPARE(node.engine()->queueMs(), 1900);

    // "Stopped" is visible, not silent: the block is counted, the status says
    // audio is waiting, and the log got exactly one warning for the first block
    // — a live source delivers ~100 blocks/s, so a line per block would flood
    // the log and stay unreadable.
    QCOMPARE(node.engine()->blocksQueuedWhileStopped(), quint64(1));
    QVERIFY2(node.statusText().contains(QStringLiteral("queued")),
             qPrintable(node.statusText()));

    // start() must not throw the queue away: it is played once a device exists.
    node.start();
    QVERIFY(node.engine()->bytesQueued() > 0);
    QCOMPARE(node.engine()->sinksCreated(), quint64(0));
    // …and the "stopped" segment is closed: the counter is per start().
    QCOMPARE(node.engine()->blocksQueuedWhileStopped(), quint64(0));
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

void TestAudioOutputNode::vadSegmentLengthIsNotTrimmed()
{
    // A VAD segment arrives as ONE block and VadNode's default `maxSpeechMs` is
    // 15 s. The cap used to be 10 s, so a 12 s utterance was cut — audible, and
    // nothing in the status said so. The cap now sits above the VAD ceiling.
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    config.maxQueuedMs = 3000;             // the default, far below the block
    node.setConfig(config);
    node.start();

    node.setInData(makeFloatBlock(24000 * 12, 24000.0), 0);   // 12 s, 1 152 000 B

    QCOMPARE(node.engine()->blocksPlayed(), quint64(1));
    QCOMPARE(node.engine()->blocksDropped(), quint64(0));
    QCOMPARE(node.engine()->queueMs(), 12000);              // whole segment kept
    QCOMPARE(node.engine()->bytesQueued(), qsizetype(24000 * 12 * 4));
}

void TestAudioOutputNode::blockOverHardCapIsTrimmedNotSilent()
{
    // The single-block exemption must not become an unbounded queue: one block
    // is capped at kHardBlockCapMs (20 s), and the trim is visible in the log
    // (not in status — the audio still plays).
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.deviceId = missingDeviceId();
    config.maxQueuedMs = 3000;             // the default, far below the block
    node.setConfig(config);
    node.start();

    node.setInData(makeFloatBlock(24000 * 25, 24000.0), 0);   // 25 s, 2 400 000 B

    QCOMPARE(node.engine()->blocksPlayed(), quint64(1));
    QCOMPARE(node.engine()->blocksDropped(), quint64(0));
    QCOMPARE(node.engine()->queueMs(), 20000);
    QCOMPARE(node.engine()->bytesQueued(), qsizetype(24000 * 20 * 4));
}

void TestAudioOutputNode::partiallyPlayedBlockIsNotDropped()
{
    // Drop-oldest must never cut a block whose head already reached the device:
    // that is an audible jump inside a sentence. Needs a real sink, because
    // "partially written" only exists when something was written.
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.bufferMs = 20;        // small device buffer: the head cannot be swallowed
    config.maxQueuedMs = 500;    // 10 s block is far over the bound
    node.setConfig(config);
    node.start();

    if (!node.engine()->deviceAvailable()
        || !node.engine()->isRunning()) {
        QSKIP("no audio output device on this machine — nothing is ever written");
    }

    const qsizetype headBytes = qsizetype(24000 * 10 * 4);   // 10 s @ 24 kHz f32
    node.setInData(makeFloatBlock(24000 * 10, 24000.0), 0);
    QCOMPARE(node.engine()->blocksDropped(), quint64(0));

    // Wait until the device has actually taken part of the head.
    QTRY_VERIFY_WITH_TIMEOUT(node.engine()->bytesQueued() < headBytes, 5000);
    QVERIFY(node.engine()->bytesQueued() > 0);

    // A second block puts the queue over the bound. The head is already audible,
    // so the SECOND block gives up its place instead of the first.
    node.setInData(makeFloatBlock(7200, 24000.0), 0);   // 300 ms
    QCOMPARE(node.engine()->blocksDropped(), quint64(1));

    // What survives is still the long, partially played head — not the 300 ms
    // newcomer — so the listener hears the sentence to its end.
    QVERIFY2(node.engine()->queueMs() > 5000,
             qPrintable(QString::number(node.engine()->queueMs())));
    QVERIFY(node.engine()->bytesQueued() > (headBytes / 2));

    node.stop();
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

void TestAudioOutputNode::deviceUnderrunIsCountedAndVisible()
{
    // In push mode the device reports a drain as IdleState + UnderrunError, NOT
    // StoppedState. The engine used to look at the error only when the state was
    // StoppedState, so a device that ran dry was reported as a healthy
    // "playing · …" status while the sound broke up (AC10).
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.bufferMs = 20;        // small device buffer: it drains quickly
    node.setConfig(config);
    node.start();

    if (!node.engine()->deviceAvailable() || !node.engine()->isRunning())
        QSKIP("no audio output device on this machine — nothing runs dry");

    // One short block and then silence from the producer: the device plays it
    // and runs dry, which is exactly what the counter has to show.
    node.setInData(makeBlock(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(node.engine()->underruns() > 0, 5000);
    QVERIFY2(node.statusText().contains(QStringLiteral("underrun(s)")),
             qPrintable(node.statusText()));
    // Counted on the transition only: the latched error must not add one per
    // state change, or a dry device would report thousands.
    QVERIFY2(node.engine()->underruns() < 100,
             qPrintable(QString::number(node.engine()->underruns())));

    node.stop();
    // A stopped engine must not report underruns from the previous run in the
    // status line — the counter is per run, like the other diagnostics.
    QVERIFY(!node.statusText().contains(QStringLiteral("underrun(s)")));
}

void TestAudioOutputNode::queuedAudioReportsPlayingEvenWhileTheSinkIdles()
{
    // Measured on Qt6 (REQ-SW-PL-076, Qt6 starvation fix): a live push-mode sink
    // holds roughly ONE server period of audio (a PipeWire quantum, 21.3 ms),
    // so it drops to IdleState between the writes of a healthy stream while the
    // block that was just queued is still waiting. The status used to be
    // derived from `state() == ActiveState` alone, which made an audibly playing
    // graph report "ready". "Ready" now means: nothing queued AND an inactive
    // device — the only honest reading of "there is nothing to play".
    AudioOutputNode node;
    AudioOutputNode::Config config;
    config.bufferMs = 20;        // small device buffer: the device fills instantly
    node.setConfig(config);
    node.start();

    if (!node.engine()->deviceAvailable() || !node.engine()->isRunning())
        QSKIP("no audio output device on this machine — nothing to keep fed");

    // 1.9 s in one block against a 20 ms device buffer: whatever the device does
    // with the head of the block, the rest is still queued — and queue means
    // sound on its way, i.e. "playing".
    node.setInData(makeSentenceBlock(), 0);
    QVERIFY(node.engine()->bytesQueued() > 0);
    QVERIFY2(node.statusText().startsWith(QStringLiteral("playing")),
             qPrintable(node.statusText()));

    node.stop();
    // Nothing queued and no device: "ready" — or "stopped", never "playing".
    QVERIFY2(!node.statusText().startsWith(QStringLiteral("playing")),
             qPrintable(node.statusText()));
}
