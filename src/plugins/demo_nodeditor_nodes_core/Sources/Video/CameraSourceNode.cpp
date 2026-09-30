#include "CameraSourceNode.h"

#include "AudioBufferToSampled.h"
#include "NodeDataTypes/VideoFrameData.h"

#include <QCamera>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

CameraSourceNode::CameraSourceNode()
    : m_videoFrameOut(std::make_shared<VideoFrameData>())
{
    refreshDeviceList();
}

CameraSourceNode::~CameraSourceNode()
{
    // Single shutdown path: stop() stops the camera capture (REQ-SW-PL-050).
    stop();
}

void CameraSourceNode::stop()
{
    // Idempotent: stopCamera() on an already-stopped camera is a no-op.
    stopCamera();
}

/// Start camera capture programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onStartStopClicked() when not running.
void CameraSourceNode::start()
{
    if (m_running)
        return;
    startCamera();
}

QJsonObject CameraSourceNode::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();

    if (m_selectedDeviceIndex >= 0 && m_selectedDeviceIndex < m_devices.size())
        modelJson["cameraId"] = VideoCompat::cameraId(m_devices.at(m_selectedDeviceIndex));
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
            m_selectedDeviceIndex = i;
            break;
        }
    }
    // Push the restored selection into the (possibly already built) widget.
    Q_EMIT devicesChanged(deviceDescriptions(), m_selectedDeviceIndex + 1);
}

unsigned int CameraSourceNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::Out:
        // Port 0: "video-frame" (zero-copy), port 1: "sample" (audio,
        // no gap — REQ-SW-PL-022).
        return 2;
    default:
        return 0;
    }
}

NodeDataType CameraSourceNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    if (portIndex == 1)
        return SampledData().type();
    return VideoFrameData().type();
}

std::shared_ptr<NodeData> CameraSourceNode::outData(PortIndex port)
{
    if (port == 1)
        return m_audioOut;
    return m_videoFrameOut;
}

void CameraSourceNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    Q_UNUSED(data);
    Q_UNUSED(portIndex);
}

void CameraSourceNode::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 1)
        ++m_audioPortConnectionCount;
}

void CameraSourceNode::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 1 && m_audioPortConnectionCount > 0)
        --m_audioPortConnectionCount;
}

QStringList CameraSourceNode::deviceDescriptions() const
{
    // Entry 0 always represents the platform default camera; entries 1..n map
    // to m_devices (so the combo index is `m_selectedDeviceIndex + 1`).
    QStringList descriptions;
    descriptions.reserve(m_devices.size() + 1);
    descriptions.append(tr("Default camera"));
    for (int i = 0; i < m_devices.size(); ++i)
        descriptions.append(VideoCompat::cameraDescription(m_devices.at(i)));
    return descriptions;
}

void CameraSourceNode::refreshDeviceList()
{
    m_devices = VideoCompat::availableCameras();

    if (m_selectedDeviceIndex >= m_devices.size())
        m_selectedDeviceIndex = m_devices.isEmpty() ? -1 : m_devices.size() - 1;

    if (m_devices.isEmpty())
        setStatus(tr("No camera found"), false);

    Q_EMIT devicesChanged(deviceDescriptions(), m_selectedDeviceIndex + 1);
}

VideoCompat::CameraDevice CameraSourceNode::selectedDevice() const
{
    if (m_selectedDeviceIndex >= 0 && m_selectedDeviceIndex < m_devices.size())
        return m_devices.at(m_selectedDeviceIndex);
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

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5: capture camera audio via QAudioProbe (QCamera is a QMediaObject).
    // Qt6 does not expose captured audio buffers on QMediaCaptureSession, so
    // the sample port emits invalid data there (see header doc).
    m_audioProbe = new QAudioProbe(this);
    if (m_audioProbe->setSource(m_camera)) {
        connect(m_audioProbe, &QAudioProbe::audioBufferProbed,
                this, &CameraSourceNode::onAudioBufferReceived);
    } else {
        m_audioProbe->deleteLater();
        m_audioProbe = nullptr;
    }
#endif

    VideoCompat::connectCameraError(
        m_camera, this,
        [this](int error, const QString &errorString) {
            Q_UNUSED(error);
            setStatus(tr("Camera error: %1").arg(errorString), false);
        });

    m_camera->start();
    m_running = true;
    Q_EMIT runningChanged(true);
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (m_audioProbe != nullptr) {
        m_audioProbe->deleteLater();
        m_audioProbe = nullptr;
    }
#endif

    m_running = false;
    Q_EMIT runningChanged(false);
    setStatus(tr("Stopped"), false);
}

void CameraSourceNode::onDeviceIndexChanged(int index)
{
    // The combo holds a leading "Default camera" entry, so shift by one.
    const int deviceIndex = index - 1;
    if (deviceIndex < -1 || deviceIndex >= m_devices.size())
        return;
    if (deviceIndex == m_selectedDeviceIndex)
        return;
    m_selectedDeviceIndex = deviceIndex;

    // Restart with the newly selected device when capture is already running.
    if (m_running) {
        stopCamera();
        startCamera();
    }
}

void CameraSourceNode::onStartStopRequested()
{
    if (m_running)
        stopCamera();
    else
        startCamera();
}

void CameraSourceNode::onFrameAvailable(const QVideoFrame &frame)
{
    // Runtime profiling (REQ-SW-PL-027): inter-frame gap is a proxy for the
    // decode cadence (the backend decodes before onFrameAvailable) and the
    // HW/SW markers tag the actual frame path. Everything is a no-op while the
    // "video" domain is disabled — the PERF_ENABLED guard avoids the clock read.
    if (PERF_ENABLED("video")) {
        const std::int64_t gapNs = m_perfWatch.mark();
        if (!m_perfFirstFrame && gapNs > 0)
            Daqster::Perf::Domain::get("video").record("source.frame_interval", gapNs);
        m_perfFirstFrame = false;

        m_lastHandleType = static_cast<int>(frame.handleType());
        m_lastPixelFormat = VideoCompat::pixelFormatInt(frame);
    }

    {
        PERF_SCOPE("video", "source.wrap_emit");
        // Fresh VideoFrameData per frame — never mutate the shared object a
        // consumer may still hold (frame aliasing). VideoCompat::frameToFrame()
        // dispatches internally: Qt6 = identity (ref-count bump), Qt5 = owned
        // copy (frameToOwnedFrame, may fail for unsupported formats).
        m_videoFrameOut = std::make_shared<VideoFrameData>(VideoCompat::frameToFrame(frame));
        if (m_videoFrameOut->hasFrame())
            Q_EMIT dataUpdated(0);
    }
}

void CameraSourceNode::onAudioBufferReceived(const QAudioBuffer &buffer)
{
    // Invalid/empty buffer (end-of-stream flush): ignore — no EOS type is
    // emitted (REQ-SW-PL-022 §4).
    if (!buffer.isValid() || buffer.byteCount() <= 0)
        return;

    // Wrap only — no sample conversion in the handler (REQ-SW-PL-022 §4).
    m_audioOut = AudioBufferToSampled::wrapBuffer(buffer, name(), 10.0);
    if (!m_audioOut)
        return;

    // Emit only while a downstream consumer is connected (connection-count
    // model, like the video-frame port).
    if (m_audioPortConnectionCount <= 0)
        return;

    Q_EMIT dataUpdated(audioPortIndex());
}

void CameraSourceNode::setStatus(const QString &text, bool ok)
{
    Q_EMIT statusChanged(text, ok);
}
