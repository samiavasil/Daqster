#ifndef TESTFRAMEWINDOWNODE_H
#define TESTFRAMEWINDOWNODE_H

#include <QtTest/QtTest>

/**
 * @brief Behavioural tests for FrameWindowNode + FrameWindowData
 * (REQ-SW-PL-086).
 *
 * QTEST_GUILESS_MAIN-compatible: the model owns no widget, and QImage /
 * QVideoFrame construction needs only QCoreApplication.
 */
class TestFrameWindowNode : public QObject
{
    Q_OBJECT

private slots:
    // ── contract with the graph model ─────────────────────────────────────
    void portsAndDataTypes();
    void embeddedWidgetIsNull();
    void optOutOfGeometryRecompute();
    void nameAndCaption();

    // ── emit modes ────────────────────────────────────────────────────────
    void fullModeWaitsForWindowThenEmitsEveryFrame();
    void everyFrameModeEmitsOnEveryFrame();
    void intervalModeEmitsAtMostOncePerInterval();

    // ── window size ───────────────────────────────────────────────────────
    void windowKeepsOnlyLastFrames();
    void windowSizeIsClamped();

    // ── zero-copy ─────────────────────────────────────────────────────────
    void itemsShareTheInputSharedPointers();

    // ── empty input ───────────────────────────────────────────────────────
    void nullInputClearsWindowAndInvalidates();
    void nonVideoInputClearsWindow();

    // ── persistence ───────────────────────────────────────────────────────
    void saveLoadRoundTrip();
};

#endif // TESTFRAMEWINDOWNODE_H
