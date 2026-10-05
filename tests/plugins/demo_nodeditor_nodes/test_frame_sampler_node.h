#pragma once

#include <QtTest>

/// Behavioural tests for FrameSamplerNode (REQ-SW-PL-030, AC 6).
///
/// The node is a gate, not a converter: it decides which incoming
/// VideoFrameData passes through and which are dropped, and when it does
/// pass one, it must pass the *same* object (REQ-SW-PL-032 zero-copy). Both
/// halves are invisible from the flow graph — a gate that always passes looks
/// correct on screen while quietly destroying the point of the node, and a
/// gate that copies looks correct too while doubling the per-frame cost of
/// every pipeline it sits in. Hence these tests assert on identity and on
/// emission counts, not on pixel data.
///
/// QTEST_GUILESS_MAIN provides QCoreApplication. The node owns no widget
/// (embeddedWidget() is nullptr by REQ-SW-PL-051), so nothing here needs a
/// QApplication, no OpenCV and no GL context.
class TestFrameSamplerNode : public QObject
{
    Q_OBJECT

private slots:
    // Contract with the palette / graph model.
    void portsAndDataTypeAreVideoFrameData();
    void embeddedWidgetIsNull();
    void optOutOfGeometryRecompute();

    // EveryNth gate.
    void everyNthPassesEveryNthFrame();
    void everyNthWithOnePassesEverything();

    // MaxFps gate. The gate is timer-based, so these use a real sleep rather
    // than a fake clock — the alternative would be asserting on an
    // implementation detail (the QElapsedTimer restart) instead of the
    // observable rate.
    //
    // Measured behaviour (see maxFpsDropsTheFirstFrameForOneInterval): the
    // interval timer STARTS on the incoming frame rather than passing it, so
    // the first frame after every reset is dropped and the next one is let
    // through one interval later.
    void maxFpsDropsTheFirstFrameForOneInterval();
    void maxFpsDropsFramesWithinTheInterval();
    void maxFpsPassesAgainAfterTheInterval();

    // Changing any parameter must restart the gate, otherwise switching mode
    // mid-stream inherits a half-consumed counter and the first frame after
    // the switch is silently dropped (or, for MaxFps, blocked until a stale
    // interval has elapsed).
    void modeChangeResetsGate();
    void everyNChangeResetsGate();
    void maxFpsChangeResetsGate();
    void loadResetsGate();

    /// Pins the one-interval initial latency of MaxFps, and its practical
    /// consequence. Recorded here rather than "fixed": it is the behaviour the
    /// implementation documents, "at most maxFps frames per second" does not
    /// forbid it, and whether a live view should show a one-interval blank on
    /// (re)start is a product decision, not a test decision.
    void maxFpsLatencyIsOneInterval();

    // Zero-copy / fan-out invariants (REQ-SW-PL-030, REQ-SW-PL-032).
    void zeroCopySharesTheSameFrame();
    void fanOutToManyConsumersDoesNotCopy();
    void droppedFrameKeepsThePreviousOutput();

    // Invalid input must clear the output, not pass a stale frame on.
    void invalidInputClearsOutput();
    void wrongDataTypeClearsOutput();
    void nullInputClearsOutput();

    // Persistence: a gate that comes back from a saved flow with the wrong
    // parameters silently changes the frame rate of every replay.
    void saveLoadRoundTripsParameters();
    void saveCarriesModelName();
    void parameterValuesAreClamped();
    void loadToleratesGarbage();
};
