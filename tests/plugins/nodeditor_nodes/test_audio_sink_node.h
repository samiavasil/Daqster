#pragma once

#include <QtTest>

/**
 * @brief Behavioural tests for AudioSinkNode (REQ-SW-PL-077).
 *
 * The node moved out of the private AI plugin, but two things about it are a
 * contract with every `.flow` file in the wild and are therefore pinned here:
 *
 *  - `name()` is still `"AudioSinkNode"` and `save()` still emits
 *    `model-name`/`outputPath`/`playbackEnabled`/`volume`. Renaming either
 *    would silently drop the node from existing scenes;
 *  - a recording appends **every** block. The private implementation opened the
 *    file with `QIODevice::Truncate` on each block and rewrote the header, so a
 *    two-block stream left only the second block on disk. `threeBlocksAreAllIn
 *    OneValidWav` fails if that defect comes back.
 *
 * QTEST_GUILESS_MAIN provides QCoreApplication: the model owns no widget
 * (embeddedWidget() is nullptr, REQ-SW-PL-051), so no QApplication and no
 * offscreen platform plugin are needed. No test needs a sound card — the
 * recording path never opens a device, and `playbackEnabled` stays false.
 */
class TestAudioSinkNode : public QObject
{
    Q_OBJECT

private slots:
    // Contract with the palette / graph model (AC1).
    void nameAndCaptionFollowTheSinkConvention();
    void portsAndDataTypeAreSampledData();
    void dataArrivalChangesNothingVisible();

    // Persistence (AC2).
    void saveCarriesModelName();
    void saveLoadRoundTripsAllThreeFields();
    void loadPartialKeepsDefaults();
    void loadInvalidValueIsIgnored();
    void configSchemaDescribesExactlyWhatSaveEmits();
    void loadExpandsEnvironmentVariables();

    // Recording (AC4) — the defect this requirement closes.
    void oneBlockWritesAValidWav();
    void threeBlocksAreAllInOneValidWav();
    void existingFileIsReplacedNotAppendedTo();
    void noPathMeansNoFile();
    void emptyInputWritesNothing();
    void rateChangeRestartsTheRecording();

    // Deprecated playback delegation.
    void enablingPlaybackDoesNotOpenADeviceOnLoad();
    void recordingWorksWithPlaybackEnabledOnADeviceLessBox();

    // Lifecycle.
    void stopFinalizesAndIsIdempotent();
};
