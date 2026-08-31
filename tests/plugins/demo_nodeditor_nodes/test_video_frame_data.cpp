#include "test_video_frame_data.h"
#include "NodeDataTypes/VideoFrameData.h"

#include <QImage>
#include <QVideoFrame>

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
#endif

QTEST_GUILESS_MAIN(VideoFrameDataTest)
