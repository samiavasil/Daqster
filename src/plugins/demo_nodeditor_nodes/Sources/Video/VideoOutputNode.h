#ifndef VIDEOOUTPUTNODE_H
#define VIDEOOUTPUTNODE_H

#include <QtNodes/NodeDelegateModel>

#include <QImage>
#include <memory>

class QLabel;
class QWidget;

class ImageData;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QCheckBox;
class QTimer;
class QVideoWidget;
class VideoFrameData;
#endif

/**
 * @brief Video output node: displays incoming video frames.
 *
 * On Qt6 the node has two input ports (REQ-SW-PL-020):
 *   - port 0 "video-frame" — zero-copy VideoFrameData; presented on a detached
 *     QVideoWidget (GPU path: HW buffer → RHI texture → screen, no QImage copy).
 *   - port 1 "image" — ImageData; displayed on the embedded QLabel (backward
 *     compatible with processing chains that emit ImageData).
 *
 * On Qt5 the node keeps the original single "image" input port (QImage path).
 *
 * The embedded QLabel shows a static placeholder ("GPU display active — see
 * detached window") while the Qt6 GPU path is active, so the node remains
 * usable inside the node editor scene without per-frame QImage conversion.
 * The QVideoWidget is a separate top-level window (QTBUG-35299 prevents
 * hosting it in the scene).
 *
 * The node also passes the frame through on its output port so output chains
 * can be built (e.g. output of a modifier). On Qt6 the per-frame QImage
 * conversion + ImageData output only runs while a downstream consumer is
 * connected to the output port (tracked via outputConnectionCreated/Deleted).
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

    /// Track downstream connections on the output port (Qt6) so the per-frame
    /// QImage conversion only happens while a processing consumer is connected.
    void outputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

    /// Track the port-0 "video-frame" input connection. Disconnecting the edge
    /// does NOT stop the source player — frames keep arriving in setInData() —
    /// so the connection flag (not widget nullness) is the guard that prevents
    /// the detached QVideoWidget popup from being resurrected after disconnect.
    void inputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void inputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void updateDisplay();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    /// Lazily create the detached QVideoWidget on the first video-frame input.
    void ensureVideoWidget();

    /// Refresh the perf overlay badge from the "video" domain aggregates
    /// (fired on a ~500 ms timer while the Perf checkbox is enabled).
    void updatePerfBadge();
#endif

    QWidget *m_widget = nullptr;
    QLabel *m_label = nullptr;
    QImage m_image;
    std::shared_ptr<ImageData> m_output;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    std::shared_ptr<VideoFrameData> m_videoFrame;
    QVideoWidget *m_videoWidget = nullptr;
    int m_outputConnectionCount = 0;
    /// True while a port-0 "video-frame" edge exists. Guards setInData() so
    /// frames that keep flowing from a still-playing source after the edge is
    /// removed cannot resurrect the detached popup.
    bool m_videoInputConnected = false;

    // Perf overlay (REQ-SW-PL-027): the checkbox lives in the embedded widget;
    // the badge is a child of the detached QVideoWidget; the timer refreshes it.
    QCheckBox *m_perfCheck = nullptr;
    QLabel *m_perfBadge = nullptr;
    QTimer *m_perfTimer = nullptr;
    int m_lastHandleType = 0;      // QVideoFrame::HandleType (NoHandle = 0)
    int m_lastPixelFormat = -1;    // QVideoFrameFormat::PixelFormat
#endif
};

#endif // VIDEOOUTPUTNODE_H
