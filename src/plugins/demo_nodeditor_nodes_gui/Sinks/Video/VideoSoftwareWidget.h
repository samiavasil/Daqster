#pragma once

#include "VideoDisplayWidget.h"

#include <QImage>
#include <QPaintEvent>
#include <QWidget>

class VideoSoftwareWidget : public QWidget, public VideoDisplayWidget
{
    Q_OBJECT

public:
    explicit VideoSoftwareWidget(QWidget *parent = nullptr);
    ~VideoSoftwareWidget() override;

    void presentFrame(const QVideoFrame &frame) override;
    void presentImage(const QImage &image) override;
    void presentTexture(const VideoTextureHandle &handle,
                        std::shared_ptr<VideoFrameData> owner) override;
    void presentYuvTexture(const VideoTextureHandle &handle,
                           std::shared_ptr<VideoFrameData> owner) override;
    void setVideoSize(const QSize &size) override;
    void clear() override;
    QString backendName() const override { return QStringLiteral("Software"); }
    bool isGpuBackend() const override { return false; }
    QWidget *widget() override { return this; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_image;      // current frame (RGB)
    bool m_hasImage = false;
};