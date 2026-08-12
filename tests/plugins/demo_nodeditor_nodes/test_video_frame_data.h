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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void fromQImage_hasFrame();
    void fromQImage_toImage_roundTrip();
    void setFrame_replaces();
#else
    void hasFrame_alwaysFalse_qt5();
#endif
};
