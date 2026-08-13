#ifndef VIDEOOUTPUTNODE_H
#define VIDEOOUTPUTNODE_H

#include <QtNodes/NodeDelegateModel>

#include "ProcessCpu.h"

#include <QImage>
#include <memory>

class QLabel;
class QWidget;

class ImageData;
class QCheckBox;
class QTimer;
class VideoFrameData;
class VideoGLBlitWidget;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QVideoWidget;
#endif

/**
 * @brief Video output node: displays incoming video frames.
 *
 * The node has two input ports on BOTH Qt versions (REQ-SW-PL-020, NV12-direct):
 *   - port 0 "video-frame" — zero-copy VideoFrameData; presented on the
 *     detached GL blit window (default on Qt5; Qt6 uses QVideoWidget unless
 *     DAQSTER_GL_BLIT=1). Qt5 frames are owned copies (frameToOwnedFrame),
 *     Qt6 frames are the decoded probe frames.
 *   - port 1 "image" — ImageData; displayed on the embedded QLabel (backward
 *     compatible with processing chains that emit ImageData).
 *
 * GL blit display selection (REQ-SW-PL-021):
 *   - Default per Qt version: Qt5 = GL blit ON (fastest measured display path,
 *     ~15% CPU vs ~34% software), Qt6 = native QVideoWidget (GL blit OFF).
 *   - Env override, applied at STARTUP only: `DAQSTER_GL_BLIT=0` forces the
 *     software path (also Qt5), `DAQSTER_GL_BLIT=1` forces the GL path (also
 *     Qt6). Unset → per-Qt default above. The VALUE matters: "0" disables
 *     (the old presence check treated even "=0" as enabled).
 *   - UI: the "GPU display" checkbox (Qt5 only, checked by default; hidden on
 *     Qt6 because the native QVideoWidget is already GPU-accelerated) has the
 *     final word at runtime — toggling switches the display backend at the
 *     next frame without crashing or losing video.
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
 * can be built (e.g. output of a modifier). The per-frame QImage conversion +
 * ImageData output only runs while a downstream consumer is connected to the
 * output port (tracked via outputConnectionCreated/Deleted).
 *
 * NOTE (NV12-direct renumbering): on Qt5 port 0 changed from "image" to
 * "video-frame" — old saved Qt5 graphs that connected an ImageData producer to
 * input port 0 will lose that edge and must be re-connected to the image port
 * (port 1).
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

    /// Log the single-line console perf report (5 s timer, both Qt5 + Qt6).
    void logPerfLine();

    /// Apply the "GPU display" checkbox: toggling GL blit OFF closes the
    /// detached GL window immediately; toggling ON is applied at the next
    /// frame (the window is created lazily by ensureVideoWidget()).
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
    /// Qt5 without GL.
    void ensureVideoWidget();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
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
    std::shared_ptr<ImageData> m_output;

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

    /// "GPU display" toggle (REQ-SW-PL-021): Qt5 visible + checked by default,
    /// Qt6 hidden (native QVideoWidget is already GPU-accelerated; GL blit
    /// stays available via DAQSTER_GL_BLIT=1 at startup).
    QCheckBox *m_glCheck = nullptr;
    /// Current GL blit display preference. Initialized from
    /// glBlitStartupEnabled() (env override + per-Qt default) and then driven
    /// by the checkbox — the UI has the final word after startup.
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVideoWidget *m_videoWidget = nullptr;

    // Perf overlay (REQ-SW-PL-027): a separate top-level frameless tool window
    // (NOT a child of the display window) because the video renders in its own
    // layer and does not composite child widgets on top of it. The 500 ms
    // timer refreshes its text and repositions it over the display window.
    QLabel *m_perfBadge = nullptr;
    QTimer *m_perfTimer = nullptr;
#endif
};

#endif // VIDEOOUTPUTNODE_H
