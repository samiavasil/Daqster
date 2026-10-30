#pragma once

#include <QtTest>

/**
 * @brief Real-time pacing of the File Playback source (REQ-SW-PL-076).
 *
 * The defect these tests pin: `startPlayback()` computed the timer interval as
 * `chunkSize / sampleRate * 1000`, i.e. bytes divided by FRAMES per second. At
 * FLOAT32 (4 B/sample) that is 4x the true duration of the chunk — 92 ms of
 * wall time scheduled for 23 ms of audio — so the file played 4x too slowly and
 * the downstream sink starved (one underrun every block). At 8-bit mono only,
 * bytes == frames, which is why it looked correct.
 *
 * Two layers are covered:
 *   - FilePlaybackTiming (the arithmetic: bytes → frames → ms, never
 *     hardcoding 2 or 4 bytes per sample);
 *   - the real FilePlaybackModel, driven through its public slots, asserting
 *     the QTimer interval it actually starts with. The model-level tests fail on
 *     the old byte-based formula (23 ms expected vs 92 ms produced for F32).
 */
class TestFilePlaybackTiming : public QObject
{
    Q_OBJECT

private slots:
    /// N frames at 44100 Hz → N * 1000 / 44100 ms, for a stream whose bytes
    /// must never be mistaken for frames (S16 mono: 2 bytes/frame).
    void intervalIsDerivedFromFramesNotBytes();

    /// Same frame count, different sample type → same interval. This is the
    /// property the byte-based formula broke: F32 got 4x the interval of S16.
    void sameFrameCountGivesSameIntervalRegardlessOfSampleType();

    /// An unusable layout (no channels / no sample rate) must report "no
    /// interval" so the caller refuses to start instead of dividing by zero.
    void unusableLayoutHasNoInterval();

    /// The byte budget is rounded down to whole frames — a chunk must never
    /// split a frame (INT24 mono: 3 bytes/frame, 4096 B → 1365 frames = 4095 B).
    void chunkBudgetIsRoundedToWholeFrames();

    /// The model actually starts its timer with the frame-derived interval:
    /// the duration of the FRAMES in a 4096-byte chunk (23 ms for F32 mono,
    /// 46 ms for S16 mono), never 92 ms — the byte-based value for both.
    void modelTimerUsesRealTimeInterval();

    /// Invalid descriptors are refused (no timer, clear status) instead of
    /// playing at a nonsense tempo.
    void modelRefusesUnusableDescriptor();
};
