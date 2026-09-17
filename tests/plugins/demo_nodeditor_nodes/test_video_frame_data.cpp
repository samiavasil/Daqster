#include "test_video_frame_data.h"
#include "NodeDataTypes/VideoFrameData.h"

#include <QImage>
#include <QVideoFrame>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtMultimedia/QAbstractVideoBuffer>

namespace {

/// Minimal planar video buffer for constructing NV12/YUV420P QVideoFrames in
/// tests (mirrors VideoCompat::OwnedPlanarVideoBuffer).
class TestPlanarVideoBuffer : public QAbstractPlanarVideoBuffer
{
public:
    TestPlanarVideoBuffer(const QList<QByteArray> &planes, const QList<int> &strides)
        : QAbstractPlanarVideoBuffer(QAbstractVideoBuffer::NoHandle)
        , m_planes(planes)
        , m_strides(strides)
    {
        for (int i = 0; i < m_planes.size(); ++i) {
            m_planeData[i] = reinterpret_cast<uchar *>(
                const_cast<char *>(m_planes.at(i).constData()));
        }
    }

    MapMode mapMode() const override
    {
        return QAbstractVideoBuffer::ReadOnly;
    }

    int map(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]) override
    {
        if (mode != QAbstractVideoBuffer::ReadOnly)
            return 0;
        const int planeCount = m_planes.size();
        for (int i = 0; i < planeCount; ++i) {
            bytesPerLine[i] = m_strides.at(i);
            data[i] = m_planeData[i];
        }
        if (numBytes != nullptr) {
            int total = 0;
            for (const QByteArray &plane : m_planes)
                total += plane.size();
            *numBytes = total;
        }
        return planeCount;
    }

    void unmap() override {}

private:
    QList<QByteArray> m_planes;
    QList<int> m_strides;
    uchar *m_planeData[4] = {nullptr, nullptr, nullptr, nullptr};
};

} // namespace
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
namespace {

/// Build a solid-color NV12/YUV420P frame on Qt6 (QVideoFrameFormat + a
/// WriteOnly map — the Qt6 equivalent of the Qt5 TestPlanarVideoBuffer).
QVideoFrame makePlanarFrame(const QSize &size, QVideoFrameFormat::PixelFormat pf,
                            uchar yVal, uchar uVal, uchar vVal)
{
    QVideoFrameFormat fmt(size, pf);
    QVideoFrame frame(fmt);
    if (!frame.isValid() || !frame.map(QVideoFrame::WriteOnly))
        return QVideoFrame();

    const int w = size.width();
    const int h = size.height();
    const int chromaW = (w + 1) / 2;
    const int chromaH = (h + 1) / 2;

    const int yStride = frame.bytesPerLine(0);
    uchar *y = frame.bits(0);
    for (int row = 0; row < h; ++row)
        for (int col = 0; col < w; ++col)
            y[row * yStride + col] = yVal;

    const int uStride = frame.bytesPerLine(1);
    uchar *u = frame.bits(1);
    if (pf == QVideoFrameFormat::Format_NV12) {
        for (int row = 0; row < chromaH; ++row)
            for (int col = 0; col < chromaW; ++col) {
                u[row * uStride + col * 2] = uVal;
                u[row * uStride + col * 2 + 1] = vVal;
            }
    } else {
        const int vStride = frame.bytesPerLine(2);
        uchar *v = frame.bits(2);
        for (int row = 0; row < chromaH; ++row)
            for (int col = 0; col < chromaW; ++col) {
                u[row * uStride + col] = uVal;
                v[row * vStride + col] = vVal;
            }
    }
    frame.unmap();
    return frame;
}

} // namespace
#endif

void VideoFrameDataTest::defaultCtor_hasNoFrame()
{
    VideoFrameData vfd;
    QVERIFY(!vfd.hasFrame());
    const auto t = vfd.type();
    QCOMPARE(t.id, QStringLiteral("video-frame"));
    QCOMPARE(t.name, QStringLiteral("Video Frame"));
}

void VideoFrameDataTest::fromQImage_hasFrame()
{
    QImage img(320, 240, QImage::Format_ARGB32);
    img.fill(Qt::red);

    QVideoFrame vf(img);
    QVERIFY(vf.isValid());

    VideoFrameData vfd(vf);
    QVERIFY(vfd.hasFrame());
    QCOMPARE(vfd.frame().size(), QSize(320, 240));
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)

void VideoFrameDataTest::fromQImage_toImage_roundTrip()
{
    QImage img(320, 240, QImage::Format_ARGB32);
    img.fill(Qt::red);

    QVideoFrame vf(img);
    VideoFrameData vfd(vf);
    QVERIFY(vfd.hasFrame());

    // Map the frame for reading so toImage() can access the pixel data.
    QVideoFrame &frame = const_cast<QVideoFrame &>(vfd.frame());
    QVERIFY(frame.map(QVideoFrame::ReadOnly));
    const QImage roundTripped = frame.toImage();
    frame.unmap();

    QCOMPARE(roundTripped.size(), img.size());
    QCOMPARE(roundTripped.pixelColor(100, 100), Qt::red);
}

#endif  // QT_VERSION >= 0x060000

void VideoFrameDataTest::setFrame_replaces()
{
    QImage img1(100, 100, QImage::Format_ARGB32);
    img1.fill(Qt::blue);
    QVideoFrame vf1(img1);

    VideoFrameData vfd(vf1);
    QVERIFY(vfd.hasFrame());
    QCOMPARE(vfd.frame().size(), QSize(100, 100));

    // Replace with a different frame.
    QImage img2(200, 200, QImage::Format_ARGB32);
    img2.fill(Qt::green);
    QVideoFrame vf2(img2);
    vfd.setFrame(vf2);

    QVERIFY(vfd.hasFrame());
    QCOMPARE(vfd.frame().size(), QSize(200, 200));
}

// ── frameToImageCpu (REQ-SW-PL-039): thread-safe CPU conversion ─────────────
//
// The ComputePool worker must convert its frame copy WITHOUT toImage()/image():
// Qt6 toImage() can route through RHI/GPU conversion — forbidden off the GUI
// thread (QTBUG-131107); Qt5 image() returns null for NV12/YUV420P.
// frameToImageCpu() is the pure-CPU replacement: RGB formats wrap the mapped
// bits, NV12/YUV420P go through the BT.601 yuvToImage() path.

void VideoFrameDataTest::frameToImageCpu_rgb32_wraps()
{
    QImage img(64, 48, QImage::Format_RGB32);
    img.fill(QColor(12, 34, 56));
    QVideoFrame vf(img);
    QVERIFY(vf.isValid());

    const QImage out = VideoFrameData::frameToImageCpu(vf);
    QVERIFY(!out.isNull());
    QCOMPARE(out.size(), img.size());
    QCOMPARE(out.pixelColor(10, 10), QColor(12, 34, 56));
}

void VideoFrameDataTest::frameToImageCpu_argb32_wraps()
{
    QImage img(64, 48, QImage::Format_ARGB32);
    img.fill(QColor(200, 100, 50, 128));
    QVideoFrame vf(img);
    QVERIFY(vf.isValid());

    const QImage out = VideoFrameData::frameToImageCpu(vf);
    QVERIFY(!out.isNull());
    QCOMPARE(out.size(), img.size());
    QCOMPARE(out.pixelColor(10, 10), QColor(200, 100, 50, 128));
}

void VideoFrameDataTest::frameToImageCpu_nv12_converts()
{
    // 4x4 NV12. BT.601 limited-range red: Y=81, U=90, V=240 → RGB ≈ (255, 0, 0).
    const QSize size(4, 4);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVideoFrame vf = makePlanarFrame(size, QVideoFrameFormat::Format_NV12, 81, 90, 240);
#else
    QByteArray y(size.width() * size.height(), static_cast<char>(81));
    QByteArray uv((size.width() / 2) * (size.height() / 2) * 2, 0);
    for (int i = 0; i < uv.size(); i += 2) {
        uv[i] = static_cast<char>(90);       // U
        uv[i + 1] = static_cast<char>(240);  // V
    }
    QVideoFrame vf(new TestPlanarVideoBuffer({y, uv}, {size.width(), size.width()}),
                   size, QVideoFrame::Format_NV12);
#endif
    QVERIFY(vf.isValid());

    const QImage img = VideoFrameData::frameToImageCpu(vf);
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), size);
    QCOMPARE(img.pixelColor(0, 0).red(), 255);
    QCOMPARE(img.pixelColor(0, 0).green(), 0);
    QCOMPARE(img.pixelColor(0, 0).blue(), 0);
}

void VideoFrameDataTest::frameToImageCpu_yuv420p_converts()
{
    // 4x4 YUV420P. BT.601 limited-range red: Y=81, U=90, V=240 → RGB ≈ (255, 0, 0).
    const QSize size(4, 4);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QVideoFrame vf = makePlanarFrame(size, QVideoFrameFormat::Format_YUV420P, 81, 90, 240);
#else
    QByteArray y(size.width() * size.height(), static_cast<char>(81));
    QByteArray u((size.width() / 2) * (size.height() / 2), static_cast<char>(90));
    QByteArray v((size.width() / 2) * (size.height() / 2), static_cast<char>(240));
    QVideoFrame vf(new TestPlanarVideoBuffer({y, u, v},
                                             {size.width(), size.width() / 2, size.width() / 2}),
                   size, QVideoFrame::Format_YUV420P);
#endif
    QVERIFY(vf.isValid());

    const QImage img = VideoFrameData::frameToImageCpu(vf);
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), size);
    QCOMPARE(img.pixelColor(0, 0).red(), 255);
    QCOMPARE(img.pixelColor(0, 0).green(), 0);
    QCOMPARE(img.pixelColor(0, 0).blue(), 0);
}

void VideoFrameDataTest::frameToImageCpu_unsupported_returnsNull()
{
    QVideoFrame invalid;
    QVERIFY(!invalid.isValid());
    QVERIFY(VideoFrameData::frameToImageCpu(invalid).isNull());
}

// ── Qt5 NV12-direct (REQ-SW-PL-020): VideoFrameData is no longer a stub ──────
//
// The Qt5 default-constructed frame is still invalid; the type becomes valid
// when it wraps a QVideoFrame (sources build owned copies via
// VideoCompat::frameToOwnedFrame). Constructing a QImage-backed frame exercises
// the same value semantics as on Qt6.
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void VideoFrameDataTest::qt5_wrapsOwnedFrames()
{
    QImage img(64, 64, QImage::Format_ARGB32);
    img.fill(Qt::cyan);
    QVideoFrame vf(img);
    QVERIFY(vf.isValid());

    VideoFrameData vfd(vf);
    QVERIFY(vfd.hasFrame());
    QCOMPARE(vfd.frame().size(), QSize(64, 64));
}

// ── Qt5 NV12 → QImage (REQ-SW-PL-039 Bug B) ──────────────────────────────────
//
// QVideoFrame::image() on Qt5 only wraps RGB formats — NV12 returns a null
// QImage. asImage() must fall back to the manual YUV→RGB conversion so CPU
// effects work on real (NV12 owned-copy) video frames.
void VideoFrameDataTest::qt5_nv12_asImage_converts()
{
    // 4x4 NV12: Y plane (16 bytes) + interleaved UV (2x2 chroma * 2 = 8 bytes).
    // BT.601 limited-range red: Y=81, U=90, V=240 → RGB ≈ (255, 0, 0).
    const int w = 4, h = 4;
    QByteArray y(w * h, static_cast<char>(81));
    QByteArray uv((w / 2) * (h / 2) * 2, 0);
    for (int i = 0; i < uv.size(); i += 2) {
        uv[i] = static_cast<char>(90);       // U
        uv[i + 1] = static_cast<char>(240);  // V
    }
    QVideoFrame vf(new TestPlanarVideoBuffer({y, uv}, {w, w}), QSize(w, h),
                   QVideoFrame::Format_NV12);
    QVERIFY(vf.isValid());

    VideoFrameData vfd(vf);
    QVERIFY(vfd.hasFrame());
    const QImage img = vfd.asImage();
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), QSize(w, h));
    QCOMPARE(img.pixelColor(0, 0).red(), 255);
    QCOMPARE(img.pixelColor(0, 0).green(), 0);
    QCOMPARE(img.pixelColor(0, 0).blue(), 0);
}

// ── Qt5 YUV420P → QImage (REQ-SW-PL-039 Bug B) ───────────────────────────────
void VideoFrameDataTest::qt5_yuv420p_asImage_converts()
{
    // 4x4 YUV420P: Y (16) + U (4) + V (4). BT.601 red: Y=81, U=90, V=240.
    const int w = 4, h = 4;
    QByteArray y(w * h, static_cast<char>(81));
    QByteArray u((w / 2) * (h / 2), static_cast<char>(90));
    QByteArray v((w / 2) * (h / 2), static_cast<char>(240));
    QVideoFrame vf(new TestPlanarVideoBuffer({y, u, v}, {w, w / 2, w / 2}),
                   QSize(w, h), QVideoFrame::Format_YUV420P);
    QVERIFY(vf.isValid());

    VideoFrameData vfd(vf);
    QVERIFY(vfd.hasFrame());
    const QImage img = vfd.asImage();
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), QSize(w, h));
    QCOMPARE(img.pixelColor(0, 0).red(), 255);
    QCOMPARE(img.pixelColor(0, 0).green(), 0);
    QCOMPARE(img.pixelColor(0, 0).blue(), 0);
}
#endif

QTEST_GUILESS_MAIN(VideoFrameDataTest)
