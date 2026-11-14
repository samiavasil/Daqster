#pragma once

// SPDX-License-Identifier: MIT
//
// FirDecimator — integer-M rate reduction with an anti-alias kernel
// (REQ-SW-PL-092).
//
// Decimation by M is not "throw away M−1 of every M samples": everything above
// the *output* Nyquist folds back into the band that survives, and the fold is
// permanent. `configure()` therefore insists on a kernel (normally
// `FirCoefficients::antiAliasForDecimation(M)`), and `process()` filters then
// keeps every M-th sample — only the kept samples are ever computed, so the
// cost is (taps / M) multiplies per output sample, not per input sample.
//
// Streaming contract:
//   * `process()` is **stateless from the caller's point of view** — pass any
//     chunking of the same input and the concatenated output is identical,
//     because the state is (history, input count, output count) and the emit
//     condition is an integer index comparison, never a float accumulator;
//   * the head of the stream is treated as zero-padded, so the first output
//     appears after M·(1 + delay) input samples instead of after `taps`
//     samples of dead air;
//   * `out` is **replaced**, not appended to — a caller that wants to
//     accumulate does so itself;
//   * mono. A multi-channel node runs one instance per channel (the kernel is
//     identical, so the identity recording stays one line per stream).
//
// QtCore only — no QtGui, no threads, no processes.

#include "DaqsterCommonExport.h"
#include "Dsp/FirCoefficients.h"

#include <QString>
#include <QVector>

#include <cstdint>

/**
 * @brief Streaming FIR decimator with an integer factor M.
 *
 * @code
 * FirDecimator decimator;
 * QString error;
 * if (!decimator.configure(3, FirCoefficients::antiAliasForDecimation(3), &error))
 *     return error;
 * QVector<float> out;
 * decimator.process(input.constData(), input.size(), out);   // fs → fs/3
 * @endcode
 */
class DAQSTERCOMMON_EXPORT FirDecimator
{
public:
    FirDecimator() = default;

    /**
     * @brief Install the factor and the kernel, clearing any history.
     *
     * @param decimation Integer factor M ≥ 1.
     * @param coefficients Kernel to apply before decimation; validated here.
     * @param error Receives the reason on failure (may be null).
     * @return true when the decimator is ready to process.
     */
    bool configure(int decimation, const FirCoefficients &coefficients, QString *error = nullptr);

    /// Drop the history so the next block behaves like the start of a stream.
    /// The configuration (factor, kernel) survives.
    void reset();

    /**
     * @brief Filter + decimate one block of mono samples.
     *
     * @param in Input samples (may be null when `inFrames` is 0).
     * @param inFrames Number of valid input samples.
     * @param out Replaced with the output samples this call produced
     *        (⌈inFrames / M⌉ at most, minus the samples still pending).
     */
    void process(const float *in, int inFrames, QVector<float> &out);

    bool isConfigured() const { return m_decimation > 0 && m_taps > 0; }
    int decimation() const { return m_decimation; }
    int taps() const { return m_taps; }

    /// Group delay in **output** samples: (taps − 1) / (2·M).
    double latencySamples() const;

    /// The same delay expressed in input samples: (taps − 1) / 2.
    double latencyInputSamples() const;

    /// Provenance of the installed kernel — record it next to the result.
    QString coefficientRef() const { return m_coefficients.ref(); }
    QString coefficientSha256() const { return m_coefficients.sha256OfTaps(); }
    const FirCoefficients &coefficients() const { return m_coefficients; }

private:
    /// Global index of `m_buffer[m_readPos]`; lower indices are gone.
    qint64 m_bufferStart = 0;
    QVector<float> m_buffer;
    int m_readPos = 0;
    qint64 m_received = 0;   ///< total input samples seen (stream length)
    qint64 m_produced = 0;   ///< total output samples produced

    int m_decimation = 0;
    int m_taps = 0;
    FirCoefficients m_coefficients;
};
