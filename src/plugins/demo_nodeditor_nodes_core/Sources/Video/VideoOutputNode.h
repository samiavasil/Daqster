#ifndef VIDEOOUTPUTNODE_H
#define VIDEOOUTPUTNODE_H

#include <QtNodes/NodeDelegateModel>

#include "IVideoOutputUi.h"
#include "ProcessCpu.h"
#include "VideoEffectGLProcessor.h"
#include "VideoEffectOps.h"
#include "shared/IStoppable.h"

#include <QImage>
#include <QtMultimedia/QVideoFrame>
#include <QTimer>
#include <memory>

class QWidget;
class VideoFrameData;

/**
 * @brief Video output node: displays incoming video frames.
 *
 * Single input port on BOTH Qt versions (REQ-SW-PL-020, NV12-direct, single
 * video-frame type REQ-SW-PL-032):
 *   - port 0 "video-frame" — zero-copy VideoFrameData.
 *
 * This model holds NO widget and constructs none. Every QWidget — the unified
 * display backend, the splitter, the preview label, the perf panel, the effect
 * parameter pages — belongs to demo_nodeditor_nodes_gui and arrives through
 * IWidgetProvider as an IVideoOutputUi (REQ-SW-PL-055, REQ-SW-PL-051).
 * embeddedWidget() therefore returns nullptr; that is the documented contract
 * in IWidgetProvider.h — the editor asks providers BEFORE falling back to
 * embeddedWidget(), so the controls still appear.
 *
 * Display architecture (ported from consolidate, which had no core/gui split and
 * could build all of it inside the model):
 *   The node widget holds a two-page QStackedWidget:
 *     - preview page — one static snapshot, active while embedded in the editor
 *       scene proxy. A QOpenGLWidget cannot be hosted in a QGraphicsProxyWidget
 *       (Qt paints WA_PaintOnScreen widgets on their own layer), which is the
 *       whole reason the GL display is confined to the live page.
 *     - live page — the display (GL blit or software, chosen once) beside the
 *       controls. Active when deembedded into a floating window or in run mode.
 *
 * Unified display backends (REQ-SW-PL-055, auto-detected once at construction):
 *   - VideoGLBlitWidget (GPU) — QOpenGLWidget, zero-copy presentation of
 *     GPU-resident textures (GpuRgba → presentTexture, GpuYuv →
 *     presentYuvTexture, CPU → presentFrame), letterboxing, shaders.
 *   - VideoSoftwareWidget (CPU fallback) — converts to QImage and paints with
 *     keep-aspect-ratio.
 *   - DAQSTER_VIDEO_BACKEND=gl|software overrides; unset → hasHardwareGL().
 *
 * Preview: fires ONCE per Play (~1 s after the first frame) via a single-shot
 * timer, converting scale-before-convert to ~200 px so the full frame is never
 * materialized. Zero ongoing cost. The timer only arms while the preview page is
 * active, and updatePreview() also skips when the label is not visible — so
 * standalone --run mode converts nothing at all.
 *
 * Presentation is gated on the live page being active: in editor-embedded mode
 * no frames are presented (the static preview is shown instead), which is zero
 * GPU cost and avoids presenting into a GL widget whose context never
 * initialized.
 *
 * The node also passes the frame through on its output port so output chains can
 * be built. That per-frame path only runs while a downstream consumer is
 * connected (tracked via outputConnectionCreated/Deleted).
 */
class VideoOutputNode : public QtNodes::NodeDelegateModel,
                        public Daqster::IStoppable,
                        public IVideoOutputUiHandler
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
    /// updates itself. Opts out of the full scene geometry recompute cascade.
    bool dataArrivalChangesGeometry() const override { return false; }

    /// The node BODY does not depend on data. Opts out of the body repaint.
    bool dataArrivalChangesWidget() const override { return false; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    /// Always nullptr: the UI is supplied by the GUI plugin through
    /// IWidgetProvider. See IWidgetProvider.h — providers are consulted first.
    QWidget *embeddedWidget() override { return nullptr; }

    /// Stop background work (timers) and clear the display. Idempotent —
    /// safe to call multiple times (REQ-SW-PL-050).
    void stop() override;

    /// Attach the GUI-side UI. Called by the GUI plugin factory right after it
    /// constructs the IVideoOutputUi, and only there. Null until then, which is
    /// why every use below is guarded — a model can exist (tests, headless flow
    /// parsing) without any UI at all.
    void setUi(IVideoOutputUi *ui) { m_ui = ui; }
    IVideoOutputUi *ui() const { return m_ui; }

    /// Handed to the GUI plugin so it can build the parameter widgets bound to
    /// this model's live EffectParams (REQ-SW-PL-034).
    const QVector<EffectSpec> &effectSpecs() const { return m_specs; }
    EffectParams *effectParams() { return &m_params; }
    IVideoOutputUiHandler *uiHandler() { return this; }

    /// The display backend, or nullptr when no UI is attached.
    /// Test/diagnostic accessor.
    VideoDisplayWidget *displayWidget() const;

    /// True when the live page is active. Test/diagnostic accessor.
    bool isLiveDisplayActive() const;

    /// The controls widget, or nullptr. Test/diagnostic accessor.
    QWidget *controlsWidget() const;

    /// Enable/disable the Perf toggle (REQ-SW-PL-055). Invoked by the editor
    /// through QMetaObject::invokeMethod — that caller is a third plugin which
    /// must not link _core or _gui, so the meta-object is the transport.
    Q_INVOKABLE void setPerfEnabled(bool enabled);

    /// Force the LIVE page. Also invoked through QMetaObject::invokeMethod by the
    /// runtime layout builder when it deembeds a node widget: display mode must
    /// follow PLACEMENT CONTEXT, not Show events, or a missed Show leaves the
    /// widget on the dark "No video" preview page.
    Q_INVOKABLE void forceLiveDisplay();

    /// Emit the [PERF] video console line (REQ-SW-PL-027). Driven by a 5 s timer
    /// while Perf is on; the measurement harness greps for this line.
    void logPerfLine();

    /// Track downstream connections so the per-frame passthrough only runs while
    /// a consumer is connected.
    void outputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

    /// Track the port-0 "video-frame" edge. Disconnecting does NOT stop the
    /// source player — frames keep arriving — so this flag, not widget nullness,
    /// is the guard that stops the display being resurrected.
    void inputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void inputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

    // ── IVideoOutputUiHandler (called by the GUI plugin) ───────────
    void onUiVisibilityChanged(bool inProxy) override;
    void onEffectSelectionChanged(int comboIndex) override;
    void onEffectParamsChanged() override;
    void onPerfToggled(bool enabled) override;

private:
    /// Convert the latest frame ONCE to a small preview image and hand it to the
    /// UI. Scale-before-convert, so a 1080p frame never becomes a full QImage.
    void updatePreview();

    /// Preview page active → live page active, and arm/stop the preview timer.
    void driveDisplayMode();

    /// Re-apply the current effect/parameters to the last received frame.
    /// Called after load() and after any effect control change.
    void reprocessCurrentFrame();

    /// Select the embedded effect by combo index (REQ-SW-PL-034). Index 0 is the
    /// "No effect" placeholder (m_effectEnabled = false); 1..N map to
    /// m_specs[0..N-1].
    void setEffectIndex(int index);

    /// Sample the "video" perf domain and push one snapshot to the UI.
    void samplePerfStats();

    void resetVideoState();

    std::shared_ptr<VideoFrameData> m_output;

    // ── In-node preview ─────────────────────────────────────────────────────
    /// Single-shot timer for the static preview. Armed when a frame arrives
    /// while the preview page is active, or on widget Show. Fires ONCE.
    QTimer *m_previewTimer = nullptr;
    /// Latch: true once the preview fired for the current Play, so the timer is
    /// not re-armed on every subsequent frame. Reset on stop/disconnect.
    bool m_previewFired = false;
    /// Last received frame, kept for that single conversion.
    std::shared_ptr<VideoFrameData> m_lastFrameForPreview;

    /// GUI-side UI. Null until the GUI plugin attaches one.
    IVideoOutputUi *m_ui = nullptr;

    // Perf (REQ-SW-PL-027). The domain, the samplers and the timers stay in
    // core — they are QtCore/PERF facilities; only the rendered text is a widget.
    /// 500 ms refresh timer feeding the UI perf panel.
    QTimer *m_perfRefreshTimer = nullptr;
    /// 5 s timer emitting the [PERF] console line.
    QTimer *m_consoleTimer = nullptr;
    /// Self-CPU for the 500 ms UI refresh. sample() is destructive (resets the
    /// baseline), so it MUST NOT be shared with the console timer.
    Daqster::Perf::ProcessCpu m_cpu;
    /// Own instance for the 5 s console line, so its cpu= stays a true 5 s delta
    /// instead of a delta over whichever 0-500 ms window the UI timer last reset.
    Daqster::Perf::ProcessCpu m_consoleCpu;
    int m_lastHandleType = 0;      // QVideoFrame::HandleType (NoHandle = 0)
    int m_lastPixelFormat = -1;    // normalized (Qt6 numbering, see VideoCompat)

    std::shared_ptr<VideoFrameData> m_lastInput;
    int m_outputConnectionCount = 0;
    /// True while a port-0 "video-frame" edge exists.
    bool m_videoInputConnected = false;

    // ── Embedded effects (REQ-SW-PL-034, optional, default none) ────────────
    QVector<EffectSpec> m_specs;
    /// Selected effect index into m_specs; -1 = no effect.
    int m_effectIndex = -1;
    /// True when an effect is selected and applied. When false the effect block
    /// in setInData() is skipped entirely — the passthrough stays byte-identical
    /// to a node without embedded effects.
    bool m_effectEnabled = false;
    /// Effect parameters. Bound directly by the GUI plugin's sliders, so there is
    /// no copy and no setter round-trip.
    EffectParams m_params;
    /// GPU backend for GpuOrCpu effects (shared GL context, zero-copy).
    VideoEffectGLProcessor m_glProcessor;
};

#endif // VIDEOOUTPUTNODE_H
