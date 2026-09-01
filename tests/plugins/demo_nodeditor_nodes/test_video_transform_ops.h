#pragma once

#include <QtTest>

// Test class for VideoTransformOps (REQ-SW-PL-018/019). Declared in a header
// (instead of relying on QTEST_GUILESS_MAIN inside the .cpp) so the two video
// test classes can share a single test binary - see test_main.cpp.
class TestVideoTransformOps : public QObject
{
    Q_OBJECT

private slots:
    void swapRedBlue_swapsChannels();
    void swapRedBlue_inputNotModified();
    void grayscale_luminance();
    void invert_colors();
    void brightness_deltaZeroUnchanged();
    void brightness_clamps();
    void contrast_percent100Unchanged();
    void contrast_clampsAndMidGray();
    void blur_radiusZeroUnchanged();
    void blur_uniformUnchanged();
    void blur_radius1TwoPassBox();
    void flip_horizontal();
    void flip_vertical();
    void sepia_white();
    void sepia_dark();
    void nullInput_returnsNull();
};
