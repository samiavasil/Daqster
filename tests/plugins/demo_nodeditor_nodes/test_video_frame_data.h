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

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void qt5_wrapsOwnedFrames();
    void qt5_nv12_asImage_converts();
    void qt5_yuv420p_asImage_converts();
#endif
};
