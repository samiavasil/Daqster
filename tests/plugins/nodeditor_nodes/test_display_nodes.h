#ifndef TESTDISPLAYNODES_H
#define TESTDISPLAYNODES_H

#include <QtTest/QtTest>

/**
 * @brief Behavioural tests for the node debug displays (REQ-SW-PL-090).
 *
 * Covers the three new consumer nodes (TextDisplay / ImageDisplay /
 * FrameWindowDisplay) and the EmbeddingData extension of the existing
 * NumberDisplayDataModel.
 *
 * QTEST_GUILESS_MAIN-compatible: every model is widget-free
 * (embeddedWidget() == nullptr) and QImage / QVideoFrame construction needs
 * only QCoreApplication.
 */
class TestDisplayNodes : public QObject
{
    Q_OBJECT

private slots:
    // ── contract with the graph model ─────────────────────────────────────
    void textDisplayIsAConsumer();
    void imageDisplayIsAConsumer();
    void frameWindowDisplayIsAConsumer();

    // ── TextDisplay ───────────────────────────────────────────────────────
    void textAccumulatesCounters();
    void textSnapshotTruncatesPreviewOnly();
    void textSnapshotCountsLines();
    void textRejectsForeignData();
    void textSnapshotIsDelivered();

    // ── ImageDisplay ──────────────────────────────────────────────────────
    void imageSnapshotCarriesGeometryAndFormat();
    void imageThumbnailIsBounded();
    void imageThumbnailDoesNotAliasTheSource();
    void imageNodeDoesNotMutateThePayload();
    void imageFormatNamesAreStable();

    // ── FrameWindowDisplay ────────────────────────────────────────────────
    void frameWindowSnapshotReportsMetadata();
    void frameWindowKeepsAtMostFourThumbnails();
    void frameWindowWithoutCpuFramesIsMetadataOnly();

    // ── NumberDisplay + EmbeddingData ─────────────────────────────────────
    void embeddingSummaryReportsStatistics();
    void embeddingPortTypeFollowsTheSelector();
    void embeddingEmptyIsRejected();
    void numberDisplaySaveLoadRoundTrip();
    void numberDisplayDoubleBehaviourIsUnchanged();
};

#endif // TESTDISPLAYNODES_H
