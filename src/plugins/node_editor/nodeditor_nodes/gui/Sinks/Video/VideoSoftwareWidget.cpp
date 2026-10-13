#include "VideoSoftwareWidget.h"

#include "NodeDataTypes/VideoFrameData.h"

#include <QPainter>
#include <QPaintEvent>

VideoSoftwareWidget::VideoSoftwareWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
}

VideoSoftwareWidget::~VideoSoftwareWidget() = default;

void VideoSoftwareWidget::presentFrame(const QVideoFrame &frame)
{
    m_image = VideoFrameData::frameToImageCpu(frame);
    if (m_image.isNull()) {
        // Keep the placeholder — the frame could not be converted.
        m_hasImage = false;
        update();
        return;
    }
    m_hasImage = true;
    setVideoSize(m_image.size());
    update();
}

void VideoSoftwareWidget::presentImage(const QImage &image)
{
    if (image.isNull())
        return;
    m_image = image;
    m_hasImage = true;
    setVideoSize(m_image.size());
    update();
}

void VideoSoftwareWidget::presentTexture(const VideoTextureHandle &handle,
                                         std::shared_ptr<VideoFrameData> owner)
{
    Q_UNUSED(handle);
    if (!owner)
        return;
    // Software backend: readback the GPU-resident texture via the owner's
    // lazy QImage cache (REQ-SW-PL-032) — at most one conversion per frame.
    m_image = owner->asImage();
    if (m_image.isNull())
        return;
    m_hasImage = true;
    setVideoSize(m_image.size());
    update();
}

void VideoSoftwareWidget::presentYuvTexture(const VideoTextureHandle &handle,
                                            std::shared_ptr<VideoFrameData> owner)
{
    Q_UNUSED(handle);
    if (!owner)
        return;
    // Software backend: readback the cached YUV textures via the owner's lazy
    // QImage cache (REQ-SW-PL-032) — at most one conversion per frame.
    m_image = owner->asImage();
    if (m_image.isNull())
        return;
    m_hasImage = true;
    setVideoSize(m_image.size());
    update();
}

void VideoSoftwareWidget::setVideoSize(const QSize &size)
{
    m_videoSize = size;
}

void VideoSoftwareWidget::clear()
{
    m_hasImage = false;
    m_image = QImage();
    update();
}

void VideoSoftwareWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (m_hasImage && !m_image.isNull()) {
        // Keep-aspect-ratio render, centered (letterbox / pillarbox).
        const QImage scaled = m_image.scaled(rect().size(), Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation);
        const int x = (rect().width() - scaled.width()) / 2;
        const int y = (rect().height() - scaled.height()) / 2;
        painter.drawImage(QPoint(x, y), scaled);
    } else {
        painter.setPen(QColor(Qt::gray));
        painter.drawText(rect(), Qt::AlignCenter, tr("No video input"));
    }
}