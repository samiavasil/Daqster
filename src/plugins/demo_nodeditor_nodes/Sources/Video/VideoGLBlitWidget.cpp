#include "VideoGLBlitWidget.h"

#include "NodeDataTypes/VideoFrameData.h"
#include "VideoCompat.h"
#include "VideoGLShaders.h"

#include <QElapsedTimer>
#include <QOpenGLContext>
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
// and QVideoFrame planes is the top row, so no flip is needed.
const GLfloat kQuadVertices[] = {
    -1.0f, -1.0f, 0.0f, 1.0f,
     1.0f, -1.0f, 1.0f, 1.0f,
    -1.0f,  1.0f, 0.0f, 0.0f,
     1.0f,  1.0f, 1.0f, 0.0f,
};

// ── Quad for GPU-resident FBO textures (effect / custom-shader output) ───────
// FBO-attached textures are bottom-up (row 0 = scene bottom), so the v
// coordinate is inverted relative to the top-down QImage/YUV quads above.
// Drawing with v' = 1 - v compensates for the FBO convention; without this the
// display inversion would cancel the flip effect's vertical flip (REQ-SW-PL-032).
const GLfloat kQuadVerticesFbo[] = {
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
};

/// Classify a frame's pixel layout for the shader selection and report a
/// human-readable format name (Qt6: QVideoFrameFormat names; Qt5: the
/// QVideoFrame::PixelFormat names — no QVideoFrameFormat on Qt5).
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

VideoGLBlitWidget::VideoGLBlitWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    const QString matrixEnv = qEnvironmentVariable("DAQSTER_GL_MATRIX", QStringLiteral("bt709")).toLower();
    const QString rangeEnv = qEnvironmentVariable("DAQSTER_GL_RANGE", QStringLiteral("full")).toLower();
    m_matrix = (matrixEnv == QStringLiteral("bt601")) ? 0 : 1;
    m_range = (rangeEnv == QStringLiteral("limited")) ? 0 : 1;

    // Disable vsync so presents are free-running. On this NVIDIA GLX setup the
    // default swap interval throttles QOpenGLWidget repaints to ~1 Hz
    // (verified with a minimal QOpenGLWidget repro), which starves the video
    // pipeline to 1 fps. The default surface format must be set BEFORE the
    // widget's GL context is created (lazily on first show/paint).
    QSurfaceFormat fmt = format();
    fmt.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(fmt);
    setFormat(fmt);
}

VideoGLBlitWidget::~VideoGLBlitWidget() = default;

void VideoGLBlitWidget::presentFrame(const QVideoFrame &frame)
{
    if (!frame.isValid())
        return;

    m_textureOwner.reset();
    m_textureHandle = VideoTextureHandle();
    m_frame = frame;
    m_image = QImage();
    m_yuvW = frame.width();
    m_yuvH = frame.height();

    QString fmt;
    const YuvLayout layout = classifyYuv(frame, &fmt);
    m_formatName = fmt;
    switch (layout) {
    case YuvLayout::Nv12:
        m_hasYuv = true;
        m_useNv12 = true;
        break;
    case YuvLayout::Yuv420p:
        m_hasYuv = true;
        m_useNv12 = false;
        break;
    default:
        // RGB formats or anything else: fall back to a CPU QImage conversion
        // inside the GL path (honest — the conversion cost lands in the blit).
        // NOTE: this widget receives a raw QVideoFrame (not a VideoFrameData),
        // so it cannot reuse the lazy asImage() cache (REQ-SW-PL-032) — the
        // conversion here is per-presentation and unavoidable on this path.
        m_hasYuv = false;
        m_image = VideoCompat::frameToImage(frame);
        m_formatName += QStringLiteral(" -> toImage(%1)").arg(m_image.format());
        break;
    }
    update();
}

void VideoGLBlitWidget::presentImage(const QImage &image)
{
    if (image.isNull())
        return;
    m_textureOwner.reset();
    m_textureHandle = VideoTextureHandle();
    m_image = image;
    m_hasYuv = false;
    m_frame = QVideoFrame();
    m_formatName = QStringLiteral("QImage(%1)").arg(m_image.format());
    update();
}

void VideoGLBlitWidget::presentTexture(const VideoTextureHandle &handle,
                                       std::shared_ptr<VideoFrameData> owner)
{
    if (handle.texY == 0 || handle.width <= 0 || handle.height <= 0)
        return;
    // Hold the owning frame so the texture stays alive until the next present
    // (the deferred repaint binds it in the shared GL context).
    m_textureOwner = std::move(owner);
    m_textureHandle = handle;
    m_hasYuv = false;
    m_image = QImage();
    m_frame = QVideoFrame();
    m_yuvW = handle.width;
    m_yuvH = handle.height;
    m_formatName = QStringLiteral("Texture(RGBA)");
    update();
}

void VideoGLBlitWidget::presentYuvTexture(const VideoTextureHandle &handle,
                                          std::shared_ptr<VideoFrameData> owner)
{
    if (handle.texY == 0 || handle.width <= 0 || handle.height <= 0) {
        // Invalid handle — fall back to the CPU frame path.
        if (owner && owner->hasFrame())
            presentFrame(owner->frame());
        return;
    }
    // Hold the owning frame so the cached textures stay alive until the next
    // present (the deferred repaint binds them in the shared GL context).
    m_textureOwner = std::move(owner);
    m_textureHandle = handle;
    m_hasYuv = true;
    m_useNv12 = handle.nv12;
    m_image = QImage();
    m_frame = QVideoFrame();
    m_yuvW = handle.width;
    m_yuvH = handle.height;
    m_formatName = handle.nv12 ? QStringLiteral("Texture(NV12)")
                               : QStringLiteral("Texture(YUV420P)");
    update();
}

void VideoGLBlitWidget::initializeGL()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    const QSurfaceFormat fmt = QOpenGLContext::currentContext()->format();

    const bool forceCore = qEnvironmentVariableIntValue("DAQSTER_GL_FORCE_CORE") == 1;
    m_useCore = forceCore || (fmt.profile() == QSurfaceFormat::CoreProfile);

    qInfo().noquote() << QStringLiteral("GLBLIT | context GL%1.%2 profile=%3 useCore=%4 matrix=%5 range=%6")
        .arg(fmt.majorVersion()).arg(fmt.minorVersion())
        .arg(static_cast<int>(fmt.profile())).arg(m_useCore)
        .arg(m_matrix == 1 ? QStringLiteral("bt709") : QStringLiteral("bt601"))
        .arg(m_range == 1 ? QStringLiteral("full") : QStringLiteral("limited"));

    // ── Programs (NV12 + YUV420P + RGBA; version-appropriate GLSL) ───────────
    const auto makeProgram = [this](const QString &vert, const QString &frag) {
        auto *prog = new QOpenGLShaderProgram(this);
        if (!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vert)
            || !prog->addShaderFromSourceCode(QOpenGLShader::Fragment, frag)
            || !prog->link()) {
            qWarning().noquote() << QStringLiteral("GLBLIT | shader link failed: %1")
                .arg(prog->log());
            prog->deleteLater();
            return static_cast<QOpenGLShaderProgram *>(nullptr);
        }
        return prog;
    };

    const QString vert = buildVertexSource(m_useCore);
    m_programNv12 = makeProgram(vert, buildYuvFragmentSource(m_useCore, true));
    m_program420p = makeProgram(vert, buildYuvFragmentSource(m_useCore, false));
    m_programRgba = makeProgram(vert, buildRgbaFragmentSource(m_useCore));

    // ── Quad VBO + (core) VAO ────────────────────────────────────────────────
    f->glGenBuffers(1, &m_vbo);
    f->glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    f->glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
    f->glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Flipped-v quad VBO for bottom-up FBO textures (REQ-SW-PL-032).
    f->glGenBuffers(1, &m_vboFbo);
    f->glBindBuffer(GL_ARRAY_BUFFER, m_vboFbo);
    f->glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerticesFbo), kQuadVerticesFbo, GL_STATIC_DRAW);
    f->glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (m_useCore) {
        m_vao = new QOpenGLVertexArrayObject(this);
        m_vao->create();
    }

    // ── Textures ─────────────────────────────────────────────────────────────
    f->glGenTextures(1, &m_texY);
    f->glGenTextures(1, &m_texU);
    f->glGenTextures(1, &m_texV);
    f->glGenTextures(1, &m_texUV);
    f->glGenTextures(1, &m_texRgba);
    setupTextureParams(f, m_texY);
    setupTextureParams(f, m_texU);
    setupTextureParams(f, m_texV);
    setupTextureParams(f, m_texUV);
    setupTextureParams(f, m_texRgba);
}

void VideoGLBlitWidget::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);
}

void VideoGLBlitWidget::paintGL()
{
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    const int vw = width() * devicePixelRatio();
    const int vh = height() * devicePixelRatio();

    f->glViewport(0, 0, vw, vh);
    f->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    f->glClear(GL_COLOR_BUFFER_BIT);

    // ── Letterbox to preserve aspect ratio ───────────────────────────────────
    int srcW = 0, srcH = 0;
    if (m_hasYuv) {
        srcW = m_yuvW;
        srcH = m_yuvH;
    } else if (m_textureHandle.texY != 0) {
        srcW = m_textureHandle.width;
        srcH = m_textureHandle.height;
    } else if (!m_image.isNull()) {
        srcW = m_image.width();
        srcH = m_image.height();
    }
    if (srcW > 0 && srcH > 0) {
        const float srcAspect = static_cast<float>(srcW) / static_cast<float>(srcH);
        const float dstAspect = static_cast<float>(vw) / static_cast<float>(vh);
        int x = 0, y = 0, w = vw, h = vh;
        if (srcAspect > dstAspect) {
            w = vw;
            h = qMax(1, static_cast<int>(vw / srcAspect + 0.5f));
            y = (vh - h) / 2;
        } else {
            h = vh;
            w = qMax(1, static_cast<int>(vh * srcAspect + 0.5f));
            x = (vw - w) / 2;
        }
        f->glViewport(x, y, w, h);
    }

    QElapsedTimer timer;
    timer.start();
    uploadFrame();
    drawQuad();
    logBlitStats(timer.nsecsElapsed());
}

void VideoGLBlitWidget::uploadFrame()
{
    if (m_textureHandle.texY != 0) {
        // GPU-resident frame (RGBA effect output OR cached YUV planes from
        // asTexture) — already on the GPU, nothing to upload
        // (REQ-SW-PL-032 Stage 2A/2B).
        return;
    }
    if (m_hasYuv) {
        if (!m_frame.isValid())
            return;
        if (!VideoCompat::mapForRead(m_frame)) {
            ++m_blitFailures;
            return;
        }

        const GLenum yInternal = m_useCore ? GL_R8 : GL_LUMINANCE;
        const GLenum yFormat = m_useCore ? GL_RED : GL_LUMINANCE;
        const GLenum uvInternal = m_useCore ? GL_RG8 : GL_LUMINANCE_ALPHA;
        const GLenum uvFormat = m_useCore ? GL_RG : GL_LUMINANCE_ALPHA;

        const int chromaW = (m_yuvW + 1) / 2;
        const int chromaH = (m_yuvH + 1) / 2;

        QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
        f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        f->glBindTexture(GL_TEXTURE_2D, m_texY);
        f->glPixelStorei(GL_UNPACK_ROW_LENGTH, m_frame.bytesPerLine(0));
        f->glTexImage2D(GL_TEXTURE_2D, 0, yInternal, m_yuvW, m_yuvH, 0,
                        yFormat, GL_UNSIGNED_BYTE, m_frame.bits(0));

        if (m_useNv12) {
            f->glBindTexture(GL_TEXTURE_2D, m_texUV);
            f->glPixelStorei(GL_UNPACK_ROW_LENGTH, m_frame.bytesPerLine(1) / 2);
            f->glTexImage2D(GL_TEXTURE_2D, 0, uvInternal, chromaW, chromaH, 0,
                            uvFormat, GL_UNSIGNED_BYTE, m_frame.bits(1));
        } else {
            f->glBindTexture(GL_TEXTURE_2D, m_texU);
            f->glPixelStorei(GL_UNPACK_ROW_LENGTH, m_frame.bytesPerLine(1));
            f->glTexImage2D(GL_TEXTURE_2D, 0, yInternal, chromaW, chromaH, 0,
                            yFormat, GL_UNSIGNED_BYTE, m_frame.bits(1));
            f->glBindTexture(GL_TEXTURE_2D, m_texV);
            f->glPixelStorei(GL_UNPACK_ROW_LENGTH, m_frame.bytesPerLine(2));
            f->glTexImage2D(GL_TEXTURE_2D, 0, yInternal, chromaW, chromaH, 0,
                            yFormat, GL_UNSIGNED_BYTE, m_frame.bits(2));
        }
        f->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        m_frame.unmap();
        return;
    }

    // ── QImage path (image port / RGB-format frames) ─────────────────────────
    if (m_image.isNull())
        return;

    QImage img = m_image;
    GLenum internalFormat = m_useCore ? GL_RGBA8 : GL_RGBA;
    GLenum format = GL_RGBA;
    int bytesPerPixel = 4;

    switch (img.format()) {
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
        // little-endian 0xffRRGGBB → memory bytes B,G,R,A → GL_BGRA
        format = GL_BGRA;
        break;
    case QImage::Format_RGBA8888:
        format = GL_RGBA;
        break;
    case QImage::Format_RGB888:
        format = GL_RGB;
        internalFormat = m_useCore ? GL_RGB8 : GL_RGB;
        bytesPerPixel = 3;
        break;
    default:
        img = img.convertToFormat(QImage::Format_RGBA8888);
        format = GL_RGBA;
        break;
    }

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    f->glBindTexture(GL_TEXTURE_2D, m_texRgba);
    f->glPixelStorei(GL_UNPACK_ROW_LENGTH, img.bytesPerLine() / bytesPerPixel);
    f->glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, img.width(), img.height(), 0,
                    format, GL_UNSIGNED_BYTE, img.constBits());
    f->glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void VideoGLBlitWidget::drawQuad()
{
    QOpenGLShaderProgram *prog = nullptr;
    if (m_hasYuv)
        prog = m_useNv12 ? m_programNv12 : m_program420p;
    else if (m_textureHandle.texY != 0 || !m_image.isNull())
        prog = m_programRgba;

    if (prog == nullptr)
        return;

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    prog->bind();

    // GPU-resident FBO textures are bottom-up, so they use the flipped-v quad
    // (REQ-SW-PL-032); QImage/YUV quads are top-down and use the standard quad.
    bool useFboQuad = false;

    if (m_textureHandle.texY != 0 && m_hasYuv) {
        // GPU-resident YUV textures (asTexture cache, REQ-SW-PL-032): bind the
        // cached planes directly — no duplicate upload. Uploaded from the
        // top-down CPU frame, so the standard (non-FBO) quad is used.
        f->glActiveTexture(GL_TEXTURE0);
        f->glBindTexture(GL_TEXTURE_2D, m_textureHandle.texY);
        prog->setUniformValue("u_texY", 0);
        if (m_useNv12) {
            f->glActiveTexture(GL_TEXTURE1);
            f->glBindTexture(GL_TEXTURE_2D, m_textureHandle.texUV);
            prog->setUniformValue("u_texUV", 1);
        } else {
            f->glActiveTexture(GL_TEXTURE1);
            f->glBindTexture(GL_TEXTURE_2D, m_textureHandle.texU);
            prog->setUniformValue("u_texU", 1);
            f->glActiveTexture(GL_TEXTURE2);
            f->glBindTexture(GL_TEXTURE_2D, m_textureHandle.texV);
            prog->setUniformValue("u_texV", 2);
        }
        prog->setUniformValue("u_matrix", m_matrix);
        prog->setUniformValue("u_range", m_range);
    } else if (m_textureHandle.texY != 0) {
        // GPU-resident RGBA texture (effect output): bind it directly — no
        // upload (REQ-SW-PL-032 Stage 2B).
        useFboQuad = true;
        f->glActiveTexture(GL_TEXTURE0);
        f->glBindTexture(GL_TEXTURE_2D, m_textureHandle.texY);
        prog->setUniformValue("u_tex", 0);
    } else if (m_hasYuv) {
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
    } else {
        f->glActiveTexture(GL_TEXTURE0);
        f->glBindTexture(GL_TEXTURE_2D, m_texRgba);
        prog->setUniformValue("u_tex", 0);
    }

    const int posLoc = prog->attributeLocation("a_position");
    const int texLoc = prog->attributeLocation("a_texcoord");

    if (m_vao != nullptr)
        m_vao->bind();

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
}

void VideoGLBlitWidget::logBlitStats(qint64 elapsedNs)
{
    ++m_blitFrames;
    m_blitSumNs += elapsedNs;
    if (elapsedNs > m_blitMaxNs)
        m_blitMaxNs = elapsedNs;

    if (m_blitFrames % 150 == 0) {
        const double avgUs = static_cast<double>(m_blitSumNs) / static_cast<double>(m_blitFrames) / 1000.0;
        const double maxUs = static_cast<double>(m_blitMaxNs) / 1000.0;
        qInfo().noquote() << QStringLiteral("GLBLIT | fmt=%1 | frames=%2 | avg=%3 us | max=%4 us | failures=%5")
            .arg(m_formatName)
            .arg(m_blitFrames)
            .arg(avgUs, 0, 'f', 1)
            .arg(maxUs, 0, 'f', 1)
            .arg(m_blitFailures);
        m_blitSumNs = 0;
        m_blitMaxNs = 0;
    }
}
