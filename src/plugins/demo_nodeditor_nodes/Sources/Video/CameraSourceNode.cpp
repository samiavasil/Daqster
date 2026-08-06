#include "CameraSourceNode.h"

#include "NodeDataTypes/ImageData.h"

#include <QCamera>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

CameraSourceNode::CameraSourceNode()
    : m_output(std::make_shared<ImageData>())
{
    buildWidget();
    refreshDeviceList();
}

CameraSourceNode::~CameraSourceNode()
{
    stopCamera();
    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QJsonObject CameraSourceNode::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();

    const int deviceIndex = m_deviceCombo->currentData().toInt(-1);
    if (deviceIndex >= 0 && deviceIndex < m_devices.size())
        modelJson["cameraId"] = VideoCompat::cameraId(m_devices.at(deviceIndex));
    modelJson["running"] = m_running;

    return modelJson;
}

void CameraSourceNode::load(QJsonObject const &p)
{
    if (!p.contains("cameraId"))
        return;

    const QString savedId = p["cameraId"].toString();
    for (int i = 0; i < m_devices.size(); ++i) {
        if (VideoCompat::cameraId(m_devices.at(i)) == savedId) {
            m_deviceCombo->setCurrentIndex(i + 1);
            break;
        }
    }
}

unsigned int CameraSourceNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType CameraSourceNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return ImageData().type();
}

std::shared_ptr<NodeData> CameraSourceNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void CameraSourceNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(data);
    Q_UNUSED(portIndex);
}

QWidget *CameraSourceNode::embeddedWidget()
{
    return m_widget;
}

void CameraSourceNode::buildWidget()
{
    m_widget = new QWidget();
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_deviceCombo = new QComboBox(m_widget);
    m_deviceCombo->setMinimumWidth(180);
    layout->addWidget(m_deviceCombo);

    auto *buttonRow = new QHBoxLayout();
    m_startStopButton = new QPushButton(tr("Start"), m_widget);
    m_statusLabel = new QLabel(tr("Stopped"), m_widget);
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray;"));
    buttonRow->addWidget(m_startStopButton);
    buttonRow->addWidget(m_statusLabel, 1);
    layout->addLayout(buttonRow);

    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraSourceNode::onDeviceChanged);
    connect(m_startStopButton, &QPushButton::clicked,
            this, &CameraSourceNode::onStartStopClicked);
}

void CameraSourceNode::refreshDeviceList()
{
    m_deviceCombo->blockSignals(true);
    m_deviceCombo->clear();
    m_devices = VideoCompat::availableCameras();

    // First entry always represents the platform default camera.
    m_deviceCombo->addItem(tr("Default camera"), -1);

    for (int i = 0; i < m_devices.size(); ++i)
        m_deviceCombo->addItem(VideoCompat::cameraDescription(m_devices.at(i)), i);

    if (m_devices.isEmpty())
        setStatus(tr("No camera found"), false);

    m_deviceCombo->blockSignals(false);
}

VideoCompat::CameraDevice CameraSourceNode::selectedDevice() const
{
    const int deviceIndex = m_deviceCombo->currentData().toInt(-1);
    if (deviceIndex >= 0 && deviceIndex < m_devices.size())
        return m_devices.at(deviceIndex);
    return VideoCompat::defaultCamera();
}

void CameraSourceNode::startCamera()
{
    if (m_running)
        return;

    const VideoCompat::CameraDevice device = selectedDevice();
    if (VideoCompat::isNull(device)) {
        setStatus(tr("No camera available"), false);
        return;
    }

    m_camera = new QCamera(device, this);
    m_frameProbe = new VideoCompat::FrameProbe(this);

    if (!VideoCompat::attachFrameProbe(m_camera, m_frameProbe)) {
        stopCamera();
        setStatus(tr("Failed to attach frame capture"), false);
        return;
    }

    VideoCompat::connectFrameProbed(
        m_frameProbe, this,
        [this](const QVideoFrame &frame) { onFrameAvailable(frame); });

    connect(m_camera, &QCamera::errorOccurred, this,
            [this](QCamera::Error error, const QString &errorString) {
                Q_UNUSED(error);
                setStatus(tr("Camera error: %1").arg(errorString), false);
            });

    m_camera->start();
    m_running = true;
    m_startStopButton->setText(tr("Stop"));
    setStatus(tr("Running"), true);
}

void CameraSourceNode::stopCamera()
{
    if (m_camera != nullptr)
        m_camera->stop();

    if (m_camera != nullptr) {
        m_camera->deleteLater();
        m_camera = nullptr;
    }
    if (m_frameProbe != nullptr) {
        m_frameProbe->deleteLater();
        m_frameProbe = nullptr;
    }

    m_running = false;
    m_startStopButton->setText(tr("Start"));
    setStatus(tr("Stopped"), false);
}

void CameraSourceNode::onDeviceChanged(int index)
{
    Q_UNUSED(index);
    // Restart with the newly selected device when capture is already running.
    if (m_running) {
        stopCamera();
        startCamera();
    }
}

void CameraSourceNode::onStartStopClicked()
{
    if (m_running)
        stopCamera();
    else
        startCamera();
}

void CameraSourceNode::onFrameAvailable(const QVideoFrame &frame)
{
    const QImage image = VideoCompat::frameToImage(frame);
    if (image.isNull())
        return;

    m_output = std::make_shared<ImageData>(image);
    Q_EMIT dataUpdated(0);
}

void CameraSourceNode::setStatus(const QString &text, bool ok)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(ok ? QStringLiteral("color: green;")
                                    : QStringLiteral("color: gray;"));
}
