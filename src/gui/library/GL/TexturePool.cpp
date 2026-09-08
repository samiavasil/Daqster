#include "TexturePool.h"

#include "GL/VideoGLContextManager.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>

namespace {

void setupTextureParams(QOpenGLFunctions *f, GLuint id)
{
    f->glBindTexture(GL_TEXTURE_2D, id);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

} // namespace

TexturePool::~TexturePool()
{
    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    // GL never initialized (context creation failed — e.g. headless CI without
    // mesa): nothing to delete. The wasCurrent comparison below would compare
    // currentContext() against a null context and could evaluate true when no
    // context is current, bypassing the makeCurrent() guard and null-derefing
    // in context()->functions().
    if (mgr.context() == nullptr)
        return;
    const bool wasCurrent = (QOpenGLContext::currentContext() == mgr.context());
    if (!wasCurrent && !mgr.makeCurrent())
        return;
    QOpenGLFunctions *f = mgr.context()->functions();
    for (const Entry &e : m_all) {
        if (e.id != 0)
            f->glDeleteTextures(1, &e.id);
    }
    m_all.clear();
    m_free.clear();
    if (!wasCurrent)
        mgr.doneCurrent();
}

GLuint TexturePool::acquire(int w, int h)
{
    if (w <= 0 || h <= 0)
        return 0;

    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    // GL unavailable (context creation failed): no textures can be allocated —
    // the caller falls back to the CPU path.
    if (mgr.context() == nullptr)
        return 0;
    const bool wasCurrent = (QOpenGLContext::currentContext() == mgr.context());
    if (!wasCurrent && !mgr.makeCurrent())
        return 0;
    QOpenGLFunctions *f = mgr.context()->functions();

    // Reuse a free texture with the exact size — no GL work needed.
    for (auto it = m_free.begin(); it != m_free.end(); ++it) {
        if (it->width == w && it->height == h) {
            const GLuint id = it->id;
            m_free.erase(it);
            if (!wasCurrent)
                mgr.doneCurrent();
            return id;
        }
    }

    // Free texture with a different size: re-allocate storage on the same
    // texture name (handles resolution changes without a new glGenTextures).
    if (!m_free.empty()) {
        Entry e = m_free.back();
        m_free.pop_back();
        f->glBindTexture(GL_TEXTURE_2D, e.id);
        setupTextureParams(f, e.id);
        f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        f->glBindTexture(GL_TEXTURE_2D, 0);
        e.width = w;
        e.height = h;
        if (!wasCurrent)
            mgr.doneCurrent();
        return e.id;
    }

    // Allocate a new texture.
    GLuint id = 0;
    f->glGenTextures(1, &id);
    f->glBindTexture(GL_TEXTURE_2D, id);
    setupTextureParams(f, id);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    f->glBindTexture(GL_TEXTURE_2D, 0);
    m_all.push_back(Entry{id, w, h});
    if (!wasCurrent)
        mgr.doneCurrent();
    return id;
}

void TexturePool::release(GLuint tex)
{
    if (tex == 0)
        return;

    // Find the entry in the ownership list. Ignore unknown ids (defensive —
    // a texture this pool never allocated must not be pooled).
    for (Entry &e : m_all) {
        if (e.id != tex)
            continue;
        // Double-release guard: already in the free list — ignore.
        for (const Entry &free : m_free) {
            if (free.id == tex)
                return;
        }
        m_free.push_back(e);
        return;
    }
}