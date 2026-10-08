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
    void portTopology();
    void videoInputConnectionGuard();
    void outputConnectionCounter();
    void outputChain();

    // Embedded effects (REQ-SW-PL-034):
    void defaultNoEffectPassthrough();
    void loadEffectPersistsSave();
    void loadAbsentEffectIsNoEffect();
    void loadAppliesEffectToFrame();

    // REQ-SW-PL-039 Bug A: a CPU-only effect must apply to a GpuRgba input
    // (asImage() readback) instead of being skipped by the old isGpuRgba gate.
    void cpuEffectAppliesToGpuRgbaInput();

    // REQ-SW-PL-055 unified display. NOT guarded by Qt version: with
    // DAQSTER_VIDEO_BACKEND replacing the per-Qt DAQSTER_GL_BLIT switch, the
    // routing decision is identical on Qt5 and Qt6, and these tests no longer
    // need a GL context because they observe the seam, not the pixels.
    void gpuRgbaRoutesZeroCopyToDisplay();
    void cpuFrameUsesYuvPathOnSameDisplay();
    void noDisplayUntilUiIsAttached();
    void backendChosenOnceNotPerFrame();
};
