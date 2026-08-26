#pragma once

#include <QtGlobal>
#include <QtNodes/NodeDelegateModel>

#include <QImage>
#include <QtMultimedia/QVideoFrame>

#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoTextureHandle.h"

#include <QOpenGLFunctions>
#include <QSurfaceFormat>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtMultimedia/QVideoFrameFormat>
#else
#include <QtMultimedia/QAbstractVideoBuffer>
#endif

/**
 * @brief Zero-copy video frame node data type.
 *
 * Wraps a QVideoFrame and transports it through the graph as
 * std::shared_ptr<NodeData> — only the reference count is bumped, the
 * decoded pixel buffer is never copied. QVideoFrame is implicitly shared
 * (QSharedData), so holding and passing it is safe.
 *
 * On Qt6 the wrapped frame is the decoded probe frame (ref-count bump only).
 * On Qt5 probe frames are NOT safe to hold beyond the signal (the backend
 * recycles the buffers), so sources wrap an OWNED copy instead: the planes
 * are memcpy'd into QByteArray storage via VideoCompat::frameToOwnedFrame()
 * (NV12 / YUV420P; unsupported formats stay on the QImage path). The
 * transport through the graph is then zero-copy on both versions — the copy
 * happens exactly once in the source (REQ-SW-PL-020, NV12-direct for Qt5).
 *
 * GPU-resident transport (REQ-SW-PL-032, Stage 2A): a frame can also live on
 * the GPU as GL textures in the shared context (VideoGLContextManager).
 * asTexture() lazily uploads the CPU frame once and caches the handle;
 * fromTexture() wraps an already GPU-resident RGBA texture (effect output).
 * The texture cache is invalidated by setFrame() and the owned textures are
 * deleted in the destructor. Copies do NOT take texture ownership (the copy
 * re-uploads on demand) — VideoFrameData is meant to be shared, not copied.
 */
class VideoFrameData : public QtNodes::NodeData
{
public:
    VideoFrameData() = default;

    explicit VideoFrameData(const QVideoFrame &frame) : m_frame(frame) {}

    ~VideoFrameData() override
    {
        releaseTextures();
    }

    /// Copies share the frame payload but NOT the GL texture cache: the copy
    /// does not own the textures (the original deletes them), so a copied
    /// frame re-uploads on the next asTexture() call.
    VideoFrameData(const VideoFrameData &other)
        : QtNodes::NodeData(other)
        , m_frame(other.m_frame)
        , m_imageCache(other.m_imageCache)
        , m_residency(other.m_residency)
    {
    }

    VideoFrameData &operator=(const VideoFrameData &other)
    {
        if (this != &other) {
            releaseTextures();
            QtNodes::NodeData::operator=(other);
            m_frame = other.m_frame;
            m_imageCache = other.m_imageCache;
            m_residency = other.m_residency;
        }
        return *this;
    }

    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType {"video-frame", "Video Frame"};
    }

    /// The wrapped frame. Valid only when the payload is CPU-resident
    /// (hasFrame() && !isGpuResident()).
    const QVideoFrame &frame() const { return m_frame; }

    /// Replaces the wrapped frame (ref-count bump only — zero-copy).
    /// Invalidates the lazy QImage and GL texture caches (REQ-SW-PL-032).
    void setFrame(const QVideoFrame &frame)
    {
        releaseTextures();
        m_frame = frame;
        m_residency = VideoFrameResidency::Cpu;
        m_imageCache = QImage();
    }

    bool hasFrame() const { return m_frame.isValid() || m_textureValid; }

    /// True when the payload lives on the GPU (GpuYuv / GpuRgba).
    bool isGpuResident() const { return m_residency != VideoFrameResidency::Cpu; }

    /// Lazy CPU QImage representation (REQ-SW-PL-032 AC 1/2/3).
    ///
    /// Converts the wrapped frame to a QImage at most once per frame and
    /// caches the result, so N consumers sharing the same VideoFrameData
    /// share a single conversion ("at most one copy per frame representation,
    /// lazy and shared"). The cache is invalidated by setFrame().
    ///
    /// For a GPU-resident RGBA frame the conversion is a readback at the
    /// domain boundary: the RGBA texture is bound to an FBO and read with
    /// glReadPixels (GPU→CPU copy is unavoidable when a CPU consumer needs
    /// the pixels).
    ///
    /// GUI-thread only: the whole node processing graph runs on the GUI
    /// thread, so no mutex is needed around the mutable cache.
    const QImage &asImage() const
    {
        if (m_imageCache.isNull()) {
            if (m_residency == VideoFrameResidency::GpuRgba && m_textureValid) {
                m_imageCache = readbackTexture(m_textureCache);
            } else if (m_frame.isValid()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                m_imageCache = m_frame.toImage();
#else
                m_imageCache = m_frame.image();
#endif
            }
        }
        return m_imageCache;
    }

    /// Lazy GPU texture representation (REQ-SW-PL-032 AC 1/5).
    ///
    /// Uploads the Y/U/V planes of the wrapped frame into the shared GL
    /// context (VideoGLContextManager) at most once per frame and caches the
    /// texture ids, so N GPU consumers share a single upload. The cache is
    /// invalidated by setFrame().
    ///
    /// Returns false (and leaves *out untouched) for non NV12/YUV420P frames,
    /// missing GL support, or a failed map/upload — the caller falls back to
    /// the CPU representation.
    bool asTexture(VideoTextureHandle *out) const
    {
        if (m_textureValid) {
            if (out != nullptr)
                *out = m_textureCache;
            return true;
        }
        if (m_residency == VideoFrameResidency::GpuRgba) {
            // Already GPU-resident: the handle IS the payload, nothing to upload.
            if (out != nullptr)
                *out = m_textureCache;
            return m_textureValid;
        }
        if (!m_frame.isValid())
            return false;

        const bool nv12 = isNv12(m_frame);
        const bool yuv420p = isYuv420p(m_frame);
        if (!nv12 && !yuv420p)
            return false;

        VideoGLContextManager &mgr = VideoGLContextManager::instance();
        if (!mgr.makeCurrent())
            return false;
        VideoGLContextManager::CurrentGuard guard(mgr);

        QOpenGLFunctions *f = mgr.context()->functions();

        // Core vs compatibility profile — same detection as
        // VideoEffectGLProcessor / VideoGLBlitWidget.
        const bool forceCore = qEnvironmentVariableIntValue("DAQSTER_GL_FORCE_CORE") == 1;
        const bool core = forceCore
            || (mgr.context()->format().profile() == QSurfaceFormat::CoreProfile);
        const GLenum yInternal = core ? GL_R8 : GL_LUMINANCE;
        const GLenum yFormat = core ? GL_RED : GL_LUMINANCE;
        const GLenum uvInternal = core ? GL_RG8 : GL_LUMINANCE_ALPHA;
        const GLenum uvFormat = core ? GL_RG : GL_LUMINANCE_ALPHA;

        // Qt5: map()/bits() are non-const — implicit-share local copy (the
        // probe frame itself is never modified).
        QVideoFrame mappable = m_frame;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const bool mapped = mappable.map(QVideoFrame::ReadOnly);
#else
        const bool mapped = mappable.map(QAbstractVideoBuffer::ReadOnly);
#endif
        if (!mapped)
            return false;

        const int w = mappable.width();
        const int h = mappable.height();
        const int chromaW = (w + 1) / 2;
        const int chromaH = (h + 1) / 2;

        GLuint texY = 0, texUV = 0, texU = 0, texV = 0;
        f->glGenTextures(1, &texY);
        if (nv12)
            f->glGenTextures(1, &texUV);
        else {
            f->glGenTextures(1, &texU);
            f->glGenTextures(1, &texV);
        }

        f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        f->glBindTexture(GL_TEXTURE_2D, texY);
        setupTextureParams(f, texY);
        f->glPixelStorei(GL_UNPACK_ROW_LENGTH, mappable.bytesPerLine(0));
        f->glTexImage2D(GL_TEXTURE_2D, 0, yInternal, w, h, 0,
                        yFormat, GL_UNSIGNED_BYTE, mappable.bits(0));

        if (nv12) {
            f->glBindTexture(GL_TEXTURE_2D, texUV);
            setupTextureParams(f, texUV);
            f->glPixelStorei(GL_UNPACK_ROW_LENGTH, mappable.bytesPerLine(1) / 2);
            f->glTexImage2D(GL_TEXTURE_2D, 0, uvInternal, chromaW, chromaH, 0,
                            uvFormat, GL_UNSIGNED_BYTE, mappable.bits(1));
        } else {
            f->glBindTexture(GL_TEXTURE_2D, texU);
            setupTextureParams(f, texU);
            f->glPixelStorei(GL_UNPACK_ROW_LENGTH, mappable.bytesPerLine(1));
            f->glTexImage2D(GL_TEXTURE_2D, 0, yInternal, chromaW, chromaH, 0,
                            yFormat, GL_UNSIGNED_BYTE, mappable.bits(1));
            f->glBindTexture(GL_TEXTURE_2D, texV);
            setupTextureParams(f, texV);
            f->glPixelStorei(GL_UNPACK_ROW_LENGTH, mappable.bytesPerLine(2));
            f->glTexImage2D(GL_TEXTURE_2D, 0, yInternal, chromaW, chromaH, 0,
                            yFormat, GL_UNSIGNED_BYTE, mappable.bits(2));
        }
        f->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        mappable.unmap();

        m_textureCache = VideoTextureHandle{texY, texUV, texU, texV, w, h, nv12, false};
        m_textureValid = true;
        m_residency = VideoFrameResidency::GpuYuv;
        if (out != nullptr)
            *out = m_textureCache;
        return true;
    }

    /// Wraps a GPU-resident RGBA texture (effect output) as a frame. The
    /// handle's textures are owned by the returned VideoFrameData and deleted
    /// on destruction. m_frame stays invalid; m_residency = GpuRgba.
    static std::shared_ptr<VideoFrameData> fromTexture(const VideoTextureHandle &h)
    {
        auto data = std::make_shared<VideoFrameData>();
        data->m_textureCache = h;
        data->m_textureCache.rgba = true;
        data->m_textureValid = true;
        data->m_residency = VideoFrameResidency::GpuRgba;
        return data;
    }

private:
    static bool isNv12(const QVideoFrame &frame)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return frame.surfaceFormat().pixelFormat() == QVideoFrameFormat::Format_NV12;
#else
        return frame.pixelFormat() == QVideoFrame::Format_NV12;
#endif
    }

    static bool isYuv420p(const QVideoFrame &frame)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        return frame.surfaceFormat().pixelFormat() == QVideoFrameFormat::Format_YUV420P;
#else
        return frame.pixelFormat() == QVideoFrame::Format_YUV420P;
#endif
    }

    static void setupTextureParams(QOpenGLFunctions *f, GLuint id)
    {
        f->glBindTexture(GL_TEXTURE_2D, id);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    /// GPU→CPU readback of an RGBA texture at the domain boundary: bind the
    /// texture to an FBO and glReadPixels into a top-down QImage.
    static QImage readbackTexture(const VideoTextureHandle &h)
    {
        if (h.texY == 0 || h.width <= 0 || h.height <= 0)
            return QImage();
        VideoGLContextManager &mgr = VideoGLContextManager::instance();
        if (!mgr.makeCurrent())
            return QImage();
        VideoGLContextManager::CurrentGuard guard(mgr);
        QOpenGLFunctions *f = mgr.context()->functions();

        GLuint fbo = 0;
        f->glGenFramebuffers(1, &fbo);
        f->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_TEXTURE_2D, h.texY, 0);
        const GLenum status = f->glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            f->glBindFramebuffer(GL_FRAMEBUFFER, 0);
            f->glDeleteFramebuffers(1, &fbo);
            return QImage();
        }
        QImage img(h.width, h.height, QImage::Format_RGBA8888);
        f->glReadPixels(0, 0, h.width, h.height, GL_RGBA, GL_UNSIGNED_BYTE, img.bits());
        f->glBindFramebuffer(GL_FRAMEBUFFER, 0);
        f->glDeleteFramebuffers(1, &fbo);
        // GL readback is bottom-up; QImage is top-down — flip vertically.
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
        return img.flipped(Qt::Vertical);
#else
        return img.mirrored(false, true);
#endif
    }

    /// Deletes the owned GL textures (if any) in the shared context.
    void releaseTextures()
    {
        if (!m_textureValid)
            return;
        VideoGLContextManager &mgr = VideoGLContextManager::instance();
        mgr.deleteTexture(m_textureCache.texY);
        mgr.deleteTexture(m_textureCache.texUV);
        mgr.deleteTexture(m_textureCache.texU);
        mgr.deleteTexture(m_textureCache.texV);
        m_textureValid = false;
        m_textureCache = VideoTextureHandle();
    }

    QVideoFrame m_frame;
    /// Lazy QImage cache — populated on first asImage() call, cleared on
    /// setFrame(). Mutable because asImage() is const (cache is a
    /// representation detail, not part of the frame identity).
    mutable QImage m_imageCache;
    /// Lazy GL texture cache — populated on first asTexture() call, cleared
    /// on setFrame(). Mutable because asTexture() is const.
    mutable VideoTextureHandle m_textureCache;
    mutable bool m_textureValid = false;
    /// Mutable: asTexture() promotes a CPU frame to GpuYuv on first upload.
    mutable VideoFrameResidency m_residency = VideoFrameResidency::Cpu;
};