#include "VideoOutputNode.h"

#include "NodeDataTypes/ImageData.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "PerfProfiler.h"
#include "VideoCompat.h"
#include "VideoGLBlitWidget.h"
#include "VideoPerfBadge.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QLabel>
#include <QPixmap>
#include <QSize>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QVideoWidget>
#endif

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {

/// Env toggle selects the GL blit display path (QVideoFrame -> YUV textures ->
/// shader on both Qt versions; QImage -> RGBA texture for the image port).
bool glBlitEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("DAQSTER_GL_BLIT");
    return enabled;
}

} // namespace

VideoOutputNode::VideoOutputNode()
    : m_videoFrame(std::make_shared<VideoFrameData>())
{
    // Display nodes must never get a graphics effect (perf): the shadow blur
    // runs per repaint and costs ~46% CPU during video playback (PERF results,
    // tests/performance/performance-video-display-2026-08-13.md).
    QtNodes::NodeStyle s = this->nodeStyle();
    s.ShadowEnabled = false;
    this->setNodeStyle(s);

    m_widget = new QWidget();
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);

    m_label = new QLabel(m_widget);
    m_label->setMinimumSize(320, 240);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setText(tr("No video input"));
    m_label->setStyleSheet(QStringLiteral("background-color: black; color: gray;"));
    layout->addWidget(m_label);

    // Perf toggle + console line (REQ-SW-PL-027, both Qt5 + Qt6): enables the
    // "video" profiling domain live and drives the 5 s console timer. On Qt6 it
    // also drives the on-screen badge refresh timer (500 ms); Qt5 keeps the
    // QImage path with no overlay but still logs the copy-paste-able line.
    m_perfCheck = new QCheckBox(tr("Perf"), m_widget);
    layout->addWidget(m_perfCheck);

    m_consoleTimer = new QTimer(this);
    m_consoleTimer->setInterval(5000);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_perfTimer = new QTimer(this);
    m_perfTimer->setInterval(500);
#endif

    connect(m_perfCheck, &QCheckBox::toggled, this, [this](bool checked) {
        Daqster::Perf::Domain::get("video").setEnabled(checked);
        if (checked) {
            m_consoleTimer->start();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            m_perfTimer->start();
#endif
        } else {
            m_consoleTimer->stop();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            m_perfTimer->stop();
            if (m_perfBadge != nullptr)
                m_perfBadge->hide();
#endif
        }
    });
    connect(m_consoleTimer, &QTimer::timeout, this, &VideoOutputNode::logPerfLine);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_perfTimer, &QTimer::timeout, this, &VideoOutputNode::updatePerfBadge);
#endif

    m_label->installEventFilter(this);
}

VideoOutputNode::~VideoOutputNode()
{
    if (m_consoleTimer != nullptr)
        m_consoleTimer->stop();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (m_perfTimer != nullptr)
        m_perfTimer->stop();
#endif

    // GL blit window (both Qt5 + Qt6) — destroyed like the video widget.
    if (m_glWidget != nullptr) {
        m_glWidget->hide();
        m_glWidget->deleteLater();
        m_glWidget = nullptr;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (m_videoWidget != nullptr) {
        m_videoWidget->hide();
        m_videoWidget->deleteLater();
        m_videoWidget = nullptr;
    }
    // The badge is a top-level window (not a child of m_videoWidget), so it
    // must be closed explicitly to avoid a dangling overlay window.
    if (m_perfBadge != nullptr) {
        m_perfBadge->hide();
        m_perfBadge->deleteLater();
        m_perfBadge = nullptr;
    }
#endif
    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QJsonObject VideoOutputNode::save() const
{
    return QtNodes::NodeDelegateModel::save();
}

void VideoOutputNode::load(QJsonObject const &p)
{
    Q_UNUSED(p);
}

unsigned int VideoOutputNode::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
        // Port 0: "video-frame" (zero-copy GPU), port 1: "image" (software).
        return 2;
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType VideoOutputNode::dataType(PortType portType, PortIndex portIndex) const
{
    if (portType == PortType::In) {
        if (portIndex == 0)
            return VideoFrameData().type();
        return ImageData().type();
    }
    Q_UNUSED(portIndex);
    return ImageData().type();
}

std::shared_ptr<NodeData> VideoOutputNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void VideoOutputNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    if (portIndex == 0) {
        // --- Port 0: "video-frame" (zero-copy GPU display path) ---
        // "output.total" spans the whole port-0 frame processing (REQ-SW-PL-027).
        PERF_SCOPE("video", "output.total");

        auto videoFrame = std::dynamic_pointer_cast<VideoFrameData>(data);
        if (videoFrame && videoFrame->hasFrame()) {
            // CRITICAL GUARD: disconnecting the edge does NOT stop the source
            // player — frames keep flowing into setInData and would resurrect
            // the detached display popup. The connection flag (not widget
            // nullness) is the correct guard: it is cleared by
            // inputConnectionDeleted(), which also closes the popup.
            if (!m_videoInputConnected)
                return;

            // Perf markers for the badge (REQ-SW-PL-027): HW/SW path + pixel
            // format of the frame actually being presented. No-op while the
            // "video" domain is disabled.
            if (PERF_ENABLED("video")) {
                const QVideoFrame &frame = videoFrame->frame();
                m_lastHandleType = static_cast<int>(frame.handleType());
                m_lastPixelFormat = VideoCompat::pixelFormatInt(frame);
            }

            m_videoFrame = videoFrame;

            // Lazily create the detached display on the first frame.
            ensureVideoWidget();

            // Defensive: the widget may be null if the connection was just
            // removed while a frame was in flight.
            if (m_glWidget != nullptr) {
                PERF_SCOPE("video", "output.present");
                m_glWidget->presentFrame(videoFrame->frame());
            }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            else if (m_videoWidget != nullptr) {
                // GPU path: HW buffer → RHI texture → screen (no QImage copy).
                // "output.present" measures the blit (presentFrame).
                PERF_SCOPE("video", "output.present");
                VideoCompat::presentFrame(m_videoWidget->videoSink(),
                                          videoFrame->frame());
            }
#endif

            // Only do the expensive QImage conversion + ImageData output when a
            // downstream processing consumer is connected to the output port.
            // Otherwise the GPU path handles display in the detached window and
            // the in-node QLabel shows a static placeholder (no per-frame work).
            if (m_outputConnectionCount > 0) {
                const QImage image = VideoCompat::frameToImage(videoFrame->frame());
                // Propagate only real frames — never an ImageData wrapping a
                // null QImage (would emit a bogus "empty" data update).
                if (image.isNull())
                    return;
                m_image = image;
                updateDisplay();
                m_output = std::make_shared<ImageData>(image);
                Q_EMIT dataUpdated(0);
            }
        } else {
            m_videoFrame.reset();
            m_image = QImage();
            m_output.reset();
            updateDisplay();
            Q_EMIT dataInvalidated(0);
        }
        return;
    }

    if (portIndex == 1) {
        // --- Port 1: "image" (software / backward-compatible path) ---
        m_image = QImage();
        m_output.reset();

        auto imageData = std::dynamic_pointer_cast<ImageData>(data);
        if (imageData && !imageData->isEmpty()) {
            m_image = imageData->image();
            m_output = imageData;
            updateDisplay();
            Q_EMIT dataUpdated(0);
        } else {
            updateDisplay();
            Q_EMIT dataInvalidated(0);
        }
        return;
    }
}

void VideoOutputNode::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 0)
        ++m_outputConnectionCount;
}

void VideoOutputNode::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    if (conId.outPortIndex == 0 && m_outputConnectionCount > 0)
        --m_outputConnectionCount;
}

void VideoOutputNode::inputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    if (conId.inPortIndex == 0)
        m_videoInputConnected = true;
}

void VideoOutputNode::inputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    if (conId.inPortIndex != 0)
        return;

    // The port-0 "video-frame" edge was removed. Disconnecting does NOT stop
    // the source player — frames keep flowing into setInData(), so first clear
    // the connection flag that guards the port-0 branch, then close the
    // detached popup and reset all video state (mirror of the destructor).
    m_videoInputConnected = false;

    if (m_glWidget != nullptr) {
        m_glWidget->hide();
        m_glWidget->deleteLater();
        m_glWidget = nullptr;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (m_videoWidget != nullptr) {
        m_videoWidget->hide();
        m_videoWidget->deleteLater();
        m_videoWidget = nullptr;
    }
    // The badge is a top-level window (not a child of m_videoWidget), so close
    // it explicitly to avoid a dangling overlay window.
    if (m_perfBadge != nullptr) {
        m_perfBadge->hide();
        m_perfBadge->deleteLater();
        m_perfBadge = nullptr;
    }
#endif
    m_videoFrame.reset();
    m_image = QImage();
    m_output.reset();
    m_label->setText(tr("No video input"));
    updateDisplay();
}

QWidget *VideoOutputNode::embeddedWidget()
{
    return m_widget;
}

bool VideoOutputNode::eventFilter(QObject *object, QEvent *event)
{
    // In GL blit mode the detached GL window has its own size — label resizes
    // must not trigger per-frame re-presents.
    if (object == m_label && event->type() == QEvent::Resize && !glBlitEnabled())
        updateDisplay();
    return false;
}

void VideoOutputNode::updateDisplay()
{
    if (glBlitEnabled()) {
        // GL blit path: present the QImage (image port / RGB fallback)
        // on the detached GL window instead of QPixmap + smooth-scale.
        if (m_image.isNull()) {
            if (m_glWidget != nullptr)
                m_glWidget->hide();
            m_label->setPixmap(QPixmap());
            return;
        }
        ensureGlWidget();
        m_glWidget->presentImage(m_image);
        return;
    }

    if (m_image.isNull()) {
        m_label->setPixmap(QPixmap());
        return;
    }

    QSize targetSize = m_label->size();
    if (!targetSize.isValid() || targetSize.isEmpty())
        targetSize = m_label->minimumSize();

    const QPixmap pixmap = QPixmap::fromImage(m_image);
    m_label->setPixmap(pixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void VideoOutputNode::ensureGlWidget()
{
    if (m_glWidget != nullptr) {
        // Disconnecting an input edge only HIDES the GL window (the
        // updateDisplay() null-image branch) — it is not destroyed. Re-show it
        // here so a re-connect brings the detached display back instead of
        // leaving a hidden window behind (Qt5 image port / Qt6 image port).
        if (!m_glWidget->isVisible()) {
            m_glWidget->show();
            m_label->setText(tr("GPU display active — see detached window"));
        }
        return;
    }

    m_glWidget = new VideoGLBlitWidget();
    m_glWidget->setWindowTitle(tr("Video Output — %1 (GL blit)").arg(caption()));
    m_glWidget->resize(640, 480);
    m_glWidget->show();
    m_label->setText(tr("GPU display active — see detached window"));
}

void VideoOutputNode::logPerfLine()
{
    auto &domain = Daqster::Perf::Domain::get("video");
    if (!domain.enabled())
        return;

    // Sample self-CPU first: the first sample only establishes the baseline and
    // returns 0.0 (the "cpu=0.0%" on the very first line is expected).
    const double cpuPercent = m_cpu.sample();

    // Log only once there are actual frame records (count > 0).
    if (domain.count("output.total") <= 0
        && domain.count("source.frame_interval") <= 0) {
        return;
    }

    // Log at Info level WITHOUT a category (qInfo() instead of qCDebug(lcPerf)):
    // the "daqster.perf" category is disabled by default in LogManager, so a
    // qCDebug(lcPerf) line would be silently filtered and the [PERF] report
    // would never reach the console. qInfo() is unconditional (like the FFmpeg
    // [INF] lines) and guarantees the report is always visible when Perf is on.
    qInfo().noquote()
        << formatPerfLine(domain.avg("source.frame_interval"),
                          domain.avg("output.present"),
                          domain.avg("output.total"),
                          cpuPercent,
                          m_lastHandleType, m_lastPixelFormat);
}

void VideoOutputNode::ensureVideoWidget()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (m_videoWidget != nullptr || m_glWidget != nullptr)
        return;
#else
    if (m_glWidget != nullptr)
        return;
#endif

    // GL blit path (DAQSTER_GL_BLIT=1): use the experimental GL window instead
    // of the QVideoWidget (Qt6) / QPixmap path (Qt5) so the two display
    // backends can be A/B tested. On Qt5 the GL window is the only detached
    // display backend for the video-frame port.
    if (glBlitEnabled()) {
        ensureGlWidget();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        createPerfBadge();
#endif
        m_label->setText(tr("GPU display active — see detached window"));
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // QVideoWidget is a native QWindow with its own RHI swapchain and cannot
    // be hosted inside the node editor scene (QTBUG-35299). Detach it into a
    // separate top-level window on the first video-frame input.
    m_videoWidget = new QVideoWidget();
    m_videoWidget->setWindowTitle(tr("Video Output — %1").arg(caption()));
    m_videoWidget->resize(640, 480);
    m_videoWidget->show();

    createPerfBadge();

    // The in-node QLabel shows a static placeholder while the GPU path is
    // active — it is not updated per-frame (avoids redundant QImage conversion).
    m_label->setText(tr("GPU display active — see detached window"));
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void VideoOutputNode::createPerfBadge()
{
    if (m_perfBadge != nullptr)
        return;

    // Perf overlay badge (REQ-SW-PL-027): a separate top-level frameless tool
    // window (NOT a child of the detached QVideoWidget). QVideoWidget renders
    // the video in its own native layer (RHI swapchain) and does NOT composite
    // child widgets on top of it, so a child QLabel would stay invisible over
    // the video. A frameless, transparent-for-mouse, always-on-top tool window
    // that tracks the video window's position is the reliable approach.
    m_perfBadge = new QLabel(nullptr,
        Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    m_perfBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_perfBadge->setAttribute(Qt::WA_TranslucentBackground);
    m_perfBadge->setAttribute(Qt::WA_ShowWithoutActivating);
    m_perfBadge->setStyleSheet(
        QStringLiteral("background-color: rgba(0,0,0,140); color: #0f0; padding: 2px;"));
    m_perfBadge->adjustSize();
    m_perfBadge->hide();
    positionPerfBadge();
}

void VideoOutputNode::updatePerfBadge()
{
    if (m_perfBadge == nullptr)
        return;

    auto &domain = Daqster::Perf::Domain::get("video");
    if (!domain.enabled()) {
        m_perfBadge->hide();
        return;
    }

    m_perfBadge->setText(formatPerfBadge(
        domain.avg("source.frame_interval"),
        domain.avg("output.present"),
        domain.avg("output.total"),
        m_lastHandleType, m_lastPixelFormat));
    m_perfBadge->adjustSize();
    positionPerfBadge();
    m_perfBadge->raise();
    m_perfBadge->show();
}

void VideoOutputNode::positionPerfBadge()
{
    if (m_perfBadge == nullptr)
        return;

    QWidget *displayWindow = (m_glWidget != nullptr)
        ? static_cast<QWidget *>(m_glWidget)
        : static_cast<QWidget *>(m_videoWidget);
    if (displayWindow == nullptr)
        return;

    // The badge is a top-level window, so it must be positioned in global
    // coordinates. Pin it to the top-left corner of the video window (client
    // area origin mapped to global), offset by a few pixels.
    const QPoint topLeft = displayWindow->mapToGlobal(QPoint(0, 0)) + QPoint(4, 4);
    m_perfBadge->move(topLeft);
}
#endif
