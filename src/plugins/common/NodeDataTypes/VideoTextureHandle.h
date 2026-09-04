#pragma once

// SPDX-License-Identifier: MIT
//
// GPU-resident video frame transport handle (REQ-SW-PL-032, Stage 2A).
//
// Carries the GL texture ids of a frame uploaded into the shared GL context
// (VideoGLContextManager). Two layouts are supported:
//   - NV12:    texY (Y plane) + texUV (interleaved U,V) — nv12 = true
//   - YUV420P: texY + texU + texV (three planes)        — nv12 = false
//   - RGBA:    single texY texture (effect output)      — rgba = true
//
// The handle is a plain value type: it does NOT own the textures. Ownership
// lives in VideoFrameData (which deletes them in its destructor) or in the
// producer that created them via fromTexture().
//
// pooled (REQ-SW-PL-038): true when the texture ids came from the global
// TexturePool (VideoFrameData::asTexture uploads and the effect processors'
// output textures). On destruction such textures are returned to the pool
// (TexturePool::release) instead of being glDeleteTextures'd. Owned textures
// (pooled == false, legacy glGenTextures path) are deleted as before.

#include <QtGui/qopengl.h>

/// GL texture ids of a GPU-resident frame in the shared context.
struct VideoTextureHandle
{
    GLuint texY = 0;   // Y plane (NV12/YUV420P) OR the single RGBA output
    GLuint texUV = 0;  // interleaved UV (NV12); 0 for YUV420P / RGBA
    GLuint texU = 0;   // U plane (YUV420P); 0 for NV12 / RGBA
    GLuint texV = 0;   // V plane (YUV420P); 0 for NV12 / RGBA
    int width = 0;
    int height = 0;
    bool nv12 = false;  // interleaved UV layout (texUV valid)
    bool rgba = false;  // true = single RGBA texture in texY
    bool pooled = false;  // true = textures owned by the global TexturePool
};

/// Where the frame payload currently lives.
enum class VideoFrameResidency
{
    Cpu,     // QVideoFrame payload (default)
    GpuYuv,  // Y/U/V textures uploaded from a CPU frame (asTexture cache)
    GpuRgba  // GPU-resident RGBA texture (effect output, fromTexture)
};