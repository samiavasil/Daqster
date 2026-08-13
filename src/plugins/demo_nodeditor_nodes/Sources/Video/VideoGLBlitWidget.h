#pragma once

// SPDX-License-Identifier: MIT
//
// Custom OpenGL YUV->RGB blit widget (enabled with DAQSTER_GL_BLIT=1).
//
// Uploads decoded CPU frames (NV12 / YUV420P) as GL textures and converts to
// RGB with a fragment shader — the GPU path used by VideoOutputNode's detached
// display window (much cheaper than the QVideoWidget / QPixmap paths).
//
// Design:
//  - presentFrame(QVideoFrame) keeps the frame (ref-count bump only) and
//    schedules a repaint; paintGL maps it, uploads the Y/U/V planes honoring
//    bytesPerLine strides, and draws a fullscreen quad. Qt5 frames are OWNED
//    copies (VideoCompat::frameToOwnedFrame) so holding them is safe; Qt6
//    frames are the decoded probe frames.
//  - presentImage(QImage) uploads the QImage as an RGBA/BGRA texture (image
//    port / RGB-format fallback).
//  - Shader selection is context-driven: GL 2.0 compatibility (Qt default) =>
//    "#version 120" + texture2D() + GL_LUMINANCE/LUMINANCE_ALPHA; core profile
//    (>= 3.0 CoreProfile) => "#version 150" + texture() + GL_RED/GL_RG.
//    DAQSTER_GL_FORCE_CORE=1 forces the core path for A/B testing.
//  - Matrix/range: DAQSTER_GL_MATRIX=bt709|bt601, DAQSTER_GL_RANGE=full|limited.
//  - The constructor disables vsync (swapInterval 0): on NVIDIA GLX the default
//    swap interval throttles QOpenGLWidget repaints to ~1 Hz, which starves the
//    video pipeline to 1 fps.
//  - paintGL timing is self-logged (qInfo "GLBLIT") every 150 frames so the
//    real map+upload+draw cost can be compared against the [PERF] output.present.

#include <QOpenGLWidget>
#include <QImage>
#include <QString>

#include <QtMultimedia/QVideoFrame>

class QOpenGLShaderProgram;
class QOpenGLVertexArrayObject;

class VideoGLBlitWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit VideoGLBlitWidget(QWidget *parent = nullptr);
    ~VideoGLBlitWidget() override;

    /// Zero-copy present: stores the frame, schedules a repaint.
    void presentFrame(const QVideoFrame &frame);
    /// QImage fallback (image port / RGB formats): stores the image, schedules a repaint.
    void presentImage(const QImage &image);

    QString lastFormatName() const { return m_formatName; }
    bool lastFrameYuv() const { return m_hasYuv; }

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    bool setupShaders();
    bool setupTexture(GLuint *texId, GLenum internalFormat, GLenum format,
                      int width, int height);
    void uploadFrame();
    void drawQuad();
    void logBlitStats(qint64 elapsedNs);

    QOpenGLShaderProgram *m_programNv12 = nullptr;  // NV12 (interleaved UV) program
    QOpenGLShaderProgram *m_program420p = nullptr;  // YUV420P (planar) program
    QOpenGLShaderProgram *m_programRgba = nullptr;  // RGBA/BGRA program
    QOpenGLVertexArrayObject *m_vao = nullptr;

    GLuint m_texY = 0;
    GLuint m_texU = 0;
    GLuint m_texV = 0;
    GLuint m_texUV = 0;
    GLuint m_texRgba = 0;
    GLuint m_vbo = 0;

    bool m_useCore = false;   // core-profile path (GL_RED/RG + #version 150)
    bool m_useNv12 = false;   // interleaved UV plane (plane 1 = U,V pairs)
    bool m_hasYuv = false;    // current payload is a YUV frame (vs QImage)
    int m_matrix = 1;         // 0 = BT.601, 1 = BT.709
    int m_range = 1;          // 0 = limited, 1 = full
    int m_yuvW = 0, m_yuvH = 0;   // frame dimensions (Y plane)
    QString m_formatName;

    int m_attribPos = -1;
    int m_attribTex = -1;
    int m_uniformMatrix = -1;
    int m_uniformRange = -1;
    int m_uniformRgba = -1;

    // paintGL blit stats (self-logged, spike measurement)
    qint64 m_blitSumNs = 0;
    qint64 m_blitMaxNs = 0;
    quint64 m_blitFrames = 0;
    quint64 m_blitFailures = 0;

    QVideoFrame m_frame;  // held until the next present (Qt6: probe frame, Qt5: owned copy)
    QImage m_image;       // QImage fallback payload
};
