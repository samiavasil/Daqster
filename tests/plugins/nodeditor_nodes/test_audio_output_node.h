#pragma once

#include <QtTest>

/**
 * @brief Behavioural tests for AudioOutputNode + AudioPlaybackEngine
 * (REQ-SW-PL-076).
 *
 * Two properties cannot be seen from a `.flow` file and are therefore pinned
 * here:
 *
 *  - **one sink for many blocks** — the private `AudioSinkNode` this node
 *    replaces created a `QAudioSink` per block and destroyed it immediately,
 *    which "worked" (no crash, no error) while nothing was ever audible. The
 *    engine counts the sinks it opened (`sinksCreated()`), so a regression to
 *    per-block creation fails a test instead of silently going quiet;
 *  - **no device is a normal state** — headless runs and CI boxes have no sound
 *    card. The node must count blocks and say why, never crash and never block.
 *
 * No test here *requires* a real audio device: the no-device path is exercised
 * with a `deviceId` that cannot resolve (a fixed string), and the device path
 * is asserted only when `AudioCompat::deviceFromId("")` happens to be non-null.
 * The two tests that can only exist on a real device (`firstBlockAutoArmsPlayback`
 * after its no-device branch, `partiallyPlayedBlockIsNotDropped`) `QSKIP` on a
 * box without a sound card instead of failing.
 *
 * The queue tests (`largeBlockIsQueuedNotDropped`, ...) exist because the node
 * used to drop a block whenever `bytesFree() < block size`: `TtsNode` emits a
 * whole sentence as ONE block (~1.9 s), so at `bufferMs: 200` every sentence was
 * silent. The device buffer decides a write CHUNK, never whether a block is
 * accepted.
 *
 * QTEST_GUILESS_MAIN provides QCoreApplication. The model owns no widget
 * (embeddedWidget() is nullptr, REQ-SW-PL-051), so nothing needs a
 * QApplication or the offscreen platform plugin.
 */
class TestAudioOutputNode : public QObject
{
    Q_OBJECT

private slots:
    // Contract with the palette / graph model.
    void nameAndCaptionMatchTheSinkConvention();
    void portsAndDataTypeAreSampledData();

    // Persistence (AC3).
    void saveLoadRoundTripsAllFields();
    void saveCarriesModelName();
    void configSchemaDescribesExactlyWhatSaveEmits();
    void volumeAndBufferAreClamped();
    void loadClampsOutOfRangeValues();

    // Format bridge / status plumbing.
    void statusIsNeverEmpty();
    void statusChangesAreForwarded();
    // The encoding ladder behind AC4/AC9 (rate kept, encoding negotiated).
    void encodingLadderBuildsEveryCandidate();
    void conversionCarriesThePhaseAcrossBlocks();

    // No output device (AC5) — the path every headless run takes.
    void unknownDeviceQueuesBlocksAndOpensNothing();
    void noDeviceStatusExplainsWhy();
    void stopIsSafeBeforeStartAndIdempotent();

    // One persistent sink for N blocks (AC2).
    void manyBlocksUseAtMostOneSink();
    void nullInputChangesNothing();

    // Queue semantics: a block is never limited by the device buffer.
    void deviceIsResolvedOnceAndCached();
    void firstBlockAutoArmsPlayback();
    void explicitStopIsNotUndoneByAutoArm();
    void disabledNodeDoesNotAutoArm();
    void blocksBeforeStartAreQueuedNotLost();
    void largeBlockIsQueuedNotDropped();
    void blockOverQueueBoundDropsOldest();
    void singleSentenceUnderTheOldBoundIsNotDropped();
    void vadSegmentLengthIsNotTrimmed();
    void blockOverHardCapIsTrimmedNotSilent();
    void partiallyPlayedBlockIsNotDropped();
    void queueStaysBoundedByTime();
    void stopClearsTheQueue();
    // The device ran dry (AC10): visible instead of a healthy-looking status.
    void deviceUnderrunIsCountedAndVisible();
    // Step 3 of the Qt6 starvation fix: a sink between bursts is still playing
    // while audio is queued — "ready" only when there is really nothing left.
    void queuedAudioReportsPlayingEvenWhileTheSinkIdles();
};
