#include "test_frame_sampler_node.h"

#include "Processing/Video/FrameSamplerNode.h"
#include "NodeDataTypes/VideoFrameData.h"

#include <QElapsedTimer>
#include <QImage>
#include <QJsonObject>
#include <QSignalSpy>
#include <QThread>
#include <QVideoFrame>

using QtNodes::NodeData;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

/// A VideoFrameData carrying a real frame, built from a QImage. This is the
/// cheapest way to get hasFrame() == true on BOTH Qt versions: the planar
/// buffer that QVideoFrame::fromBuffer needs is Qt6-only in this code base,
/// and QVideoFrame(QImage) works on Qt5 and Qt6 alike without OpenCV, a
/// decoder or a GL context.
std::shared_ptr<VideoFrameData> makeFrame(int seed = 0)
{
    QImage img(64, 48, QImage::Format_RGB32);
    img.fill(QColor(seed * 7 % 256, seed * 13 % 256, seed * 29 % 256));
    QVideoFrame frame(img);
    if (!frame.isValid())
        return nullptr;
    return std::make_shared<VideoFrameData>(frame);
}

/// Distinct-but-cheap frames for counting passes: each carries its own colour
/// so a test can tell *which* frame came out, not just how many.
std::shared_ptr<VideoFrameData> makeTintedFrame(int tint)
{
    QImage img(32, 32, QImage::Format_RGB32);
    img.fill(QColor(tint % 256, (tint * 3) % 256, (tint * 7) % 256));
    QVideoFrame frame(img);
    return std::make_shared<VideoFrameData>(frame);
}

/// Feeds `count` distinct frames through the node and returns how many
/// `dataUpdated(0)` signals came out.
int feedFrames(FrameSamplerNode &node, int count, int firstTint = 0)
{
    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);
    for (int i = 0; i < count; ++i)
        node.setInData(makeTintedFrame(firstTint + i), 0);
    return updated.count();
}

} // namespace

// ── Contract with the graph model ─────────────────────────────────────────────

void TestFrameSamplerNode::portsAndDataTypeAreVideoFrameData()
{
    FrameSamplerNode node;
    QCOMPARE(node.nPorts(QtNodes::PortType::In), 1u);
    QCOMPARE(node.nPorts(QtNodes::PortType::Out), 1u);
    QCOMPARE(node.nPorts(QtNodes::PortType::None), 0u);

    const QtNodes::NodeDataType in = node.dataType(QtNodes::PortType::In, 0);
    const QtNodes::NodeDataType out = node.dataType(QtNodes::PortType::Out, 0);
    QCOMPARE(in.id, QStringLiteral("video-frame"));
    QCOMPARE(out.id, in.id);
    QCOMPARE(node.name(), QStringLiteral("FrameSampler"));
}

void TestFrameSamplerNode::embeddedWidgetIsNull()
{
    // REQ-SW-PL-051: the core model constructs no widget; the GUI plugin
    // supplies it through NodeWidgetFactory. A non-null return here would put
    // QtWidgets back on the core link line.
    FrameSamplerNode node;
    QCOMPARE(node.embeddedWidget(), nullptr);
}

void TestFrameSamplerNode::optOutOfGeometryRecompute()
{
    // A gate re-emits the same shared frame on every pass. If the graph model
    // treated that as a geometry change, every pass would trigger a full
    // scene recompute — the exact cascade REQ-SW-PL-030 exists to avoid.
    FrameSamplerNode node;
    QVERIFY(!node.dataArrivalChangesGeometry());
    QVERIFY(!node.dataArrivalChangesWidget());
}

// ── EveryNth ─────────────────────────────────────────────────────────────────

void TestFrameSamplerNode::everyNthPassesEveryNthFrame()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.onEveryNChanged(3);

    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);
    // Frames are numbered 1..9; the gate lets 3 and 6 through (the counter is
    // incremented *before* the modulo test, so the first frame of a fresh
    // gate is frame 1, not frame 0).
    for (int i = 1; i <= 9; ++i) {
        const auto in = makeTintedFrame(i);
        node.setInData(in, 0);
        if (i % 3 == 0) {
            QCOMPARE(updated.count(), i / 3);
            QCOMPARE(node.outData(0).get(), in.get());
        }
    }
    QCOMPARE(updated.count(), 3);
}

void TestFrameSamplerNode::everyNthWithOnePassesEverything()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.onEveryNChanged(1);
    QCOMPARE(feedFrames(node, 5), 5);
}

// ── MaxFps ───────────────────────────────────────────────────────────────────

void TestFrameSamplerNode::maxFpsDropsTheFirstFrameForOneInterval()
{
    // passesGate() starts the interval timer ON the incoming frame and then
    // tests nsecsElapsed() >= interval — which is false for the frame that
    // just started it. So the first frame after a reset never passes; the gate
    // emits starting with the second.
    //
    // Asserted as the current contract, not endorsed: see
    // maxFpsLatencyIsOneInterval for why this was not "fixed" here.
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::MaxFps));
    node.onMaxFpsChanged(10); // 100 ms interval

    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);
    node.setInData(makeTintedFrame(0), 0);
    QCOMPARE(updated.count(), 0);
    QVERIFY(!node.outData(0));

    // …and the timer is now running, so the frames inside the interval are
    // dropped too.
    QThread::msleep(10);
    node.setInData(makeTintedFrame(1), 0);
    QCOMPARE(updated.count(), 0);
}

void TestFrameSamplerNode::maxFpsDropsFramesWithinTheInterval()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::MaxFps));
    node.onMaxFpsChanged(5); // 200 ms interval — nothing can pass inside 20 ms
    // The first frame starts the timer and is dropped; the next 49 are inside
    // the interval and must also be dropped. Exactly one interval has not
    // elapsed across the whole loop, so nothing passes.
    QCOMPARE(feedFrames(node, 50), 0);
}

void TestFrameSamplerNode::maxFpsPassesAgainAfterTheInterval()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::MaxFps));
    node.onMaxFpsChanged(50); // 20 ms interval
    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);

    // First frame starts the timer (dropped), then a busy loop for ~120 ms.
    // At 50 fps that is ~6 passes; assert a range rather than an exact count so
    // the test is not a timing assertion in disguise. Zero would mean the timer
    // never restarts; one would mean the gate is stuck after the first frame.
    node.setInData(makeTintedFrame(0), 0);
    QCOMPARE(updated.count(), 0);

    QElapsedTimer busy;
    busy.start();
    int fed = 0;
    while (busy.elapsed() < 120)
        node.setInData(makeTintedFrame(++fed), 0);

    const int total = updated.count();
    QVERIFY2(total >= 3,
             qPrintable(QStringLiteral("only %1 passes in 120 ms at 50 fps — the gate is "
                                       "not re-arming after the first interval")
                            .arg(total)));
    QVERIFY2(total <= 20,
             qPrintable(QStringLiteral("%1 passes in 120 ms at 50 fps (expected ~6) — the "
                                       "gate is not limiting the rate")
                            .arg(total)));
}

void TestFrameSamplerNode::maxFpsLatencyIsOneInterval()
{
    // Documents the observable consequence of the above: a MaxFps-gated
    // output stays blank for one full interval after every reset — on start,
    // on a mode change, and on every scene reload. At 5 fps that is a 200 ms
    // black view, which is visible but not a spec violation ("at most maxFps
    // frames per second"). Pinned here so that a future change to the
    // start-on-first-frame behaviour is a deliberate, visible diff rather than
    // an accident.
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::MaxFps));
    node.onMaxFpsChanged(5); // 200 ms

    QElapsedTimer timer;
    timer.start();
    int emitted = 0;
    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);
    while (timer.elapsed() < 250)
        node.setInData(makeTintedFrame(++emitted), 0);

    QVERIFY2(updated.count() >= 1,
             "no frame passed within 250 ms at 5 fps — the gate never re-arms");
    // The first pass cannot happen before one interval, so a frame emitted in
    // the first 100 ms would mean the timer starts somewhere else now.
    QVERIFY2(timer.elapsed() >= 100,
             "a frame passed well inside the 200 ms interval — the gate is not limiting "
             "the rate any more");
}

// ── Gate reset on parameter change ───────────────────────────────────────────

void TestFrameSamplerNode::modeChangeResetsGate()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.onEveryNChanged(4);
    node.setInData(makeTintedFrame(1), 0); // counter -> 1
    node.setInData(makeTintedFrame(2), 0); // counter -> 2, dropped

    // Switching modes with a half-consumed counter must restart from zero.
    // Switch to MaxFps: the reset is observable as the timer restarting, so
    // the next frame is the interval-starting one and is dropped. Without the
    // reset the timer would still hold the EveryNth phase (nothing to observe
    // there) and the stale counter would carry into the next switch back —
    // which is asserted by maxFpsChangeResetsGate below.
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::MaxFps));
    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);
    node.setInData(makeTintedFrame(3), 0);
    QCOMPARE(updated.count(), 0); // MaxFps drops its interval-starting frame

    // Switching back to EveryNth must count from 1 again, not continue from 2.
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.setInData(makeTintedFrame(4), 0); // fresh count -> 1, N=4 -> dropped
    QCOMPARE(updated.count(), 0);
    node.setInData(makeTintedFrame(5), 0); // -> 2, dropped
    node.setInData(makeTintedFrame(6), 0); // -> 3, dropped
    node.setInData(makeTintedFrame(7), 0); // -> 4, passes
    QCOMPARE(updated.count(), 1);
}

void TestFrameSamplerNode::everyNChangeResetsGate()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.onEveryNChanged(2);
    node.setInData(makeTintedFrame(1), 0); // counter -> 1, dropped
    QVERIFY(!node.outData(0));

    // Changing N mid-stream restarts the count, so the next frame is #1 again
    // and (with N=2) is dropped rather than passing on the stale count.
    node.onEveryNChanged(2);
    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);
    node.setInData(makeTintedFrame(2), 0);
    QCOMPARE(updated.count(), 0);
    node.setInData(makeTintedFrame(3), 0);
    QCOMPARE(updated.count(), 1);
}

void TestFrameSamplerNode::maxFpsChangeResetsGate()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::MaxFps));
    node.onMaxFpsChanged(1); // 1 s interval
    node.setInData(makeTintedFrame(1), 0); // starts the timer, dropped

    // The stale 1 s interval must not carry over: after raising the rate the
    // user waits at most one NEW interval (8 ms), not the old second. The
    // reset is observable as the next frame restarting the timer.
    node.onMaxFpsChanged(120);
    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);
    node.setInData(makeTintedFrame(2), 0);
    QCOMPARE(updated.count(), 0); // interval-starting frame, as always

    QThread::msleep(20); // > 1/120 s
    node.setInData(makeTintedFrame(3), 0);
    QCOMPARE(updated.count(), 1);

    // Without the reset the frame above would have been inside the stale 1 s
    // window and dropped — so this single assertion is what proves onMaxFps-
    // Changed() invalidated the timer.
}

void TestFrameSamplerNode::loadResetsGate()
{
    // Loading a scene must not inherit a running gate either — otherwise
    // replaying the same .flow yields a different first-frame latency than
    // the run that saved it.
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.onEveryNChanged(5);
    node.setInData(makeTintedFrame(1), 0); // counter -> 1

    QJsonObject saved = node.save();
    node.load(saved);
    QCOMPARE(node.everyN(), 5);
    QCOMPARE(node.mode(), FrameSamplerNode::Mode::EveryNth);

    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);
    node.setInData(makeTintedFrame(2), 0); // must be #1 of a fresh count
    QCOMPARE(updated.count(), 0);
    for (int i = 0; i < 4; ++i)
        node.setInData(makeTintedFrame(3 + i), 0);
    QCOMPARE(updated.count(), 1);
}

// ── Zero-copy / fan-out ──────────────────────────────────────────────────────

void TestFrameSamplerNode::zeroCopySharesTheSameFrame()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.onEveryNChanged(1);

    const auto in = makeTintedFrame(42);
    QVERIFY(in);
    node.setInData(in, 0);

    // The node holds the frame twice — m_lastInput (so a dropped frame can be
    // re-examined) and m_output (what consumers get) — plus the test's own
    // reference. Three refs, one object.
    QCOMPARE(in.use_count(), 3L);

    const std::shared_ptr<NodeData> out = node.outData(0);
    QVERIFY(out);
    // Identity, not equality: a gate that re-wrapped the frame in a fresh
    // VideoFrameData would still produce visually identical output, so only a
    // pointer comparison can catch the copy.
    QCOMPARE(out.get(), static_cast<NodeData *>(in.get()));
    // Taking the output only bumps the ref-count — the frame itself is not
    // duplicated. A copy would show up here as the count staying flat while a
    // second object existed, which the pointer compare above already rules out;
    // the count pins that nothing else is allocated.
    QCOMPARE(in.use_count(), 4L);
}

void TestFrameSamplerNode::fanOutToManyConsumersDoesNotCopy()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.onEveryNChanged(1);

    const auto in = makeTintedFrame(7);
    node.setInData(in, 0);
    const long base = in.use_count();

    // outData() is called once per connected consumer. Repeated calls must all
    // hand back the same object — no per-consumer copy, and no growth of the
    // frame's use_count from the reads themselves.
    const void *first = node.outData(0).get();
    for (int consumer = 0; consumer < 8; ++consumer) {
        const std::shared_ptr<NodeData> out = node.outData(0);
        QCOMPARE(out.get(), first);
        // Exactly one extra ref while `out` is alive, and exactly one fewer
        // once it goes away — a retained copy would drift upwards.
        QCOMPARE(in.use_count(), base + 1);
    }
    QCOMPARE(in.use_count(), base);
}

void TestFrameSamplerNode::droppedFrameKeepsThePreviousOutput()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.onEveryNChanged(2);

    const auto first = makeTintedFrame(1);
    node.setInData(first, 0); // #1, dropped
    QVERIFY(!node.outData(0));

    const auto second = makeTintedFrame(2);
    node.setInData(second, 0); // #2, passes
    QCOMPARE(node.outData(0).get(), static_cast<NodeData *>(second.get()));

    // A dropped frame is not an invalidation: downstream keeps the last frame
    // it was given instead of the output going black between passes. That is
    // the whole point of a sampler in a live view.
    QSignalSpy invalidated(&node, &QtNodes::NodeDelegateModel::dataInvalidated);
    node.setInData(makeTintedFrame(3), 0); // #3, dropped
    QCOMPARE(invalidated.count(), 0);
    QCOMPARE(node.outData(0).get(), static_cast<NodeData *>(second.get()));
}

// ── Invalid input ────────────────────────────────────────────────────────────

void TestFrameSamplerNode::invalidInputClearsOutput()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.onEveryNChanged(1);
    const auto in = makeTintedFrame(1);
    node.setInData(in, 0);
    QVERIFY(node.outData(0));

    // A source stopping must invalidate, not hold the last frame forever —
    // otherwise a downstream VideoOutput keeps painting a frozen image with
    // no indication the graph is dead.
    QSignalSpy invalidated(&node, &QtNodes::NodeDelegateModel::dataInvalidated);
    node.setInData(std::make_shared<VideoFrameData>(), 0); // no frame
    QCOMPARE(invalidated.count(), 1);
    QVERIFY(!node.outData(0));
}

void TestFrameSamplerNode::wrongDataTypeClearsOutput()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.onEveryNChanged(1);
    node.setInData(makeTintedFrame(1), 0);
    QVERIFY(node.outData(0));

    // The graph model can hand a gate any NodeData when a port is re-typed in
    // a loaded scene; the dynamic_pointer_cast must fail closed, not pass the
    // foreign object through as if it were a frame.
    QSignalSpy invalidated(&node, &QtNodes::NodeDelegateModel::dataInvalidated);
    node.setInData(nullptr, 0);
    QCOMPARE(invalidated.count(), 1);
    QVERIFY(!node.outData(0));
}

void TestFrameSamplerNode::nullInputClearsOutput()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::EveryNth));
    node.onEveryNChanged(1);
    node.setInData(makeTintedFrame(1), 0);
    QVERIFY(node.outData(0));

    // Disconnecting a source delivers a null shared_ptr on the input port.
    QSignalSpy invalidated(&node, &QtNodes::NodeDelegateModel::dataInvalidated);
    node.setInData(nullptr, 0);
    QCOMPARE(invalidated.count(), 1);
    QVERIFY(!node.outData(0));
}

// ── Persistence ──────────────────────────────────────────────────────────────

void TestFrameSamplerNode::saveLoadRoundTripsParameters()
{
    FrameSamplerNode node;
    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::MaxFps));
    node.onEveryNChanged(7);
    node.onMaxFpsChanged(23);

    FrameSamplerNode restored;
    restored.load(node.save());

    QCOMPARE(restored.mode(), FrameSamplerNode::Mode::MaxFps);
    QCOMPARE(restored.everyN(), 7);
    QCOMPARE(restored.maxFps(), 23);
}

void TestFrameSamplerNode::saveCarriesModelName()
{
    // Same invariant as the whole demo_nodeditor suite: without "model-name"
    // the node saves happily and loads back as "<unnamed>", taking every
    // connection to it with it. See test_model_name_serialization.
    FrameSamplerNode node;
    node.onEveryNChanged(4);
    const QJsonObject saved = node.save();
    QCOMPARE(saved.value(QStringLiteral("model-name")).toString(), QStringLiteral("FrameSampler"));
    QCOMPARE(saved.value(QStringLiteral("everyN")).toInt(), 4);
    QCOMPARE(saved.value(QStringLiteral("mode")).toString(), QStringLiteral("everyNth"));

    node.onModeChanged(static_cast<int>(FrameSamplerNode::Mode::MaxFps));
    QCOMPARE(node.save().value(QStringLiteral("mode")).toString(), QStringLiteral("maxFps"));
}

void TestFrameSamplerNode::parameterValuesAreClamped()
{
    FrameSamplerNode node;

    // EveryNth is used as a divisor; 0 would be a division by zero in the
    // modulo and 1000+ is a UI-value accident. MaxFps feeds a nanosecond
    // interval, so it is bounded too.
    node.onEveryNChanged(0);
    QCOMPARE(node.everyN(), 1);
    node.onEveryNChanged(-5);
    QCOMPARE(node.everyN(), 1);
    node.onEveryNChanged(99999);
    QCOMPARE(node.everyN(), 1000);

    node.onMaxFpsChanged(0);
    QCOMPARE(node.maxFps(), 1);
    node.onMaxFpsChanged(-1);
    QCOMPARE(node.maxFps(), 1);
    node.onMaxFpsChanged(100000);
    QCOMPARE(node.maxFps(), 120);

    // The clamps must hold through load() too, not only through the slots:
    // a hand-edited .flow is the way an out-of-range value actually arrives.
    QJsonObject hostile = node.save();
    hostile[QStringLiteral("everyN")] = 0;
    hostile[QStringLiteral("maxFps")] = -20;
    node.load(hostile);
    QCOMPARE(node.everyN(), 1);
    QCOMPARE(node.maxFps(), 1);
}

void TestFrameSamplerNode::loadToleratesGarbage()
{
    FrameSamplerNode node;

    // Missing keys must fall back to the constructor defaults rather than to
    // zero — otherwise loading a scene saved by an older version that did not
    // have "maxFps" would set a 0 fps gate that never emits.
    QJsonObject empty;
    node.load(empty);
    QCOMPARE(node.mode(), FrameSamplerNode::Mode::EveryNth);
    QCOMPARE(node.everyN(), 2);
    QCOMPARE(node.maxFps(), 10);

    // An unknown mode string must fall back to EveryNth (the safe default),
    // not become an invalid enum value that matches no branch.
    QJsonObject bogus;
    bogus[QStringLiteral("mode")] = QStringLiteral("everyOtherThing");
    bogus[QStringLiteral("everyN")] = QStringLiteral("not-a-number");
    bogus[QStringLiteral("maxFps")] = QStringLiteral("not-a-number");
    node.load(bogus);
    QCOMPARE(node.mode(), FrameSamplerNode::Mode::EveryNth);
    QCOMPARE(node.everyN(), 2);
    QCOMPARE(node.maxFps(), 10);

    // …and the node must still work afterwards rather than gate everything.
    node.onEveryNChanged(1);
    QCOMPARE(feedFrames(node, 3), 3);
}

QTEST_GUILESS_MAIN(TestFrameSamplerNode)

#include "test_frame_sampler_node.moc"
