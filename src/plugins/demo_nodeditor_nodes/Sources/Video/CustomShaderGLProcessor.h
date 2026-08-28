#pragma once

// SPDX-License-Identifier: MIT
//
// GPU backend for CustomShaderNode (REQ-SW-PL-029). Compiles and applies
// user-written GLSL shaders at runtime in a Shadertoy-style mainImage
// contract: the user writes `void mainImage(out vec4 fragColor, in vec2
// fragCoord)` and the processor wraps it in a fullscreen quad pass.
//
// Design (mirrors VideoEffectGLProcessor):
//  - Uses the process-wide shared GL context (VideoGLContextManager) so
//    textures created by VideoFrameData::asTexture() are visible here.
//  - For YUV inputs (NV12 / YUV420P) a YUV→RGBA pre-pass renders into an
//    intermediate RGBA texture; the user shader then samples `u_tex`.
//  - For RGBA inputs the pre-pass is skipped — the user shader samples
//    `u_tex` directly.
//  - Compiles one program per (userSource hash, core/compat) key.
//  - processTexture() returns a new RGBA texture per call (same ownership
//    contract as VideoEffectGLProcessor::processTexture()).
//
// No new CMake dependencies: QOpenGLFramebufferObject / QOffscreenSurface
// are part of Qt::Gui.

#include "NodeDataTypes/VideoTextureHandle.h"

#include <QHash>
#include <QString>

#include <QtGui/qopengl.h>

class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;
class QOpenGLVertexArrayObject;

/// User-supplied shader parameters exposed as uniforms.
struct ShaderParams
{
    float param0 = 0.0f;  ///< 0..1
    float param1 = 0.0f;  ///< 0..1
    float param2 = 0.0f;  ///< 0..1
    float param3 = 0.0f;  ///< 0..1
    bool animate = false;  ///< When true, u_time advances
    float time = 0.0f;    ///< Elapsed seconds (used when animate is true)
};

/// GL processor for user-written runtime shaders (REQ-SW-PL-029).
///
/// The processor is owned by the node (not a singleton) — same lifetime
/// pattern as VideoEffectGLProcessor in VideoEffectNode.
class CustomShaderGLProcessor
{
public:
    CustomShaderGLProcessor();
    ~CustomShaderGLProcessor();

    /// Apply the user's GLSL to a GPU-resident texture. The input may be
    /// YUV (NV12 / YUV420P) or RGBA. Returns true on success with *out
    /// set to a new RGBA texture (caller owns it via VideoFrameData::fromTexture);
    /// returns false on compile/link/GL error with lastErrorLog() returning
    /// the diagnostics.
    bool processTexture(const VideoTextureHandle &input,
                        const QString &userSource,
                        const ShaderParams &params,
                        VideoTextureHandle *out);

    /// Human-readable log from the last failed compile/link attempt.
    QString lastErrorLog() const;

private:
    bool ensureContext();
    bool ensureYuvProgram(bool nv12);
    bool ensureCustomProgram(const QString &userSource, bool core);
    /// Draws a fullscreen quad with the given program. useFboQuad == true
    /// selects the flipped-v quad for bottom-up FBO-produced RGBA textures
    /// (REQ-SW-PL-032 orientation fix).
    bool drawQuad(QOpenGLShaderProgram *prog, bool useFboQuad);
    GLuint createOutputTexture(int w, int h);

    /// Build the wrapped fragment source around the user's mainImage body.
    QString buildFragmentSource(const QString &userSource, bool core) const;

    QOpenGLFramebufferObject *m_fbo = nullptr;
    QOpenGLShaderProgram *m_yuvProgram = nullptr;   // YUV→RGBA pre-pass
    QOpenGLShaderProgram *m_customProgram = nullptr; // user shader

    GLuint m_vbo = 0;
    /// Second VBO with the flipped-v quad (v' = 1 - v) for bottom-up
    /// FBO-produced RGBA textures (REQ-SW-PL-032 orientation fix).
    GLuint m_vboFbo = 0;
    QOpenGLVertexArrayObject *m_vao = nullptr;

    QString m_yuvProgramKey;    // cache key for YUV program
    QString m_customProgramKey; // cache key for custom program (qHash)
    bool m_useCore = false;
    int m_matrix = 1;  // 0 = BT.601, 1 = BT.709
    int m_range = 1;   // 0 = limited, 1 = full

    QString m_lastError;
};
