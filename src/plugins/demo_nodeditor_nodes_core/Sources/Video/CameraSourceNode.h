#ifndef CAMERASOURCENODE_H
#define CAMERASOURCENODE_H

#include "demo_nodeditor_nodes_core_export.h"

#include "VideoCompat.h"

#include "NodeDataTypes/SampledData.h"
#include "PerfProfiler.h"
#include "shared/IStoppable.h"
#include "shared/IStartable.h"

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/internal/Definitions.hpp>

#include <QList>
#include <QStringList>
#include <memory>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtMultimedia/QAudioProbe>
#endif

class QAudioBuffer;
class QCamera;
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
 *
 * REQ-SW-PL-051: this model owns NO widgets. The device selector, the
 * Start/Stop button and the status label live in CameraSourceWidget (GUI
 * plugin) and are created through NodeWidgetFactory. The selection is kept
 * here as `m_selectedDeviceIndex` and reported to the widget through
 * devicesChanged(); user actions come back through the public slots below.
 */
class DEMO_NODEDITOR_NODES_CORE_EXPORT CameraSourceNode : public QtNodes::NodeDelegateModel, public Daqster::IStoppable, public Daqster::IStartable
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

    /// Core model has no QtWidgets dependency — the widget is created by the
    /// GUI plugin and wired through NodeWidgetFactory (REQ-SW-PL-051).
    QWidget *embeddedWidget() override { return nullptr; }

    /// Stop the camera capture. Idempotent — safe to call multiple times
    /// (REQ-SW-PL-050).
    void stop() override;

    /// Start camera capture programmatically (runtime autoStart, REQ-SW-PL-048).
    void start() override;

    /// Track downstream "sample" connections so wrapping is only emitted
    /// while a consumer is connected.
    void outputConnectionCreated(QtNodes::ConnectionId const &conId) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &conId) override;

    // ── State read by the GUI widget ───────────────────────────────────────
    /// Camera device descriptions for the selector. Entry 0 is always the
    /// platform default camera; entries 1..n map to m_devices.
    QStringList deviceDescriptions() const;
    /// Index into deviceDescriptions() of the current selection.
    int selectedDeviceIndex() const { return m_selectedDeviceIndex; }
    /// Is the capture running?
    bool isRunning() const { return m_running; }

signals:
    /// The available camera list changed (or the selection was restored) —
    /// rebuild the selector and highlight deviceDescriptions().at(index).
    void devicesChanged(const QStringList& descriptions, int selectedIndex);
    /// Status text for the node's status line.
    void statusChanged(const QString& text, bool ok);
    /// The capture started/stopped — switch the button label.
    void runningChanged(bool running);

public slots:
    /// Device selector changed.
    void onDeviceIndexChanged(int index);
    /// Start/Stop button pressed.
    void onStartStopRequested();

private slots:
    void onFrameAvailable(const QVideoFrame &frame);
    void onAudioBufferReceived(const QAudioBuffer &buffer);

private:
    void refreshDeviceList();
    VideoCompat::CameraDevice selectedDevice() const;
    void startCamera();
    void stopCamera();
    void setStatus(const QString &text, bool ok);
    static QtNodes::PortIndex audioPortIndex()
    {
        return 1; // 0 = video-frame, 1 = audio (no gap)
    }

    QList<VideoCompat::CameraDevice> m_devices;
    /// Index into m_devices; -1 = platform default camera.
    int m_selectedDeviceIndex = -1;
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
