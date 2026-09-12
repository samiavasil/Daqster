#include "test_video_output_node.h"

#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "VideoDisplayBackend.h"
#include "VideoGLBlitWidget.h"
#include "VideoOutputNode.h"
#include "VideoSoftwareWidget.h"

#include <QCheckBox>
#include <QColor>
#include <QCoreApplication>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QImage>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSplitter>
#include <QVideoFrame>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QApplication>
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

// ── cpuEffectAppliesToGpuRgbaInput (REQ-SW-PL-039 Bug A) ─────────────────────
//
// A CPU-only embedded effect (blur) on a GpuRgba input must run the CPU path
// (asImage() readback) instead of being skipped by the old
// `if (!videoFrame->isGpuRgba())` gate. Requires a real GL texture so the
// readback has pixels — skipped when no GL context can be created.
void VideoOutputNodeTest::cpuEffectAppliesToGpuRgbaInput()
{
    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    if (!mgr.makeCurrent())
        QSKIP("No GL context — GpuRgba readback requires GL");
    VideoGLContextManager::CurrentGuard guard(mgr);
    QOpenGLFunctions *f = mgr.context()->functions();

    // Upload a solid red RGBA texture (64x48).
    const int w = 64, h = 48;
    QImage red(w, h, QImage::Format_RGBA8888);
    red.fill(Qt::red);
    GLuint tex = 0;
    f->glGenTextures(1, &tex);
    f->glBindTexture(GL_TEXTURE_2D, tex);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    red.constBits());
    f->glBindTexture(GL_TEXTURE_2D, 0);

    VideoTextureHandle hnd;
    hnd.texY = tex;
    hnd.width = w;
    hnd.height = h;
    hnd.rgba = true;
    auto gpuFrame = VideoFrameData::fromTexture(hnd);

    VideoOutputNode node;
    node.inputConnectionCreated(makeConId(0, 0));
    node.outputConnectionCreated(makeConId(0, 0));

    // Select a CPU-only effect (blur) and feed the GpuRgba frame.
    QJsonObject graph;
    graph[QStringLiteral("effect")] = QStringLiteral("blur");
    graph[QStringLiteral("blurRadius")] = 3;
    node.load(graph);
    node.setInData(gpuFrame, 0);

    auto out = std::dynamic_pointer_cast<VideoFrameData>(node.outData(0));
    QVERIFY(out != nullptr);
    // The CPU path must have produced a CPU-resident frame (not GpuRgba).
    QVERIFY(!out->isGpuRgba());
    QCOMPARE(out->asImage().size(), QSize(w, h));
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
// ── gpuRgbaRoutesToGlBlitWidget (REQ-SW-PL-053) ──────────────────────────────
//
// Three-mode design: the unified display (REQ-SW-PL-053) selects the backend
// ONCE at construction — hardware GL → VideoGLBlitWidget (GPU), else
// VideoSoftwareWidget (CPU). The display widget lives in the live page of
// embeddedWidget() (not in a detached window). GpuRgba frames (effect outputs)
// route to presentTexture (zero-copy) on the GL backend.
void VideoOutputNodeTest::gpuRgbaRoutesToGlBlitWidget()
{
    if (!VideoGLContextManager::hasHardwareGL())
        QSKIP("No hardware GL — GL blit backend requires hardware GL");
    if (detectVideoBackend() != VideoBackend::Gl)
        QSKIP("Video backend is not GL (DAQSTER_VIDEO_BACKEND override) — GL blit routing not applicable");

    VideoOutputNode node;
    node.inputConnectionCreated(makeConId(0, 0));

    // (b) The display widget is created in the constructor and lives in the
    // live page of embeddedWidget() (three-mode design).
    QWidget *embedded = node.embeddedWidget();
    QVERIFY(embedded != nullptr);
    // After construction: the display widget already exists.
    QVERIFY(node.displayWidget() != nullptr);

    // (a) GpuRgba frame (effect output) routes to the GL blit widget.
    // The texture handle is a placeholder — the routing decision only needs
    // isGpuRgba() + hardware GL; texY must be non-zero so presentTexture()
    // accepts it (it only stores the handle + schedules a repaint, no GL call).
    VideoTextureHandle h;
    h.width = 320;
    h.height = 240;
    h.rgba = true;
    h.texY = 1;
    node.setInData(VideoFrameData::fromTexture(h), 0);

    // The GL blit widget received the texture present (zero-copy presentTexture).
    // VideoDisplayWidget is a pure interface (not QObject-derived) — use
    // dynamic_cast to recover the concrete GL backend.
    auto *glDisplay = dynamic_cast<VideoGLBlitWidget *>(node.displayWidget());
    QVERIFY(glDisplay != nullptr);  // GL backend selected (hardware GL)
    QCOMPARE(glDisplay->lastFormatName(), QStringLiteral("Texture(RGBA)"));
}
#endif  // QT_VERSION >= 0x060000

// ── embeddedWidgetContainsDisplayAndControls (REQ-SW-PL-053) ─────────────
//
// Three-mode design: embeddedWidget() is the single home of the display and
// controls. The live page (page 1 of the QStackedWidget) contains the video
// display widget (GL blit or software backend) and the controls splitter with
// the Perf toggle checkbox — reachable via findChildren in run mode.
void VideoOutputNodeTest::embeddedWidgetContainsDisplayAndControls()
{
    VideoOutputNode node;

    QWidget *embedded = node.embeddedWidget();
    QVERIFY(embedded != nullptr);

    // The display widget (one of the two backends) lives inside embeddedWidget().
    // VideoDisplayWidget is a pure interface (not QObject-derived), so
    // findChildren uses the concrete backend types.
    const auto glDisplays = embedded->findChildren<VideoGLBlitWidget *>();
    const auto swDisplays = embedded->findChildren<VideoSoftwareWidget *>();
    QVERIFY(!glDisplays.isEmpty() || !swDisplays.isEmpty());

    // The Perf toggle checkbox is reachable via findChildren (run mode).
    const auto checkBoxes = embedded->findChildren<QCheckBox *>();
    bool foundPerf = false;
    for (QCheckBox *cb : checkBoxes) {
        if (cb->text() == QStringLiteral("Perf")) {
            foundPerf = true;
            break;
        }
    }
    QVERIFY(foundPerf);

    // The live page splitter (display left + controls right) exists.
    QVERIFY(!embedded->findChildren<QSplitter *>().isEmpty());
}

// ── displayModeSwitchesWithProxyEmbedding (REQ-SW-PL-053) ────────────────
//
// Mode detection is m_widget->graphicsProxyWidget() != nullptr:
//   - no proxy → live page active (deembedded / run mode);
//   - embedded in a QGraphicsProxyWidget → preview page active (editor mode);
//   - detached from the proxy → live page active again.
// The real deembed flow (NodeGraphicsObject::setWidgetEmbedded(false)) detaches
// the proxy, reparents to a top-level window, and re-shows the widget — the
// Show event drives updateDisplayMode(). The test replicates that re-show.
// The proxy MUST be detached (setWidget(nullptr)) before the node is
// destroyed — QGraphicsProxyWidget takes ownership of the widget.
void VideoOutputNodeTest::displayModeSwitchesWithProxyEmbedding()
{
    VideoOutputNode node;

    // No proxy → live page active.
    QVERIFY(node.isLiveDisplayActive());

    // Embed in a scene proxy → preview page active.
    QGraphicsScene scene;
    QGraphicsProxyWidget *proxy = scene.addWidget(node.embeddedWidget());
    QCoreApplication::processEvents();
    QVERIFY(!node.isLiveDisplayActive());

    // Deembed (detach the proxy, then re-show like the real flow) → live page
    // active again. setWidget(nullptr) alone fires no event — the widget stays
    // visible as a top-level window; the re-show (Show event) is what triggers
    // updateDisplayMode().
    proxy->setWidget(nullptr);
    node.embeddedWidget()->hide();
    node.embeddedWidget()->show();
    QCoreApplication::processEvents();
    QVERIFY(node.isLiveDisplayActive());
}

QTEST_MAIN(VideoOutputNodeTest)
