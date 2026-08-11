#pragma once

#include <QtTest>

// Test class for the QAudioBuffer -> SampledData capture glue
// (AudioBufferToSampled.h, REQ-SW-PL-022 §4). Declared in a header (instead
// of relying on QTEST_GUILESS_MAIN inside the .cpp) so the audio-buffer test
// class can share the demo_nodeditor_nodes test binary with the video and
// sampled-data test classes - see test_main.cpp.
class AudioBufferToSampledTest : public QObject
{
    Q_OBJECT

private slots:
    void descriptorFromFormat_int16Stereo();
    void wrapBuffer_producesExpectedFrames();
    void wrapBuffer_decodeMatchesInput();
};
