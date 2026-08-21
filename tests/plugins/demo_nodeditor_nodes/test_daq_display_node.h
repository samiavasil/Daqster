#pragma once

#include <QtTest>

/// GUI tests for DaqDisplayNode save()/restore() round-trip (REQ-SW-PL-023 §6,
/// REQ-SW-PL-025 §4). QTEST_MAIN in test_daq_display_node.cpp provides a
/// QApplication main; the binary runs headless via the offscreen platform
/// plugin (QT_QPA_PLATFORM=offscreen set by the CTest ENVIRONMENT property).
class DaqDisplayNodeTest : public QObject
{
    Q_OBJECT

private slots:
    void save_restore_v1Compat();
    void save_restore_v2_ringSeconds();
    void save_restore_multipleCards();
    void restore_v1_fileDefaults();
    void save_restore_mode_physical();

    // Worker-owned rolling ring buffer (REQ-SW-PL-025 AC 3/AC 4), exercised
    // through the friend-declared static appendRingBlock() + ComputeState.
    void ringBuffer_rollingWindowDropsOldest();
    void ringBuffer_descriptorChangeReset();
};
