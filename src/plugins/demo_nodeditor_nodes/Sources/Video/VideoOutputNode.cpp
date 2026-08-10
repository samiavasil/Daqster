#include "VideoOutputNode.h"

#include "NodeDataTypes/ImageData.h"
#include "NodeDataTypes/VideoFrameData.h"
#include "VideoCompat.h"

#include <QEvent>
#include <QLabel>
#include <QPixmap>
#include <QSize>
#include <QVBoxLayout>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QVideoWidget>
#endif

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

VideoOutputNode::VideoOutputNode()
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    : m_videoFrame(std::make_shared<VideoFrameData>())
#endif
{
    m_widget = new QWidget();
    auto *layout = new QVBoxLayout(m_widget);
    layout->setContentsMargins(4, 4, 4, 4);

    m_label = new QLabel(m_widget);
    m_label->setMinimumSize(320, 240);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setText(tr("No video input"));
    m_label->setStyleSheet(QStringLiteral("background-color: black; color: gray;"));
    layout->addWidget(m_label);

    m_label->installEventFilter(this);
}

VideoOutputNode::~VideoOutputNode()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (m_videoWidget != nullptr) {
        m_videoWidget->hide();
        m_videoWidget->deleteLater();
        m_videoWidget = nullptr;
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        // Port 0: "video-frame" (zero-copy GPU), port 1: "image" (software).
        return 2;
#else
        return 1;
#endif
    case PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType VideoOutputNode::dataType(PortType portType, PortIndex portIndex) const
{
    if (portType == PortType::In) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (portIndex == 0)
            return VideoFrameData().type();
        return ImageData().type();
#else
        Q_UNUSED(portIndex);
        return ImageData().type();
#endif
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (portIndex == 0) {
        // --- Port 0: "video-frame" (zero-copy GPU display path) ---
        auto videoFrame = std::dynamic_pointer_cast<VideoFrameData>(data);
        if (videoFrame && videoFrame->hasFrame()) {
            m_videoFrame = videoFrame;

            // Lazily create the detached QVideoWidget on the first frame.
            ensureVideoWidget();

            // GPU path: HW buffer → RHI texture → screen (no QImage copy).
            VideoCompat::presentFrame(m_videoWidget->videoSink(),
                                      videoFrame->frame());

            // Only do the expensive QImage conversion + ImageData output when a
            // downstream processing consumer is connected to the output port.
            // Otherwise the GPU path handles display in the detached window and
            // the in-node QLabel shows a static placeholder (no per-frame work).
            if (m_outputConnectionCount > 0) {
                const QImage image = VideoCompat::frameToImage(videoFrame->frame());
                if (!image.isNull()) {
                    m_image = image;
                    updateDisplay();
                }
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
#else
    Q_UNUSED(portIndex);

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
#endif
}

void VideoOutputNode::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (conId.outPortIndex == 0)
        ++m_outputConnectionCount;
#else
    Q_UNUSED(conId);
#endif
}

void VideoOutputNode::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (conId.outPortIndex == 0 && m_outputConnectionCount > 0)
        --m_outputConnectionCount;
#else
    Q_UNUSED(conId);
#endif
}

QWidget *VideoOutputNode::embeddedWidget()
{
    return m_widget;
}

bool VideoOutputNode::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_label && event->type() == QEvent::Resize)
        updateDisplay();
    return false;
}

void VideoOutputNode::updateDisplay()
{
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void VideoOutputNode::ensureVideoWidget()
{
    if (m_videoWidget != nullptr)
        return;

    // QVideoWidget is a native QWindow with its own RHI swapchain and cannot
    // be hosted inside the node editor scene (QTBUG-35299). Detach it into a
    // separate top-level window on the first video-frame input.
    m_videoWidget = new QVideoWidget();
    m_videoWidget->setWindowTitle(tr("Video Output — %1").arg(caption()));
    m_videoWidget->resize(640, 480);
    m_videoWidget->show();

    // The in-node QLabel shows a static placeholder while the GPU path is
    // active — it is not updated per-frame (avoids redundant QImage conversion).
    m_label->setText(tr("GPU display active — see detached window"));
}
#endif
