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
//                           design-rate Nyquist. The built-in default (160 taps,
//                           β = 8, cutoff 0.90) is what a node uses when the
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
    /// Default kernel shape: 160 taps, Kaiser β = 8, cutoff 0.90 of the Nyquist
    /// of the rate the filter is designed for (REQ-SW-PL-092).
    ///
    /// **Why 0.90 and not 0.45.** 0.45 put the −6 dB point at 3.6 kHz for
    /// 48 kHz → 16 kHz — a telephone filter that cut speech away: measured
    /// −6 dB @ 3604 Hz, −20 dB @ 4477, −60 dB @ 5400, and an A/B through
    /// whisper changed the transcript (`they` → `you`) away from the reference.
    /// 0.90 keeps 7.2 kHz and puts the stopband edge (`1.0 × Nyquist_out`,
    /// 8 kHz) 60 dB down, which is what "anti-alias" is supposed to mean.
    ///
    /// **Why 160 taps.** `tapsForTransition()` is the number the 0.90 rule
    /// demands: 64 taps hold only −18 dB at 8 kHz for ×3 (measured) because
    /// the transition is `0.1 · Nyquist_out` wide. β = 8 needs 153 taps for
    /// that edge (the estimate is 154); 160 keeps the default a multiple of 16 and
    /// covers every decimation up to ×3 with margin.
    static constexpr int kDefaultTaps = 160;
    static constexpr double kDefaultBeta = 8.0;
    static constexpr double kDefaultCutoffRatio = 0.90;
    /// A kernel shorter than this cannot hold a usable transition band.
    static constexpr int kMinTaps = 8;
    /// Longer than this is a truncated file, not a design decision.
    ///
    /// 8192 (not 4096) because the 0.90 rule demands it for rational ratios:
    /// 44.1 → 48 k = 160/147 needs 8170 taps on the prototype to be 60 dB down
    /// at the lower Nyquist (22.05 kHz), i.e. 52 per polyphase branch.
    static constexpr int kMaxTaps = 8192;
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
     * designed for** — 0.90 (the default) means the −6 dB point sits at
     * 0.90·fs/2. A decimator by M therefore passes 0.90/M (the lower Nyquist
     * belongs to the *output* rate) and a resampler by up/down passes
     * 0.90·min(up,down)/up at the up-sampled design rate; `antiAliasForDecimation()` /
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
     * @brief Kaiser's tap estimate: how many taps a transition band needs.
     *
     * `N ≈ (A − 8) / (2.285 · Δω)` with `Δω = 2π · Δf / fs` in rad/sample is
     * the classical Kaiser estimate. The window's own stopband level is
     * `A(β) = β / 0.1102 + 8.7` dB, so the pair (β, N) is what a design
     * actually has: asking for a deeper window at a fixed edge means more
     * taps. Using `A(β)` instead of a nominal 60 dB is what makes the estimate
     * agree with the taps — β = 8 at a 0.1·Nyquist transition needs 153 taps,
     * and a 144-tap kernel measures −62 dB at that edge while a 64-tap one
     * measures −18 dB.
     *
     * @param transitionCyclesPerSample Full transition width, cycles/sample of
     *        the design rate (from the passband edge to the stopband edge — the
     *        −6 dB point lies in the middle, so it is twice the distance from
     *        the cutoff to the stopband edge).
     * @param beta Kaiser β the taps will be windowed with.
     * @return A tap count clamped to [kMinTaps, kMaxTaps].
     */
    static int tapsForTransition(double transitionCyclesPerSample,
                                 double beta = kDefaultBeta);

    /**
     * @brief Convenience: the anti-alias kernel for decimation by `M`.
     *
     * Design rate is the input rate, so the cutoff is `factor / M` (the lower
     * Nyquist belongs to the decimated rate), and the transition is
     * `(1 − factor)/M` cycles/sample wide. `taps` is raised to what
     * `tapsForTransition()` demands when the caller asks for fewer — an
     * anti-alias kernel that does not suppress the alias is not one.
     *
     * @param M Integer decimation factor, ≥ 1.
     * @param taps Minimum tap count (default kDefaultTaps = 160).
     * @param factor Cutoff as a fraction of the *lower* Nyquist (default 0.90).
     * @param beta Kaiser β (default 8).
     */
    static FirCoefficients antiAliasForDecimation(int M, int taps = kDefaultTaps,
                                                  double factor = kDefaultCutoffRatio,
                                                  double beta = kDefaultBeta);

    /**
     * @brief Convenience: the anti-alias kernel for rational resampling.
     *
     * Design rate is `up ×` the input rate, so the passband must stop at
     * `factor · min(fs_in, fs_out)/2` — i.e. at `factor · min(up,down)/(up·down)`
     * of the **design-rate Nyquist**. The `up·down` in the denominator is not a
     * typo: the design Nyquist is `up` times the input's, and the lower of the
     * two rates brings the `down`. Omitting it makes the prototype `down×` too
     * wide, and then every polyphase branch has a different DC gain (measured:
     * a 0.5 sine came out with peaks of 31 at phase 0 for 160/147 — the phase
     * whose tap set contains the kernel's centre).
     *
     * The same 0.90 rule applies to the transition: `(1 − factor)·min/(up·down)`
     * of the design rate, so `taps` is raised to `tapsForTransition()`'s
     * estimate when it is smaller (the estimate wins over a caller asking for
     * fewer taps; `kMaxTaps` caps extreme ratios).
     *
     * @param up Interpolation factor, ≥ 1.
     * @param down Decimation factor, ≥ 1.
     * @param taps Tap count at the interpolated rate (default kDefaultTaps).
     *        Raised to at least `up` so every phase owns a coefficient: fewer
     *        taps leaves whole phases empty, and those output samples are
     *        exactly zero. Eight to sixteen taps per phase (`8·up … 16·up`) is
     *        the useful range; the result is capped at `kMaxTaps`.
     * @param factor Cutoff as a fraction of the *lower* Nyquist (default 0.90).
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
