#ifndef CAMERASOURCENODE_H
#define CAMERASOURCENODE_H

#include "VideoCompat.h"

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
 * Emits at the camera frame rate. On Qt6 the node has two output ports
 * (REQ-SW-PL-020): port 0 "video-frame" (zero-copy VideoFrameData, always
 * emitted) and port 1 "image" (ImageData, converted only while a processing
 * consumer is connected). On Qt5 it keeps the single "image" output.
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

    /// Track downstream "image" connections (port 1, Qt6) so the QImage
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Reused per frame via setFrame() — no allocation per frame (REQ-SW-PL-020).
    std::shared_ptr<VideoFrameData> m_videoFrameOut;
    int m_imagePortConnectionCount = 0;
#endif
    std::shared_ptr<ImageData> m_output;
    bool m_running = false;
};

#endif // CAMERASOURCENODE_H
