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
 * No test here requires a real audio device: the no-device path is exercised
 * with a `deviceId` that cannot resolve (a fixed string), and the device path
 * is asserted only when `AudioCompat::deviceFromId("")` happens to be non-null.
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

    // No output device (AC5) — the path every headless run takes.
    void unknownDeviceQueuesBlocksAndOpensNothing();
    void noDeviceStatusExplainsWhy();
    void stopIsSafeBeforeStartAndIdempotent();

    // One persistent sink for N blocks (AC2).
    void manyBlocksUseAtMostOneSink();
    void nullInputChangesNothing();

    // Queue semantics: a block is never limited by the device buffer.
    void deviceIsResolvedOnceAndCached();
    void blocksBeforeStartAreQueuedNotLost();
    void largeBlockIsQueuedNotDropped();
    void blockOverQueueBoundDropsOldest();
    void singleSentenceUnderTheOldBoundIsNotDropped();
    void blockOverHardCapIsTrimmedNotSilent();
    void queueStaysBoundedByTime();
    void stopClearsTheQueue();
};
