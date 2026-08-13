#ifndef CAMERASOURCENODE_H
#define CAMERASOURCENODE_H

#include "VideoCompat.h"

#include "PerfProfiler.h"

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/internal/Definitions.hpp>

#include <QList>
#include <memory>

class QCamera;
class QComboBox;
class QLabel;
class QPushButton;
class QWidget;

class ImageData;
class VideoFrameData;

/**
 * @brief Camera source node: captures frames from a local camera device.
 *
 * Emits at the camera frame rate. On both Qt versions the node has two output
 * ports (REQ-SW-PL-020): port 0 "video-frame" (zero-copy VideoFrameData, always
 * emitted; Qt5 wraps an OWNED copy via VideoCompat::frameToOwnedFrame) and
 * port 1 "image" (ImageData, converted only while a processing consumer is
 * connected). NOTE (NV12-direct renumbering): on Qt5 the image port moved from
 * 0 to 1 — old saved Qt5 graphs that connected the source's port 0 (was
 * "image") to an ImageData consumer will lose that edge and must be
 * re-connected to the new image port (port 1).
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

    /// Track downstream "image" connections (port 1) so the QImage
    /// conversion only happens while a processing consumer is connected.
    void outputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

private slots:
    void onDeviceChanged(int index);
    void onStartStopClicked();
    void onFrameAvailable(const QVideoFrame &frame);

private:
    void buildWidget();
    void refreshDeviceList();
    VideoCompat::CameraDevice selectedDevice() const;
    void startCamera();
    void stopCamera();
    void setStatus(const QString &text, bool ok);

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
    // Reused per frame via setFrame() — no allocation per frame (REQ-SW-PL-020).
    // On Qt5 the frame is an owned copy (frameToOwnedFrame); on Qt6 the decoded
    // probe frame (ref-count bump only).
    std::shared_ptr<VideoFrameData> m_videoFrameOut;
    int m_imagePortConnectionCount = 0;
    // Runtime profiling (REQ-SW-PL-027): last-frame HW/SW markers
    // (handleType/pixelFormat) for source-side diagnostics.
    int m_lastHandleType = 0;      // QVideoFrame::HandleType (NoHandle = 0)
    int m_lastPixelFormat = -1;    // normalized (Qt6 numbering, see VideoCompat)
    std::shared_ptr<ImageData> m_output;
    bool m_running = false;
};

#endif // CAMERASOURCENODE_H
