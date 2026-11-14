#ifndef AUDIOFILTERRESOLVER_H
#define AUDIOFILTERRESOLVER_H

// SPDX-License-Identifier: MIT

// FilterResolver — one answer to "which taps does this node use?" (REQ-SW-PL-092).
//
// `SampleDecimatorNode` and `SampleResamplerNode` both need a kernel and both
// must *record* which one they got. Writing that twice is how two nodes end up
// disagreeing about the order of fallbacks, so the lookup lives here once:
//
//   1. `filterRef`     → `<DEPS>/filters/<ref>.json` (SceneLayout, machine table)
//   2. `filterPath`    → a file, resolved with `SceneVariables::resolvePath()`
//                        so `"filters/x.json"` next to the scene works
//   3. built-in default → `antiAliasForDecimation(M)` / `antiAliasForResampling(L,M)`
//
// A ref that names a file which is not there does **not** abort: it falls
// through and the miss is recorded in `source`, because a scene written on
// another machine must still run and the status line must say why the numbers
// differ. A file that *is* there but is invalid (unnormalised, too short, bad
// sha256, non-finite taps) is an error that names the value — a wrong kernel is
// worse than no kernel: it is a silently wrong result.
//
// QtCore only, no widgets.

#include "Dsp/FirCoefficients.h"

#include <QString>

/**
 * @brief How the kernel is designed when no file is configured.
 *
 * The two cases have different design rates: a decimator filters at the input
 * rate (cutoff `factor/M`), a resampler at `up ×` the input rate (cutoff
 * `factor·min(up,down)/up`). `FirCoefficients` spells both out; this enum only
 * says which one the caller wants.
 */
enum class AudioFilterKind {
    Decimation,  ///< integer M: `antiAliasForDecimation(down)`
    Resampling,  ///< rational L/M: `antiAliasForResampling(up, down)`
};

/// The outcome of a lookup: the kernel, where it came from, and why it failed.
struct AudioFilterResolution {
    FirCoefficients coefficients;
    /// Provenance for the node's status/tooltip: `filters/<file>`,
    /// `path:<resolved path>`, `default` — plus a `→ default` note when a named
    /// ref was missing, or the error text when `ok` is false.
    QString source;
    /// Human-readable failure reason; empty when `ok`.
    QString error;
    bool ok = false;

    /// `source · sha256 <first 8 hex digits>` — the identity a user compares
    /// between two runs. The error text when the lookup failed.
    QString describe() const;
};

/**
 * @brief Resolve the kernel a rate-changing node should use.
 *
 * @param kind        Which default to design (decimation or resampling).
 * @param up          Interpolation factor L (1 for decimation).
 * @param down        Decimation factor M (the integer factor for decimation).
 * @param filterRef   Name under `<DEPS>/filters/`; empty = not configured.
 * @param filterPath  Path (may be scene-relative or carry `${VAR}`); empty =
 *                    not configured. Ignored when `filterRef` resolved.
 * @return The kernel plus its provenance. `ok` is false only for a file that
 *         exists but is unusable, or for a factor outside the contract — never
 *         for "nothing configured": that is the built-in default.
 */
AudioFilterResolution resolveAudioFilter(AudioFilterKind kind, int up, int down,
                                         const QString& filterRef,
                                         const QString& filterPath);

#endif // AUDIOFILTERRESOLVER_H
