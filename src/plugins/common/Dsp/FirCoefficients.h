#pragma once

// SPDX-License-Identifier: MIT
//
// FirCoefficients — FIR tap sets as DATA (REQ-SW-PL-092).
//
// A filter kernel is not a compile-time constant: it is a small numeric vector
// with metadata, and the difference between "the same result" and "a different
// result nobody can explain" is whether that vector was recorded. So this class
// owns three things and nothing else:
//
//   1. **the taps** (double precision, ordered h[0] … h[N-1]);
//   2. **provenance** — `ref` (a human name such as
//      "default/kaiser-64-b8" or a file name) and `sha256` of the taps. Two
//      runs that report the same `sha256` used the same numbers, full stop;
//   3. **validation** — a kernel that does not sum to 1 (within 1e-3) is not a
//      low-pass, it is a gain change with extra steps; a kernel of length 7 or
//      of 5000 is almost always a truncated file.
//
// Three loaders exist and they are deliberately separate:
//
//   * `makeDefault()`     — windowed-sinc, Kaiser β, cutoff as a ratio of the
//                           design-rate Nyquist. The built-in default (64 taps,
//                           β = 8, cutoff 0.45) is what a node uses when the
//                           scene names no file;
//   * `fromJson()`        — a document with metadata, so the numbers travel
//                           with their intent (cutoff, rate, delay, sha256);
//   * `fromTapsFile()`    — bare numbers, one per line, for the "I designed
//                           this in Octave / Python and I have the taps" case.
//
// QtCore only: no QtGui, no threads, no processes. The DSP engine that consumes
// these taps (`FirDecimator`, `PolyphaseResampler`) is stateful; this class is
// immutable once loaded.

#include "DaqsterCommonExport.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

/**
 * @brief A validated FIR kernel plus its provenance.
 *
 * The default-constructed value is *empty* (`taps` empty, `valid()` false); use
 * one of the static factories. `validate()` is not called implicitly by the
 * factories' results in a way that hides errors: `fromJson()`/`fromTapsFile()`
 * validate and report the reason through `error`, `makeDefault()` produces a
 * normalised kernel by construction.
 */
class DAQSTERCOMMON_EXPORT FirCoefficients
{
public:
    /// Default kernel shape: 64 taps, Kaiser β = 8, cutoff 0.45 of the Nyquist
    /// of the rate the filter is designed for (REQ-SW-PL-092).
    static constexpr int kDefaultTaps = 64;
    static constexpr double kDefaultBeta = 8.0;
    static constexpr double kDefaultCutoffRatio = 0.45;
    /// A kernel shorter than this cannot hold a usable transition band.
    static constexpr int kMinTaps = 8;
    /// Longer than this is a truncated file, not a design decision.
    static constexpr int kMaxTaps = 4096;
    /// |sum(h) − 1| must stay below this (DC gain of a passband filter).
    static constexpr double kGainTolerance = 1e-3;
    /// Delay metadata tolerance, in samples — a declared delay that disagrees
    /// with (taps−1)/2 by more than this is not the same filter.
    static constexpr double kDelayTolerance = 0.51;

    FirCoefficients() = default;

    // ── Factories ───────────────────────────────────────────────────────────

    /**
     * @brief Build a windowed-sinc low-pass with a Kaiser window.
     *
     * `cutoffRatio` is relative to the **Nyquist of the rate the filter is
     * designed for** — 0.45 means the −6 dB point sits at 0.45·fs/2. A
     * decimator by M therefore passes 0.45/M (the lower Nyquist belongs to the
     * *output* rate) and a resampler by up/down passes 0.45·min(up,down)/up at
     * the up-sampled design rate; `antiAliasForDecimation()` /
     * `antiAliasForResampling()` spell those two cases out.
     *
     * The taps are normalised so that sum(h) == 1 exactly (up to double
     * rounding): a low-pass whose DC gain is 0.9999 is a bug, not a design.
     *
     * @param taps Number of taps, clamped to [kMinTaps, kMaxTaps].
     * @param cutoffRatio −6 dB point as a fraction of the Nyquist; (0, 1).
     * @param beta Kaiser window shape (≈ 0.1102·(A − 8.7) for A dB of
     *        stopband attenuation; 8 ≈ 80 dB).
     * @return A normalised kernel with a generated `ref`.
     */
    static FirCoefficients makeDefault(int taps = kDefaultTaps,
                                       double cutoffRatio = kDefaultCutoffRatio,
                                       double beta = kDefaultBeta);

    /**
     * @brief Convenience: the anti-alias kernel for decimation by `M`.
     *
     * Design rate is the input rate, so the cutoff is `factor / M` (the lower
     * Nyquist belongs to the decimated rate).
     *
     * @param M Integer decimation factor, ≥ 1.
     * @param taps Tap count (default 64).
     * @param factor Cutoff as a fraction of the *lower* Nyquist (default 0.45).
     * @param beta Kaiser β (default 8).
     */
    static FirCoefficients antiAliasForDecimation(int M, int taps = kDefaultTaps,
                                                  double factor = kDefaultCutoffRatio,
                                                  double beta = kDefaultBeta);

    /**
     * @brief Convenience: the anti-alias kernel for rational resampling.
     *
     * Design rate is `up ×` the input rate, so the cutoff is
     * `factor · min(up, down) / up` — i.e. `factor` of the lower Nyquist, in the
     * units of the interpolated rate.
     *
     * @param up Interpolation factor, ≥ 1.
     * @param down Decimation factor, ≥ 1.
     * @param taps Tap count at the interpolated rate (default 64).
     * @param factor Cutoff as a fraction of the *lower* Nyquist (default 0.45).
     * @param beta Kaiser β (default 8).
     */
    static FirCoefficients antiAliasForResampling(int up, int down, int taps = kDefaultTaps,
                                                  double factor = kDefaultCutoffRatio,
                                                  double beta = kDefaultBeta);

    /**
     * @brief Load a kernel from a JSON document.
     *
     * Accepted shape (every key optional except a tap source):
     * @code
     * {
     *   "taps": [0.001, 0.004, …],        // or "file": "lowpass_64.json"
     *   "ref": "audio/decim3-kaiser64",   // provenance, echoed by ref()
     *   "sha256": "9f86d081…",            // optional, verified when present
     *   "cutoff": 0.15, "sampleRate": 48000.0, "beta": 8.0,
     *   "delay": 31.5,                    // optional, cross-checked
     *   "designRate": 16000.0             // informational
     * }
     * @endcode
     * A `file` value is resolved relative to `baseDirectory` (the JSON's own
     * directory when the caller passes nothing) and may be a bare-taps file or
     * another JSON document carrying `taps`.
     *
     * @param object Parsed JSON object.
     * @param error Receives the reason on failure (may be null).
     * @param baseDirectory Directory for a relative `file` entry.
     * @return The validated kernel, or an empty one on failure.
     */
    static FirCoefficients fromJson(const QJsonObject &object, QString *error,
                                    const QString &baseDirectory = QString());

    /**
     * @brief Load bare numbers — one tap per line, `#` starts a comment.
     *
     * Also accepts a JSON document with a `taps` array (so a `.json` passed to a
     * "taps file" field does not fail for a trivial reason). Blank lines are
     * skipped; any other non-numeric token is an error naming the line.
     *
     * @param path File to read (UTF-8).
     * @param error Receives the reason on failure (may be null).
     */
    static FirCoefficients fromTapsFile(const QString &path, QString *error);

    // ── Taps and provenance ─────────────────────────────────────────────────

    /// The taps themselves, h[0] … h[N−1].
    const QVector<double> &taps() const { return m_taps; }
    /// Number of taps. (`taps()` is the vector; this is the count.)
    int tapCount() const { return m_taps.size(); }
    bool isEmpty() const { return m_taps.isEmpty(); }

    /// Human-readable provenance: a generated name for defaults, the file path
    /// or the JSON `ref` otherwise. Never empty for a loaded kernel.
    QString ref() const { return m_ref; }
    void setRef(const QString &ref) { m_ref = ref; }

    /// Lower-case hex SHA-256 of the tap values (`tapsSha256()` is the getter;
    /// the name here matches the ADR vocabulary).
    QString sha256OfTaps() const;

    /// sum(h) — the DC gain a constant input sees.
    double gain() const;

    /// Group delay in **design-rate samples**: (taps − 1) / 2.
    double delaySamples() const;

    /// Metadata carried by the source document; 0 / empty when the source did
    /// not declare it.
    double declaredCutoff() const { return m_cutoff; }
    double designedSampleRate() const { return m_sampleRate; }
    double declaredBeta() const { return m_beta; }
    double declaredDelay() const { return m_delay; }

    /**
     * @brief Check the load rules: finite, length, DC gain, delay consistency.
     *
     * @param error Receives the reason on failure (may be null).
     * @return true when the kernel may be used for interpolation/decimation.
     */
    bool validate(QString *error = nullptr) const;

private:
    QVector<double> m_taps;
    QString m_ref;
    double m_cutoff = 0.0;      ///< declared cutoff ratio (0 = undeclared)
    double m_sampleRate = 0.0;  ///< declared design rate
    double m_beta = 0.0;        ///< declared Kaiser β
    double m_delay = -1.0;      ///< declared group delay (−1 = undeclared)
};

/// I0(β) — zeroth-order modified Bessel of the first kind, series form.
///
/// Public because the window formula is part of the contract (a test needs to
/// rebuild the same taps independently) and because QtCore has no Bessel.
double daqsterBesselI0(double x);
