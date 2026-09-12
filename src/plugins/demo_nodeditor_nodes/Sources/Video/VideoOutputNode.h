#ifndef VIDEOOUTPUTNODE_H
#define VIDEOOUTPUTNODE_H

#include <QtNodes/NodeDelegateModel>

#include "ProcessCpu.h"
#include "VideoEffectGLProcessor.h"
#include "VideoEffectOps.h"
#include "shared/IStoppable.h"

#include <QImage>
#include <QStackedWidget>
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
 * Embedded three-mode display (REQ-SW-PL-053):
 *
 * The node has a single input port on BOTH Qt versions (REQ-SW-PL-020,
 * NV12-direct, single video-frame type REQ-SW-PL-032):
 *   - port 0 "video-frame" — zero-copy VideoFrameData.
 *
 * Display architecture (three-mode, one widget — embeddedWidget()):
 *   The node widget (m_widget) contains a QStackedWidget (m_videoStack) with
 *   two pages:
 *   1. **Preview page** (m_previewPage, index 0): the static QLabel (m_preview)
 *      showing a single ~200px QImage snapshot. Active when the node is
 *      embedded in the scene proxy (editor mode).
 *   2. **Live page** (m_livePage, index 1): a QSplitter with the unified
 *      VideoDisplayWidget (GL blit default / SW fallback) on the left and
 *      controls (Perf toggle + PerfStatsPanel + Effects) on the right. Active
 *      when the node is deembedded (floating window in the editor) or in run
 *      mode (MDI layout).
 *
 * Mode detection (single source of truth): m_widget->graphicsProxyWidget() !=
 * nullptr → embedded → preview page; nullptr → deembedded/run → live page.
 * QOpenGLWidget cannot be embedded in a QGraphicsProxyWidget (documented Qt
 * limitation — WA_PaintOnScreen widgets), so the GL display lives on the live
 * page only; run mode deembeds embeddedWidget() into QMdiArea as plain
 * QWidgets (no proxy), so GL works there.
 *
 * Preview: fires ONCE per Play (~1 s after first frame) via a single-shot
 * timer — zero ongoing CPU cost. Scale-before-convert: the full frame is never
 * materialized. No GL, no scene repaint problem. Gated on proxy embedding: the
 * timer only arms while the preview page is active (event filter) and
 * updatePreview() skips when not visible — standalone --run mode costs zero.
 * On new Play the preview resets (clear / "No video" placeholder) and a fresh
 * frame is converted once.
 *
 * Presentation is gated on the live page being active (isLiveDisplayActive()):
 * in editor-embedded mode no frames are presented (zero GPU cost — the preview
 * page shows the static snapshot); in run/deembedded mode frames present
 * normally. This also avoids presenting to a GL widget whose context never
 * initialized.
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
 *   - Presents to the live page's VideoDisplayWidget while the live page is
 *     active (deembedded / run mode).
 *   - Stores last frame; preview timer converts → QImage → m_preview pixmap
 *     while embedded in the scene proxy.
 *   - Optionally passes through on output port (while downstream consumer is
 *     connected, tracked via outputConnectionCreated/Deleted).
 *
 * Perf domain: auto-enabled when controls visible + manual Perf toggle override.
 * Perf stats read from the live display's perf domain ("video").
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

    /// Show/hide event filter on m_widget (REQ-SW-PL-053): re-evaluates the
    /// display mode (updateDisplayMode()) when the node widget becomes visible
    /// (embedded in scene proxy, or shown as a floating window / MDI
    /// sub-window) and stops the preview timer when hidden — no conversion
    /// while not visible. In standalone --run mode the canvas is hidden, so
    /// the timer never arms → zero preview conversion cost.
    bool eventFilter(QObject *watched, QEvent *event) override;

    /// The live display widget (VideoGLBlitWidget / VideoSoftwareWidget).
    /// Non-null after construction — created in the constructor, parented to
    /// the live page. Test/diagnostic accessor.
    VideoDisplayWidget *displayWidget() const { return m_display; }

    /// True when the live video page is active (deembedded/run mode).
    /// Test/diagnostic accessor.
    bool isLiveDisplayActive() const
    { return m_videoStack != nullptr && m_videoStack->currentWidget() == m_livePage; }

    /// The controls widget (Perf toggle + PerfStatsPanel + Effects) housed in
    /// the live page's splitter (right pane). Created in the constructor.
    QWidget *controlsWidget() const { return m_controlsWidget; }

    /// Stop background work (timers). Idempotent — safe to call multiple times
    /// (REQ-SW-PL-050). Stops the preview and perf refresh timers, clears the
    /// live display, and resets the in-node preview to "No video".
    void stop() override;

    /// Enable/disable the Perf toggle (REQ-SW-PL-053). The Perf checkbox lives
    /// in the live page of embeddedWidget() — reachable via findChildren in
    /// run mode; this slot is invoked through the meta-object system
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

    /// Switch the QStackedWidget page based on proxy embedding state. Embedded
    /// (graphicsProxyWidget() != nullptr) → preview page; deembedded/run →
    /// live page. Arms/stops the preview timer accordingly.
    void updateDisplayMode();

    /// Update the in-node QLabel preview from the latest frame snapshot.
    /// Single-shot callback: converts ONCE per Play (scale-before-convert to
    /// ~200px via VideoFrameData::frameToImageScaled()), sets the pixmap, and
    /// returns — zero ongoing cost. The timer is NOT re-armed after this call.
    /// Skips entirely when the label is not visible (standalone --run mode =
    /// zero cost).
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

    // ── In-node preview (page 0 of m_videoStack) ────────────────────────────
    /// QLabel showing a single static QImage snapshot of the latest video frame.
    /// Updated once per Play by the single-shot m_previewTimer (~1 s after
    /// first frame). No GL — avoids scene repaint problem. Shows "No video"
    /// placeholder text when no frame has arrived. Lives on m_previewPage
    /// (page 0 of m_videoStack), active only while embedded in the scene proxy.
    QLabel *m_preview = nullptr;

    /// Single-shot timer for the in-node preview (REQ-SW-PL-053). Armed on
    /// first frame arrival (setInData) or widget Show (event filter). Fires
    /// ONCE, converts the latest frame to a SMALL QImage (scale-before-convert),
    /// sets m_preview pixmap, and stops — zero ongoing CPU cost. Stopped on flow
    /// stop / input disconnect / widget Hide.
    QTimer *m_previewTimer = nullptr;

    /// Latch: true once the preview has fired for the current Play. Prevents
    /// setInData() / the Show event filter from re-arming the single-shot timer
    /// on every subsequent frame — the preview stays STATIC (one frame per
    /// Play) instead of updating at ~1 fps. Reset on flow stop (stop()),
    /// input disconnect, and invalid-frame reset so the next Play fires again.
    bool m_previewFired = false;

    /// Last received video frame, kept for the single-shot preview timer to
    /// convert once per Play (scale-before-convert to ~200px). Avoids per-frame
    /// QImage conversion (the display widget gets the frame directly for
    /// zero-copy presentation).
    std::shared_ptr<VideoFrameData> m_lastFrameForPreview;

    // ── Embedded three-mode display (REQ-SW-PL-053) ─────────────────────────
    /// Page container inside m_widget: page 0 = preview (m_previewPage),
    /// page 1 = live (m_livePage). The active page is driven by
    /// updateDisplayMode() based on proxy embedding state.
    QStackedWidget *m_videoStack = nullptr;

    /// Page 0 of m_videoStack: holds the static preview QLabel (m_preview).
    /// Active when the node is embedded in the scene proxy (editor mode).
    QWidget *m_previewPage = nullptr;

    /// Page 1 of m_videoStack: holds the live splitter (video display left +
    /// controls right). Active when the node is deembedded (floating window in
    /// the editor) or in run mode (MDI layout).
    QWidget *m_livePage = nullptr;

    /// Controls widget (Perf toggle + PerfStatsPanel + Effects) — created in
    /// the constructor, parented to m_livePage (right pane of the splitter).
    QWidget *m_controlsWidget = nullptr;

    /// Unified display widget (REQ-SW-PL-053): VideoGLBlitWidget (GPU) or
    /// VideoSoftwareWidget (CPU), selected once at construction. Parented to
    /// m_livePage (not m_widget directly) — lives in the live page.
    VideoDisplayWidget *m_display = nullptr;

    // Perf console line (REQ-SW-PL-027, both Qt5 + Qt6): the "Perf" toggle in
    /// the controls widget enables the "video" profiling domain live and drives
    /// the 5 s console timer; m_cpu samples self-CPU; the markers tag the last
    /// presented frame (Qt5 via the normalized VideoCompat::pixelFormatInt()).
    QTimer *m_perfRefreshTimer = nullptr;
    /// 5 s timer emitting the [PERF] video console line while Perf is enabled
    /// (REQ-SW-PL-027). Restored in REQ-SW-PL-053 after the detached-window
    /// refactor removed it — the measurement harness depends on this line.
    QTimer *m_consoleTimer = nullptr;
    /// Self-CPU sampler for the 500 ms UI refresh timer (CPU label in the
    /// controls panel). sample() is destructive (resets the baseline each
    /// call), so it MUST NOT be shared with the 5 s console timer — otherwise
    /// the console's cpu= value would be a delta over ~0-500 ms (idle windows
    /// → 0 values) instead of a true 5 s average.
    Daqster::Perf::ProcessCpu m_cpu;
    /// Self-CPU sampler for the 5 s console timer (logPerfLine). Its OWN
    /// instance keeps the [PERF] video cpu= value a true 5 s delta, immune to
    /// the 500 ms UI refresh timer resetting the shared baseline ~10x between
    /// console prints.
    Daqster::Perf::ProcessCpu m_consoleCpu;
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

    /// Horizontal splitter in the LIVE page: left = video display (stretch=1),
    /// right = controls (collapsible, min width 220px). Created in the
    /// constructor.
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