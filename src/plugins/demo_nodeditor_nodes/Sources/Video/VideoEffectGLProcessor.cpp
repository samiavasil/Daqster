#include "VideoEffectGLProcessor.h"

#include "GL/VideoGLContextManager.h"
#include "VideoGLShaders.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QSurfaceFormat>

namespace {

// ── Quad: x, y, u, v (triangle strip) ────────────────────────────────────────
// Texture v=0 maps to the top of the screen; first memory row of both QImage
// and QVideoFrame planes is the top row, so no flip is needed (the flip effect
// flips the texture coordinate through the u_flipY uniform instead).
const GLfloat kQuadVertices[] = {
    -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 0.0f,
};

void setupTextureParams(QOpenGLFunctions *f, GLuint id)
{
    f->glBindTexture(GL_TEXTURE_2D, id);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

} // namespace

VideoEffectGLProcessor::VideoEffectGLProcessor()
{
    const QString matrixEnv = qEnvironmentVariable("DAQSTER_GL_MATRIX", QStringLiteral("bt709")).toLower();
    const QString rangeEnv = qEnvironmentVariable("DAQSTER_GL_RANGE", QStringLiteral("full")).toLower();
    m_matrix = (matrixEnv == QStringLiteral("bt601")) ? 0 : 1;
    m_range = (rangeEnv == QStringLiteral("limited")) ? 0 : 1;
}

VideoEffectGLProcessor::~VideoEffectGLProcessor()
{
    // All GL resources live in the shared context (VideoGLContextManager).
    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    if (mgr.makeCurrent()) {
        delete m_program;
        m_program = nullptr;
        delete m_fbo;
        m_fbo = nullptr;
        delete m_vao;
        m_vao = nullptr;
        QOpenGLFunctions *f = mgr.context()->functions();
        if (m_vbo != 0) {
            f->glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        }
        mgr.doneCurrent();
    }
}

bool VideoEffectGLProcessor::ensureContext()
{
    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    if (!mgr.makeCurrent())
        return false;

    // One-time GL resource setup (VBO + optional core VAO).
    if (m_vbo == 0) {
        QOpenGLFunctions *f = mgr.context()->functions();
        f->glGenBuffers(1, &m_vbo);
        f->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        f->glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
        f->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    return true;
}

bool VideoEffectGLProcessor::ensureProgram(const EffectSpec &spec, TextureLayout layout)
{
    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    const bool forceCore = qEnvironmentVariableIntValue("DAQSTER_GL_FORCE_CORE") == 1;
    const bool core = forceCore
        || (mgr.context()->format().profile() == QSurfaceFormat::CoreProfile);

    // Program cache key: "<effectId>:<layout>:<profile>" with layout ∈
    // {nv12, 420p, rgba} (REQ-SW-PL-032 Stage 2B adds the rgba layout).
    const QString layoutKey = (layout == TextureLayout::Nv12) ? QStringLiteral("nv12")
        : (layout == TextureLayout::Yuv420p) ? QStringLiteral("420p")
        : QStringLiteral("rgba");
    const QString key = spec.id + QStringLiteral(":") + layoutKey
        + (core ? QStringLiteral(":core") : QStringLiteral(":compat"));
    if (m_program != nullptr && m_programKey == key)
        return true;

    m_programKey = key;
    m_useCore = core;

    delete m_program;
    m_program = nullptr;

    auto *prog = new QOpenGLShaderProgram();
    const QString vert = buildVertexSource(core);
    QString frag;
    if (layout == TextureLayout::Rgba)
        frag = buildRgbaEffectFragmentSource(core, spec.glslBody);
    else
        frag = buildEffectFragmentSource(core, (layout == TextureLayout::Nv12), spec.glslBody);
    if (!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vert)
        || !prog->addShaderFromSourceCode(QOpenGLShader::Fragment, frag)
        || !prog->link()) {
        qWarning().noquote() << QStringLiteral("VideoEffectGLProcessor | shader link failed: %1")
            .arg(prog->log());
        delete prog;
        return false;
    }
    m_program = prog;

    if (m_useCore && m_vao == nullptr) {
        m_vao = new QOpenGLVertexArrayObject();
        m_vao->create();
    }
    return true;
}

bool VideoEffectGLProcessor::drawQuad(const EffectSpec &spec, const EffectParams &params,
                                      const VideoTextureHandle &input)
{
    if (m_program == nullptr || m_fbo == nullptr)
        return false;

    // The caller binds the FBO (process() / processTexture()) — this method
    // only sets the viewport and draws.
    QOpenGLFunctions *f = VideoGLContextManager::instance().context()->functions();
    f->glViewport(0, 0, m_fbo->width(), m_fbo->height());

    QOpenGLShaderProgram *prog = m_program;
    prog->bind();

    // Bind the input textures from the handle — no upload (REQ-SW-PL-032
    // Stage 2B). RGBA: single texture; NV12: Y + interleaved UV; YUV420P:
    // three planes.
    if (input.rgba) {
        f->glActiveTexture(GL_TEXTURE0);
        f->glBindTexture(GL_TEXTURE_2D, input.texY);
        prog->setUniformValue("u_tex", 0);
    } else if (input.nv12) {
        f->glActiveTexture(GL_TEXTURE0);
        f->glBindTexture(GL_TEXTURE_2D, input.texY);
        prog->setUniformValue("u_texY", 0);
        f->glActiveTexture(GL_TEXTURE1);
        f->glBindTexture(GL_TEXTURE_2D, input.texUV);
        prog->setUniformValue("u_texUV", 1);
    } else {
        f->glActiveTexture(GL_TEXTURE0);
        f->glBindTexture(GL_TEXTURE_2D, input.texY);
        prog->setUniformValue("u_texY", 0);
        f->glActiveTexture(GL_TEXTURE1);
        f->glBindTexture(GL_TEXTURE_2D, input.texU);
        prog->setUniformValue("u_texU", 1);
        f->glActiveTexture(GL_TEXTURE2);
        f->glBindTexture(GL_TEXTURE_2D, input.texV);
        prog->setUniformValue("u_texV", 2);
    }
    prog->setUniformValue("u_matrix", m_matrix);
    prog->setUniformValue("u_range", m_range);

    // Effect uniforms — set only when the compiled program actually uses them
    // (uniformLocation() returns -1 for optimized-out uniforms).
    if (prog->uniformLocation("u_brightness") != -1)
        prog->setUniformValue("u_brightness", params.brightness / 100.0f);
    if (prog->uniformLocation("u_contrast") != -1)
        prog->setUniformValue("u_contrast", params.contrast / 100.0f);
    if (prog->uniformLocation("u_flipX") != -1)
        prog->setUniformValue("u_flipX",
            (spec.id == QStringLiteral("flip") && params.flipHorizontal) ? 1 : 0);
    if (prog->uniformLocation("u_flipY") != -1)
        prog->setUniformValue("u_flipY",
            (spec.id == QStringLiteral("flip") && !params.flipHorizontal) ? 1 : 0);

    const int posLoc = prog->attributeLocation("a_position");
    const int texLoc = prog->attributeLocation("a_texcoord");

    if (m_vao != nullptr)
        m_vao->bind();

    f->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    prog->enableAttributeArray(posLoc);
    prog->setAttributeBuffer(posLoc, GL_FLOAT, 0, 2, 4 * static_cast<int>(sizeof(GLfloat)));
    prog->enableAttributeArray(texLoc);
    prog->setAttributeBuffer(texLoc, GL_FLOAT, 2 * static_cast<int>(sizeof(GLfloat)), 2, 4 * static_cast<int>(sizeof(GLfloat)));

    f->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (m_vao != nullptr)
        m_vao->release();
    f->glBindBuffer(GL_ARRAY_BUFFER, 0);
    prog->release();
    return true;
}

bool VideoEffectGLProcessor::processTexture(const VideoTextureHandle &input,
                                            const EffectSpec &spec,
                                            const EffectParams &params,
                                            VideoTextureHandle *out)
{
    if (input.texY == 0 || input.width <= 0 || input.height <= 0 || spec.id.isEmpty())
        return false;

    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    if (!ensureContext())
        return false;
    VideoGLContextManager::CurrentGuard guard(mgr);

    const TextureLayout layout = input.rgba ? TextureLayout::Rgba
        : (input.nv12 ? TextureLayout::Nv12 : TextureLayout::Yuv420p);
    if (!ensureProgram(spec, layout))
        return false;

    const int w = input.width;
    const int h = input.height;

    // FBO sized to the input (reused across frames — it does not own the
    // output texture).
    if (m_fbo == nullptr || m_fbo->width() != w || m_fbo->height() != h) {
        delete m_fbo;
        m_fbo = new QOpenGLFramebufferObject(w, h);
        if (!m_fbo->isValid()) {
            qWarning().noquote() << QStringLiteral("VideoEffectGLProcessor | FBO invalid (%1x%2)").arg(w).arg(h);
            return false;
        }
    }

    // Output texture: a NEW RGBA texture per call. Ownership is handed to the
    // caller (VideoFrameData::fromTexture deletes it), so the processor must
    // never reuse a texture it has handed off — the owner would delete it
    // while the processor still references it.
    QOpenGLFunctions *f = mgr.context()->functions();
    GLuint outTex = 0;
    f->glGenTextures(1, &outTex);
    f->glBindTexture(GL_TEXTURE_2D, outTex);
    setupTextureParams(f, outTex);
    f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    f->glBindTexture(GL_TEXTURE_2D, 0);

    // Attach the output texture as the FBO's color attachment, draw, then
    // detach it and restore the FBO's own texture — the output texture
    // outlives the FBO (QOpenGLFramebufferObject has no takeTexture() on Qt5).
    m_fbo->bind();
    f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
    const GLenum status = f->glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        qWarning().noquote() << QStringLiteral("VideoEffectGLProcessor | FBO incomplete (0x%1)").arg(status, 0, 16);
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fbo->texture(), 0);
        m_fbo->release();
        f->glDeleteTextures(1, &outTex);
        return false;
    }

    const bool drawn = drawQuad(spec, params, input);
    f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fbo->texture(), 0);
    m_fbo->release();
    if (!drawn) {
        f->glDeleteTextures(1, &outTex);
        return false;
    }

    if (out != nullptr)
        *out = VideoTextureHandle{outTex, 0, 0, 0, w, h, false, true};
    return true;
}