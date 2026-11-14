#pragma once

#include <QtTest>

/// DSP engine unit tests (REQ-SW-PL-092) — `FirCoefficients`, `FirDecimator`,
/// `PolyphaseResampler`.
///
/// Every assertion is a property the ADR promises, not a golden vector: the
/// numbers (DC gain, alias attenuation in dB, group delay, bit-identical
/// chunking) are computed from the signal itself, so a redesign of the kernel
/// that keeps the promise keeps the test green, and a redesign that breaks it
/// cannot be excused by "the reference file changed".
///
/// QtCore only — no Multimedia, no widgets, no GPU, no model weights.
class TestDspFilters : public QObject
{
    Q_OBJECT

private slots:
    /// The built-in default: normalised, linear phase, stable hash, kDefaultTaps.
    void defaultKernelIsNormalisedLinearPhaseAndHashed();

    /// Constant 1.0 in → 1.0 out for both engines (a rate change must not be a
    /// gain change).
    void constantInputKeepsUnityDcGain();

    /// 48 kHz with 1 kHz + 20 kHz, decimated by 3: the 20 kHz component folds
    /// to 4 kHz and must land ≥60 dB below the 1 kHz tone.
    void decimatorSuppressesAliasAboveSixtyDb();

    /// The built-in anti-alias default is the 0.90 rule: −6 dB at 0.9 of the
    /// output Nyquist and ≥60 dB at the output Nyquist itself.
    void antiAliasDefaultHoldsSixtyDbAtOutputNyquist();

    /// A 6.5 kHz tone survives 48 kHz → 16 kHz within ±5 % (the 0.45 default
    /// killed it; that is the defect this guard exists for).
    void decimatorPassesSixPointFiveKhz();

    /// 1 kHz at 48 kHz → 16 kHz keeps amplitude (±1 %) and frequency.
    void decimatorPreservesSineAmplitudeAndFrequency();

    /// Two fresh instances, same input, bit-identical output.
    void processingIsDeterministic();

    /// `latencySamples()` equals the measured impulse-response centroid (±1).
    void reportedLatencyMatchesMeasuredGroupDelay();

    /// Valid JSON / bare-taps files load; bad ones are rejected with a reason.
    void coefficientFilesLoadAndValidate();

    /// The key streaming property: 5 × 100 samples == 1 × 480 samples.
    void resamplerChunkingDoesNotDrift();

    /// Every polyphase branch has (approximately) the same DC gain: a 0.5 sine
    /// through 160/147 comes out at 0.5, with no phase whose taps blow up.
    ///
    /// Guards the cutoff formula: with `factor·min/up` instead of
    /// `factor·min/(up·down)` the prototype is `down×` too wide, the branch
    /// that contains the kernel's centre has a gain of ~up·h[0], and the
    /// output is silence punctuated by spikes (measured peak 31 for 160/147).
    void interpolatorKeepsUnityGainAcrossPhases();
};
