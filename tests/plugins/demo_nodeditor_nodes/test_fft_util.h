#pragma once

#include <QtTest>

// Test class for the shared radix-2 FFT utilities (REQ-SW-PL-023).
// Declared in a header (instead of relying on QTEST_GUILESS_MAIN inside the
// .cpp) so the FFT test class can share the demo_nodeditor_nodes test binary
// with the other test classes - see test_main.cpp.
class FftUtilTest : public QObject
{
    Q_OBJECT

private slots:
    void sinePeakAtExpectedBin();
    void emptyInput_returnsEmpty();
    void singleSample_returnsEmpty();
    void deterministic_output();
    void respectsMaxFftSize();
};
