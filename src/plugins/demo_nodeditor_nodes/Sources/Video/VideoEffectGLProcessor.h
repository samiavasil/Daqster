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
//  - Compiles one effect program per (effect id, NV12/YUV420P, core/compat)
//    key; core/compat detection matches VideoGLBlitWidget (DAQSTER_GL_FORCE_CORE
//    override + context profile).
//  - Uploads the Y/U/V planes honoring bytesPerLine strides, draws the
//    fullscreen quad into a QOpenGLFramebufferObject sized to the frame, then
//    reads the result with toImage() (which applies the built-in vertical
//    flip, so the output QImage is top-down).
//  - hasHardwareGL() distinguishes hardware GL from the software renderers
//    (llvmpipe / softpipe / SwiftShader) so the node can pick the CPU backend
//    headlessly. Moved to VideoGLContextManager.
//
// No new CMake dependencies: QOpenGLFramebufferObject / QOffscreenSurface are
// part of Qt::Gui.

#include "VideoEffectOps.h"

#include <QImage>
#include <QString>

#include <QtGui/qopengl.h>
#include <QtMultimedia/QVideoFrame>

class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;
class QOpenGLVertexArrayObject;

class VideoEffectGLProcessor
{
public:
    VideoEffectGLProcessor();
    ~VideoEffectGLProcessor();

    /// Apply an effect to a YUV frame on the GPU. Returns a null QImage when
    /// the frame format is unsupported (non NV12/YUV420P) or a GL step fails —
    /// the caller falls back to the CPU backend.
    QImage process(const QVideoFrame &frame, const EffectSpec &spec,
                   const EffectParams &params);

private:
    bool ensureContext();
    bool ensureProgram(const EffectSpec &spec, bool nv12);
    bool uploadFrame(const QVideoFrame &frame);
    bool drawQuad(const EffectSpec &spec, const EffectParams &params);

    QOpenGLFramebufferObject *m_fbo = nullptr;
    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLVertexArrayObject *m_vao = nullptr;

    GLuint m_texY = 0;
    GLuint m_texU = 0;
    GLuint m_texV = 0;
    GLuint m_texUV = 0;
    GLuint m_vbo = 0;

    QString m_programKey;   // program cache key: "<effectId>:<layout>:<profile>"
    bool m_useCore = false; // core-profile path (GL_RED/RG + #version 150)
    bool m_useNv12 = false; // interleaved UV plane (plane 1 = U,V pairs)
    int m_matrix = 1;       // 0 = BT.601, 1 = BT.709
    int m_range = 1;        // 0 = limited, 1 = full
};