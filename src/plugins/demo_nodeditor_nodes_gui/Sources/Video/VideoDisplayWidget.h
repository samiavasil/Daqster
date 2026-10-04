#pragma once

#include <QImage>
#include <QSize>
#include <QString>

#include <QtMultimedia/QVideoFrame>
#include <memory>

#include "NodeDataTypes/VideoTextureHandle.h"

class VideoFrameData;
class QWidget;

/// Abstract video display surface. Both backends (GL blit, software) are
/// QWidget subclasses so they are layout-friendly and work embedded (node
/// scene / runtime workspace) and detached (floating window via the nodeeditor
/// deembed mechanism on the parent node widget).
///
/// NOTE: this is a PURE INTERFACE (not a QWidget base). The GL backend must
/// remain a QOpenGLWidget (zero-copy GPU presentation, REQ-SW-PL-053 AC 1) and
/// Qt forbids inheriting from two QObject-derived classes — so the interface
/// itself cannot derive from QWidget. Each backend combines the interface with
/// its own widget base:
///   VideoGLBlitWidget   : public QOpenGLWidget, public VideoDisplayWidget
///   VideoSoftwareWidget : public QWidget,       public VideoDisplayWidget
class VideoDisplayWidget
{
public:
    virtual ~VideoDisplayWidget() = default;

    /// Present a CPU QVideoFrame (NV12/YUV420P/RGB). Zero-copy on the GL
    /// backend (upload in paintGL); the software backend converts to QImage.
    virtual void presentFrame(const QVideoFrame &frame) = 0;

    /// Present a QImage directly (RGB fallback / image port).
    virtual void presentImage(const QImage &image) = 0;

    /// Present a GPU-resident RGBA texture (effect output) — zero-copy on GL.
    /// The software backend falls back to owner->asImage() (readback).
    virtual void presentTexture(const VideoTextureHandle &handle,
                                std::shared_ptr<VideoFrameData> owner) = 0;

    /// Present GPU-resident YUV textures (asTexture cache) — zero-copy on GL.
    /// The software backend falls back to owner->asImage() (readback).
    virtual void presentYuvTexture(const VideoTextureHandle &handle,
                                   std::shared_ptr<VideoFrameData> owner) = 0;

    /// Set the source video size (for aspect-ratio letterboxing).
    virtual void setVideoSize(const QSize &size) = 0;
    QSize videoSize() const { return m_videoSize; }

    /// Clear the display back to the placeholder ("No video input").
    virtual void clear() = 0;

    /// Backend introspection (for the perf badge / diagnostics).
    virtual QString backendName() const = 0;
    virtual bool isGpuBackend() const = 0;

    /// The concrete QWidget of this backend (each backend combines the
    /// interface with its own widget base). Lets the node embed the display in
    /// a layout without knowing the concrete backend type.
    virtual QWidget *widget() = 0;

protected:
    QSize m_videoSize;
};