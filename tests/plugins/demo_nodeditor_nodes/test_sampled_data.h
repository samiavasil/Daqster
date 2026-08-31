#pragma once

#include <QtTest>

// Test class for the SampledData decoders (REQ-SW-PL-022 AC 3,
// REQ-SW-PL-025 AC 1). Declared in a header (instead of relying on
// QTEST_GUILESS_MAIN inside the .cpp) so the sampled-data test class can
// share the demo_nodeditor_nodes test binary with the video test classes -
// see test_main.cpp.
class SampledDataTest : public QObject
{
    Q_OBJECT

private slots:
    void decodeNormalized_returnsInRange();
    void decodeNormalizedF32_matchesNormalized();
    void decodePhysical_int16_scaleOffset();
    void decodePhysical_uint16_noClamp();
    void decodePhysical_float32_passthrough();
    void decodePhysical_emptyBuffer();
};
