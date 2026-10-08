#ifdef HAVE_OPENCV

#include "VideoTransformOps.h"

#include <opencv2/opencv.hpp>

#include <QImage>

#include <algorithm>

namespace VideoTransformOps {

namespace {

/**
 * @brief Convert a QImage to a BGR cv::Mat (deep copy owned by the caller).
 *
 * The input is first normalized to QImage::Format_RGB888 (cv::Mat is BGR by
 * default; the RGB->BGR conversion happens explicitly here). The cv::Mat that
 * wraps the QImage bits is used only inside this function — the returned mat
 * is an independent deep copy, so it can safely outlive the QImage.
 */
cv::Mat qImageToBgrMat(const QImage &image)
{
    const QImage rgb = (image.format() == QImage::Format_RGB888)
        ? image
        : image.convertToFormat(QImage::Format_RGB888);

    // bytesPerLine may exceed width*3 (32-bit alignment) — pass it as step.
    const cv::Mat rgbMat(rgb.height(), rgb.width(), CV_8UC3,
                         const_cast<uchar *>(rgb.constBits()), rgb.bytesPerLine());

    cv::Mat bgrMat;
    cv::cvtColor(rgbMat, bgrMat, cv::COLOR_RGB2BGR);
    return bgrMat;
}

/**
 * @brief Convert a BGR cv::Mat back into an RGB32 QImage.
 *
 * The result is copied into a new QImage (the mat is not wrapped). Matches the
 * format contract of the other VideoTransformOps: output is always RGB32.
 */
QImage bgrMatToRgb32(const cv::Mat &bgr)
{
    if (bgr.empty())
        return QImage();

    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);

    const QImage wrapped(rgb.data, rgb.cols, rgb.rows,
                         static_cast<int>(rgb.step), QImage::Format_RGB888);
    const QImage copy = wrapped.copy();
    return copy.convertToFormat(QImage::Format_RGB32);
}

/**
 * @brief Convert a single-channel cv::Mat into an RGB32 grayscale QImage.
 *
 * Grayscale outputs (Canny/Threshold) are emitted as RGB32 grayscale for
 * pipeline uniformity with the other operations.
 */
QImage grayMatToRgb32(const cv::Mat &gray)
{
    if (gray.empty())
        return QImage();

    const QImage wrapped(gray.data, gray.cols, gray.rows,
                         static_cast<int>(gray.step), QImage::Format_Grayscale8);
    const QImage copy = wrapped.copy();
    return copy.convertToFormat(QImage::Format_RGB32);
}

} // namespace

QImage gaussianBlur(const QImage &image, int kernel)
{
    if (image.isNull())
        return QImage();

    const cv::Mat bgr = qImageToBgrMat(image);
    const int size = std::max(1, kernel | 1); // odd kernel >= 1

    cv::Mat dst;
    cv::GaussianBlur(bgr, dst, cv::Size(size, size), 0.0);
    return bgrMatToRgb32(dst);
}

QImage canny(const QImage &image, int low, int high)
{
    if (image.isNull())
        return QImage();

    const cv::Mat bgr = qImageToBgrMat(image);

    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

    cv::Mat edges;
    cv::Canny(gray, edges, low, high);
    return grayMatToRgb32(edges);
}

QImage threshold(const QImage &image, int value)
{
    if (image.isNull())
        return QImage();

    const cv::Mat bgr = qImageToBgrMat(image);

    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);

    cv::Mat dst;
    cv::threshold(gray, dst, value, 255, cv::THRESH_BINARY);
    return grayMatToRgb32(dst);
}

} // namespace VideoTransformOps

#endif // HAVE_OPENCV
