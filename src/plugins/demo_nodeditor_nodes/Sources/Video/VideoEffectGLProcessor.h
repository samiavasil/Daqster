#pragma once

// SPDX-License-Identifier: MIT
//
// GPU backend for VideoEffectNode (REQ-SW-PL-028). Renders a YUV frame with an
// effect fragment shader into an offscreen framebuffer and reads the result
// back as a QImage.
//
// Design (mirrors VideoGLBlitWidget):
//  - Uses the process-wide shared GL context (VideoGLContextManager,
//    REQ-SW-PL-032 Stage 2A) instead of owning a private context — the same
//    share group as the QOpenGLWidget display, so textures uploaded by
//    VideoFrameData::asTexture() are visible here (and vice versa).
//  - Compiles one effect program per (effect id, NV12/YUV420P/RGBA, core/compat)
//    key; core/compat detection matches VideoGLBlitWidget (DAQSTER_GL_FORCE_CORE
//    override + context profile).
//  - processTexture() (REQ-SW-PL-032 Stage 2B): binds the input textures from
//    a VideoTextureHandle — no upload — renders the effect into an offscreen
//    FBO and returns the FBO's RGBA texture wrapped in a new VideoTextureHandle
//    — no toImage() readback. The output texture is created per call and
//    ownership is handed to the caller (VideoFrameData::fromTexture deletes
//    it), so the processor never reuses a texture it has handed off.
//  - hasHardwareGL() distinguishes hardware GL from the software renderers
//    (llvmpipe / softpipe / SwiftShader) so the node can pick the CPU backend
//    headlessly. Moved to VideoGLContextManager.
//
// No new CMake dependencies: QOpenGLFramebufferObject / QOffscreenSurface are
// part of Qt::Gui.

#include "VideoEffectOps.h"
#include "NodeDataTypes/VideoTextureHandle.h"

#include <QImage>
#include <QString>

#include <QtGui/qopengl.h>

class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;
class QOpenGLVertexArrayObject;

class VideoEffectGLProcessor
{
public:
    VideoEffectGLProcessor();
    ~VideoEffectGLProcessor();

    /// Apply an effect to a GPU-resident texture (YUV or RGBA) on the GPU
    /// (REQ-SW-PL-032 Stage 2B). Binds the input textures from the handle —
    /// no upload — renders into an offscreen FBO and returns the FBO's RGBA
    /// texture wrapped in a new VideoTextureHandle — no toImage() readback.
    /// The returned handle owns a freshly created texture; the caller takes
    /// ownership (VideoFrameData::fromTexture deletes it). Returns false when
    /// a GL step fails — the caller falls back to the CPU backend.
    bool processTexture(const VideoTextureHandle &input, const EffectSpec &spec,
                        const EffectParams &params, VideoTextureHandle *out);

private:
    /// Input texture layout for program selection (program cache key layout
    /// component: nv12 / 420p / rgba).
    enum class TextureLayout { Nv12, Yuv420p, Rgba };

    bool ensureContext();
    bool ensureProgram(const EffectSpec &spec, TextureLayout layout);
    bool drawQuad(const EffectSpec &spec, const EffectParams &params,
                  const VideoTextureHandle &input);

    QOpenGLFramebufferObject *m_fbo = nullptr;
    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLVertexArrayObject *m_vao = nullptr;

    GLuint m_vbo = 0;

    QString m_programKey;   // program cache key: "<effectId>:<layout>:<profile>"
    bool m_useCore = false; // core-profile path (GL_RED/RG + #version 150)
    int m_matrix = 1;       // 0 = BT.601, 1 = BT.709
    int m_range = 1;        // 0 = limited, 1 = full
};