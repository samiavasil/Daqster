#include "test_video_output_node.h"

#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "VideoGLBlitWidget.h"
#include "Sinks/Video/VideoOutputNode.h"

#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QSignalSpy>
#include <QVideoFrame>

#include <QApplication>

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
// ── Recording seam (REQ-SW-PL-055) ─────────────────────────────────────────────
//
// The unified display means ONE VideoDisplayWidget for the whole node lifetime,
// chosen once at construction by detectVideoBackend() — not per frame and not by
// pixel residency. Before REQ-SW-PL-055 the model swapped a QVideoWidget for a
// detached VideoGLBlitWidget whenever a GpuRgba frame arrived, so this file used
// to scrape QApplication::topLevelWidgets() to see which widget existed.
//
// Scraping widgets cannot observe the routing decision, and it needed a GL
// context. RecordingDisplay below observes the decision itself: it counts which
// present*() the model chose. That is the property REQ-SW-PL-032 (zero-copy, no
// readback) and REQ-SW-PL-055 (one backend, chosen once) actually care about, and
// it is deterministic on Qt5, Qt6 and on a software renderer.
class RecordingDisplay : public VideoDisplayWidget
{
public:
    QWidget *widget() override { return m_widget; }
    void presentFrame(const QVideoFrame &) override { ++frameCalls; }
    void presentImage(const QImage &) override { ++imageCalls; }
    void presentTexture(const VideoTextureHandle &,
                        std::shared_ptr<VideoFrameData>) override
    { ++textureCalls; }
    void presentYuvTexture(const VideoTextureHandle &,
                           std::shared_ptr<VideoFrameData>) override
    { ++yuvTextureCalls; }
    void setVideoSize(const QSize &) override {}
    void clear() override { ++clearCalls; }
    QString backendName() const override { return QStringLiteral("recording"); }
    bool isGpuBackend() const override { return false; }

    int frameCalls = 0;
    int imageCalls = 0;
    int textureCalls = 0;
    int yuvTextureCalls = 0;
    int clearCalls = 0;

private:
    QWidget *m_widget = nullptr;
};

// Minimal IVideoOutputUi: everything the model touches is recorded, nothing is
// drawn. Implements every pure virtual so the test cannot silently drift from
// the interface.
class RecordingUi : public IVideoOutputUi
{
public:
    explicit RecordingUi(IVideoOutputUiHandler *handler)
        : m_handler(handler) {}

    QWidget *rootWidget() const override { return m_root; }
    VideoDisplayWidget *display() const override { return &m_display; }
    QWidget *controlsWidget() const override { return m_controls; }
    void setLiveActive(bool live) override { liveActive = live; }
    bool isLiveActive() const override { return liveActive; }
    bool isEmbeddedInProxy() const override { return inProxy; }
    void setPreviewImage(const QImage &) override { ++previewCalls; }
    void clearPreview() override { ++clearPreviewCalls; }
    bool previewVisible() const override { return false; }
    void setEffectPage(int index) override { effectPage = index; }
    void syncEffectParams() override { ++syncCalls; }
    void updatePerfStats(const VideoPerfStats &s) override { lastStats = s; }
    void setPerfChecked(bool checked) override { perfOn = checked; }
    bool perfChecked() const override { return perfOn; }

    void setInProxy(bool value) { inProxy = value; }

    // mutable: display() is a const accessor returning a non-const pointer (the
    // model presents through it), so a direct &m_display in a const method would
    // otherwise deduce const and fail to convert.
    mutable RecordingDisplay m_display;
    QWidget *m_root = nullptr;
    QWidget *m_controls = nullptr;
    bool liveActive = true;
    bool inProxy = false;
    bool perfOn = false;
    int effectPage = -1;
    int previewCalls = 0;
    int clearPreviewCalls = 0;
    int syncCalls = 0;
    VideoPerfStats lastStats;

private:
    IVideoOutputUiHandler *m_handler = nullptr;
};

// Build a model with its UI already attached, as the GUI plugin would.
static void attachUi(VideoOutputNode &node, RecordingUi &ui)
{
    node.setUi(&ui);
    node.forceLiveDisplay();   // live page, so setInData presents
}

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

// ── Unified display routing (REQ-SW-PL-055 + REQ-SW-PL-032) ───────────────────

// A GpuRgba frame must go to presentTexture — the handle is handed over, never
// read back. This is the property that made the old native-QVideoWidget fallback
// unacceptable: asImage() on a GPU-resident frame costs a full download plus a
// per-sink RHI upload, every frame.
void VideoOutputNodeTest::gpuRgbaRoutesZeroCopyToDisplay()
{
    VideoOutputNode node;
    RecordingUi ui(node.uiHandler());
    attachUi(node, ui);
    node.inputConnectionCreated(makeConId(0, 0));

    VideoTextureHandle h;
    h.width = 320;
    h.height = 240;
    h.rgba = true;
    node.setInData(VideoFrameData::fromTexture(h), 0);

    QCOMPARE(ui.m_display.textureCalls, 1);
    QCOMPARE(ui.m_display.imageCalls, 0);      // no readback
    QCOMPARE(ui.m_display.frameCalls, 0);      // not the CPU frame path
    QCOMPARE(ui.m_display.yuvTextureCalls, 0);
}

// A CPU-resident YUV frame must take the YUV path — also as a texture handle
// (VideoFrameData exposes NV12/YUV420P planes as a GPU-uploadable handle), never
// through presentImage.
void VideoOutputNodeTest::cpuFrameUsesYuvPathOnSameDisplay()
{
    VideoOutputNode node;
    RecordingUi ui(node.uiHandler());
    attachUi(node, ui);
    node.inputConnectionCreated(makeConId(0, 0));

    QImage img(320, 240, QImage::Format_ARGB32);
    img.fill(Qt::red);
    node.setInData(std::make_shared<VideoFrameData>(QVideoFrame(img)), 0);

    // Whichever of the two YUV routes the frame supports, the point is that it
    // is NOT presentImage and NOT presentTexture(GpuRgba).
    QVERIFY(ui.m_display.yuvTextureCalls + ui.m_display.frameCalls == 1);
    QCOMPARE(ui.m_display.imageCalls, 0);
    QCOMPARE(ui.m_display.textureCalls, 0);
}

// No UI attached → no display at all, and embeddedWidget() is null. This is the
// IWidgetProvider contract: the editor asks providers first, and only falls back
// to embeddedWidget() — so a null embeddedWidget() with a provider registered is
// the intended configuration, not a broken node.
void VideoOutputNodeTest::noDisplayUntilUiIsAttached()
{
    VideoOutputNode node;
    QVERIFY(node.embeddedWidget() == nullptr);
    QVERIFY(node.displayWidget() == nullptr);
    QVERIFY(node.controlsWidget() == nullptr);

    // Frames must still be accepted with no UI — a headless flow run has no
    // widgets at all, and it must not drop frames or crash.
    node.inputConnectionCreated(makeConId(0, 0));
    VideoTextureHandle h;
    h.width = 16; h.height = 16; h.rgba = true;
    node.setInData(VideoFrameData::fromTexture(h), 0);
}

// The backend is decided ONCE, at UI construction. A change in pixel residency
// must not swap the display object — that swap was the bug REQ-SW-PL-055 fixed.
void VideoOutputNodeTest::backendChosenOnceNotPerFrame()
{
    VideoOutputNode node;
    RecordingUi ui(node.uiHandler());
    attachUi(node, ui);
    node.inputConnectionCreated(makeConId(0, 0));

    VideoDisplayWidget *first = node.displayWidget();
    QVERIFY(first != nullptr);

    // GpuRgba, then CPU, then GpuRgba again — three different residencies.
    VideoTextureHandle gpuHandle;
    gpuHandle.width = 64; gpuHandle.height = 64; gpuHandle.rgba = true;
    node.setInData(VideoFrameData::fromTexture(gpuHandle), 0);

    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(Qt::blue);
    node.setInData(std::make_shared<VideoFrameData>(QVideoFrame(img)), 0);

    node.setInData(VideoFrameData::fromTexture(gpuHandle), 0);

    QCOMPARE(node.displayWidget(), first);   // same object throughout
    QCOMPARE(ui.m_display.backendName(), QStringLiteral("recording"));
    // Exactly one present per frame — no frame is presented twice because a
    // backend swap re-issued it.
    QCOMPARE(ui.m_display.textureCalls + ui.m_display.imageCalls
                 + ui.m_display.frameCalls + ui.m_display.yuvTextureCalls,
             3);
}


QTEST_MAIN(VideoOutputNodeTest)
