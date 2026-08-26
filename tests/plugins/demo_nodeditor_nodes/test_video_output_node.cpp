#include "test_video_output_node.h"

#include "NodeDataTypes/VideoFrameData.h"
#include "VideoOutputNode.h"

#include <QImage>
#include <QSignalSpy>
#include <QVideoFrame>

using QtNodes::ConnectionId;
using QtNodes::PortIndex;
using QtNodes::PortType;

// ── Construct a valid ConnectionId for input/output connection tests ──────────
//
// VideoOutputNode only inspects inPortIndex / outPortIndex; the NodeId values
// are irrelevant for isolated unit tests.
static ConnectionId makeConId(PortIndex inPort, PortIndex outPort)
{
    return {0, outPort, 0, inPort};
}

// ── portTopology ─────────────────────────────────────────────────────────────
//
// On both Qt versions the node exposes a single input port (video-frame) plus
// one output (REQ-SW-PL-020, NV12-direct, single video-frame type
// REQ-SW-PL-032).
void VideoOutputNodeTest::portTopology()
{
    VideoOutputNode node;

    QCOMPARE(node.nPorts(PortType::In), 1u);
    // Port 0: "video-frame"
    {
        const auto dt = node.dataType(PortType::In, 0);
        QCOMPARE(dt.id, QStringLiteral("video-frame"));
        QCOMPARE(dt.name, QStringLiteral("Video Frame"));
    }

    QCOMPARE(node.nPorts(PortType::Out), 1u);
    {
        const auto dt = node.dataType(PortType::Out, 0);
        QCOMPARE(dt.id, QStringLiteral("video-frame"));
        QCOMPARE(dt.name, QStringLiteral("Video Frame"));
    }
}

// ── videoInputConnectionGuard ────────────────────────────────────────────────
//
// 1. inputConnectionCreated(0) + outputConnectionCreated(0) → both connected
// 2. Feed a valid VideoFrameData → accepted (dataUpdated emitted via output
//    chain, verifying the frame was not blocked)
// 3. inputConnectionDeleted(0) → m_videoInputConnected = false
// 4. Feed another VideoFrameData → REJECTED (no dataUpdated emitted)
void VideoOutputNodeTest::videoInputConnectionGuard()
{
    VideoOutputNode node;
    QSignalSpy spyUpdated(&node, &QtNodes::NodeDelegateModel::dataUpdated);

    // Step 1: establish the port-0 input connection AND an output connection
    // so the per-frame QImage conversion path is active (dataUpdated fires).
    node.inputConnectionCreated(makeConId(0, 0));
    node.outputConnectionCreated(makeConId(0, 0));

    // Step 2: feed a valid frame → must be accepted (dataUpdated emitted).
    {
        QImage img(320, 240, QImage::Format_ARGB32);
        img.fill(Qt::red);
        QVideoFrame vf(img);
        node.setInData(std::make_shared<VideoFrameData>(vf), 0);
    }
    QCOMPARE(spyUpdated.count(), 1);

    // Step 3: disconnect the port-0 edge.
    spyUpdated.clear();
    node.inputConnectionDeleted(makeConId(0, 0));

    // Step 4: feed another valid frame → must be silently rejected.
    {
        QImage img(320, 240, QImage::Format_ARGB32);
        img.fill(Qt::blue);
        QVideoFrame vf(img);
        node.setInData(std::make_shared<VideoFrameData>(vf), 0);
    }
    // No dataUpdated was emitted — the guard blocked the frame.
    QCOMPARE(spyUpdated.count(), 0);
}

// ── outputConnectionCounter ──────────────────────────────────────────────────
//
// outputConnectionCreated/deleted(0) are tracked as a ref-count. The
// counter controls whether the per-frame QImage conversion runs: only
// when counter > 0 does setInData(port 0) produce a VideoFrameData output.
// We verify indirectly via the presence of dataUpdated(0).
void VideoOutputNodeTest::outputConnectionCounter()
{
    VideoOutputNode node;
    QSignalSpy spyUpdated(&node, &QtNodes::NodeDelegateModel::dataUpdated);

    // Establish the video input connection so frames are accepted.
    node.inputConnectionCreated(makeConId(0, 0));

    // Helper: feed a red 320x240 frame.
    auto feedFrame = [&]() {
        QImage img(320, 240, QImage::Format_ARGB32);
        img.fill(Qt::red);
        QVideoFrame vf(img);
        node.setInData(std::make_shared<VideoFrameData>(vf), 0);
    };

    // Without any output connection, no QImage conversion → no dataUpdated.
    feedFrame();
    QCOMPARE(spyUpdated.count(), 0);

    // outputConnectionCreated(0) → counter becomes 1.
    node.outputConnectionCreated(makeConId(0, 0));
    feedFrame();
    QCOMPARE(spyUpdated.count(), 1);

    // outputConnectionDeleted(0) → counter back to 0.
    node.outputConnectionDeleted(makeConId(0, 0));
    spyUpdated.clear();
    feedFrame();
    QCOMPARE(spyUpdated.count(), 0);
}

// ── outputChain ──────────────────────────────────────────────────────────────
//
// With an output connection present, feeding a synthetic VideoFrameData
// produces VideoFrameData on outData(0) and emits dataUpdated(0). The output
// frame must match the source frame's dimensions.
void VideoOutputNodeTest::outputChain()
{
    VideoOutputNode node;
    QSignalSpy spyUpdated(&node, &QtNodes::NodeDelegateModel::dataUpdated);

    // Establish both input and output connections.
    node.inputConnectionCreated(makeConId(0, 0));
    node.outputConnectionCreated(makeConId(0, 0));

    // Feed a synthetic video frame (QImage → QVideoFrame → VideoFrameData).
    const QSize frameSize(320, 240);
    QImage img(frameSize, QImage::Format_ARGB32);
    img.fill(Qt::red);
    QVideoFrame vf(img);
    node.setInData(std::make_shared<VideoFrameData>(vf), 0);

    QCOMPARE(spyUpdated.count(), 1);
    QCOMPARE(spyUpdated.at(0).at(0).toInt(), 0);

    // outData(0) must contain the converted VideoFrameData.
    auto out = node.outData(0);
    QVERIFY(out != nullptr);
    auto outFrame = std::dynamic_pointer_cast<VideoFrameData>(out);
    QVERIFY(outFrame != nullptr);
    QCOMPARE(outFrame->asImage().width(), frameSize.width());
    QCOMPARE(outFrame->asImage().height(), frameSize.height());
}

QTEST_MAIN(VideoOutputNodeTest)
