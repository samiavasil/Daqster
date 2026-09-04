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
