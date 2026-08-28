#include "CustomShaderGLProcessor.h"

#include "GL/VideoGLContextManager.h"
#include "VideoCompat.h"
#include "VideoGLShaders.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QSurfaceFormat>
#include <QVector2D>

#include <array>

namespace {

// ── Quad: x, y, u, v (triangle strip) ────────────────────────────────────────
const std::array<GLfloat, 16> kQuadVertices = {
    -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 0.0f,
};

// ── Quad for bottom-up FBO-produced RGBA textures (v' = 1 - v) ───────────────
// FBO-attached textures are bottom-up (row 0 = scene bottom), so the v
// coordinate is inverted relative to the top-down QImage/YUV quads above.
// Drawing with v' = 1 - v compensates for the FBO convention — mirrors the
// display path (VideoGLBlitWidget.cpp kQuadVerticesFbo). The custom shader
// pass samples either the YUV→RGBA pre-pass intermediate (an FBO) or an RGBA
// input (a previous effect output, also FBO-produced) — both are bottom-up,
// so the flipped quad keeps a single CustomShaderNode pass upright
// (REQ-SW-PL-032).
const std::array<GLfloat, 16> kQuadVerticesFbo = {
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
};

void setupTextureParams(QOpenGLFunctions *f, GLuint id)
{
    f->glBindTexture(GL_TEXTURE_2D, id);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

/// Input texture layout for the pre-pass decision.
enum class TextureLayout { Nv12, Yuv420p, Rgba };

} // namespace

CustomShaderGLProcessor::CustomShaderGLProcessor()
    : m_texturePool(std::make_shared<TexturePool>())
{
    const QString matrixEnv = qEnvironmentVariable("DAQSTER_GL_MATRIX", QStringLiteral("bt709")).toLower();
    const QString rangeEnv = qEnvironmentVariable("DAQSTER_GL_RANGE", QStringLiteral("full")).toLower();
    m_matrix = (matrixEnv == QStringLiteral("bt601")) ? 0 : 1;
    m_range = (rangeEnv == QStringLiteral("limited")) ? 0 : 1;
}

CustomShaderGLProcessor::~CustomShaderGLProcessor()
{
    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    if (mgr.makeCurrent()) {
        delete m_customProgram;
        m_customProgram = nullptr;
        delete m_yuvProgram;
        m_yuvProgram = nullptr;
        delete m_fbo;
        m_fbo = nullptr;
        delete m_vao;
        m_vao = nullptr;
        QOpenGLFunctions *f = mgr.context()->functions();
        if (m_vbo != 0) {
            f->glDeleteBuffers(1, &m_vbo);
            m_vbo = 0;
        }
        if (m_vboFbo != 0) {
            f->glDeleteBuffers(1, &m_vboFbo);
            m_vboFbo = 0;
        }
        mgr.doneCurrent();
    } else {
        qWarning() << "CustomShaderGLProcessor: could not make GL context current for cleanup";
    }
}

bool CustomShaderGLProcessor::ensureContext()
{
    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    if (!mgr.makeCurrent())
        return false;

    if (m_vbo == 0) {
        QOpenGLFunctions *f = mgr.context()->functions();
        f->glGenBuffers(1, &m_vbo);
        f->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        f->glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices.data(), GL_STATIC_DRAW);
        f->glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Second VBO with the flipped-v quad for bottom-up FBO-produced RGBA
        // textures (REQ-SW-PL-032 orientation fix).
        f->glGenBuffers(1, &m_vboFbo);
        f->glBindBuffer(GL_ARRAY_BUFFER, m_vboFbo);
        f->glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerticesFbo), kQuadVerticesFbo.data(), GL_STATIC_DRAW);
        f->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    return true;
}

bool CustomShaderGLProcessor::ensureYuvProgram(bool nv12)
{
    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    const bool forceCore = qEnvironmentVariableIntValue("DAQSTER_GL_FORCE_CORE") == 1;
    const bool core = forceCore
        || (mgr.context()->format().profile() == QSurfaceFormat::CoreProfile);

    const QString layoutKey = nv12 ? QStringLiteral("nv12") : QStringLiteral("420p");
    const QString key = QStringLiteral("__yuvPrePass:") + layoutKey
        + (core ? QStringLiteral(":core") : QStringLiteral(":compat"));
    if (m_yuvProgram != nullptr && m_yuvProgramKey == key)
        return true;

    m_yuvProgramKey = key;
    m_useCore = core;

    delete m_yuvProgram;
    m_yuvProgram = nullptr;

    auto *prog = new QOpenGLShaderProgram();
    const QString vert = buildVertexSource(core);
    const QString frag = buildYuvFragmentSource(core, nv12);
    if (!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vert)
        || !prog->addShaderFromSourceCode(QOpenGLShader::Fragment, frag)
        || !prog->link()) {
        m_lastError = prog->log();
        qWarning().noquote() << QStringLiteral("CustomShaderGLProcessor | YUV program link failed: %1")
            .arg(m_lastError);
        delete prog;
        return false;
    }
    m_yuvProgram = prog;
    return true;
}

QString CustomShaderGLProcessor::buildFragmentSource(const QString &userSource,
                                                     bool core) const
{
    // Shadertoy-style template: the user writes mainImage(out vec4, in vec2)
    // and the template provides uniforms + the main() that calls it.
    if (core) {
        return QStringLiteral(
            "#version 150 core\n"
            "uniform sampler2D u_tex;\n"
            "uniform vec2 u_resolution;\n"
            "uniform float u_time;\n"
            "uniform float u_param0;\n"
            "uniform float u_param1;\n"
            "uniform float u_param2;\n"
            "uniform float u_param3;\n"
            "in vec2 v_texcoord;\n"
            "out vec4 fragColor;\n"
            "void mainImage(out vec4 fragColor, in vec2 fragCoord);\n"
            "void main() {\n"
            "    mainImage(fragColor, v_texcoord * u_resolution);\n"
            "}\n"
        ) + userSource;
    }
    // Compatibility profile: #version 120, varying, gl_FragColor.
    // GLSL 120 has texture2D() but not the unified texture() overload
    // introduced in GLSL 130.  Provide a thin shim (see VideoGLShaders.h) so
    // user code that writes texture() — the modern / core-profile convention —
    // works transparently on compatibility contexts as well.
    return QStringLiteral(
        "#version 120\n"
        "uniform sampler2D u_tex;\n"
        "uniform vec2 u_resolution;\n"
        "uniform float u_time;\n"
        "uniform float u_param0;\n"
        "uniform float u_param1;\n"
        "uniform float u_param2;\n"
        "uniform float u_param3;\n"
        "varying vec2 v_texcoord;\n"
    ) + buildTextureCompatShim() + QStringLiteral(
        "void mainImage(out vec4 fragColor, in vec2 fragCoord);\n"
        "void main() {\n"
        "    mainImage(gl_FragColor, v_texcoord * u_resolution);\n"
        "}\n"
    ) + userSource;
}

bool CustomShaderGLProcessor::ensureCustomProgram(const QString &userSource, bool core)
{
    const QString profileTag = core ? QStringLiteral(":core") : QStringLiteral(":compat");
    const QString key = QString::number(qHash(userSource)) + profileTag;
    if (m_customProgram != nullptr && m_customProgramKey == key)
        return true;

    m_customProgramKey = key;
    m_useCore = core;

    delete m_customProgram;
    m_customProgram = nullptr;

    auto *prog = new QOpenGLShaderProgram();
    const QString vert = buildVertexSource(core);
    const QString frag = buildFragmentSource(userSource, core);
    if (!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vert)
        || !prog->addShaderFromSourceCode(QOpenGLShader::Fragment, frag)
        || !prog->link()) {
        m_lastError = prog->log();
        qWarning().noquote() << QStringLiteral("CustomShaderGLProcessor | custom shader link failed: %1")
            .arg(m_lastError);
        delete prog;
        return false;
    }
    m_customProgram = prog;

    if (m_useCore && m_vao == nullptr) {
        m_vao = new QOpenGLVertexArrayObject();
        m_vao->create();
    }
    return true;
}

bool CustomShaderGLProcessor::drawQuad(QOpenGLShaderProgram *prog, bool useFboQuad)
{
    if (prog == nullptr || m_fbo == nullptr)
        return false;

    QOpenGLFunctions *f = VideoGLContextManager::instance().context()->functions();
    f->glViewport(0, 0, m_fbo->width(), m_fbo->height());

    prog->bind();

    const int posLoc = prog->attributeLocation("a_position");
    const int texLoc = prog->attributeLocation("a_texcoord");

    if (m_vao != nullptr)
        m_vao->bind();

    // useFboQuad == true when the sampled texture is bottom-up FBO-produced
    // (the YUV→RGBA pre-pass intermediate or an RGBA input) — the flipped-v
    // quad keeps the output upright (REQ-SW-PL-032). The YUV pre-pass itself
    // samples top-down CPU uploads and uses the standard quad.
    f->glBindBuffer(GL_ARRAY_BUFFER, useFboQuad ? m_vboFbo : m_vbo);
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

GLuint CustomShaderGLProcessor::createOutputTexture(int w, int h)
{
    // Acquired from the pool (REQ-SW-PL-032 Issue #7) — reused across frames
    // instead of a glGenTextures per call. The caller returns it via the
    // pool release callback when the frame is destroyed.
    return m_texturePool->acquire(w, h);
}

bool CustomShaderGLProcessor::processTexture(const VideoTextureHandle &input,
                                             const QString &userSource,
                                             const ShaderParams &params,
                                             VideoTextureHandle *out)
{
    if (input.texY == 0 || input.width <= 0 || input.height <= 0 || userSource.isEmpty()) {
        m_lastError = QStringLiteral("Invalid input: no texture or empty source");
        return false;
    }

    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    if (!ensureContext())
        return false;
    VideoGLContextManager::CurrentGuard guard(mgr);

    const bool forceCore = qEnvironmentVariableIntValue("DAQSTER_GL_FORCE_CORE") == 1;
    const bool core = forceCore
        || (mgr.context()->format().profile() == QSurfaceFormat::CoreProfile);
    m_useCore = core;

    const int w = input.width;
    const int h = input.height;

    // Ensure FBO sized to input.
    if (m_fbo == nullptr || m_fbo->width() != w || m_fbo->height() != h) {
        delete m_fbo;
        m_fbo = new QOpenGLFramebufferObject(w, h);
        if (!m_fbo->isValid()) {
            m_lastError = QStringLiteral("FBO invalid (%1x%2)").arg(w).arg(h);
            qWarning().noquote() << QStringLiteral("CustomShaderGLProcessor | %1").arg(m_lastError);
            return false;
        }
    }

    QOpenGLFunctions *f = mgr.context()->functions();

    // ── Step 1: Determine the texture layout and ensure the right program(s).
    TextureLayout layout;
    if (input.rgba) {
        layout = TextureLayout::Rgba;
    } else if (input.nv12) {
        layout = TextureLayout::Nv12;
    } else {
        layout = TextureLayout::Yuv420p;
    }

    // We need a single RGBA texture for the user shader's u_tex.
    // For RGBA inputs: use input.texY directly.
    // For YUV inputs: render YUV→RGBA into an intermediate texture first.
    GLuint rgbaTex = 0;
    bool ownsRgbaTex = false;

    if (layout == TextureLayout::Rgba) {
        rgbaTex = input.texY;
    } else {
        // YUV→RGBA pre-pass: compile the YUV program if needed.
        if (!ensureYuvProgram(input.nv12))
            return false;

        // Create intermediate RGBA texture.
        rgbaTex = createOutputTexture(w, h);
        ownsRgbaTex = true;

        // Create a temporary FBO for the pre-pass (reuse m_fbo).
        m_fbo->bind();
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rgbaTex, 0);
        const GLenum status = f->glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            m_lastError = QStringLiteral("YUV pre-pass FBO incomplete (0x%1)").arg(status, 0, 16);
            qWarning().noquote() << QStringLiteral("CustomShaderGLProcessor | %1").arg(m_lastError);
            f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fbo->texture(), 0);
            m_fbo->release();
            m_texturePool->release(rgbaTex);
            return false;
        }

        m_yuvProgram->bind();
        // Bind input YUV textures.
        f->glActiveTexture(GL_TEXTURE0);
        f->glBindTexture(GL_TEXTURE_2D, input.texY);
        m_yuvProgram->setUniformValue("u_texY", 0);
        if (input.nv12) {
            f->glActiveTexture(GL_TEXTURE1);
            f->glBindTexture(GL_TEXTURE_2D, input.texUV);
            m_yuvProgram->setUniformValue("u_texUV", 1);
        } else {
            f->glActiveTexture(GL_TEXTURE1);
            f->glBindTexture(GL_TEXTURE_2D, input.texU);
            m_yuvProgram->setUniformValue("u_texU", 1);
            f->glActiveTexture(GL_TEXTURE2);
            f->glBindTexture(GL_TEXTURE_2D, input.texV);
            m_yuvProgram->setUniformValue("u_texV", 2);
        }
        m_yuvProgram->setUniformValue("u_matrix", m_matrix);
        m_yuvProgram->setUniformValue("u_range", m_range);

        // YUV inputs are top-down CPU uploads — standard quad.
        drawQuad(m_yuvProgram, false);
        m_yuvProgram->release();

        // Restore the FBO's own texture and detach our intermediate.
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fbo->texture(), 0);
        m_fbo->release();
    }

    // ── Step 2: Compile and run the custom shader with u_tex = rgbaTex.
    if (!ensureCustomProgram(userSource, core)) {
        // lastError already set by ensureCustomProgram.
        if (ownsRgbaTex)
            m_texturePool->release(rgbaTex);
        return false;
    }

    // Create the output texture (new per call, caller owns it).
    GLuint outTex = createOutputTexture(w, h);

    m_fbo->bind();
    f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
    const GLenum status = f->glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        m_lastError = QStringLiteral("Custom shader FBO incomplete (0x%1)").arg(status, 0, 16);
        qWarning().noquote() << QStringLiteral("CustomShaderGLProcessor | %1").arg(m_lastError);
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fbo->texture(), 0);
        m_fbo->release();
        m_texturePool->release(outTex);
        if (ownsRgbaTex)
            m_texturePool->release(rgbaTex);
        return false;
    }

    m_customProgram->bind();
    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D, rgbaTex);
    m_customProgram->setUniformValue("u_tex", 0);
    m_customProgram->setUniformValue("u_resolution", QVector2D(static_cast<float>(w), static_cast<float>(h)));
    m_customProgram->setUniformValue("u_time", params.time);
    m_customProgram->setUniformValue("u_param0", params.param0);
    m_customProgram->setUniformValue("u_param1", params.param1);
    m_customProgram->setUniformValue("u_param2", params.param2);
    m_customProgram->setUniformValue("u_param3", params.param3);

    // The custom pass samples a bottom-up FBO-produced RGBA texture (the
    // pre-pass intermediate or an RGBA input) — flipped quad (REQ-SW-PL-032).
    drawQuad(m_customProgram, true);
    m_customProgram->release();

    // Restore FBO's own texture.
    f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fbo->texture(), 0);
    m_fbo->release();

    // Clean up the intermediate RGBA texture (was only needed for this call) —
    // return it to the pool for reuse (REQ-SW-PL-032 Issue #7).
    if (ownsRgbaTex)
        m_texturePool->release(rgbaTex);

    if (out != nullptr)
        *out = VideoTextureHandle{outTex, 0, 0, 0, w, h, false, true};
    return true;
}

QString CustomShaderGLProcessor::lastErrorLog() const
{
    return m_lastError;
}
