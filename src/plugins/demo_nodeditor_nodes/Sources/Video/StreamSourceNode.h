#ifndef STREAMSOURCENODE_H
#define STREAMSOURCENODE_H

#include "VideoCompat.h"

#include "NodeDataTypes/SampledData.h"
#include "PerfProfiler.h"

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/internal/Definitions.hpp>

#include <memory>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtMultimedia/QAudioOutput>
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QtMultimedia/QAudioBufferOutput>
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtMultimedia/QAudioProbe>
#endif

class QAudioBuffer;
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
 * On Qt6 the node has three output ports (REQ-SW-PL-020/022):
 *   - port 0 "video-frame" — zero-copy VideoFrameData wrapping the decoded
 *     QVideoFrame; emitted for every frame (no QImage conversion).
 *   - port 1 "image" — ImageData, converted from the frame ONLY while a
 *     downstream processing consumer is connected.
 *   - port 2 "sample" — SampledData (domain "audio"), wrapped from the decoded
 *     audio buffers (REQ-SW-PL-022).
 *
 * On Qt5 the node has two output ports:
 *   - port 0 "image" — ImageData (QImage path).
 *   - port 1 "sample" — SampledData (domain "audio").
 *
 * The audio port is APPENDED LAST on both Qt versions so old saved graphs keep
 * their port indices (REQ-SW-PL-022 AC 8).
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

    /// Track downstream "image"/"sample" connections so conversion/wrapping
    /// is only emitted while a consumer is connected.
    void outputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

private slots:
    void onConnectClicked();
    void onFrameAvailable(const QVideoFrame &frame);
    void onAudioBufferReceived(const QAudioBuffer &buffer);
    void onPlaybackStateChanged(int state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlayerError(QMediaPlayer::Error error, const QString &errorString);

private:
    void buildWidget();
    void setStatus(const QString &text, bool ok);
    void updateConnectButton();
    static QtNodes::PortIndex audioPortIndex()
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return 2; // 0 = video-frame, 1 = image, 2 = audio (appended last)
#else
        return 1; // 0 = image, 1 = audio (appended last)
#endif
    }

    QWidget *m_widget = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QPushButton *m_connectButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    QMediaPlayer *m_player = nullptr;
    VideoCompat::FrameProbe *m_frameProbe = nullptr;
    // Runtime profiling (REQ-SW-PL-027): inter-frame gap stopwatch + first-frame
    // flag, shared by both the Qt6 zero-copy path and the Qt5 QImage path. The
    // HW/SW markers are Qt6-only (Qt5 QVideoFrame has no surfaceFormat()).
    Daqster::Perf::Stopwatch m_perfWatch;
    bool m_perfFirstFrame = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6 requires an explicit QAudioOutput; without setAudioOutput() the
    // audio is decoded but never routed and stays silent (REQ-SW-PL-022 AC 1).
    QAudioOutput *m_audioOutput = nullptr;
    // Reused per frame via setFrame() — no allocation per frame (REQ-SW-PL-020).
    std::shared_ptr<VideoFrameData> m_videoFrameOut;
    int m_imagePortConnectionCount = 0;
    // Runtime profiling (REQ-SW-PL-027): last-frame HW/SW markers
    // (handleType/pixelFormat) for source-side diagnostics.
    int m_lastHandleType = 0;      // QVideoFrame::HandleType (NoHandle = 0)
    int m_lastPixelFormat = -1;    // QVideoFrameFormat::PixelFormat
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Qt6: receives decoded audio buffers (Qt 6.8+, FFmpeg backend).
    QAudioBufferOutput *m_audioBufferOutput = nullptr;
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5: audio buffer probe (audioBufferProbed).
    QAudioProbe *m_audioProbe = nullptr;
#endif
    std::shared_ptr<SampledData> m_audioOut;
    int m_audioPortConnectionCount = 0;
    std::shared_ptr<ImageData> m_output;
    bool m_isPlaying = false;
};

#endif // STREAMSOURCENODE_H
