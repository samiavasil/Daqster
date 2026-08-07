#ifndef STREAMSOURCENODE_H
#define STREAMSOURCENODE_H

#include "VideoCompat.h"

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/internal/Definitions.hpp>

#include <memory>

class QLabel;
class QLineEdit;
class QMediaPlayer;
class QPushButton;
class QWidget;

class ImageData;
class VideoFrameData;

/**
 * @brief Stream source node: plays an HTTP/RTSP video stream and emits frames.
 *
 * Uses QMediaPlayer with a VideoCompat frame probe attached.
 *
 * On Qt6 the node has two output ports (REQ-SW-PL-020):
 *   - port 0 "video-frame" — zero-copy VideoFrameData wrapping the decoded
 *     QVideoFrame; emitted for every frame (no QImage conversion).
 *   - port 1 "image" — ImageData, converted from the frame ONLY while a
 *     downstream processing consumer is connected (avoiding hot-path
 *     toImage() when nothing needs it).
 *
 * On Qt5 the node keeps the original single "image" output port (QImage path).
 */
class StreamSourceNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    StreamSourceNode();
    ~StreamSourceNode() override;

    QString caption() const override
    { return QStringLiteral("Stream Source"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("StreamSource"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    QWidget *embeddedWidget() override;

    /// Track downstream "image" connections (port 1, Qt6) so the QImage
    /// conversion only happens while a processing consumer is connected.
    void outputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

private slots:
    void onConnectClicked();
    void onFrameAvailable(const QVideoFrame &frame);
    void onPlaybackStateChanged(int state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlayerError(QMediaPlayer::Error error, const QString &errorString);

private:
    void buildWidget();
    void setStatus(const QString &text, bool ok);
    void updateConnectButton();

    QWidget *m_widget = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    QMediaPlayer *m_player = nullptr;
    VideoCompat::FrameProbe *m_frameProbe = nullptr;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Reused per frame via setFrame() — no allocation per frame (REQ-SW-PL-020).
    std::shared_ptr<VideoFrameData> m_videoFrameOut;
    int m_imagePortConnectionCount = 0;
#endif
    std::shared_ptr<ImageData> m_output;
    bool m_isPlaying = false;
};

#endif // STREAMSOURCENODE_H
