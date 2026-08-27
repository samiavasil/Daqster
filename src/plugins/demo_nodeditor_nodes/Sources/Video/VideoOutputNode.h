#ifndef VIDEOOUTPUTNODE_H
#define VIDEOOUTPUTNODE_H

#include <QtNodes/NodeDelegateModel>

#include "ProcessCpu.h"
#include "VideoEffectGLProcessor.h"
#include "VideoEffectOps.h"

#include <QImage>
#include <QtMultimedia/QVideoFrame>
#include <QVBoxLayout>
#include <functional>
#include <memory>

class QLabel;
class QWidget;

class QCheckBox;
class QComboBox;
class QStackedWidget;
class QTimer;
class VideoFrameData;
class VideoGLBlitWidget;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QGraphicsVideoItem;
class QVideoWidget;
#endif

/**
 * @brief Video output node: displays incoming video frames.
 *
 * The node has a single input port on BOTH Qt versions (REQ-SW-PL-020,
 * NV12-direct, single video-frame type REQ-SW-PL-032):
 *   - port 0 "video-frame" — zero-copy VideoFrameData; presented on the
 *     detached GL blit window (default on Qt5; Qt6 uses QVideoWidget unless
 *     DAQSTER_GL_BLIT=1). Qt5 frames are owned copies (frameToOwnedFrame),
 *     Qt6 frames are the decoded probe frames.
 *
 * GL blit display selection (REQ-SW-PL-021):
 *   - Default per Qt version: Qt5 = GL blit ON (fastest measured display path,
 *     ~15% CPU vs ~34% software), Qt6 = native QVideoWidget (GL blit OFF).
 *   - Env override, applied at STARTUP only: `DAQSTER_GL_BLIT=0` forces the
 *     software path (also Qt5), `DAQSTER_GL_BLIT=1` forces the GL path (also
 *     Qt6). Unset → per-Qt default above. The VALUE matters: "0" disables
 *     (the old presence check treated even "=0" as enabled).
 *   - UI: the "GPU display" checkbox (visible + checked by default on both Qt
 *     versions) has the final word at runtime — toggling switches between a
 *     detached display window and in-scene rendering at the next frame without
 *     crashing or losing video. Qt5: ON = detached GL blit, OFF = software
 *     QLabel. Qt6: ON = detached (QVideoWidget / GL blit per env), OFF =
 *     in-scene QGraphicsVideoItem (REQ-SW-PL-021).
 *   - Auto-fallback: if the GL context cannot be created (VM / remote /
 *     software rendering without GL) the node logs `GL fallback: <reason>`
 *     and switches to the software path — video keeps displaying.
 *
 * The embedded QLabel shows a static placeholder ("GPU display active — see
 * detached window") while the video-frame GPU path is active, so the node
 * remains usable inside the node editor scene without per-frame QImage
 * conversion. The detached display is a separate top-level window
 * (QTBUG-35299 prevents hosting a native video surface in the scene).
 *
 * The node also passes the frame through on its output port so output chains
 * can be built (e.g. output of a modifier). The output emits VideoFrameData
 * (single video-frame type REQ-SW-PL-032); the per-frame QImage conversion +
 * output only runs while a downstream consumer is connected to the output port
 * (tracked via outputConnectionCreated/Deleted).
 */
class VideoOutputNode : public QtNodes::NodeDelegateModel
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

    /// Track downstream connections on the output port so the per-frame
    /// QImage conversion only happens while a processing consumer is connected.
    void outputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

    /// Track the port-0 "video-frame" input connection. Disconnecting the edge
    /// does NOT stop the source player — frames keep arriving in setInData() —
    /// so the connection flag (not widget nullness) is the guard that prevents
    /// the detached display popup from being resurrected after disconnect.
    void inputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void inputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void updateDisplay();

    /// Frame to present on a native sink (Qt6 QVideoWidget / in-scene item).
    /// GPU-resident RGBA frames (effect output) cannot be consumed by the
    /// native sinks — readback at the display boundary (Stage 2C will present
    /// the texture directly). CPU / GpuYuv frames pass through zero-copy.
    QVideoFrame presentableFrame(const std::shared_ptr<VideoFrameData> &frame) const;

    /// Log the single-line console perf report (5 s timer, both Qt5 + Qt6).
    void logPerfLine();

    /// Apply the "GPU display" checkbox: toggling OFF (in-scene / software)
    /// closes the detached windows immediately; toggling ON (detached) is
    /// applied at the next frame (the window is created lazily by
    /// ensureVideoWidget()).
    void setGlEnabled(bool enabled);

    /// Switch to the software display path because GL is not usable
    /// (no context / invalid context). Logs `GL fallback: <reason>`, destroys
    /// the GL window, unchecks the "GPU display" box, and remembers the
    /// failure for the rest of the session (m_glFailed).
    void fallbackToSoftware(const QString &reason);

    /// Lazily create the detached GL blit window used when GL display is
    /// enabled (both Qt versions; Qt6 presents QVideoFrames, Qt5 owned
    /// QVideoFrames). Re-shows a previously hidden window on re-connect.
    /// Falls back to the software path when GL is unavailable.
    void ensureGlWidget();

    /// Lazily create the detached display on the first video-frame input:
    /// GL blit window when GL display is enabled (both Qt versions),
    /// QVideoWidget fallback on Qt6 without GL blit, software label path on
    /// Qt5 without GL. No-op in Qt6 in-scene mode (m_detachedEnabled == false)
    /// — the in-scene QGraphicsVideoItem branch in setInData() handles display.
    void ensureVideoWidget();

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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    /// Lazily create the in-scene QGraphicsVideoItem (REQ-SW-PL-021, Qt6,
    /// "GPU display" checkbox OFF): finds the node editor scene via
    /// QApplication::topLevelWidgets() → GraphicsView → DataFlowGraphicsScene,
    /// parents the item to this node's NodeGraphicsObject and positions/sizes
    /// it over the embedded label area. Frames are presented through
    /// VideoCompat::presentFrame() on the item's video sink (GPU path — no
    /// QImage copy). No-op when the scene/item cannot be resolved (the
    /// software QLabel path then stays the fallback).
    void ensureSceneVideoItem();

    /// (Re)position/resize m_sceneVideoItem over the embedded label area.
    /// Called on creation and when the label is resized (eventFilter).
    void updateSceneVideoItemGeometry();

    /// Refresh the perf overlay badge from the "video" domain aggregates
    /// (fired on a ~500 ms timer while the Perf checkbox is enabled).
    void updatePerfBadge();

    /// Create the top-level perf badge overlay once (shared by the
    /// QVideoWidget and the GL blit window).
    void createPerfBadge();

    /// (Re)position the top-level perf badge over the top-left corner of the
    /// detached display window. Called on creation and on every badge refresh
    /// so the overlay tracks the window when it is moved or resized.
    void positionPerfBadge();
#endif

    QWidget *m_widget = nullptr;
    QLabel *m_label = nullptr;
    QImage m_image;
    std::shared_ptr<VideoFrameData> m_output;

    /// GL blit display window. Created instead of the QVideoWidget (Qt6) /
    /// QPixmap path (Qt5) while GL display is enabled (see m_glEnabled).
    VideoGLBlitWidget *m_glWidget = nullptr;

    // Perf console line (REQ-SW-PL-027, both Qt5 + Qt6): the "Perf" checkbox
    // enables the "video" domain and drives the 5 s console timer; m_cpu
    // samples self-CPU; the markers tag the last presented frame (Qt5 via the
    // normalized VideoCompat::pixelFormatInt()).
    QCheckBox *m_perfCheck = nullptr;
    QTimer *m_consoleTimer = nullptr;
    Daqster::Perf::ProcessCpu m_cpu;
    int m_lastHandleType = 0;      // QVideoFrame::HandleType (NoHandle = 0)
    int m_lastPixelFormat = -1;    // normalized (Qt6 numbering, see VideoCompat)

    /// "GPU display" toggle (REQ-SW-PL-021): visible + checked by default on
    /// BOTH Qt versions (checkbox ON = detached display window). Qt5 checked →
    /// detached GL blit window, unchecked → software QLabel path inside the
    /// node. Qt6 checked → detached window (native QVideoWidget unless
    /// DAQSTER_GL_BLIT=1 forces the GL blit widget), unchecked → in-scene
    /// QGraphicsVideoItem child of the node's NodeGraphicsObject (software
    /// QLabel auto-fallback).
    QCheckBox *m_glCheck = nullptr;
    /// Detached-vs-in-scene display mode (REQ-SW-PL-021): driven by the
    /// "GPU display" checkbox. true = video renders in a detached top-level
    /// window; false = video renders inside the node (Qt6: QGraphicsVideoItem
    /// via ensureSceneVideoItem(); Qt5: embedded QLabel software path).
    bool m_detachedEnabled = false;
    /// Detached display backend selection. Qt5: identical to
    /// m_detachedEnabled (checkbox ON means GL blit). Qt6: initialized from
    /// glBlitStartupEnabled() — DAQSTER_GL_BLIT=1 forces the GL blit widget,
    /// otherwise the native QVideoWidget is used when detached.
    bool m_glEnabled = false;
    /// True once a GL context could not be created in this session. GL is then
    /// not retried (re-checking the box falls back immediately) until the
    /// application is restarted.
    bool m_glFailed = false;

    std::shared_ptr<VideoFrameData> m_videoFrame;
    int m_outputConnectionCount = 0;
    /// True while a port-0 "video-frame" edge exists. Guards setInData() so
    /// frames that keep flowing from a still-playing source after the edge is
    /// removed cannot resurrect the detached popup.
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
    /// The embedded widget's vertical layout (set in the constructor; used by
    /// buildEffectControls() to append the effect combo + stack).
    QVBoxLayout *m_layout = nullptr;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVideoWidget *m_videoWidget = nullptr;

    /// In-scene QGraphicsVideoItem (REQ-SW-PL-021, Qt6): child of this node's
    /// NodeGraphicsObject, shows the video-frame input inside the node when the
    /// "GPU display" checkbox is OFF (in-scene mode). Null while detached.
    QGraphicsVideoItem *m_sceneVideoItem = nullptr;
    /// This node's NodeId in the editor scene, captured on input connection
    /// (needed to locate the NodeGraphicsObject for the in-scene item).
    QtNodes::NodeId m_selfNodeId = QtNodes::InvalidNodeId;
    /// True once m_selfNodeId has been captured from an input connection.
    bool m_selfNodeIdKnown = false;

    // Perf overlay (REQ-SW-PL-027): a separate top-level frameless tool window
    // (NOT a child of the display window) because the video renders in its own
    // layer and does not composite child widgets on top of it. The 500 ms
    // timer refreshes its text and repositions it over the display window.
    // In-scene mode (m_detachedEnabled == false) never creates/shows it.
    QLabel *m_perfBadge = nullptr;
    QTimer *m_perfTimer = nullptr;
#endif
};

#endif // VIDEOOUTPUTNODE_H
