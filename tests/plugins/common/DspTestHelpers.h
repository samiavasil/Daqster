#pragma once

// SPDX-License-Identifier: MIT

// Signal-generation and measurement helpers shared by the DSP tests
// (REQ-SW-PL-092).
//
// The same question — "what amplitude does this tone have in that signal?" — is
// asked by the engine tests (`tests/plugins/common/test_dsp_filters.cpp`) and
// by the node tests (`tests/plugins/nodeditor_nodes/test_audio_dsp_nodes.cpp`).
// Two copies would drift: one would get a window, the other would not, and the
// two suites would disagree about whether the same filter attenuates by 60 dB.
//
// No window is applied and none is needed: every frequency the tests measure is
// exactly on a bin of the analysis length they pass (the signals are chosen to
// be periodic over the analysed span), so a rectangular window leaks nothing
// and the amplitude of an on-bin tone is exact.

#include <QVector>

#include <cmath>

namespace DaqsterDspTest {

constexpr double kPi = 3.14159265358979323846;

/// `n` samples of `amplitude · sin(2π·freq/fs·i)`.
inline QVector<float> makeSine(double amplitude, double freq, double sampleRate, int n)
{
    QVector<float> samples(n);
    const double w = 2.0 * kPi * freq / sampleRate;
    for (int i = 0; i < n; ++i)
        samples[i] = float(amplitude * std::sin(w * double(i)));
    return samples;
}

/// `n` samples of a constant value.
inline QVector<float> makeConstant(float value, int n)
{
    return QVector<float>(n, value);
}

/// One DFT bin, evaluated directly: the amplitude of a sinusoid at `freq`.
inline double dftAmplitudeAt(const QVector<float> &signal, double freq, double sampleRate)
{
    const int n = signal.size();
    if (n <= 0)
        return 0.0;
    const double w = 2.0 * kPi * freq / sampleRate;
    double re = 0.0;
    double im = 0.0;
    for (int i = 0; i < n; ++i) {
        re += double(signal.at(i)) * std::cos(w * double(i));
        im -= double(signal.at(i)) * std::sin(w * double(i));
    }
    return 2.0 * std::sqrt(re * re + im * im) / double(n);
}

} // namespace DaqsterDspTest
