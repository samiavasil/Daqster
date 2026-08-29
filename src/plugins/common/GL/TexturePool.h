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
// Lifetime: the pool is shared via std::shared_ptr. VideoFrameData::fromTexture
// captures a shared_ptr to the pool in its release callback, so the pool
// outlives every frame that references it. The pool owns every texture it has
// ever allocated and deletes them all in its destructor — which runs only
// after the last frame is gone (the frames keep the pool alive via the
// callback), so no texture can be deleted while a frame still references it.
//
// All GL operations run in the shared context (VideoGLContextManager). The
// pool makes the context current itself when needed (same pattern as
// VideoGLContextManager::deleteTexture). GUI-thread only.

#include <QtGui/qopengl.h>

#include <vector>

class TexturePool
{
public:
    TexturePool() = default;
    ~TexturePool();

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