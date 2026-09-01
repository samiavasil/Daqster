#include <QtTest>
#include <QImage>

#include "VideoTransformOps.h"
#include "test_video_transform_ops.h"

// REQ-SW-PL-018/019 video transform unit tests. Every expected value below was
// computed from the exact arithmetic in VideoTransformOps.cpp (integer
// truncation for grayscale, static_cast<int> for the double-based sepia and
// contrast ops, per-channel integer division for the two-pass box blur).
namespace {

QImage solidImage(int w, int h, int r, int g, int b)
{
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(qRgb(r, g, b));
    return img;
}

QImage pixelImage1x1(int r, int g, int b)
{
    return solidImage(1, 1, r, g, b);
}

} // namespace

void TestVideoTransformOps::swapRedBlue_swapsChannels()
{
    const QImage result = VideoTransformOps::swapRedBlue(pixelImage1x1(10, 20, 30));
    QVERIFY(!result.isNull());
    QCOMPARE(qRed(result.pixel(0, 0)), 30);
    QCOMPARE(qGreen(result.pixel(0, 0)), 20);
    QCOMPARE(qBlue(result.pixel(0, 0)), 10);
}

void TestVideoTransformOps::swapRedBlue_inputNotModified()
{
    // The operations take a const QImage& and must never mutate the input.
    const QImage input = pixelImage1x1(10, 20, 30);
    VideoTransformOps::swapRedBlue(input);
    QCOMPARE(qRed(input.pixel(0, 0)), 10);
    QCOMPARE(qGreen(input.pixel(0, 0)), 20);
    QCOMPARE(qBlue(input.pixel(0, 0)), 30);
}

void TestVideoTransformOps::grayscale_luminance()
{
    // (299*100 + 587*150 + 114*200)/1000 = 140750/1000 = 140 (int division).
    const QImage result = VideoTransformOps::grayscale(pixelImage1x1(100, 150, 200));
    QVERIFY(!result.isNull());
    QCOMPARE(qRed(result.pixel(0, 0)), 140);
    QCOMPARE(qGreen(result.pixel(0, 0)), 140);
    QCOMPARE(qBlue(result.pixel(0, 0)), 140);
}

void TestVideoTransformOps::invert_colors()
{
    const QImage result = VideoTransformOps::invert(pixelImage1x1(10, 20, 30));
    QVERIFY(!result.isNull());
    QCOMPARE(qRed(result.pixel(0, 0)), 245);
    QCOMPARE(qGreen(result.pixel(0, 0)), 235);
    QCOMPARE(qBlue(result.pixel(0, 0)), 225);
}

void TestVideoTransformOps::brightness_deltaZeroUnchanged()
{
    const QImage result = VideoTransformOps::brightness(pixelImage1x1(10, 20, 30), 0);
    QVERIFY(!result.isNull());
    QCOMPARE(qRed(result.pixel(0, 0)), 10);
    QCOMPARE(qGreen(result.pixel(0, 0)), 20);
    QCOMPARE(qBlue(result.pixel(0, 0)), 30);
}

void TestVideoTransformOps::brightness_clamps()
{
    // delta +100 -> shift 255: 10/20/30 + 255 clamps to 255.
    QImage bright = VideoTransformOps::brightness(pixelImage1x1(10, 20, 30), 100);
    QCOMPARE(qRed(bright.pixel(0, 0)), 255);
    QCOMPARE(qGreen(bright.pixel(0, 0)), 255);
    QCOMPARE(qBlue(bright.pixel(0, 0)), 255);

    // delta -100 -> shift -255: clamps to 0.
    QImage dark = VideoTransformOps::brightness(pixelImage1x1(10, 20, 30), -100);
    QCOMPARE(qRed(dark.pixel(0, 0)), 0);
    QCOMPARE(qGreen(dark.pixel(0, 0)), 0);
    QCOMPARE(qBlue(dark.pixel(0, 0)), 0);
}

void TestVideoTransformOps::contrast_percent100Unchanged()
{
    const QImage result = VideoTransformOps::contrast(pixelImage1x1(10, 20, 30), 100);
    QVERIFY(!result.isNull());
    QCOMPARE(qRed(result.pixel(0, 0)), 10);
    QCOMPARE(qGreen(result.pixel(0, 0)), 20);
    QCOMPARE(qBlue(result.pixel(0, 0)), 30);
}

void TestVideoTransformOps::contrast_clampsAndMidGray()
{
    // percent 200 on (10,20,30): (c-128)*2 + 128 is negative -> clamps to 0.
    QImage high = VideoTransformOps::contrast(pixelImage1x1(10, 20, 30), 200);
    QCOMPARE(qRed(high.pixel(0, 0)), 0);
    QCOMPARE(qGreen(high.pixel(0, 0)), 0);
    QCOMPARE(qBlue(high.pixel(0, 0)), 0);

    // percent 0: (c-128)*0 + 128 = 128 for every channel.
    QImage flat = VideoTransformOps::contrast(pixelImage1x1(200, 100, 50), 0);
    QCOMPARE(qRed(flat.pixel(0, 0)), 128);
    QCOMPARE(qGreen(flat.pixel(0, 0)), 128);
    QCOMPARE(qBlue(flat.pixel(0, 0)), 128);

    // percent 50 on 255: (255-128)*0.5 + 128 = 191.5 -> int truncation -> 191.
    QImage half = VideoTransformOps::contrast(pixelImage1x1(255, 255, 255), 50);
    QCOMPARE(qRed(half.pixel(0, 0)), 191);
    QCOMPARE(qGreen(half.pixel(0, 0)), 191);
    QCOMPARE(qBlue(half.pixel(0, 0)), 191);
}

void TestVideoTransformOps::blur_radiusZeroUnchanged()
{
    const QImage result = VideoTransformOps::blur(pixelImage1x1(17, 42, 99), 0);
    QVERIFY(!result.isNull());
    QCOMPARE(qRed(result.pixel(0, 0)), 17);
    QCOMPARE(qGreen(result.pixel(0, 0)), 42);
    QCOMPARE(qBlue(result.pixel(0, 0)), 99);
}

void TestVideoTransformOps::blur_uniformUnchanged()
{
    // A uniform image is a fixed point of the box average.
    const QImage result = VideoTransformOps::blur(solidImage(4, 4, 100, 150, 200), 2);
    QVERIFY(!result.isNull());
    for (int y = 0; y < result.height(); ++y) {
        for (int x = 0; x < result.width(); ++x) {
            QCOMPARE(qRed(result.pixel(x, y)), 100);
            QCOMPARE(qGreen(result.pixel(x, y)), 150);
            QCOMPARE(qBlue(result.pixel(x, y)), 200);
        }
    }
}

void TestVideoTransformOps::blur_radius1TwoPassBox()
{
    // 3x3 with center 255 and every other pixel 0, radius 1 (clamped edges).
    // Horizontal pass: row 1 -> [127, 85, 127], other rows all 0.
    // Vertical pass over the horizontal result:
    //   col 0 -> [63, 42, 63], col 1 -> [42, 28, 42], col 2 -> [63, 42, 63].
    QImage input(3, 3, QImage::Format_RGB32);
    input.fill(qRgb(0, 0, 0));
    input.setPixel(1, 1, qRgb(255, 255, 255));

    const QImage result = VideoTransformOps::blur(input, 1);
    QVERIFY(!result.isNull());
    const int expected[3][3] = {
        { 63, 42, 63 },
        { 42, 28, 42 },
        { 63, 42, 63 }
    };
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 3; ++x) {
            QCOMPARE(qRed(result.pixel(x, y)), expected[y][x]);
            QCOMPARE(qGreen(result.pixel(x, y)), expected[y][x]);
            QCOMPARE(qBlue(result.pixel(x, y)), expected[y][x]);
        }
    }
}

void TestVideoTransformOps::flip_horizontal()
{
    QImage input(2, 2, QImage::Format_RGB32);
    input.setPixel(0, 0, qRgb(10, 20, 30));  // A
    input.setPixel(1, 0, qRgb(40, 50, 60));  // B
    input.setPixel(0, 1, qRgb(70, 80, 90));  // C
    input.setPixel(1, 1, qRgb(100, 110, 120)); // D

    const QImage result = VideoTransformOps::flip(input, true);
    QVERIFY(!result.isNull());
    QCOMPARE(qRed(result.pixel(0, 0)), 40);  // B
    QCOMPARE(qRed(result.pixel(1, 0)), 10);  // A
    QCOMPARE(qRed(result.pixel(0, 1)), 100); // D
    QCOMPARE(qRed(result.pixel(1, 1)), 70);  // C
}

void TestVideoTransformOps::flip_vertical()
{
    QImage input(2, 2, QImage::Format_RGB32);
    input.setPixel(0, 0, qRgb(10, 20, 30));    // A
    input.setPixel(1, 0, qRgb(40, 50, 60));    // B
    input.setPixel(0, 1, qRgb(70, 80, 90));    // C
    input.setPixel(1, 1, qRgb(100, 110, 120)); // D

    const QImage result = VideoTransformOps::flip(input, false);
    QVERIFY(!result.isNull());
    QCOMPARE(qRed(result.pixel(0, 0)), 70);   // C
    QCOMPARE(qRed(result.pixel(1, 0)), 100);  // D
    QCOMPARE(qRed(result.pixel(0, 1)), 10);   // A
    QCOMPARE(qRed(result.pixel(1, 1)), 40);   // B
}

void TestVideoTransformOps::sepia_white()
{
    // outR = int(1.351*255)=344 -> clamp 255; outG = int(1.203*255)=306 -> 255;
    // outB = int(0.937*255) = int(238.935) = 238 (int truncation).
    const QImage result = VideoTransformOps::sepia(pixelImage1x1(255, 255, 255));
    QVERIFY(!result.isNull());
    QCOMPARE(qRed(result.pixel(0, 0)), 255);
    QCOMPARE(qGreen(result.pixel(0, 0)), 255);
    QCOMPARE(qBlue(result.pixel(0, 0)), 238);
}

void TestVideoTransformOps::sepia_dark()
{
    // outR = int(24.98) = 24; outG = int(22.25) = 22; outB = int(17.33) = 17.
    const QImage result = VideoTransformOps::sepia(pixelImage1x1(10, 20, 30));
    QVERIFY(!result.isNull());
    QCOMPARE(qRed(result.pixel(0, 0)), 24);
    QCOMPARE(qGreen(result.pixel(0, 0)), 22);
    QCOMPARE(qBlue(result.pixel(0, 0)), 17);
}

void TestVideoTransformOps::nullInput_returnsNull()
{
    QVERIFY(VideoTransformOps::swapRedBlue(QImage()).isNull());
    QVERIFY(VideoTransformOps::grayscale(QImage()).isNull());
    QVERIFY(VideoTransformOps::invert(QImage()).isNull());
    QVERIFY(VideoTransformOps::sepia(QImage()).isNull());
    QVERIFY(VideoTransformOps::brightness(QImage(), 50).isNull());
    QVERIFY(VideoTransformOps::contrast(QImage(), 100).isNull());
    QVERIFY(VideoTransformOps::blur(QImage(), 1).isNull());
    QVERIFY(VideoTransformOps::flip(QImage(), true).isNull());
}

// No QTEST_GUILESS_MAIN here: the two video test classes share one binary
// whose main lives in test_main.cpp.
