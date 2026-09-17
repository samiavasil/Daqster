#ifndef CAMERASOURCENODE_H
#define CAMERASOURCENODE_H

#include "VideoCompat.h"

#include "NodeDataTypes/SampledData.h"
#include "PerfProfiler.h"

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/internal/Definitions.hpp>

#include <QList>
#include <memory>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtMultimedia/QAudioProbe>
#endif

class QAudioBuffer;
class QCamera;
class QComboBox;
class QLabel;
class QPushButton;
class QWidget;

class VideoFrameData;

/**
 * @brief Camera source node: captures frames from a local camera device.
 *
 * Emits at the camera frame rate. On both Qt versions the node has two
 * output ports (REQ-SW-PL-020/022, single video-frame type REQ-SW-PL-032):
 *   - port 0 "video-frame" — zero-copy VideoFrameData wrapping the captured
 *     QVideoFrame; emitted for every frame (no QImage conversion).
 *   - port 1 "sample" — SampledData (domain "audio"). Qt5 captures camera
 *     audio via QAudioProbe; Qt6 does not expose captured audio buffers on
 *     QMediaCaptureSession (QAudioBufferOutput is playback-only), so the
 *     sample port emits invalid data on Qt6.
 * The embedded widget lets the user pick a camera device (or the platform
 * default) and start or stop the capture.
 */
class CameraSourceNode : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    CameraSourceNode();
    ~CameraSourceNode() override;

    QString caption() const override
    { return QStringLiteral("Camera Source"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("CameraSource"); }

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
    void onDeviceChanged(int index);
    void onStartStopClicked();
    void onFrameAvailable(const QVideoFrame &frame);
    void onAudioBufferReceived(const QAudioBuffer &buffer);

private:
    void buildWidget();
    void refreshDeviceList();
    VideoCompat::CameraDevice selectedDevice() const;
    void startCamera();
    void stopCamera();
    void setStatus(const QString &text, bool ok);
    static QtNodes::PortIndex audioPortIndex()
    {
        return 1; // 0 = video-frame, 1 = audio (no gap)
    }

    QWidget *m_widget = nullptr;
    QComboBox *m_deviceCombo = nullptr;
    QPushButton *m_startStopButton = nullptr;
    QLabel *m_statusLabel = nullptr;

    QList<VideoCompat::CameraDevice> m_devices;
    QCamera *m_camera = nullptr;
    VideoCompat::FrameProbe *m_frameProbe = nullptr;
    // Runtime profiling (REQ-SW-PL-027): inter-frame gap stopwatch + first-frame
    // flag. The HW/SW markers are filled on both Qt versions (Qt5 via the
    // normalized VideoCompat::pixelFormatInt()).
    Daqster::Perf::Stopwatch m_perfWatch;
    bool m_perfFirstFrame = true;
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5: audio buffer probe (audioBufferProbed) on the camera.
    QAudioProbe *m_audioProbe = nullptr;
#endif
    std::shared_ptr<SampledData> m_audioOut;
    int m_audioPortConnectionCount = 0;
    bool m_running = false;
};

#endif // CAMERASOURCENODE_H
