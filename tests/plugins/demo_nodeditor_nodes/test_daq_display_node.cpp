#include <QtTest>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>

#include "DaqDisplayNode.h"
#include "GenericDisplayNode.h"
#include "NodeDataTypes/SampledData.h"
#include "test_daq_display_node.h"

#include <QtNodes/NodeDelegateModelRegistry>

using PT = DaqDisplayNode::PlotCard::ProcessingType;
using DM = DaqDisplayNode::PlotCard::DecodeMode;

// ── ring buffer test helpers (REQ-SW-PL-025 AC 3/AC 4) ──────────────────────
namespace {

// Local mirror of the plugin's AudioDisplayAlias (DemoNodeEditorNodesObject.cpp):
// a thin DaqDisplayNode subclass that only overrides name() to return the
// historical "AudioDisplay" registry key. No Q_OBJECT — no new signals/slots,
// the QObject meta-object is inherited from DaqDisplayNode.
class AudioDisplayAlias : public DaqDisplayNode
{
public:
    QString name() const override
    { return QStringLiteral("AudioDisplay"); }
};

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

// Read the sampleIndex-th little-endian int16 sample from a raw byte blob.
qint16 readInt16LE(const QByteArray &bytes, int sampleIndex)
{
    const unsigned char b0 =
        static_cast<unsigned char>(bytes.at(sampleIndex * 2));
    const unsigned char b1 =
        static_cast<unsigned char>(bytes.at(sampleIndex * 2 + 1));
    return static_cast<qint16>(b0 | (static_cast<quint16>(b1) << 8));
}

// Mono/stereo int16 descriptor at the given rate (little-endian default).
SampledStreamDescriptor makeInt16Descriptor(int channelCount, double sampleRate)
{
    SampledStreamDescriptor desc;
    desc.sampleRate = sampleRate;
    for (int ch = 0; ch < channelCount; ++ch)
        desc.channels.append(
            { QStringLiteral("Ch%1").arg(ch), SampleType::INT16 });
    return desc;
}

// Interleave per-channel sample columns into one little-endian int16 buffer:
// column c holds that channel's consecutive samples ([ch0_s0][ch1_s0]...).
QByteArray makeInterleavedInt16(const QVector<QVector<qint16>> &channelColumns)
{
    QByteArray buffer;
    if (channelColumns.isEmpty())
        return buffer;
    const int frameCount = channelColumns.first().size();
    for (int frame = 0; frame < frameCount; ++frame)
        for (const QVector<qint16> &column : channelColumns)
            appendInt16LE(buffer, column.at(frame));
    return buffer;
}

// QByteArray::size() is int on Qt5 but qsizetype on Qt6 — normalize for QCOMPARE.
int byteCount(const QByteArray &bytes)
{
    return static_cast<int>(bytes.size());
}

} // namespace

// ── save/restore round-trip: v1-compatible format ───────────────────────────
//
// Create a node, add one TimeDomain card, save() → JSON → restore() to a fresh
// node, then verify the restored card has the same title, processing type, and
// channel index. v2 fields (mode, unitAxes) should be at their defaults since
// the original node was created with defaults.
void DaqDisplayNodeTest::save_restore_v1Compat()
{
    DaqDisplayNode node1;
    node1.addPlotCard(QStringLiteral("Test1"), PT::TimeDomain, 0, DM::Normalized, true);
    QCOMPARE(node1.m_cards.size(), 1);

    const QJsonObject json = node1.save();

    DaqDisplayNode node2;
    node2.restore(json);

    QCOMPARE(node2.m_cards.size(), 1);
    QCOMPARE(node2.m_cards.at(0).title, QStringLiteral("Test1"));
    QCOMPARE(node2.m_cards.at(0).processingType, PT::TimeDomain);
    QCOMPARE(node2.m_cards.at(0).channelIndex, 0);
    QCOMPARE(node2.m_cards.at(0).mode, DM::Normalized);
    QCOMPARE(node2.m_cards.at(0).unitAxes, true);
}

// ── save/restore ringSeconds (v2 field) ─────────────────────────────────────
//
// The default is 10.0 s. Changing it to 20.0 must survive a round-trip.
void DaqDisplayNodeTest::save_restore_v2_ringSeconds()
{
    DaqDisplayNode node1;
    QCOMPARE(node1.m_ringSeconds, 10.0);

    node1.m_ringSeconds = 20.0;
    const QJsonObject json = node1.save();
    QVERIFY(json.contains(QStringLiteral("ringSeconds")));
    QCOMPARE(json.value(QStringLiteral("ringSeconds")).toDouble(), 20.0);

    DaqDisplayNode node2;
    node2.restore(json);
    QCOMPARE(node2.m_ringSeconds, 20.0);
}

// ── save/restore multiple cards ─────────────────────────────────────────────
//
// Three cards (1 time + 2 FFT, different channels and modes), round-trip all
// properties.
void DaqDisplayNodeTest::save_restore_multipleCards()
{
    DaqDisplayNode node1;
    node1.addPlotCard(QStringLiteral("Time Plot"), PT::TimeDomain, 0, DM::Normalized, true);
    node1.addPlotCard(QStringLiteral("FFT Plot 1"), PT::FrequencySpectrum, 1,
                      DM::Normalized, true);
    node1.addPlotCard(QStringLiteral("FFT Plot 2"), PT::FrequencySpectrum, 2,
                      DM::Physical, false);
    QCOMPARE(node1.m_cards.size(), 3);

    const QJsonObject json = node1.save();

    DaqDisplayNode node2;
    node2.restore(json);
    QCOMPARE(node2.m_cards.size(), 3);

    // Card 0: TimeDomain, channel 0, Normalized, unitAxes=true
    QCOMPARE(node2.m_cards.at(0).title, QStringLiteral("Time Plot"));
    QCOMPARE(node2.m_cards.at(0).processingType, PT::TimeDomain);
    QCOMPARE(node2.m_cards.at(0).channelIndex, 0);
    QCOMPARE(node2.m_cards.at(0).mode, DM::Normalized);
    QCOMPARE(node2.m_cards.at(0).unitAxes, true);

    // Card 1: FrequencySpectrum, channel 1, Normalized
    QCOMPARE(node2.m_cards.at(1).title, QStringLiteral("FFT Plot 1"));
    QCOMPARE(node2.m_cards.at(1).processingType, PT::FrequencySpectrum);
    QCOMPARE(node2.m_cards.at(1).channelIndex, 1);
    QCOMPARE(node2.m_cards.at(1).mode, DM::Normalized);
    QCOMPARE(node2.m_cards.at(1).unitAxes, true);

    // Card 2: FrequencySpectrum, channel 2, Physical, unitAxes=false
    QCOMPARE(node2.m_cards.at(2).title, QStringLiteral("FFT Plot 2"));
    QCOMPARE(node2.m_cards.at(2).processingType, PT::FrequencySpectrum);
    QCOMPARE(node2.m_cards.at(2).channelIndex, 2);
    QCOMPARE(node2.m_cards.at(2).mode, DM::Physical);
    QCOMPARE(node2.m_cards.at(2).unitAxes, false);
}

// ── restore from v1-format JSON (backward compat) ───────────────────────────
//
// A v1 JSON has no ringSeconds, no per-card mode, no per-card unitAxes. After
// restore(), all v2 fields must hold their documented defaults.
void DaqDisplayNodeTest::restore_v1_fileDefaults()
{
    // Construct a v1-format JSON: one card, no v2 fields.
    QJsonObject v1Json;
    QJsonArray plots;
    QJsonObject plot;
    plot[QStringLiteral("title")] = QStringLiteral("Legacy Plot");
    plot[QStringLiteral("processing")] = QStringLiteral("time");
    plot[QStringLiteral("channel")] = 0;
    plots.append(plot);
    v1Json[QStringLiteral("plots")] = plots;

    DaqDisplayNode node;
    node.restore(v1Json);

    QCOMPARE(node.m_cards.size(), 1);
    QCOMPARE(node.m_cards.at(0).title, QStringLiteral("Legacy Plot"));
    QCOMPARE(node.m_cards.at(0).processingType, PT::TimeDomain);
    QCOMPARE(node.m_cards.at(0).channelIndex, 0);

    // v2 defaults (AC 7) — undocumented fields fall back to their defaults.
    QCOMPARE(node.m_cards.at(0).mode, DM::Normalized);
    QCOMPARE(node.m_cards.at(0).unitAxes, true);
    QCOMPARE(node.m_ringSeconds, 10.0);
}

// ── save/restore physical decode mode ───────────────────────────────────────
//
// A card with DecodeMode::Physical must round-trip the mode string "physical".
void DaqDisplayNodeTest::save_restore_mode_physical()
{
    DaqDisplayNode node1;
    node1.addPlotCard(QStringLiteral("Physical Plot"), PT::TimeDomain, 0,
                      DM::Physical, true);

    const QJsonObject json = node1.save();
    const QJsonArray plots = json.value(QStringLiteral("plots")).toArray();
    QCOMPARE(plots.size(), 1);
    QCOMPARE(plots.at(0).toObject().value(QStringLiteral("mode")).toString(),
             QStringLiteral("physical"));

    DaqDisplayNode node2;
    node2.restore(json);

    QCOMPARE(node2.m_cards.size(), 1);
    QCOMPARE(node2.m_cards.at(0).title, QStringLiteral("Physical Plot"));
    QCOMPARE(node2.m_cards.at(0).mode, DM::Physical);
}

// ── ring buffer: rolling window drops oldest (REQ-SW-PL-025 AC 3) ───────────
//
// appendRingBlock() must keep only the most recent ringSeconds × sampleRate
// samples per channel, trimming from the FRONT. Pure ComputeState exercise —
// no node instantiation, no GUI thread involvement.
void DaqDisplayNodeTest::ringBuffer_rollingWindowDropsOldest()
{
    DaqDisplayNode::ComputeState ring;

    const double kSampleRate = 1000.0;
    // Capacity = qsizetype(0.01 × 1000) × 2 bytes = 10 int16 samples = 20 bytes.
    const double kRingSeconds = 0.01;
    const SampledStreamDescriptor desc = makeInt16Descriptor(1, kSampleRate);

    // Block 1: samples 0..7 — under capacity, nothing may be trimmed yet.
    SampledData block1(makeInterleavedInt16({{0, 1, 2, 3, 4, 5, 6, 7}}), desc);
    DaqDisplayNode::appendRingBlock(ring, block1, kRingSeconds, 1);
    QCOMPARE(ring.channels.size(), 1);
    QCOMPARE(byteCount(ring.channels.at(0).bytes), 8 * 2);

    // Block 2: samples 8..15 — total 16 samples > capacity → front-trimmed to
    // the most recent 10 (samples 6..15).
    SampledData block2(makeInterleavedInt16({{8, 9, 10, 11, 12, 13, 14, 15}}),
                       desc);
    DaqDisplayNode::appendRingBlock(ring, block2, kRingSeconds, 2);
    QCOMPARE(byteCount(ring.channels.at(0).bytes), 10 * 2);

    // Block 3: samples 16..23 — window now holds exactly samples 14..23.
    SampledData block3(makeInterleavedInt16({{16, 17, 18, 19, 20, 21, 22, 23}}),
                       desc);
    DaqDisplayNode::appendRingBlock(ring, block3, kRingSeconds, 3);
    QCOMPARE(byteCount(ring.channels.at(0).bytes), 10 * 2);

    // Content check: oldest dropped, newest kept, sample order preserved.
    const QByteArray &bytes = ring.channels.at(0).bytes;
    for (int i = 0; i < 10; ++i)
        QCOMPARE(readInt16LE(bytes, i), static_cast<qint16>(14 + i));
}

// ── ring buffer: descriptor change resets the ring (REQ-SW-PL-025 AC 4) ─────
//
// A descriptor change (sampleRate / channel count / bytesPerFrame / ...) must
// clear the ring BEFORE the new block accumulates — formats are never mixed.
void DaqDisplayNodeTest::ringBuffer_descriptorChangeReset()
{
    DaqDisplayNode::ComputeState ring;

    // Phase 1: stereo int16 @ 1000 Hz — both channels accumulate 3 samples.
    const SampledStreamDescriptor stereo1k = makeInt16Descriptor(2, 1000.0);
    SampledData stereoBlock(
        makeInterleavedInt16({{100, 101}, {102, 103}, {104, 105}}), stereo1k);
    DaqDisplayNode::appendRingBlock(ring, stereoBlock, 10.0, 1);
    QCOMPARE(ring.channels.size(), 2);
    QCOMPARE(ring.sampleRate, 1000.0);
    QCOMPARE(ring.bytesPerFrame, 4);
    QCOMPARE(byteCount(ring.channels.at(0).bytes), 3 * 2);
    QCOMPARE(byteCount(ring.channels.at(1).bytes), 3 * 2);

    // Phase 2: SAME layout, different sampleRate → full reset, then the new
    // block alone. Ring must hold ONLY the new block's samples.
    const SampledStreamDescriptor stereo2k = makeInt16Descriptor(2, 2000.0);
    SampledData rateChangedBlock(makeInterleavedInt16({{900}, {901}}), stereo2k);
    DaqDisplayNode::appendRingBlock(ring, rateChangedBlock, 10.0, 2);
    QCOMPARE(ring.sampleRate, 2000.0);
    QCOMPARE(ring.bytesPerFrame, 4);
    QCOMPARE(byteCount(ring.channels.at(0).bytes), 1 * 2);
    QCOMPARE(readInt16LE(ring.channels.at(0).bytes, 0), qint16(900));
    QCOMPARE(readInt16LE(ring.channels.at(1).bytes, 0), qint16(901));

    // Phase 3: channel-count change (stereo → mono @ 1000 Hz) → reset again;
    // the channel vector is resized and only the mono block remains.
    const SampledStreamDescriptor mono1k = makeInt16Descriptor(1, 1000.0);
    SampledData monoBlock(makeInterleavedInt16({{700, 701}}), mono1k);
    DaqDisplayNode::appendRingBlock(ring, monoBlock, 10.0, 3);
    QCOMPARE(ring.channels.size(), 1);
    QCOMPARE(byteCount(ring.channels.at(0).bytes), 2 * 2);
    QCOMPARE(readInt16LE(ring.channels.at(0).bytes, 0), qint16(700));
    QCOMPARE(readInt16LE(ring.channels.at(0).bytes, 1), qint16(701));
}

// ── GenericDisplayNode: thin DaqDisplayNode alias (behavioral parity) ───────
//
// GenericDisplayNode overrides only name()/caption() — everything else
// (SampledData input, plot cards, save/restore) is inherited. save() must
// carry the "GenericDisplay" model-name and restore() must round-trip a
// DaqDisplayNode-format JSON.
void DaqDisplayNodeTest::genericDisplay_aliasBehavior()
{
    GenericDisplayNode node;
    QCOMPARE(node.name(), QStringLiteral("GenericDisplay"));
    QCOMPARE(node.caption(), QStringLiteral("Generic Display"));

    node.addPlotCard(QStringLiteral("Alias Plot"), PT::TimeDomain, 0, DM::Normalized, true);
    const QJsonObject json = node.save();
    QCOMPARE(json.value(QStringLiteral("model-name")).toString(),
             QStringLiteral("GenericDisplay"));

    GenericDisplayNode restored;
    restored.restore(json);

    const QJsonObject json2 = restored.save();
    QCOMPARE(json2.value(QStringLiteral("model-name")).toString(),
             QStringLiteral("GenericDisplay"));
    const QJsonArray plots = json2.value(QStringLiteral("plots")).toArray();
    QCOMPARE(plots.size(), 1);
    QCOMPARE(plots.at(0).toObject().value(QStringLiteral("title")).toString(),
             QStringLiteral("Alias Plot"));
    QCOMPARE(plots.at(0).toObject().value(QStringLiteral("processing")).toString(),
             QStringLiteral("time"));
    QCOMPARE(plots.at(0).toObject().value(QStringLiteral("channel")).toInt(), 0);
}

// ── Registry resolution: "AudioDisplay" resolves to DaqDisplayNode ──────────
//
// Mirror the plugin registration: DaqDisplayNode + GenericDisplayNode +
// AudioDisplayAlias all registered under "Daq/Display". create() must return
// DaqDisplayNode-derived models for all three keys, with the right name().
void DaqDisplayNodeTest::registry_aliasResolution()
{
    QtNodes::NodeDelegateModelRegistry registry;
    registry.registerModel<DaqDisplayNode>("Daq/Display");
    registry.registerModel<GenericDisplayNode>("Daq/Display");
    registry.registerModel<AudioDisplayAlias>("Daq/Display");

    auto daq = registry.create(QStringLiteral("DaqDisplay"));
    QVERIFY(daq != nullptr);
    QVERIFY(dynamic_cast<DaqDisplayNode *>(daq.get()) != nullptr);
    QCOMPARE(daq->name(), QStringLiteral("DaqDisplay"));

    auto generic = registry.create(QStringLiteral("GenericDisplay"));
    QVERIFY(generic != nullptr);
    QVERIFY(dynamic_cast<DaqDisplayNode *>(generic.get()) != nullptr);
    QCOMPARE(generic->name(), QStringLiteral("GenericDisplay"));

    auto audio = registry.create(QStringLiteral("AudioDisplay"));
    QVERIFY(audio != nullptr);
    QVERIFY(dynamic_cast<DaqDisplayNode *>(audio.get()) != nullptr);
    QCOMPARE(audio->name(), QStringLiteral("AudioDisplay"));
}

QTEST_MAIN(DaqDisplayNodeTest)
