#pragma once

#include <QtTest>

/// Value-semantic tests for VideoFrameData (REQ-SW-PL-020, AC 6).
///
/// QTEST_GUILESS_MAIN in test_video_frame_data.cpp provides a QCoreApplication
/// main — QImage operations do not need a widget-backed event loop.
class VideoFrameDataTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultCtor_hasNoFrame();
    void fromQImage_hasFrame();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void fromQImage_toImage_roundTrip();
#endif

    void setFrame_replaces();

    // frameToImageCpu (REQ-SW-PL-039): thread-safe CPU conversion for the
    // ComputePool worker — RGB wrap + NV12/YUV420P BT.601 (both Qt versions).
    void frameToImageCpu_rgb32_wraps();
    void frameToImageCpu_argb32_wraps();
    void frameToImageCpu_nv12_converts();
    void frameToImageCpu_yuv420p_converts();
    void frameToImageCpu_unsupported_returnsNull();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void qt5_wrapsOwnedFrames();
    void qt5_nv12_asImage_converts();
    void qt5_yuv420p_asImage_converts();
#endif
};
