#pragma once

#include <QtTest>

/**
 * @brief Continuity of the audio resampler (REQ-SW-PL-076, AC9).
 *
 * The defect these tests pin: the resampler used to be a pure function called
 * once per `SampledData` block, with the source position computed as
 * `i / ratio` starting at 0 for EVERY block. A live microphone delivers ~5.7 ms
 * blocks (172 per second at 44.1 kHz), so the phase was reset 172 times per
 * second — the first output of every block was interpolated as if the stream
 * had just started, and `i1 = min(i0 + 1, lastIndex)` made the last sample of
 * the block interpolate towards itself. At 44100 → 48000 (ratio 1.0884) that
 * is a discontinuity at every block boundary: the audible "дъвчене"/clicking.
 * At ratio 2.0 (TTS at 24 kHz → 48 kHz, one block per sentence) it never
 * showed, because 2.0 is an integer: the block-local lattice happens to line up.
 *
 * A separate binary (QtCore only): the resampler has no Multimedia dependency,
 * so these assertions run anywhere, including CI without a sound card.
 */
class AudioResampleTest : public QObject
{
    Q_OBJECT

private slots:
    /// 300 blocks of 256 samples must produce what ONE block of 76 800 would:
    /// same count (±1 of `round(total * ratio)`) and the same samples.
    void manyBlocksMatchOneBlockAndTheIdealLattice();

    /// The defining artefact: the jump across a block boundary must not exceed
    /// the largest jump inside a block. The stateless version gave ~1.6-2x.
    void blockBoundaryDoesNotJump();

    /// Equal rates are the copy-free fast path: nothing is resampled and no
    /// state is consumed (`qFuzzyCompare` early-out).
    void equalRatesArePassThroughAndConsumeNoState();

    /// `reset()` restarts the phase — the engine resets on sink (re)creation and
    /// on a stream format change, where the carried phase would belong to audio
    /// that was already discarded.
    void resetRestartsThePhase();

    /// The stateless wrappers stay available and must agree with a one-block
    /// converter (backward compatibility of `resampleMono()`/`resampleChannels()`).
    void statelessWrappersAgreeWithOneBlockConverter();
};
