#include "test_video_output_node.h"

#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "VideoGLBlitWidget.h"
#include "VideoOutputNode.h"

#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QSignalSpy>
#include <QVideoFrame>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QApplication>
#include <QVideoWidget>
#endif

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

// ── defaultNoEffectPassthrough ───────────────────────────────────────────────
//
// REQ-SW-PL-034 AC 1/2: the node defaults to "No effect" — save() must NOT
// write an "effect" key, and the output frame must be byte-identical to the
// input (zero-copy passthrough preserved when no effect is selected).
void VideoOutputNodeTest::defaultNoEffectPassthrough()
{
    VideoOutputNode node;
    node.inputConnectionCreated(makeConId(0, 0));
    node.outputConnectionCreated(makeConId(0, 0));

    // Default save: no "effect" key (backward compatible with old graphs).
    const QJsonObject saved = node.save();
    QVERIFY(!saved.contains(QStringLiteral("effect")));

    // Feed a frame → output must equal the input pixels exactly.
    QImage img(64, 48, QImage::Format_ARGB32);
    img.fill(Qt::red);
    QVideoFrame vf(img);
    node.setInData(std::make_shared<VideoFrameData>(vf), 0);

    auto out = std::dynamic_pointer_cast<VideoFrameData>(node.outData(0));
    QVERIFY(out != nullptr);
    // Qt6 asImage() normalizes to ARGB32 — compare pixel content, not format.
    QCOMPARE(out->asImage().convertToFormat(img.format()), img);
}

// ── loadEffectPersistsSave ───────────────────────────────────────────────────
//
// REQ-SW-PL-034 AC 4: loading a graph with an "effect" key selects the effect
// and save() round-trips the id + parameters.
void VideoOutputNodeTest::loadEffectPersistsSave()
{
    VideoOutputNode node;

    QJsonObject graph;
    graph[QStringLiteral("effect")] = QStringLiteral("brightness");
    graph[QStringLiteral("brightness")] = 42;
    node.load(graph);

    const QJsonObject saved = node.save();
    QCOMPARE(saved.value(QStringLiteral("effect")).toString(),
             QStringLiteral("brightness"));
    QCOMPARE(saved.value(QStringLiteral("brightness")).toInt(), 42);
}

// ── loadAbsentEffectIsNoEffect ───────────────────────────────────────────────
//
// REQ-SW-PL-034 AC 4 (backward compatible): old graphs without the "effect"
// key load as no-effect — save() omits the key again.
void VideoOutputNodeTest::loadAbsentEffectIsNoEffect()
{
    VideoOutputNode node;

    // Old graph: only display-related keys, no "effect".
    QJsonObject graph;
    graph[QStringLiteral("someOldKey")] = QStringLiteral("value");
    node.load(graph);

    const QJsonObject saved = node.save();
    QVERIFY(!saved.contains(QStringLiteral("effect")));
}

// ── loadAppliesEffectToFrame ─────────────────────────────────────────────────
//
// REQ-SW-PL-034 AC 3: with an effect loaded, the output frame is transformed.
// CPU brightness maps delta -100..+100 to -255..+255 (shift = delta*255/100),
// so brightness=10 → shift=25 → 128+25 = 153.
void VideoOutputNodeTest::loadAppliesEffectToFrame()
{
    VideoOutputNode node;
    node.inputConnectionCreated(makeConId(0, 0));
    node.outputConnectionCreated(makeConId(0, 0));

    QJsonObject graph;
    graph[QStringLiteral("effect")] = QStringLiteral("brightness");
    graph[QStringLiteral("brightness")] = 10;
    node.load(graph);

    // Mid-gray input (128,128,128) → brightness +10 → (153,153,153).
    QImage img(64, 48, QImage::Format_RGB32);
    img.fill(QColor(128, 128, 128));
    QVideoFrame vf(img);
    node.setInData(std::make_shared<VideoFrameData>(vf), 0);

    auto out = std::dynamic_pointer_cast<VideoFrameData>(node.outData(0));
    QVERIFY(out != nullptr);
    const QImage outImg = out->asImage();
    QCOMPARE(outImg.pixelColor(0, 0).red(), 153);
    QCOMPARE(outImg.pixelColor(0, 0).green(), 153);
    QCOMPARE(outImg.pixelColor(0, 0).blue(), 153);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
// ── gpuRgbaRoutesToGlBlitWidget (REQ-SW-PL-032 Stage 2C) ─────────────────────
//
// On Qt6, GpuRgba frames (effect outputs) must route to the GL blit widget
// (zero-copy presentTexture) instead of the native QVideoWidget (readback +
// per-sink RHI upload). The routing requires hardware GL — on software
// renderers (llvmpipe/softpipe/SwiftShader) the native path stays and the
// test is skipped.
void VideoOutputNodeTest::gpuRgbaRoutesToGlBlitWidget()
{
    if (!VideoGLContextManager::hasHardwareGL())
        QSKIP("No hardware GL — GpuRgba routing to GL blit requires hardware GL");

    VideoOutputNode node;
    node.inputConnectionCreated(makeConId(0, 0));

    // GpuRgba frame (effect output). The texture handle is a placeholder —
    // the routing decision only needs isGpuRgba() + hardware GL.
    VideoTextureHandle h;
    h.width = 320;
    h.height = 240;
    h.rgba = true;
    node.setInData(VideoFrameData::fromTexture(h), 0);

    // The detached display must be the GL blit widget (top-level window).
    bool foundGlBlit = false;
    const QWidgetList topLevels = QApplication::topLevelWidgets();
    for (QWidget *w : topLevels) {
        if (qobject_cast<VideoGLBlitWidget *>(w) != nullptr) {
            foundGlBlit = true;
            break;
        }
    }
    QVERIFY(foundGlBlit);

    // Transition: a CPU frame must switch to the native QVideoWidget and
    // destroy the GL blit window (only one detached display at a time).
    QImage img(320, 240, QImage::Format_ARGB32);
    img.fill(Qt::red);
    QVideoFrame vf(img);
    node.setInData(std::make_shared<VideoFrameData>(vf), 0);
    QTest::qWait(10);  // flush the deferred delete of the GL blit window

    bool foundNative = false;
    bool glBlitGone = true;
    const QWidgetList topLevels2 = QApplication::topLevelWidgets();
    for (QWidget *w : topLevels2) {
        if (qobject_cast<VideoGLBlitWidget *>(w) != nullptr)
            glBlitGone = false;
        if (qobject_cast<QVideoWidget *>(w) != nullptr)
            foundNative = true;
    }
    QVERIFY(foundNative);
    QVERIFY(glBlitGone);
}
#endif  // QT_VERSION >= 0x060000

QTEST_MAIN(VideoOutputNodeTest)
