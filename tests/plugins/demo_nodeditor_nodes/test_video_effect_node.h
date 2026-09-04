#pragma once

#include <QtTest>

/// GUI tests for the VideoEffectNode CPU path on the shared ComputePool
/// (REQ-SW-PL-039). QTEST_MAIN in test_video_effect_node.cpp provides a
/// QApplication main; the binary runs headless via the offscreen platform
/// plugin (QT_QPA_PLATFORM=offscreen set by the CTest ENVIRONMENT property).
///
/// The CPU path is asynchronous: setInData() snapshots the input on the GUI
/// thread and submits applyCpu() to the pool; the result arrives via a queued
/// onCpuResult() slot. The tests wait with QTRY_VERIFY (event processing +
/// timeout) for the async result.
class VideoEffectNodeTest : public QObject
{
    Q_OBJECT

private slots:
    /// CpuOnly effect (blur) always runs on the CPU — the async result must
    /// arrive with the expected output frame.
    void cpuPath_blurEffect_asyncResult();

    /// GpuOrCpu effect (brightness) falls back to the CPU path when hardware
    /// GL is unavailable (headless test environment) — same async contract.
    void cpuPath_gpuOrCpu_fallsBackToCpu();

    /// The metric label is refreshed on each CPU result with the pool's
    /// per-key counters (completed/submitted/skipped + fps).
    void cpuPath_metricLabel_refreshed();

    /// m_totalFrames counts every valid setInData() call.
    void cpuPath_totalFrames_incremented();
};