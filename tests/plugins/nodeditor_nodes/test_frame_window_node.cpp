#include "test_frame_window_node.h"

#include "NodeDataTypes/FrameWindowData.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "Processing/Video/FrameWindowNode.h"

#include <QImage>
#include <QJsonObject>
#include <QSignalSpy>
#include <QVideoFrame>

#include <memory>

using QtNodes::NodeData;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

/// A VideoFrameData carrying a real frame. QVideoFrame(QImage) works on both
/// Qt versions without a decoder, OpenCV or GL — same helper as the
/// FrameSampler tests.
std::shared_ptr<VideoFrameData> makeFrame(int tint = 0)
{
    QImage img(32, 24, QImage::Format_RGB32);
    img.fill(QColor(tint % 256, (tint * 5) % 256, (tint * 11) % 256));
    QVideoFrame frame(img);
    if (!frame.isValid())
        return nullptr;
    return std::make_shared<VideoFrameData>(frame);
}

std::shared_ptr<FrameWindowData> lastWindow(FrameWindowNode &node)
{
    return std::dynamic_pointer_cast<FrameWindowData>(node.outData(0));
}

} // namespace

// ── Contract with the graph model ────────────────────────────────────────────

void TestFrameWindowNode::portsAndDataTypes()
{
    FrameWindowNode node;
    QCOMPARE(node.nPorts(PortType::In), 1u);
    QCOMPARE(node.nPorts(PortType::Out), 1u);
    QCOMPARE(node.nPorts(PortType::None), 0u);

    QCOMPARE(node.dataType(PortType::In, 0).id, QStringLiteral("video-frame"));
    QCOMPARE(node.dataType(PortType::Out, 0).id, QStringLiteral("frame-window"));
    QCOMPARE(FrameWindowData().type().id, QStringLiteral("frame-window"));
    QCOMPARE(FrameWindowData().type().name, QStringLiteral("Frame Window"));
}

void TestFrameWindowNode::embeddedWidgetIsNull()
{
    FrameWindowNode node;
    QCOMPARE(node.embeddedWidget(), nullptr);
}

void TestFrameWindowNode::optOutOfGeometryRecompute()
{
    FrameWindowNode node;
    QVERIFY(!node.dataArrivalChangesGeometry());
    QVERIFY(!node.dataArrivalChangesWidget());
}

void TestFrameWindowNode::nameAndCaption()
{
    FrameWindowNode node;
    QCOMPARE(node.name(), QStringLiteral("FrameWindow"));
    QCOMPARE(node.caption(), QStringLiteral("Frame Window"));
}

// ── Emit modes ───────────────────────────────────────────────────────────────

void TestFrameWindowNode::fullModeWaitsForWindowThenEmitsEveryFrame()
{
    FrameWindowNode node;
    node.onWindowChanged(4);
    node.onModeChanged(static_cast<int>(FrameWindowNode::EmitMode::Full));

    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);
    for (int i = 0; i < 3; ++i) {
        node.setInData(makeFrame(i), 0);
        QCOMPARE(updated.count(), 0); // partial window is not published
        QVERIFY(!node.outData(0));
    }

    node.setInData(makeFrame(3), 0);
    QCOMPARE(updated.count(), 1);
    auto window = lastWindow(node);
    QVERIFY(window);
    QCOMPARE(window->count(), 4);

    // After the window fills it keeps sliding and emits on every frame.
    node.setInData(makeFrame(4), 0);
    QCOMPARE(updated.count(), 2);
    window = lastWindow(node);
    QCOMPARE(window->count(), 4);
}

void TestFrameWindowNode::everyFrameModeEmitsOnEveryFrame()
{
    FrameWindowNode node;
    node.onWindowChanged(4);
    node.onModeChanged(static_cast<int>(FrameWindowNode::EmitMode::EveryFrame));

    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);
    for (int i = 0; i < 3; ++i) {
        node.setInData(makeFrame(i), 0);
        QCOMPARE(updated.count(), i + 1);
        QCOMPARE(lastWindow(node)->count(), i + 1); // partial window
    }
}

void TestFrameWindowNode::intervalModeEmitsAtMostOncePerInterval()
{
    FrameWindowNode node;
    node.onWindowChanged(4);
    node.onIntervalMsChanged(10000); // long interval: nothing else can pass
    node.onModeChanged(static_cast<int>(FrameWindowNode::EmitMode::Interval));

    QSignalSpy updated(&node, &QtNodes::NodeDelegateModel::dataUpdated);
    for (int i = 0; i < 6; ++i)
        node.setInData(makeFrame(i), 0);

    // The first frame emits immediately; the rest land inside the interval.
    QCOMPARE(updated.count(), 1);
    QCOMPARE(lastWindow(node)->count(), 1);
    QCOMPARE(node.collectedCount(), 4); // the window keeps sliding silently
}

// ── Window size ──────────────────────────────────────────────────────────────

void TestFrameWindowNode::windowKeepsOnlyLastFrames()
{
    FrameWindowNode node;
    node.onWindowChanged(3);
    node.onModeChanged(static_cast<int>(FrameWindowNode::EmitMode::EveryFrame));

    const auto first = makeFrame(1);
    const auto second = makeFrame(2);
    const auto third = makeFrame(3);
    const auto fourth = makeFrame(4);
    node.setInData(first, 0);
    node.setInData(second, 0);
    node.setInData(third, 0);
    node.setInData(fourth, 0);

    const auto window = lastWindow(node);
    QVERIFY(window);
    QCOMPARE(window->count(), 3);
    // The oldest frame was evicted; order is arrival order.
    QCOMPARE(window->items().at(0).frame.get(), second.get());
    QCOMPARE(window->items().at(1).frame.get(), third.get());
    QCOMPARE(window->items().at(2).frame.get(), fourth.get());
    QVERIFY(window->firstTimestampMs() <= window->lastTimestampMs());
}

void TestFrameWindowNode::windowSizeIsClamped()
{
    FrameWindowNode node;
    node.onWindowChanged(1); // below the minimum
    QCOMPARE(node.windowSize(), 2);
    node.onWindowChanged(1000); // above the maximum
    QCOMPARE(node.windowSize(), 32);
    node.onIntervalMsChanged(0);
    QCOMPARE(node.intervalMs(), 1);
}

// ── Zero-copy ────────────────────────────────────────────────────────────────

void TestFrameWindowNode::itemsShareTheInputSharedPointers()
{
    FrameWindowNode node;
    node.onWindowChanged(4);
    node.onModeChanged(static_cast<int>(FrameWindowNode::EmitMode::EveryFrame));

    const auto input = makeFrame(7);
    // Bits of the shared frame's lazily cached QImage — the SAME pointer must
    // come back out of the window (no pixel copy anywhere).
    const uchar *inputBits = input->asImage().constBits();

    node.setInData(input, 0);

    const auto window = lastWindow(node);
    QVERIFY(window);
    QCOMPARE(window->count(), 1);

    const auto &item = window->items().first();
    QCOMPARE(item.frame.get(), input.get());               // same shared_ptr
    QCOMPARE(item.frame.use_count() > 1, true);            // shared, not copied
    QCOMPARE(item.frame->asImage().constBits(), inputBits); // same pixel buffer
}

// ── Empty input ──────────────────────────────────────────────────────────────

void TestFrameWindowNode::nullInputClearsWindowAndInvalidates()
{
    FrameWindowNode node;
    node.onModeChanged(static_cast<int>(FrameWindowNode::EmitMode::EveryFrame));
    node.setInData(makeFrame(1), 0);
    QCOMPARE(node.collectedCount(), 1);
    QVERIFY(node.outData(0));

    QSignalSpy invalidated(&node, &QtNodes::NodeDelegateModel::dataInvalidated);
    node.setInData(nullptr, 0);

    QCOMPARE(invalidated.count(), 1);
    QCOMPARE(node.collectedCount(), 0);
    QVERIFY(!node.outData(0));
}

void TestFrameWindowNode::nonVideoInputClearsWindow()
{
    // A FrameWindowData arriving on the video input is not a frame — it must
    // be treated exactly like a null input, not silently accepted.
    FrameWindowNode node;
    node.onModeChanged(static_cast<int>(FrameWindowNode::EmitMode::EveryFrame));
    node.setInData(makeFrame(1), 0);

    QSignalSpy invalidated(&node, &QtNodes::NodeDelegateModel::dataInvalidated);
    node.setInData(std::make_shared<FrameWindowData>(), 0);

    QCOMPARE(invalidated.count(), 1);
    QCOMPARE(node.collectedCount(), 0);
}

// ── Persistence ──────────────────────────────────────────────────────────────

void TestFrameWindowNode::saveLoadRoundTrip()
{
    FrameWindowNode saved;
    saved.onWindowChanged(8);
    saved.onIntervalMsChanged(250);
    saved.onModeChanged(static_cast<int>(FrameWindowNode::EmitMode::Interval));
    const QJsonObject json = saved.save();
    QCOMPARE(json.value(QStringLiteral("emitMode")).toString(), QStringLiteral("interval"));
    QCOMPARE(json.value(QStringLiteral("window")).toInt(), 8);
    QCOMPARE(json.value(QStringLiteral("intervalMs")).toInt(), 250);

    FrameWindowNode loaded;
    loaded.load(json);
    QCOMPARE(loaded.windowSize(), 8);
    QCOMPARE(loaded.intervalMs(), 250);
    QCOMPARE(static_cast<int>(loaded.emitMode()),
             static_cast<int>(FrameWindowNode::EmitMode::Interval));
}
