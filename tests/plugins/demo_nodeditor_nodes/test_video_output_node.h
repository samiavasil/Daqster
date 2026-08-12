#pragma once

#include <QtTest>

/// Behavioural tests for VideoOutputNode (REQ-SW-PL-020, AC 6).
///
/// QTEST_MAIN in test_video_output_node.cpp provides a QApplication main;
/// the binary runs headless via the offscreen platform plugin
/// (QT_QPA_PLATFORM=offscreen set in the CTest ENVIRONMENT property).
class VideoOutputNodeTest : public QObject
{
    Q_OBJECT

private slots:
    void portTopology_qt5_qt6();
    void imageData_passthrough();
    void nullImageData_invalidates();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void videoInputConnectionGuard_qt6_only();
    void outputConnectionCounter_qt6_only();
    void outputChain_qt6_only();
#endif
};
