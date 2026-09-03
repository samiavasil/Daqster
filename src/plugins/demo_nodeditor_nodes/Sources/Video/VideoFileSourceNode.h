#ifndef VIDEOFILESOURCENODE_H
#define VIDEOFILESOURCENODE_H

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

class VideoFrameData;

/**
 * @brief Video file source node: plays a local video file and emits frames.
 *
 * Uses QMediaPlayer with a VideoCompat frame probe attached.
 *
 * On Qt6 the node has two output ports (REQ-SW-PL-020/022, single video-frame
 * type REQ-SW-PL-032):
 *   - port 0 "video-frame" — zero-copy VideoFrameData wrapping the decoded
 *     QVideoFrame; emitted for every frame (no QImage conversion).
 *   - port 1 "sample" — SampledData (domain "audio"), wrapped from the decoded
 *     audio buffers (REQ-SW-PL-022).
 *
 * On Qt5 the node has two output ports (mirror of Qt6, NV12-direct):
 *   - port 0 "video-frame" — VideoFrameData wrapping an OWNED copy of the
 *     decoded frame (VideoCompat::frameToOwnedFrame), emitted for every frame
 *     (no QImage conversion).
 *   - port 1 "sample" — SampledData (domain "audio").
 *
 * The audio port is at index 1 (no gap) — REQ-SW-PL-022.
 */
class VideoFileSourceNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    VideoFileSourceNode();
    ~VideoFileSourceNode() override;

    QString caption() const override
    { return QStringLiteral("Video File Source"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("VideoFileSource"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex portIndex) override;

    QWidget *embeddedWidget() override;

    /// Track downstream "sample" connections so wrapping is only emitted
    /// while a consumer is connected.
    void outputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

private slots:
    void onBrowseClicked();
    void onPlayPauseClicked();
    void onStopClicked();
    void onSeekBackClicked();
    void onSeekForwardClicked();
    void onFrameAvailable(const QVideoFrame &frame);
    void onAudioBufferReceived(const QAudioBuffer &buffer);
    void onPlaybackStateChanged(int state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onPlayerError(QMediaPlayer::Error error, const QString &errorString);

private:
    void buildWidget();
    void setStatus(const QString &text, bool ok);
    QString currentFilePath() const;
    void updatePlayButton();
    static QtNodes::PortIndex audioPortIndex()
    {
        return 1; // 0 = video-frame, 1 = audio (no gap)
    }

    QWidget *m_widget = nullptr;
    QLineEdit *m_fileEdit = nullptr;
    QPushButton *m_playPauseButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_seekBackButton = nullptr;
    QPushButton *m_seekForwardButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_timeLabel = nullptr;

    QMediaPlayer *m_player = nullptr;
    VideoCompat::FrameProbe *m_frameProbe = nullptr;
    // Runtime profiling (REQ-SW-PL-027): inter-frame gap stopwatch + first-frame
    // flag. The HW/SW markers are filled on both Qt versions (Qt5 via the
    // normalized VideoCompat::pixelFormatInt()).
    Daqster::Perf::Stopwatch m_perfWatch;
    bool m_perfFirstFrame = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6 requires an explicit QAudioOutput; without setAudioOutput() the
    // audio is decoded but never routed and stays silent (REQ-SW-PL-022 AC 1).
    QAudioOutput *m_audioOutput = nullptr;
#endif
    // Fresh VideoFrameData per frame — no aliasing: consumers keep the old
    // frame alive via shared_ptr while the next frame is emitted (setFrame()
    // on a shared object would delete GL textures a deferred paintGL could
    // still use). On Qt5 the frame is an owned copy (frameToOwnedFrame); on
    // Qt6 the decoded probe frame (ref-count bump only).
    std::shared_ptr<VideoFrameData> m_videoFrameOut;
    // Runtime profiling (REQ-SW-PL-027): last-frame HW/SW markers
    // (handleType/pixelFormat) for source-side diagnostics.
    int m_lastHandleType = 0;      // QVideoFrame::HandleType (NoHandle = 0)
    int m_lastPixelFormat = -1;    // normalized (Qt6 numbering, see VideoCompat)
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Qt6: receives decoded audio buffers (Qt 6.8+, FFmpeg backend).
    QAudioBufferOutput *m_audioBufferOutput = nullptr;
#elif QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5: audio buffer probe (audioBufferProbed).
    QAudioProbe *m_audioProbe = nullptr;
#endif
    std::shared_ptr<SampledData> m_audioOut;
    int m_audioPortConnectionCount = 0;
    QString m_loadedPath;
    bool m_isPlaying = false;
};

#endif // VIDEOFILESOURCENODE_H
