#include "VideoEffectGLProcessor.h"

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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtMultimedia/QVideoFrameFormat>
#endif

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

/// Classify a frame's pixel layout for the shader selection (same logic as
/// VideoGLBlitWidget.cpp).
enum class YuvLayout { Nv12, Yuv420p, Other };

YuvLayout classifyYuv(const QVideoFrame &frame, QString *formatName)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const auto pf = frame.surfaceFormat().pixelFormat();
    *formatName = QVideoFrameFormat::pixelFormatToString(pf);
    switch (pf) {
    case QVideoFrameFormat::Format_NV12:
        return YuvLayout::Nv12;
    case QVideoFrameFormat::Format_YUV420P:
        return YuvLayout::Yuv420p;
    default:
        return YuvLayout::Other;
    }
#else
    const auto pf = frame.pixelFormat();
    switch (pf) {
    case QVideoFrame::Format_NV12:
        *formatName = QStringLiteral("NV12");
        return YuvLayout::Nv12;
    case QVideoFrame::Format_YUV420P:
        *formatName = QStringLiteral("YUV420P");
        return YuvLayout::Yuv420p;
    default:
        *formatName = QStringLiteral("Format(%1)").arg(static_cast<int>(pf));
        return YuvLayout::Other;
    }
#endif
}

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
        if (m_texY != 0) {
            f->glDeleteTextures(1, &m_texY);
            m_texY = 0;
        }
        if (m_texU != 0) {
            f->glDeleteTextures(1, &m_texU);
            m_texU = 0;
        }
        if (m_texV != 0) {
            f->glDeleteTextures(1, &m_texV);
            m_texV = 0;
        }
        if (m_texUV != 0) {
            f->glDeleteTextures(1, &m_texUV);
            m_texUV = 0;
        }
        mgr.doneCurrent();
    }
}

bool VideoEffectGLProcessor::ensureContext()
{
    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    if (!mgr.makeCurrent())
        return false;

    // One-time GL resource setup (VBO + textures + optional core VAO).
    if (m_vbo == 0) {
        QOpenGLFunctions *f = mgr.context()->functions();
        f->glGenBuffers(1, &m_vbo);
        f->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        f->glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
        f->glBindBuffer(GL_ARRAY_BUFFER, 0);

        f->glGenTextures(1, &m_texY);
        f->glGenTextures(1, &m_texU);
        f->glGenTextures(1, &m_texV);
        f->glGenTextures(1, &m_texUV);
        setupTextureParams(f, m_texY);
        setupTextureParams(f, m_texU);
        setupTextureParams(f, m_texV);
        setupTextureParams(f, m_texUV);
    }
    return true;
}

bool VideoEffectGLProcessor::ensureProgram(const EffectSpec &spec, bool nv12)
{
    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    const bool forceCore = qEnvironmentVariableIntValue("DAQSTER_GL_FORCE_CORE") == 1;
    const bool core = forceCore
        || (mgr.context()->format().profile() == QSurfaceFormat::CoreProfile);

    const QString key = spec.id
        + (nv12 ? QStringLiteral(":nv12") : QStringLiteral(":420p"))
        + (core ? QStringLiteral(":core") : QStringLiteral(":compat"));
    if (m_program != nullptr && m_programKey == key)
        return true;

    m_programKey = key;
    m_useCore = core;
    m_useNv12 = nv12;

    delete m_program;
    m_program = nullptr;

    auto *prog = new QOpenGLShaderProgram();
    const QString vert = buildVertexSource(core);
    const QString frag = buildEffectFragmentSource(core, nv12, spec.glslBody);
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

bool VideoEffectGLProcessor::uploadFrame(const QVideoFrame &frame)
{
    if (!frame.isValid())
        return false;
    // Implicit share: cheap local copy so map()/unmap()/bits() work (they are
    // non-const on Qt5; the probe frame itself is never modified).
    QVideoFrame mappable = frame;
    if (!VideoCompat::mapForRead(mappable)) {
        qWarning().noquote() << QStringLiteral("VideoEffectGLProcessor | frame map failed");
        return false;
    }

    const int w = mappable.width();
    const int h = mappable.height();
    const int chromaW = (w + 1) / 2;
    const int chromaH = (h + 1) / 2;

    const GLenum yInternal = m_useCore ? GL_R8 : GL_LUMINANCE;
    const GLenum yFormat = m_useCore ? GL_RED : GL_LUMINANCE;
    const GLenum uvInternal = m_useCore ? GL_RG8 : GL_LUMINANCE_ALPHA;
    const GLenum uvFormat = m_useCore ? GL_RG : GL_LUMINANCE_ALPHA;

    QOpenGLFunctions *f = VideoGLContextManager::instance().context()->functions();
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    f->glBindTexture(GL_TEXTURE_2D, m_texY);
    f->glPixelStorei(GL_UNPACK_ROW_LENGTH, mappable.bytesPerLine(0));
    f->glTexImage2D(GL_TEXTURE_2D, 0, yInternal, w, h, 0,
                    yFormat, GL_UNSIGNED_BYTE, mappable.bits(0));

    if (m_useNv12) {
        f->glBindTexture(GL_TEXTURE_2D, m_texUV);
        f->glPixelStorei(GL_UNPACK_ROW_LENGTH, mappable.bytesPerLine(1) / 2);
        f->glTexImage2D(GL_TEXTURE_2D, 0, uvInternal, chromaW, chromaH, 0,
                        uvFormat, GL_UNSIGNED_BYTE, mappable.bits(1));
    } else {
        f->glBindTexture(GL_TEXTURE_2D, m_texU);
        f->glPixelStorei(GL_UNPACK_ROW_LENGTH, mappable.bytesPerLine(1));
        f->glTexImage2D(GL_TEXTURE_2D, 0, yInternal, chromaW, chromaH, 0,
                        yFormat, GL_UNSIGNED_BYTE, mappable.bits(1));
        f->glBindTexture(GL_TEXTURE_2D, m_texV);
        f->glPixelStorei(GL_UNPACK_ROW_LENGTH, mappable.bytesPerLine(2));
        f->glTexImage2D(GL_TEXTURE_2D, 0, yInternal, chromaW, chromaH, 0,
                        yFormat, GL_UNSIGNED_BYTE, mappable.bits(2));
    }
    f->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    mappable.unmap();
    return true;
}

bool VideoEffectGLProcessor::drawQuad(const EffectSpec &spec, const EffectParams &params)
{
    if (m_program == nullptr || m_fbo == nullptr)
        return false;

    QOpenGLFunctions *f = VideoGLContextManager::instance().context()->functions();
    m_fbo->bind();
    f->glViewport(0, 0, m_fbo->width(), m_fbo->height());

    QOpenGLShaderProgram *prog = m_program;
    prog->bind();

    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D, m_texY);
    prog->setUniformValue("u_texY", 0);
    if (m_useNv12) {
        f->glActiveTexture(GL_TEXTURE1);
        f->glBindTexture(GL_TEXTURE_2D, m_texUV);
        prog->setUniformValue("u_texUV", 1);
    } else {
        f->glActiveTexture(GL_TEXTURE1);
        f->glBindTexture(GL_TEXTURE_2D, m_texU);
        prog->setUniformValue("u_texU", 1);
        f->glActiveTexture(GL_TEXTURE2);
        f->glBindTexture(GL_TEXTURE_2D, m_texV);
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
    if (prog->uniformLocation("u_flipY") != -1)
        prog->setUniformValue("u_flipY", (spec.id == QStringLiteral("flip")) ? 1 : 0);

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
    m_fbo->release();
    return true;
}

QImage VideoEffectGLProcessor::process(const QVideoFrame &frame, const EffectSpec &spec,
                                       const EffectParams &params)
{
    if (!frame.isValid() || spec.id.isEmpty())
        return QImage();

    VideoGLContextManager &mgr = VideoGLContextManager::instance();
    if (!ensureContext())
        return QImage();

    // Ensure doneCurrent() on every exit path.
    VideoGLContextManager::CurrentGuard guard(mgr);

    QString formatName;
    const YuvLayout layout = classifyYuv(frame, &formatName);
    if (layout == YuvLayout::Other) {
        qWarning().noquote() << QStringLiteral("VideoEffectGLProcessor | unsupported format %1 — CPU fallback")
            .arg(formatName);
        return QImage();
    }
    const bool nv12 = (layout == YuvLayout::Nv12);

    if (!ensureProgram(spec, nv12))
        return QImage();

    if (!uploadFrame(frame))
        return QImage();

    const int w = frame.width();
    const int h = frame.height();
    if (m_fbo == nullptr || m_fbo->width() != w || m_fbo->height() != h) {
        delete m_fbo;
        m_fbo = new QOpenGLFramebufferObject(w, h);
        if (!m_fbo->isValid()) {
            qWarning().noquote() << QStringLiteral("VideoEffectGLProcessor | FBO invalid (%1x%2)").arg(w).arg(h);
            return QImage();
        }
    }

    if (!drawQuad(spec, params))
        return QImage();

    // toImage() applies the built-in vertical flip, so the result is top-down.
    return m_fbo->toImage();
}