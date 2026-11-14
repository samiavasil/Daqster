#pragma once

#include <QtTest>

/// Node-level tests for the audio rate domain (REQ-SW-PL-092).
///
/// The DSP *engines* are covered by `TestDspFilters` — these tests belong to
/// the nodes, where the mistakes are different: a block that carries the wrong
/// `firstIndex`, a carry that is dropped, a rate that is published as the input
/// rate, a coefficient file that a scene names but the node silently replaces
/// with the default. None of that is visible from the engine's point of view.
///
/// QtCore only — no widget, no audio device, no model weights. The nodes are
/// compiled directly into the binary (the same pattern as
/// `demo_nodeditor_framesampler_tests`).
class TestAudioDspNodes : public QObject
{
    Q_OBJECT

private slots:
    /// 10 × 960 → 5 × 1920, then 2 × 1920 → 4 × 960; `firstIndex` continuous;
    /// the carry survives calls that produce nothing.
    void resizerAggregatesSplitsAndKeepsTheCarry();

    /// 48 kHz → 16 kHz: a 1 kHz tone survives, the 20 kHz tone that folds to
    /// 4 kHz is ≥60 dB down, and the published rate is the output rate.
    void decimatorPreservesToneAndSuppressesAlias();

    /// 44.1 kHz → 48 kHz as 160/147: the length is right within one block and
    /// the `firstIndex` mapping is `⌊k·down/up⌋`.
    void resamplerAppliesRatioAndDocumentsTimebase();

    /// Resolution order: `filterRef` under `<DEPS>/filters/`, then a
    /// scene-relative `filterPath`, then the built-in default — with the chosen
    /// kernel's ref + sha256 prefix reported.
    void coefficientsResolveFromRefPathAndDefault();

    /// An invalid coefficient file is an Error that names the file and the
    /// value, not a silent fallback to the default.
    void invalidCoefficientFileIsAnError();

    /// All three schemas describe exactly `save()` and mark every field live.
    void schemasAreLiveAndCoverSaveKeys();
};
