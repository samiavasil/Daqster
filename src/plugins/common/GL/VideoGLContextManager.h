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
// LAZY creation (REQ-SW-PL-032): the context is NOT built in the constructor.
// The first caller in practice is backend detection (detectVideoBackend()) from
// a node constructor — long before any QOpenGLWidget exists, therefore long
// before QOpenGLContext::globalShareContext() is non-null. An eagerly created
// context would be its own share-group root: textures uploaded into it are
// invisible to the widget contexts and the display is black. ensureContext()
// defers construction to the first real GL operation, by which time the
// display widget has populated the global share context.
//
// Threading: the whole node processing graph runs on the GUI thread, so no
// mutex guards the context. GUI-thread only.
//
// Lifetime: the singleton is intentionally leaked (function-local static
// pointer, never deleted) to avoid static destruction-order problems with
// QOpenGLContext / QGuiApplication teardown.

#include <QtGui/qopengl.h>

#include <QDebug>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

class VideoGLContextManager
{
public:
    /// Process-wide singleton. Never destroyed (intentional leak).
    static VideoGLContextManager &instance();

    /// The shared context, creating it on first use (null when creation
    /// failed). Non-const: ensureContext() mutates the manager.
    QOpenGLContext *context();

    /// Makes the shared context current on the offscreen surface. Returns
    /// false when the context/surface is unavailable or makeCurrent fails.
    bool makeCurrent();

    /// Releases the shared context from the current thread.
    void doneCurrent();

    /// True when a hardware GL renderer is available (not llvmpipe / softpipe /
    /// SwiftShader). Lazy — the detection runs once and the result is cached
    /// for the process lifetime. Moved from VideoEffectGLProcessor.
    /// Returns false rather than crashing when no QGuiApplication exists.
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

    /// Creates the shared context + offscreen surface on first call. Cheap and
    /// self-guarding afterwards (returns false when GL is unavailable, so a
    /// caller in a headless/unit-test context never retries the failing work).
    bool ensureContext();

    QOpenGLContext *m_context = nullptr;
    QOffscreenSurface *m_surface = nullptr;
    bool m_creationFailed = false;
};

inline VideoGLContextManager &VideoGLContextManager::instance()
{
    // Function-local static pointer: intentionally leaked (never destroyed) so
    // the context outlives all consumers and no destruction-order hazard with
    // QGuiApplication / QOpenGLWidget teardown can occur.
    static VideoGLContextManager *s_instance = new VideoGLContextManager();
    return *s_instance;
}

inline VideoGLContextManager::VideoGLContextManager() = default;

inline bool VideoGLContextManager::ensureContext()
{
    if (m_context != nullptr)
        return m_context->isValid();
    // Already tried and failed (no GUI stack, no GL driver): do not retry — the
    // failure is not transient and every caller degrades the same way.
    if (m_creationFailed)
        return false;

    m_context = new QOpenGLContext();
    // Share with the application's global share context (AA_ShareOpenGLContexts
    // is set in main.cpp) so textures created here are visible to QOpenGLWidget
    // contexts (VideoGLBlitWidget / VideoOutputNode display). Must be set
    // BEFORE create().
    //
    // A null share context here means no QOpenGLWidget has been created yet.
    // That is precisely why this runs lazily: by the time a real GL operation
    // asks for the context the display widget exists and the global share
    // context is populated. Creating it eagerly (from node construction /
    // backend detection) would root a separate share group and turn every
    // texture invisible to the display — a black screen.
    QOpenGLContext *share = QOpenGLContext::globalShareContext();
    if (share != nullptr)
        m_context->setShareContext(share);
    m_context->setFormat(QSurfaceFormat::defaultFormat());
    if (!m_context->create()) {
        qWarning().noquote() << QStringLiteral("VideoGLContextManager | context create failed");
        delete m_context;
        m_context = nullptr;
        m_creationFailed = true;
        return false;
    }
    m_surface = new QOffscreenSurface();
    m_surface->setFormat(m_context->format());
    m_surface->create();
    if (!m_surface->isValid()) {
        qWarning().noquote() << QStringLiteral("VideoGLContextManager | offscreen surface invalid");
        delete m_surface;
        m_surface = nullptr;
    }
    return true;
}

inline QOpenGLContext *VideoGLContextManager::context()
{
    ensureContext();
    return m_context;
}

inline bool VideoGLContextManager::makeCurrent()
{
    if (!ensureContext())
        return false;
    if (m_surface == nullptr || !m_context->isValid())
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
    // Neither a QOpenGLContext nor a QOffscreenSurface may be constructed
    // before a QGuiApplication exists — QGuiApplication is what initialises the
    // platform integration they are built on. Calling this from a plain console
    // program or a QCoreApplication segfaults inside QOpenGLContext rather
    // than returning a value.
    //
    // The rest of this function is written to degrade (makeCurrent failure ->
    // false), so a missing GUI stack belongs on the same path: report no
    // hardware GL and let callers pick the software backend. Detection that
    // crashes on the question "is there a GPU?" answers nothing.
    if (QGuiApplication::instance() == nullptr) {
        qWarning().noquote() << QStringLiteral(
            "VideoGLContextManager | GL detection: no QGuiApplication, assuming software");
        return false;
    }
    static const bool cached = []() {
        // A THROWAWAY probe context, deliberately NOT the shared manager one.
        // Detection is called from node construction / runtime startup, before
        // any display widget exists; creating the manager's shared context here
        // would both bake in a wrong (non-share-grouped) context and make the
        // later real one impossible. The probe is created, queried and destroyed
        // immediately, leaving the manager's lazy creation to a point where
        // QOpenGLContext::globalShareContext() is populated.
        QOpenGLContext probe;
        probe.setFormat(QSurfaceFormat::defaultFormat());
        if (!probe.create()) {
            qWarning().noquote() << QStringLiteral("VideoGLContextManager | GL detection: probe create failed");
            return false;
        }
        QOffscreenSurface probeSurface;
        probeSurface.setFormat(probe.format());
        probeSurface.create();
        if (!probeSurface.isValid()) {
            qWarning().noquote() << QStringLiteral("VideoGLContextManager | GL detection: probe surface invalid");
            return false;
        }
        if (!probe.makeCurrent(&probeSurface)) {
            qWarning().noquote() << QStringLiteral("VideoGLContextManager | GL detection: makeCurrent failed");
            return false;
        }
        const QByteArray renderer = QByteArray(
            reinterpret_cast<const char *>(probe.functions()->glGetString(GL_RENDERER)));
        probe.doneCurrent();
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
    if (id == 0)
        return;
    if (!ensureContext() || m_surface == nullptr)
        return;
    const bool wasCurrent = (QOpenGLContext::currentContext() == m_context);
    if (!wasCurrent && !m_context->makeCurrent(m_surface))
        return;
    QOpenGLFunctions *f = m_context->functions();
    f->glDeleteTextures(1, &id);
    if (!wasCurrent)
        m_context->doneCurrent();
}