#pragma once

#include <QtTest>

/**
 * @brief GUI tests for AudioOutputWidget (REQ-SW-PL-076, AC6).
 *
 * QTEST_MAIN provides QApplication; the binary runs with
 * QT_QPA_PLATFORM=offscreen (see CMakeLists.txt), so no display is needed.
 *
 * What is pinned here is the model ⇄ widget contract, not the pixels:
 *  - every control shows the model's `config()` when the widget appears (a
 *    `.flow` is loaded before the widget exists);
 *  - every edit goes through `AudioOutputNode::setConfig()`, so the engine and
 *    `save()` see it — a widget that keeps its own copy looks right on screen
 *    and saves the old value;
 *  - `load()` (a flow opened, a REST config POST) re-fills the fields without
 *    writing them back;
 *  - a persisted device that this machine does not have stays visible and
 *    selected, so `save()` does not silently replace it with the default.
 *
 * No test needs a real audio device: the node is configured with an id that
 * cannot resolve, which is also the headless/CI path.
 */
class TestAudioOutputWidget : public QObject
{
    Q_OBJECT

private slots:
    void controlsShowModelConfig();
    void editingVolumeAndBufferReachesTheModel();
    void enabledToggleReachesTheModelAndGreysTheControls();
    void loadRefillsTheControls();
    void unknownPersistedDeviceStaysSelectable();
    void statusLabelFollowsTheModel();
};
