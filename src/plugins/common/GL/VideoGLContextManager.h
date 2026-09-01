#pragma once

// SPDX-License-Identifier: MIT
//
// Shared GL context manager (REQ-SW-PL-032, Stage 2A — GPU-resident transport).
//
// A single process-wide QOpenGLContext + QOffscreenSurface that all GPU video
// consumers (VideoEffectGLProcessor, later VideoGLBlitWidget / VideoOutputNode)
// share, so textures uploaded once by VideoFrameData::asTexture() are visible
// to every consumer without re-uploading (fan-out: at most one upload per
// frame representation, shared).
//
// Sharing: the context is created with
// setShareContext(QOpenGLContext::globalShareContext()) BEFORE create(). The
// application sets Qt::AA_ShareOpenGLContexts (src/apps/Daqster/main.cpp), so
// QOpenGLWidget contexts (VideoGLBlitWidget display) live in the same share
// group and can consume the textures created here.
//
// Threading: the whole node processing graph runs on the GUI thread, so no
// mutex guards the context. GUI-thread only.
//
// Lifetime: the singleton is intentionally leaked (function-local static
// pointer, never deleted) to avoid static destruction-order problems with
// QOpenGLContext / QGuiApplication teardown.

#include <QtGui/qopengl.h>

#include <QDebug>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

class VideoGLContextManager
{
public:
    /// Process-wide singleton. Never destroyed (intentional leak).
    static VideoGLContextManager &instance();

    /// The shared context (null when creation failed).
    QOpenGLContext *context() const { return m_context; }

    /// Makes the shared context current on the offscreen surface. Returns
    /// false when the context/surface is unavailable or makeCurrent fails.
    bool makeCurrent();

    /// Releases the shared context from the current thread.
    void doneCurrent();

    /// True when a hardware GL renderer is available (not llvmpipe / softpipe /
    /// SwiftShader). Lazy — the detection runs once and the result is cached
    /// for the process lifetime. Moved from VideoEffectGLProcessor.
    static bool hasHardwareGL();

    /// Deletes a texture owned by this share group. Safe to call with 0.
    void deleteTexture(GLuint id);

    /// RAII guard: calls doneCurrent() on destruction. Use after a successful
    /// makeCurrent() so every exit path releases the context.
    class CurrentGuard
    {
    public:
        explicit CurrentGuard(VideoGLContextManager &mgr) : m_mgr(&mgr) {}
        ~CurrentGuard()
        {
            if (m_mgr != nullptr)
                m_mgr->doneCurrent();
        }
        CurrentGuard(const CurrentGuard &) = delete;
        CurrentGuard &operator=(const CurrentGuard &) = delete;

    private:
        VideoGLContextManager *m_mgr;
    };

private:
    VideoGLContextManager();
    ~VideoGLContextManager() = default;

    QOpenGLContext *m_context = nullptr;
    QOffscreenSurface *m_surface = nullptr;
};

inline VideoGLContextManager &VideoGLContextManager::instance()
{
    // Function-local static pointer: intentionally leaked (never destroyed) so
    // the context outlives all consumers and no destruction-order hazard with
    // QGuiApplication / QOpenGLWidget teardown can occur.
    static VideoGLContextManager *s_instance = new VideoGLContextManager();
    return *s_instance;
}

inline VideoGLContextManager::VideoGLContextManager()
{
    m_context = new QOpenGLContext();
    // Share with the application's global share context (AA_ShareOpenGLContexts
    // is set in main.cpp) so textures created here are visible to QOpenGLWidget
    // contexts (VideoGLBlitWidget / VideoOutputNode display). Must be set
    // BEFORE create().
    QOpenGLContext *share = QOpenGLContext::globalShareContext();
    if (share != nullptr)
        m_context->setShareContext(share);
    m_context->setFormat(QSurfaceFormat::defaultFormat());
    if (!m_context->create()) {
        qWarning().noquote() << QStringLiteral("VideoGLContextManager | context create failed");
        delete m_context;
        m_context = nullptr;
        return;
    }
    m_surface = new QOffscreenSurface();
    m_surface->setFormat(m_context->format());
    m_surface->create();
    if (!m_surface->isValid()) {
        qWarning().noquote() << QStringLiteral("VideoGLContextManager | offscreen surface invalid");
        delete m_surface;
        m_surface = nullptr;
    }
}

inline bool VideoGLContextManager::makeCurrent()
{
    if (m_context == nullptr || m_surface == nullptr || !m_context->isValid())
        return false;
    return m_context->makeCurrent(m_surface);
}

inline void VideoGLContextManager::doneCurrent()
{
    if (m_context != nullptr && m_context->isValid())
        m_context->doneCurrent();
}

inline bool VideoGLContextManager::hasHardwareGL()
{
    static const bool cached = []() {
        VideoGLContextManager &mgr = instance();
        if (!mgr.makeCurrent()) {
            qWarning().noquote() << QStringLiteral("VideoGLContextManager | GL detection: makeCurrent failed");
            return false;
        }
        const QByteArray renderer = QByteArray(
            reinterpret_cast<const char *>(mgr.context()->functions()->glGetString(GL_RENDERER)));
        mgr.doneCurrent();
        const QByteArray lower = renderer.toLower();
        const bool hardware = !lower.contains("llvmpipe")
            && !lower.contains("softpipe")
            && !lower.contains("swiftshader");
        qInfo().noquote() << QStringLiteral("VideoGLContextManager | renderer=%1 hardwareGL=%2")
            .arg(QString::fromLatin1(renderer))
            .arg(hardware ? QStringLiteral("yes") : QStringLiteral("no"));
        return hardware;
    }();
    return cached;
}

inline void VideoGLContextManager::deleteTexture(GLuint id)
{
    if (id == 0 || m_context == nullptr || m_surface == nullptr || !m_context->isValid())
        return;
    const bool wasCurrent = (QOpenGLContext::currentContext() == m_context);
    if (!wasCurrent && !m_context->makeCurrent(m_surface))
        return;
    QOpenGLFunctions *f = m_context->functions();
    f->glDeleteTextures(1, &id);
    if (!wasCurrent)
        m_context->doneCurrent();
}