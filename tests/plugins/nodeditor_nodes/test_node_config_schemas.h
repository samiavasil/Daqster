#pragma once

#include <QtTest>

namespace QtNodes { class NodeDelegateModel; }

/// The IStartable public nodes must describe their own `save()` output.
///
/// REQ-SW-PL-091: the control plane answers `restartRequired` from the node's
/// schema — `"x-restart": true|false` per changed key — and falls back to "the
/// node implements IStartable" only when a key has **no** marker (a schema
/// derived from `save()`). That fallback is conservative, not correct: a live
/// field (GamepadModel::pollRateHz, SystemMonitorModel::pollIntervalSec, …)
/// would then report a restart it does not need.
///
/// So every node whose config can be written over the API has to self-describe.
/// These tests keep those schemas honest: the schema's property set must equal
/// the key set `save()` really emits, and every property must carry an
/// `x-restart` marker. A field added to `save()` without a schema entry — or a
/// schema entry for a field that no longer exists — fails here instead of
/// silently degrading the API answer.
class TestNodeConfigSchemas : public QObject
{
    Q_OBJECT

private slots:
    void cameraSource_schemaCoversSaveKeys();
    void videoFileSource_schemaCoversSaveKeys();
    void streamSource_schemaCoversSaveKeys();
    void filePlayback_schemaCoversSaveKeys();
    void fileRecord_schemaCoversSaveKeys();
    void networkSource_schemaCoversSaveKeys();
    void networkSink_schemaCoversSaveKeys();
    void gamepad_schemaCoversSaveKeys();
    void plutoSdr_schemaCoversSaveKeys();
    void jackDetect_schemaCoversSaveKeys();
    void systemMonitor_schemaCoversSaveKeys();
    void gpuMonitor_schemaCoversSaveKeys();

private:
    /// Assert that @p model's `configSchema()` describes exactly the object its
    /// `save()` emits (minus the base's read-only `model-name`) and marks every
    /// field with `x-restart`.
    ///
    /// @param model            Model under test, constructed by the test slot.
    /// @param conditionalKeys  Keys `save()` omits while the node has no device
    ///                         or file selected (the schema still describes
    ///                         them: a write may set them).
    static void verifySchemaMatchesSave(QtNodes::NodeDelegateModel& model,
                                       const QStringList& conditionalKeys = {});
};
