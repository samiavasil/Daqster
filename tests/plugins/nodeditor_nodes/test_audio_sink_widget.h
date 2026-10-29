#pragma once

#include <QtTest>

/**
 * @brief GUI tests for AudioSinkWidget (REQ-SW-PL-077).
 *
 * The widget edits the three persisted fields of `AudioSinkNode::Config` and
 * follows `configChanged()`. Two properties are easy to get subtly wrong and are
 * pinned here:
 *
 *  - **no write-back loop** — `load()`/`setConfig()` must fill the controls
 *    without the fill itself being written back to the model. An endless
 *    `configChanged` → `applyToModel` → `setConfig` loop would re-open the WAV
 *    file (and, with playback enabled, the device) on every keystroke;
 *  - **a partial edit keeps the other fields** — the widget applies one `Config`
 *    built from the model's current one, so editing the volume must not reset
 *    the path or the deprecated playback flag.
 *
 * QTEST_MAIN provides QApplication; the binary runs under
 * `QT_QPA_PLATFORM=offscreen` (set by ctest). No test opens a file dialog and no
 * test enables playback with a real device.
 */
class TestAudioSinkWidget : public QObject
{
    Q_OBJECT

private slots:
    void controlsShowModelConfig();
    void editingVolumeReachesTheModel();
    void editingPlaybackFlagReachesTheModel();
    void loadRefillsTheControlsWithoutWritingBack();
    void schemaBoundsMatchTheControls();
    void playbackFlagIsMarkedDeprecated();
};
