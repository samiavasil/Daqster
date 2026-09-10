#ifndef VIDEOOUTPUTNODE_H
#define VIDEOOUTPUTNODE_H

#include <QtNodes/NodeDelegateModel>

#include "ProcessCpu.h"
#include "VideoEffectGLProcessor.h"
#include "VideoEffectOps.h"
#include "shared/IStoppable.h"

#include <QImage>
#include <QtMultimedia/QVideoFrame>
#include <functional>
#include <memory>

class QLabel;
class QWidget;

class QComboBox;
class QCheckBox;
class QStackedWidget;
class QTimer;
class QSplitter;
class QVBoxLayout;
class VideoDisplayWidget;
class VideoFrameData;

/**
 * @brief Video output node: displays incoming video frames.
 *
 * Option B — embedded placeholder + always-detached display (REQ-SW-PL-053):
 *
 * The node has a single input port on BOTH Qt versions (REQ-SW-PL-020,
 * NV12-direct, single video-frame type REQ-SW-PL-032):
 *   - port 0 "video-frame" — zero-copy VideoFrameData.
 *
 * Display architecture (Option B):
 *   1. **In-node preview**: QLabel (m_preview) showing a small (~200px wide)
 *      QImage snapshot at ~0.5 fps (m_previewTimer, 2000 ms interval,
 *      scale-before-convert — the full frame is never materialized). No GL,
 *      no scene repaint problem. Gated on visibility: the timer only runs
 *      while the label is shown (event filter) and updatePreview() skips when
 *      not visible — standalone --run mode costs zero. A "No video"
 *      placeholder is shown when no frame has arrived. The node widget
 *      (embeddedWidget()) contains ONLY this QLabel — no controls, no GL
 *      display.
 *   2. **Detached window**: QWidget (m_displayWindow, Qt::Window flag) housing
 *      a QSplitter with the unified VideoDisplayWidget (GL blit default / SW
 *      fallback) on the left and controls (Perf toggle + PerfStatsPanel +
 *      Effects) on the right. Opens automatically on first frame arrival.
 *      Title: "Video Output — <node name>". Hidden on flow stop /
 *      input disconnect (kept for reuse). Geometry persisted in save()/load().
 *   3. **Detached splitter layout**: left pane = VideoDisplayWidget (stretch=1),
 *      right pane = m_controlsWidget (collapsible, min width 220px). The
 *      splitter handle is visibly draggable.
 *
 * Unified display backends (auto-detect once at construction):
 *   - VideoGLBlitWidget (GPU, default when hardware GL is available) —
 *     QOpenGLWidget, zero-copy presentation of GPU-resident textures
 *     (GpuRgba → presentTexture, GpuYuv → presentYuvTexture, CPU →
 *     presentFrame), letterboxing, shaders.
 *   - VideoSoftwareWidget (CPU fallback) — QWidget + paintEvent, converts the
 *     frame to QImage (VideoFrameData::frameToImageCpu() / asImage()) and
 *     renders with keep-aspect-ratio.
 *   - Env override: DAQSTER_VIDEO_BACKEND=gl|software; unset → auto-detect via
 *     VideoGLContextManager::hasHardwareGL() (cached for the process).
 *
 * Frame flow:
 *   - Node receives VideoFrame on port 0.
 *   - Forwards to detached VideoDisplayWidget (live video, GL/SW).
 *   - Stores last frame; preview timer converts → QImage → m_preview pixmap.
 *   - Optionally passes through on output port (while downstream consumer is
 *     connected, tracked via outputConnectionCreated/Deleted).
 *
 * Perf domain: auto-enabled when controls visible + manual Perf toggle override.
 * Perf stats read from the detached display's perf domain ("video").
 * Perf toggle state is persisted in save()/load().
 *
 * Effects (REQ-SW-PL-034): optional embedded effect combo + parameter stack,
 * applied before presentation. Zero-copy passthrough when no effect selected.
 */
class VideoOutputNode : public QtNodes::NodeDelegateModel, public Daqster::IStoppable
{
    Q_OBJECT

public:
    VideoOutputNode();
    ~VideoOutputNode() override;

    QString caption() const override
    { return QStringLiteral("Video Output"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("VideoOutput"); }

    /// Video nodes do not change their geometry on data arrival — the display
    /// is updated directly in setInData(). Opts out of the full scene geometry
    /// recompute cascade (repaint-only fast path on data arrival).
    bool dataArrivalChangesGeometry() const override { return false; }

    /// The node BODY (boundary, caption, ports) does not depend on data —
    /// widget content self-repaints via Qt. Opts out of the body repaint.
    bool dataArrivalChangesWidget() const override { return false; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    QWidget *embeddedWidget() override;

    /// Show/hide event filter on m_preview (REQ-SW-PL-053 perf fix): starts
    /// the preview timer when the node widget becomes visible and stops it
    /// when hidden — belt-and-suspenders on top of the isVisible() gate in
    /// updatePreview(). In standalone --run mode the canvas is hidden, so the
    /// timer never runs → zero preview conversion cost.
    bool eventFilter(QObject *watched, QEvent *event) override;

    /// The detached display widget (VideoGLBlitWidget / VideoSoftwareWidget).
    /// Null until the first frame arrives — the detached window is created
    /// lazily (Option B). Test/diagnostic accessor.
    VideoDisplayWidget *displayWidget() const { return m_display; }

    /// The controls widget (Perf toggle + PerfStatsPanel + Effects) housed in
    /// the detached window's splitter (right pane). Created in the constructor,
    /// reparented to m_displayWindow in ensureDisplayWindow().
    QWidget *controlsWidget() const { return m_controlsWidget; }

    /// Stop background work (timers). Idempotent — safe to call multiple times
    /// (REQ-SW-PL-050). Hides the detached display window and stops both the
    /// preview timer and perf refresh timer.
    void stop() override;

    /// Enable/disable the Perf toggle (REQ-SW-PL-053). The Perf checkbox lives
    /// in the DETACHED window's controls pane, so the headless autostart driver
    /// (NodeEditorIdeObject::startVideoPlayback) cannot reach it via
    /// embeddedWidget() — this slot is invoked through the meta-object system
    /// (QMetaObject::invokeMethod) to avoid a cross-plugin link dependency.
    Q_INVOKABLE void setPerfEnabled(bool enabled);

    /// Emit the [PERF] video console line (REQ-SW-PL-027, both Qt5 + Qt6).
    /// Driven by m_consoleTimer (5 s) while the Perf toggle is checked. The
    /// measurement harness greps for this line to validate playback started.
    void logPerfLine();

    /// Track downstream connections on the output port so the per-frame
    /// QImage conversion only happens while a processing consumer is connected.
    void outputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

    /// Track the port-0 "video-frame" input connection. Disconnecting the edge
    /// does NOT stop the source player — frames keep arriving in setInData() —
    /// so the connection flag (not widget nullness) is the guard that prevents
    /// the display from being resurrected after disconnect.
    void inputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void inputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

private:
    /// Re-run the port-0 processing on the last received frame (mirrors
    /// VideoEffectNode::reprocessCurrentFrame). Called after load() so a
    /// restored effect/parameter set is applied to the current frame.
    void reprocessCurrentFrame();

    /// Create the detached display window (if not yet created) and show it.
    /// Called on first frame arrival (Option B). Applies the persisted
    /// geometry (m_displayWindowGeometry) before showing.
    void ensureDisplayWindow();

    /// Update the in-node QLabel preview from the latest frame snapshot.
    /// Scale-before-convert (REQ-SW-PL-053): converts the frame DIRECTLY to a
    /// ~200px-wide QImage via VideoFrameData::frameToImageScaled() (NV12 /
    /// YUV420P subsampled during YUV→RGB — the full image is never built),
    /// throttled by m_previewTimer (~0.5 fps). Skips entirely when the label
    /// is not visible (standalone --run mode = zero cost).
    void updatePreview();

    /// Select the embedded effect by combo index (REQ-SW-PL-034). Index 0 is
    /// the "No effect" placeholder (m_effectEnabled = false); indices 1..N map
    /// to m_specs[0..N-1]. Syncs the parameter stack and the enabled flag.
    void setEffectIndex(int index);

    /// Build the embedded effect combo + parameter stack (REQ-SW-PL-034).
    /// Adds a leading "No effect" item (index 0) followed by one item per
    /// effect from VideoEffectOps::allSpecs(), with a QStackedWidget holding
    /// a blank page for index 0 and one parameter page per effect.
    void buildEffectControls();

    /// Compact parameter page: a title label + a horizontal slider bound to
    /// the given int member via onChanged. Returns the page widget.
    QWidget *createSliderPage(int &value, int min, int max, const QString &title,
                              std::function<void(int)> onChanged);

    /// Flip direction page (horizontal/vertical combo bound to
    /// m_params.flipHorizontal).
    QWidget *createFlipPage();

    /// Canny thresholds page (low/high sliders bound to m_params).
    QWidget *createCannyPage();

    /// Simple info page for parameter-less effects (grayscale/invert/sepia/
    /// channelSwap).
    QWidget *createInfoPage(const QString &text);

    QWidget *m_widget = nullptr;
    std::shared_ptr<VideoFrameData> m_output;

    // ── In-node preview (Option B) ──────────────────────────────────────────
    /// QLabel showing a scaled QImage snapshot of the latest video frame.
    /// Updated at ~1–2 fps by m_previewTimer. No GL — avoids scene repaint
    /// problem. Shows "No video" placeholder text when no frame has arrived.
    QLabel *m_preview = nullptr;

    /// Timer driving the in-node QLabel preview update. Fires every 2000 ms
    /// (~0.5 fps), grabs the latest frame from m_lastFrameForPreview, converts
    /// it to a SMALL QImage (scale-before-convert), and sets m_preview pixmap.
    /// Started on first frame arrival AND on widget Show (event filter);
    /// stopped on flow stop / input disconnect / widget Hide (REQ-SW-PL-053
    /// perf fix — the old 750 ms full-1080p conversion regressed 6/8 perf
    /// scenarios by >2pp).
    QTimer *m_previewTimer = nullptr;

    /// Last received video frame, kept for the preview timer to convert at
    /// ~0.5 fps (scale-before-convert to ~200px). Avoids per-frame QImage
    /// conversion (the display widget gets the frame directly for zero-copy
    /// presentation).
    std::shared_ptr<VideoFrameData> m_lastFrameForPreview;

    // ── Detached display window (Option B) ──────────────────────────────────
    /// Detached window housing the VideoDisplayWidget (GL blit / SW).
    /// Created LAZILY on first frame arrival (ensureDisplayWindow()), hidden
    /// on flow stop / input disconnect (kept for reuse). Position/size
    /// persisted in save()/load().
    QWidget *m_displayWindow = nullptr;

    /// Controls widget (Perf toggle + PerfStatsPanel + Effects) — created in
    /// the constructor, reparented to m_displayWindow in ensureDisplayWindow().
    /// Lives in the right pane of the detached window's QSplitter.
    QWidget *m_controlsWidget = nullptr;

    /// Unified display widget (REQ-SW-PL-053): VideoGLBlitWidget (GPU) or
    /// VideoSoftwareWidget (CPU), selected once at construction. Parented to
    /// m_displayWindow (not m_widget) — lives in the detached window.
    VideoDisplayWidget *m_display = nullptr;

    /// Geometry loaded from save()/load() — applied when the window is first
    /// created (the window is created lazily on first frame arrival).
    QByteArray m_displayWindowGeometry;

    /// Splitter state loaded from save()/load() — applied when the detached
    /// window's splitter is first created (deferred, since the splitter lives
    /// in the lazily-created detached window, not the node widget).
    QByteArray m_splitterState;

    /// True while the detached window was shown by us (first frame of a flow).
    /// Cleared on flow stop / input disconnect so the window reopens on the
    /// next flow's first frame. Stays true if the user closes the window
    /// mid-flow — the window is NOT resurrected against the user's intent.
    bool m_displayWindowShown = false;

    /// True while reprocessCurrentFrame() is re-presenting the last frame
    /// (effect change / load). Suppresses the window-show side effect so a
    /// reprocess on a stopped flow cannot resurrect the detached window.
    bool m_suppressWindowShow = false;

    // Perf console line (REQ-SW-PL-027, both Qt5 + Qt6): the "Perf" toggle in
    /// the controls widget enables the "video" profiling domain live and drives
    /// the 5 s console timer; m_cpu samples self-CPU; the markers tag the last
    /// presented frame (Qt5 via the normalized VideoCompat::pixelFormatInt()).
    QTimer *m_perfRefreshTimer = nullptr;
    /// 5 s timer emitting the [PERF] video console line while Perf is enabled
    /// (REQ-SW-PL-027). Restored in REQ-SW-PL-053 after the detached-window
    /// refactor removed it — the measurement harness depends on this line.
    QTimer *m_consoleTimer = nullptr;
    Daqster::Perf::ProcessCpu m_cpu;
    int m_lastHandleType = 0;      // QVideoFrame::HandleType (NoHandle = 0)
    int m_lastPixelFormat = -1;    // normalized (Qt6 numbering, see VideoCompat)

    std::shared_ptr<VideoFrameData> m_lastInput;
    int m_outputConnectionCount = 0;
    /// True while a port-0 "video-frame" edge exists. Guards setInData() so
    /// frames that keep flowing from a still-playing source after the edge is
    /// removed cannot resurrect the display.
    bool m_videoInputConnected = false;

    // ── Embedded effects (REQ-SW-PL-034, optional, default none) ────────────
    /// All registered effects (from VideoEffectOps::allSpecs()). Index 0 of
    /// the combo is the "No effect" placeholder, so m_specs[i] corresponds to
    /// combo index i+1.
    QVector<EffectSpec> m_specs;
    /// Selected effect index into m_specs; -1 = no effect.
    int m_effectIndex = -1;
    /// True when an effect is selected and applied. When false the effect
    /// block in setInData() is skipped entirely — zero-copy passthrough is
    /// byte-identical to a node without embedded effects.
    bool m_effectEnabled = false;
    /// Effect parameters (brightness/contrast/flip/blur/OpenCV thresholds).
    EffectParams m_params;
    /// GPU backend for GpuOrCpu effects (shared GL context, zero-copy).
    VideoEffectGLProcessor m_glProcessor;
    /// Embedded effect combo (index 0 = "No effect").
    QComboBox *m_effectCombo = nullptr;
    /// Parameter stack: page 0 = blank (no effect), page i+1 = effect i.
    QStackedWidget *m_effectStack = nullptr;

    /// Horizontal splitter in the DETACHED display window: left = video display
    /// (stretch=1), right = controls (collapsible, min width 220px). Created in
    /// ensureDisplayWindow(), not in the constructor — the splitter lives in
    /// the detached window, not in the node widget.
    QSplitter *m_splitter = nullptr;

    // Perf stats labels (updated by m_perfRefreshTimer)
    QLabel *m_fpsLabel = nullptr;
    QLabel *m_gapLabel = nullptr;
    QLabel *m_presentLabel = nullptr;
    QLabel *m_totalLabel = nullptr;
    QLabel *m_cpuLabel = nullptr;
    QLabel *m_hwSwLabel = nullptr;
    QLabel *m_formatLabel = nullptr;
    QLabel *m_handleLabel = nullptr;

    /// Perf toggle checkbox in the controls panel header. When checked: enables
    /// the "video" perf domain + starts refresh timer. When unchecked: disables
    /// domain + stops timer. Overrides the auto behavior (auto = enabled when
    /// panel visible).
    QCheckBox *m_perfToggle = nullptr;
};

#endif // VIDEOOUTPUTNODE_H