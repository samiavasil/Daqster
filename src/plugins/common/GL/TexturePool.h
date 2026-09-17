#pragma once

// SPDX-License-Identifier: MIT
//
// Texture pool for GPU video effect outputs (REQ-SW-PL-032, Issue #7).
//
// The effect processors (VideoEffectGLProcessor / CustomShaderGLProcessor)
// previously created a NEW RGBA texture per processed frame and handed
// ownership to VideoFrameData::fromTexture(), which glDeleteTextures'd it on
// destruction — a glGenTextures/glDeleteTextures pair per frame. This pool
// reuses output textures across frames: acquire() returns a free texture (or
// allocates one), release() returns it to the pool. Resolution changes are
// handled by re-allocating storage on the same texture name (glTexImage2D
// with the new size).
//
// Lifetime: the pool is a process-wide singleton (TexturePool::instance(),
// intentionally leaked — see below). The pool owns every texture it has ever
// allocated and deletes them all in its destructor; because the singleton is
// never destroyed, the textures live until process exit (the GL context is
// leaked the same way, so the driver cleans up at teardown). Frames reference
// the pool via TexturePool::instance() — no shared_ptr capture needed.
//
// All GL operations run in the shared context (VideoGLContextManager). The
// pool makes the context current itself when needed (same pattern as
// VideoGLContextManager::deleteTexture). GUI-thread only.
//
// Global pool (REQ-SW-PL-038): since the display path (VideoFrameData::
// asTexture) and the effect processors (VideoEffectGLProcessor /
// CustomShaderGLProcessor) all run on the GUI thread in the same share group,
// a single process-wide pool replaces the per-node pools — every texture the
// app allocates for video (YUV planes + RGBA effect outputs) is reused across
// frames instead of a glGenTextures/glDeleteTextures pair per frame. The
// singleton is intentionally leaked (same lifetime pattern as
// VideoGLContextManager::instance()) so the pool outlives every frame and no
// static destruction-order hazard with QOpenGLContext teardown can occur.

#include <QtGui/qopengl.h>

#include <vector>

class TexturePool
{
public:
    TexturePool() = default;
    ~TexturePool();

    /// Process-wide singleton. Never destroyed (intentional leak) — same
    /// pattern as VideoGLContextManager::instance().
    static TexturePool &instance();

    /// Returns a texture with (w, h) RGBA8 storage. Reuses a free texture when
    /// available (re-allocating storage on the same name if the size changed)
    /// or allocates a new one. Returns 0 on GL failure. The caller owns the
    /// texture until release().
    GLuint acquire(int w, int h);

    /// Returns a texture to the pool for reuse. Safe to call with 0; ignores
    /// unknown ids and double-releases.
    void release(GLuint tex);

private:
    struct Entry
    {
        GLuint id = 0;
        int width = 0;
        int height = 0;
    };

    std::vector<Entry> m_all;   // every texture ever allocated (ownership)
    std::vector<Entry> m_free;  // textures available for reuse
};

inline TexturePool &TexturePool::instance()
{
    // Function-local static pointer: intentionally leaked (never destroyed) so
    // the pool outlives all frames and no destruction-order hazard with
    // QOpenGLContext / QGuiApplication teardown can occur (same pattern as
    // VideoGLContextManager::instance()).
    static TexturePool *s_instance = new TexturePool();
    return *s_instance;
}