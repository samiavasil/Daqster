#include "VideoTransformOps.h"

#include <QtGlobal>

#include <algorithm>

namespace VideoTransformOps {

namespace {

/**
 * @brief Normalize an image to Format_RGB32 without copying when possible.
 *
 * Returns an implicitly shared copy when the format already matches; callers
 * that mutate the result must call detach() first (QImage implicit sharing).
 */
QImage toRgb32(const QImage &image)
{
    if (image.format() == QImage::Format_RGB32)
        return image;
    return image.convertToFormat(QImage::Format_RGB32);
}

int clampByte(int value)
{
    return std::max(0, std::min(255, value));
}

} // namespace

QImage swapRedBlue(const QImage &image)
{
    if (image.isNull())
        return QImage();

    QImage result = toRgb32(image);
    result.detach();

    // QImage::Format_RGB32 stores each 32-bit pixel as [x][R][G][B] on
    // big-endian and [B][G][R][x] on little-endian. Swap the red and blue
    // bytes in every pixel to produce the R<->B channel swap.
    for (int y = 0; y < result.height(); ++y) {
        uchar *line = result.scanLine(y);
        for (int x = 0; x < result.width(); ++x) {
            uchar *pixel = line + (x * 4);
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
            std::swap(pixel[1], pixel[3]);
#else
            std::swap(pixel[0], pixel[2]);
#endif
        }
    }

    return result;
}

QImage grayscale(const QImage &image)
{
    if (image.isNull())
        return QImage();

    QImage result = toRgb32(image);
    result.detach();

    for (int y = 0; y < result.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = line[x];
            const int gray = (299 * qRed(pixel) + 587 * qGreen(pixel) + 114 * qBlue(pixel)) / 1000;
            line[x] = qRgb(gray, gray, gray);
        }
    }

    return result;
}

QImage invert(const QImage &image)
{
    if (image.isNull())
        return QImage();

    QImage result = toRgb32(image);
    result.detach();

    for (int y = 0; y < result.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = line[x];
            line[x] = qRgb(255 - qRed(pixel), 255 - qGreen(pixel), 255 - qBlue(pixel));
        }
    }

    return result;
}

QImage sepia(const QImage &image)
{
    if (image.isNull())
        return QImage();

    QImage result = toRgb32(image);
    result.detach();

    for (int y = 0; y < result.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = line[x];
            const int r = qRed(pixel);
            const int g = qGreen(pixel);
            const int b = qBlue(pixel);
            const int outR = clampByte(static_cast<int>(0.393 * r + 0.769 * g + 0.189 * b));
            const int outG = clampByte(static_cast<int>(0.349 * r + 0.686 * g + 0.168 * b));
            const int outB = clampByte(static_cast<int>(0.272 * r + 0.534 * g + 0.131 * b));
            line[x] = qRgb(outR, outG, outB);
        }
    }

    return result;
}

QImage brightness(const QImage &image, int delta)
{
    if (image.isNull())
        return QImage();

    const int shift = delta * 255 / 100; // maps -100..+100 to -255..+255

    QImage result = toRgb32(image);
    result.detach();

    for (int y = 0; y < result.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = line[x];
            line[x] = qRgb(clampByte(qRed(pixel) + shift),
                           clampByte(qGreen(pixel) + shift),
                           clampByte(qBlue(pixel) + shift));
        }
    }

    return result;
}

QImage contrast(const QImage &image, int percent)
{
    if (image.isNull())
        return QImage();

    const double factor = percent / 100.0;

    QImage result = toRgb32(image);
    result.detach();

    for (int y = 0; y < result.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = line[x];
            const int r = clampByte(static_cast<int>((qRed(pixel) - 128) * factor + 128));
            const int g = clampByte(static_cast<int>((qGreen(pixel) - 128) * factor + 128));
            const int b = clampByte(static_cast<int>((qBlue(pixel) - 128) * factor + 128));
            line[x] = qRgb(r, g, b);
        }
    }

    return result;
}

QImage blur(const QImage &image, int radius)
{
    if (image.isNull())
        return QImage();

    const QImage source = toRgb32(image);
    if (radius <= 0)
        return source;

    const int w = source.width();
    const int h = source.height();
    const int k = radius;

    // Horizontal pass: box average with clamped edges.
    QImage horizontal(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y) {
        const QRgb *srcLine = reinterpret_cast<const QRgb *>(source.constScanLine(y));
        QRgb *dstLine = reinterpret_cast<QRgb *>(horizontal.scanLine(y));
        for (int x = 0; x < w; ++x) {
            int rSum = 0;
            int gSum = 0;
            int bSum = 0;
            int count = 0;
            const int x0 = std::max(0, x - k);
            const int x1 = std::min(w - 1, x + k);
            for (int xx = x0; xx <= x1; ++xx) {
                const QRgb pixel = srcLine[xx];
                rSum += qRed(pixel);
                gSum += qGreen(pixel);
                bSum += qBlue(pixel);
                ++count;
            }
            dstLine[x] = qRgb(rSum / count, gSum / count, bSum / count);
        }
    }

    // Vertical pass: box average over the horizontal result.
    QImage result(w, h, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y) {
        QRgb *dstLine = reinterpret_cast<QRgb *>(result.scanLine(y));
        const int y0 = std::max(0, y - k);
        const int y1 = std::min(h - 1, y + k);
        const int count = y1 - y0 + 1;
        for (int x = 0; x < w; ++x) {
            int rSum = 0;
            int gSum = 0;
            int bSum = 0;
            for (int yy = y0; yy <= y1; ++yy) {
                const QRgb pixel = reinterpret_cast<const QRgb *>(horizontal.constScanLine(yy))[x];
                rSum += qRed(pixel);
                gSum += qGreen(pixel);
                bSum += qBlue(pixel);
            }
            dstLine[x] = qRgb(rSum / count, gSum / count, bSum / count);
        }
    }

    return result;
}

QImage flip(const QImage &image, bool horizontal)
{
    if (image.isNull())
        return QImage();

    // Qt 6.9+ deprecates mirrored() in favor of flipped(Qt::Orientations);
    // on Qt < 6.9 (Qt5 and Qt 6.8.x) mirrored() is the only option.
    // mirrored(horizontal, vertical): flip horizontally when requested,
    // vertically otherwise (the "vertical" flag must be the inverse of
    // "horizontal").
    const QImage rgb = toRgb32(image);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    return rgb.flipped(horizontal ? Qt::Horizontal : Qt::Vertical);
#else
    return rgb.mirrored(horizontal, !horizontal);
#endif
}

} // namespace VideoTransformOps
