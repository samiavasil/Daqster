#pragma once

// SPDX-License-Identifier: MIT
//
// PolyphaseResampler — rational rate conversion by L/M (REQ-SW-PL-092).
//
// The textbook construction is "insert L−1 zeros, filter, keep every M-th
// sample". Doing that literally wastes (L−1)/L of the multiplies, so the kernel
// is split into L **phases** and only the phase the current output sample needs
// is evaluated:
//
//     y[i] = Σ_j  h[p + j·L] · x[q − j],     q = ⌊i·M / L⌋,  p = (i·M) mod L
//
// `i` is the global output index, so the position in the input stream is an
// **integer division**, not an accumulated float. That is the property the
// "chunked == one big block" test pins down: feeding 100 samples five times
// produces bit-identical output to feeding 480 once, because nothing is carried
// as a fraction that could round differently.
//
// Scaling: the prototype is multiplied by L on `configure()`, so a constant
// input of 1.0 comes out as 1.0 (the interpolation filter has to make up for
// the zeros it was fed). The kernel passed in is the design-rate kernel —
// `FirCoefficients::antiAliasForResampling(up, down)` supplies the default.
//
// QtCore only — no QtGui, no threads, no processes.

#include "DaqsterCommonExport.h"
#include "Dsp/FirCoefficients.h"

#include <QString>
#include <QVector>

#include <cstdint>

/**
 * @brief Streaming polyphase FIR resampler for a rational factor L/M.
 *
 * @code
 * PolyphaseResampler resampler;
 * QString error;
 * // 48 kHz → 44.1 kHz is 147/160
 * if (!resampler.configure(147, 160,
 *                          FirCoefficients::antiAliasForResampling(147, 160), &error))
 *     return error;
 * QVector<float> out;
 * resampler.process(input.constData(), input.size(), out);
 * @endcode
 */
class DAQSTERCOMMON_EXPORT PolyphaseResampler
{
public:
    PolyphaseResampler() = default;

    /**
     * @brief Install the ratio and the prototype kernel, clearing any history.
     *
     * @param up Interpolation factor L ≥ 1.
     * @param down Decimation factor M ≥ 1.
     * @param coefficients Prototype kernel at `up ×` the input rate; validated
     *        here. Common factors are not reduced: L/M is used as given, so
     *        `latencySamples()` matches the numbers the caller wrote.
     * @param error Receives the reason on failure (may be null).
     * @return true when the resampler is ready to process.
     */
    bool configure(int up, int down, const FirCoefficients &coefficients,
                   QString *error = nullptr);

    /// Drop the history and the phase; the ratio and kernel survive.
    void reset();

    /**
     * @brief Resample one block of mono samples.
     *
     * @param in Input samples (may be null when `inFrames` is 0).
     * @param inFrames Number of valid input samples.
     * @param out Replaced with the output samples produced by this call. The
     *        count is ⌊(inFrames·L + carry)/M⌋ minus the samples still waiting
     *        for input, so a caller that needs a fixed output size should pad.
     */
    void process(const float *in, int inFrames, QVector<float> &out);

    bool isConfigured() const { return m_up > 0 && m_down > 0 && !m_phases.isEmpty(); }
    int up() const { return m_up; }
    int down() const { return m_down; }
    int taps() const { return m_taps; }

    /// Group delay in **output** samples: (taps − 1) / (2·down).
    double latencySamples() const;

    /// The same delay in input samples: (taps − 1) / (2·up).
    double latencyInputSamples() const;

    /// Provenance of the installed kernel — record it next to the result.
    QString coefficientRef() const { return m_coefficients.ref(); }
    QString coefficientSha256() const { return m_coefficients.sha256OfTaps(); }
    const FirCoefficients &coefficients() const { return m_coefficients; }

private:
    /// Number of taps in the longest phase: ⌈taps / up⌉.
    int phaseLength() const { return (m_taps + m_up - 1) / m_up; }

    qint64 m_bufferStart = 0;  ///< global index of `m_buffer[m_readPos]`
    QVector<float> m_buffer;
    int m_readPos = 0;
    qint64 m_received = 0;
    qint64 m_produced = 0;

    int m_up = 0;
    int m_down = 0;
    int m_taps = 0;
    /// The prototype × up, flattened: phase p starts at h[p].
    QVector<double> m_phases;
    FirCoefficients m_coefficients;
};
