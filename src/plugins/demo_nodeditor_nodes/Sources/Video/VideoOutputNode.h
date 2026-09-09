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
class VideoDisplayWidget;
class VideoFrameData;

/**
 * @brief Video output node: displays incoming video frames.
 *
 * The node has a single input port on BOTH Qt versions (REQ-SW-PL-020,
 * NV12-direct, single video-frame type REQ-SW-PL-032):
 *   - port 0 "video-frame" — zero-copy VideoFrameData; presented on the
 *     unified VideoDisplayWidget (REQ-SW-PL-053).
 *
 * Unified display (REQ-SW-PL-053): ONE display concept with two backends,
 * selected ONCE at construction (auto-detect):
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
 * The display widget is a CHILD of m_widget (layout-friendly) — it works
 * embedded (node scene / runtime workspace) and detached (floating window via
 * the nodeeditor deembed mechanism on embeddedWidget()). The old in-node
 * display (Qt5 QLabel, Qt6 QGraphicsVideoItem) and the Qt6 native QVideoWidget
 * (QTBUG-35299 — cannot be embedded) are REMOVED. The "GPU display" checkbox
 * is removed — its roles are taken over by deembed + auto-detect.
 *
 * The node also passes the frame through on its output port so output chains
 * can be built (e.g. output of a modifier). The output emits VideoFrameData
 * (single video-frame type REQ-SW-PL-032); the per-frame QImage conversion +
 * output only runs while a downstream consumer is connected to the output port
 * (tracked via outputConnectionCreated/Deleted).
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

    /// Stop background work (timers). Idempotent — safe to call multiple times
    /// (REQ-SW-PL-050). The display widget is a child of m_widget — no explicit
    /// delete needed (REQ-SW-PL-053).
    void stop() override;

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

    /// Unified display widget (REQ-SW-PL-053): VideoGLBlitWidget (GPU) or
    /// VideoSoftwareWidget (CPU), selected once at construction. Child of
    /// m_widget — layout-friendly, works embedded and detached.
    VideoDisplayWidget *m_display = nullptr;

    // Perf console line (REQ-SW-PL-027, both Qt5 + Qt6): the "Perf" toggle in
    /// the controls widget enables the "video" profiling domain live and drives
    /// the 5 s console timer; m_cpu samples self-CPU; the markers tag the last
    /// presented frame (Qt5 via the normalized VideoCompat::pixelFormatInt()).
    QTimer *m_perfRefreshTimer = nullptr;
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

    /// Horizontal splitter: left = video display (stretch=1), right = controls
    /// (collapsible, min width 220px).
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