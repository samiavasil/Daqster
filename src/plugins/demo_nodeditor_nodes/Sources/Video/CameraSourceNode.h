#ifndef CAMERASOURCENODE_H
#define CAMERASOURCENODE_H

#include "VideoCompat.h"

#include <QtNodes/NodeDelegateModel>

#include <QList>
#include <memory>

class QCamera;
class QComboBox;
class QLabel;
class QPushButton;
class QWidget;

class ImageData;

/**
 * @brief Camera source node: captures frames from a local camera device.
 *
 * Emits ImageData ("image") at the camera frame rate. The embedded widget
 * lets the user pick a camera device (or the platform default) and start or
 * stop the capture.
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
    std::shared_ptr<ImageData> m_output;
    bool m_running = false;
};

#endif // CAMERASOURCENODE_H
