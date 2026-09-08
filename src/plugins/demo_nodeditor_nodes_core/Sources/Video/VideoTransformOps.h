#ifndef VIDEOTRANSFORMOPS_H
#define VIDEOTRANSFORMOPS_H

#include <QImage>

/**
 * @brief Stateless QImage transform operations used by VideoTransformNode.
 *
 * Flat namespace style (like VideoCompat): every operation takes an input
 * QImage plus optional parameters and returns a new modified QImage. The
 * input is never modified — each operation normalizes to Format_RGB32 where
 * needed, detaches before mutation (implicit sharing) and returns the result.
 *
 * The OpenCV-backed operations are declared here only when HAVE_OPENCV is
 * defined (compile-time auto-detect in CMakeLists.txt); their implementation
 * lives in OpenCVTransforms.cpp which is entirely guarded by the same macro.
 */
namespace VideoTransformOps {

/** Swap the red and blue channels of every pixel (R<->B). */
QImage swapRedBlue(const QImage &image);

/** Convert to grayscale (luminance), emitted as RGB32. */
QImage grayscale(const QImage &image);

/** Invert the color of every pixel. */
QImage invert(const QImage &image);

/** Apply a sepia tone. */
QImage sepia(const QImage &image);

/** Adjust brightness by delta in the range -100..+100 (0 = unchanged). */
QImage brightness(const QImage &image, int delta);

/** Adjust contrast by percent in the range 0..200 (100 = unchanged). */
QImage contrast(const QImage &image, int percent);

/** Simple box blur with radius in the range 0..10 (0 = unchanged). */
QImage blur(const QImage &image, int radius);

/** Flip the image horizontally (true) or vertically (false). */
QImage flip(const QImage &image, bool horizontal);

#ifdef HAVE_OPENCV
/** Gaussian blur with an odd kernel size in the range 1..31. */
QImage gaussianBlur(const QImage &image, int kernel);

/** Canny edge detection with low/high thresholds in the range 0..255. */
QImage canny(const QImage &image, int low, int high);

/** Binary threshold at value in the range 0..255. */
QImage threshold(const QImage &image, int value);
#endif // HAVE_OPENCV

} // namespace VideoTransformOps

#endif // VIDEOTRANSFORMOPS_H
