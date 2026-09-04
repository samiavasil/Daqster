#pragma once

#include <QtGlobal>
#include <QtNodes/NodeDelegateModel>

#include <QImage>
#include <QtMultimedia/QVideoFrame>

#include "GL/TexturePool.h"
#include "GL/VideoGLContextManager.h"
#include "NodeDataTypes/VideoTextureHandle.h"

#include <QOpenGLFunctions>
#include <QSurfaceFormat>

#include <functional>

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
 * deleted in the destructor. VideoFrameData is meant to be shared via
 * std::shared_ptr, never copied — the copy ctor/assignment are deleted
 * (a by-value copy would either double-delete the GL textures or silently
 * drop them; the GpuRgba copies were a dead end).
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

    /// Frames are shared via std::shared_ptr — never copied by value.
    VideoFrameData(const VideoFrameData &) = delete;
    VideoFrameData &operator=(const VideoFrameData &) = delete;

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

    /// True when the payload is a GPU-resident RGBA texture (effect output,
    /// REQ-SW-PL-032 Stage 2B). The wrapped QVideoFrame is invalid then —
    /// consumers must use asTexture()/asImage() instead of frame().
    bool isGpuRgba() const { return m_residency == VideoFrameResidency::GpuRgba; }

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
                // Qt5: QVideoFrame::image() only wraps RGB formats — NV12 /
                // YUV420P return a null QImage. Real video frames on Qt5 are
                // NV12 owned copies, so convert the YUV planes manually
                // (REQ-SW-PL-039).
                m_imageCache = m_frame.image();
                if (m_imageCache.isNull())
                    m_imageCache = yuvToImage(m_frame);
#endif
            }
        }
        return m_imageCache;
    }

    /// Thread-safe CPU-only conversion of a QVideoFrame to QImage
    /// (REQ-SW-PL-039).
    ///
    /// Unified entry point for the ComputePool workers: converts the worker's
    /// OWN frame copy without touching GL/RHI. QVideoFrame::toImage() on Qt6
    /// can route through RHI/GPU conversion, which creates a GL context on the
    /// calling thread — forbidden off the GUI thread (QTBUG-131107); Qt5's
    /// QVideoFrame::image() returns a null QImage for NV12/YUV420P. This
    /// converter is pure CPU and safe from ANY thread (no GL, no RHI, no
    /// shared mutable state).
    ///
    /// Handles:
    ///   - NV12 / YUV420P via yuvToImage() (BT.601 limited-range, pure CPU)
    ///   - RGB formats (RGB32, ARGB32, RGB888, ...) by wrapping the mapped
    ///     bits directly — no pixel conversion; the image is deep-copied so it
    ///     owns its data and stays valid after the frame is unmapped/destroyed
    ///
    /// Returns a null QImage only for unsupported formats or a failed map.
    static QImage frameToImageCpu(const QVideoFrame &frame)
    {
        if (!frame.isValid())
            return QImage();

        // RGB formats: wrap the mapped bits directly (no pixel conversion).
        QImage::Format qfmt = QImage::Format_Invalid;
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        // Qt 6.8+ renamed the pixel formats and QVideoFrame(QImage) now maps
        // QImage::Format_RGB32 → Format_BGRX8888 / Format_ARGB32 →
        // Format_BGRA8888 (the memory layouts match: [B,G,R,X] / [B,G,R,A]).
        switch (frame.surfaceFormat().pixelFormat()) {
        case QVideoFrameFormat::Format_BGRX8888:
            qfmt = QImage::Format_RGB32;
            break;
        case QVideoFrameFormat::Format_BGRA8888:
            qfmt = QImage::Format_ARGB32;
            break;
        case QVideoFrameFormat::Format_BGRA8888_Premultiplied:
            qfmt = QImage::Format_ARGB32_Premultiplied;
            break;
        case QVideoFrameFormat::Format_RGBA8888:
            qfmt = QImage::Format_RGBA8888;
            break;
        case QVideoFrameFormat::Format_RGBX8888:
            qfmt = QImage::Format_RGBX8888;
            break;
        default:
            break;
        }
#elif QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        switch (frame.surfaceFormat().pixelFormat()) {
        case QVideoFrameFormat::Format_RGB32:
            qfmt = QImage::Format_RGB32;
            break;
        case QVideoFrameFormat::Format_ARGB32:
            qfmt = QImage::Format_ARGB32;
            break;
        case QVideoFrameFormat::Format_ARGB32_Premultiplied:
            qfmt = QImage::Format_ARGB32_Premultiplied;
            break;
        case QVideoFrameFormat::Format_RGB24:
            qfmt = QImage::Format_RGB888;
            break;
        case QVideoFrameFormat::Format_RGB565:
            qfmt = QImage::Format_RGB16;
            break;
        default:
            break;
        }
#else
        switch (frame.pixelFormat()) {
        case QVideoFrame::Format_RGB32:
            qfmt = QImage::Format_RGB32;
            break;
        case QVideoFrame::Format_ARGB32:
            qfmt = QImage::Format_ARGB32;
            break;
        case QVideoFrame::Format_ARGB32_Premultiplied:
            qfmt = QImage::Format_ARGB32_Premultiplied;
            break;
        case QVideoFrame::Format_RGB24:
            qfmt = QImage::Format_RGB888;
            break;
        case QVideoFrame::Format_RGB565:
            qfmt = QImage::Format_RGB16;
            break;
        default:
            break;
        }
#endif
        if (qfmt != QImage::Format_Invalid) {
            // map()/bits() are non-const in Qt5 — implicit-share local copy.
            QVideoFrame mappable = frame;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            if (!mappable.map(QVideoFrame::ReadOnly))
                return QImage();
#else
            if (!mappable.map(QAbstractVideoBuffer::ReadOnly))
                return QImage();
#endif
            const int w = mappable.width();
            const int h = mappable.height();
            if (w <= 0 || h <= 0) {
                mappable.unmap();
                return QImage();
            }
            // Wrap the mapped bits, then deep-copy so the returned image owns
            // its data (safe after the frame is unmapped/destroyed).
            const QImage wrapped(mappable.bits(0), w, h,
                                 mappable.bytesPerLine(0), qfmt);
            const QImage owned = wrapped.copy();
            mappable.unmap();
            return owned;
        }

        // NV12 / YUV420P: pure-CPU BT.601 conversion.
        return yuvToImage(frame);
    }

    /// Pure-CPU BT.601 YUV→RGB conversion for NV12 / YUV420P frames.
    ///
    /// Qt5's QVideoFrame::image() only wraps RGB formats — NV12/YUV420P return
    /// a null QImage; Qt6's QVideoFrame::toImage() may route through RHI/GPU
    /// conversion (forbidden off the GUI thread, QTBUG-131107). This converter
    /// maps the planes and converts on the CPU, so it is safe to call from ANY
    /// thread (no GL, no RHI, no shared mutable state).
    ///
    /// Returns a null QImage for unsupported formats or a failed map.
    static QImage yuvToImage(const QVideoFrame &frame)
    {
        if (!frame.isValid())
            return QImage();

        const bool nv12 = isNv12(frame);
        const bool yuv420p = isYuv420p(frame);
        if (!nv12 && !yuv420p)
            return QImage();

        const int w = frame.width();
        const int h = frame.height();
        if (w <= 0 || h <= 0)
            return QImage();

        // map()/bits() are non-const in Qt5 — implicit-share local copy (the
        // owned frame itself is never modified).
        QVideoFrame mappable = frame;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        if (!mappable.map(QVideoFrame::ReadOnly))
            return QImage();
#else
        if (!mappable.map(QAbstractVideoBuffer::ReadOnly))
            return QImage();
#endif

        const uchar *yPlane = mappable.bits(0);
        const uchar *uPlane = mappable.bits(1);
        const uchar *vPlane = nv12 ? nullptr : mappable.bits(2);
        const int yStride = mappable.bytesPerLine(0);
        const int uStride = mappable.bytesPerLine(1);
        const int vStride = nv12 ? 0 : mappable.bytesPerLine(2);
        const int chromaW = (w + 1) / 2;
        const int chromaH = (h + 1) / 2;

        QImage img(w, h, QImage::Format_RGB32);
        for (int row = 0; row < h; ++row) {
            QRgb *dst = reinterpret_cast<QRgb *>(img.scanLine(row));
            const uchar *yRow = yPlane + row * yStride;
            const int chromaRow = row / 2;
            const uchar *uvRow = nv12 ? (uPlane + chromaRow * uStride) : nullptr;
            for (int col = 0; col < w; ++col) {
                const int y = yRow[col];
                const int chromaCol = col / 2;
                int u, v;
                if (nv12) {
                    u = uvRow[chromaCol * 2];
                    v = uvRow[chromaCol * 2 + 1];
                } else {
                    u = uPlane[chromaRow * uStride + chromaCol];
                    v = vPlane[chromaRow * vStride + chromaCol];
                }
                // BT.601 limited-range YUV → RGB.
                const int c = y - 16;
                const int d = u - 128;
                const int e = v - 128;
                const int r = qBound(0, (298 * c + 409 * e + 128) >> 8, 255);
                const int g = qBound(0, (298 * c - 100 * d - 208 * e + 128) >> 8, 255);
                const int b = qBound(0, (298 * c + 516 * d + 128) >> 8, 255);
                dst[col] = qRgb(r, g, b);
            }
        }
        mappable.unmap();
        return img;
    }

    /// Lazy GPU texture representation (REQ-SW-PL-032 AC 1/5).
    ///
    /// Uploads the Y/U/V planes of the wrapped frame into the shared GL
    /// context (VideoGLContextManager) at most once per frame and caches the
    /// texture ids, so N GPU consumers share a single upload. The cache is
    /// invalidated by setFrame().
    ///
    /// The textures are acquired from the global TexturePool (REQ-SW-PL-038)
    /// and marked pooled — releaseTextures() returns them to the pool instead
    /// of deleting them, so the display path no longer does a
    /// glGenTextures/glDeleteTextures pair per frame.
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

        // Textures come from the global TexturePool (REQ-SW-PL-038) instead of
        // a glGenTextures per frame — the same pool the effect processors use,
        // so every video texture in the process is reused across frames. The
        // handle is marked pooled so releaseTextures() returns the textures to
        // the pool instead of deleting them. acquire() returns 0 on GL failure
        // — release the partial set and fall back to the CPU path.
        TexturePool &pool = TexturePool::instance();
        GLuint texY = pool.acquire(w, h);
        if (texY == 0)
            return false;
        GLuint texUV = 0, texU = 0, texV = 0;
        if (nv12) {
            texUV = pool.acquire(chromaW, chromaH);
            if (texUV == 0) {
                pool.release(texY);
                return false;
            }
        } else {
            texU = pool.acquire(chromaW, chromaH);
            texV = pool.acquire(chromaW, chromaH);
            if (texU == 0 || texV == 0) {
                if (texU != 0)
                    pool.release(texU);
                if (texV != 0)
                    pool.release(texV);
                pool.release(texY);
                return false;
            }
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

        m_textureCache = VideoTextureHandle{texY, texUV, texU, texV, w, h, nv12, false, true};
        m_textureValid = true;
        m_residency = VideoFrameResidency::GpuYuv;
        if (out != nullptr)
            *out = m_textureCache;
        return true;
    }

    /// Wraps a GPU-resident RGBA texture (effect output) as a frame. The
    /// handle's textures are owned by the returned VideoFrameData and deleted
    /// on destruction. m_frame stays invalid; m_residency = GpuRgba.
    ///
    /// When releaseCallback is provided (texture-pool path, REQ-SW-PL-032
    /// Issue #7), the callback is invoked instead of glDeleteTextures when the
    /// frame is destroyed — it returns the texture to the pool. With the
    /// global pool (REQ-SW-PL-038) the callback calls TexturePool::instance().
    /// release() — the singleton is intentionally leaked, so no capture is
    /// needed to keep the pool alive.
    ///
    /// The handle is always marked pooled (REQ-SW-PL-038): effect output
    /// textures come from the global TexturePool, so even without a release
    /// callback releaseTextures() returns the texture to the pool instead of
    /// deleting it (a pooled texture must never be glDeleteTextures'd — the
    /// pool still owns it).
    static std::shared_ptr<VideoFrameData> fromTexture(
        const VideoTextureHandle &h,
        std::function<void()> releaseCallback = {})
    {
        auto data = std::make_shared<VideoFrameData>();
        data->m_textureCache = h;
        data->m_textureCache.rgba = true;
        data->m_textureCache.pooled = true;
        data->m_textureValid = true;
        data->m_residency = VideoFrameResidency::GpuRgba;
        data->m_releaseCallback = std::move(releaseCallback);
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

    /// Deletes the owned GL textures (if any) in the shared context — or, for
    /// pooled textures (REQ-SW-PL-032 Issue #7 / REQ-SW-PL-038), returns them
    /// to the pool: the release callback (effect outputs) or the global pool
    /// directly (asTexture uploads, pooled flag on the handle).
    void releaseTextures()
    {
        if (!m_textureValid)
            return;
        if (m_releaseCallback) {
            // Pooled texture: return it to the pool instead of deleting. Move
            // the callback out first so it fires exactly once even if the
            // pool's release() re-enters (it cannot — release() is a plain
            // list push, but the guard is free).
            auto cb = std::move(m_releaseCallback);
            m_releaseCallback = nullptr;
            cb();
        } else if (m_textureCache.pooled) {
            // Pooled textures (REQ-SW-PL-038): return them to the global pool
            // for reuse instead of deleting. release() ignores 0 ids, unknown
            // ids and double-releases, so this is safe for any handle state.
            TexturePool &pool = TexturePool::instance();
            pool.release(m_textureCache.texY);
            pool.release(m_textureCache.texUV);
            pool.release(m_textureCache.texU);
            pool.release(m_textureCache.texV);
        } else {
            VideoGLContextManager &mgr = VideoGLContextManager::instance();
            mgr.deleteTexture(m_textureCache.texY);
            mgr.deleteTexture(m_textureCache.texUV);
            mgr.deleteTexture(m_textureCache.texU);
            mgr.deleteTexture(m_textureCache.texV);
        }
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
    /// Optional release callback for pooled textures (REQ-SW-PL-032 Issue #7):
    /// invoked once in releaseTextures() instead of glDeleteTextures. With the
    /// global pool (REQ-SW-PL-038) the callback calls TexturePool::instance().
    /// release() — the singleton is intentionally leaked, so no capture is
    /// needed to keep the pool alive.
    std::function<void()> m_releaseCallback;
};