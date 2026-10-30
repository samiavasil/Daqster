#ifndef TEST_AUDIO_CAPTURE_BUFFER_H
#define TEST_AUDIO_CAPTURE_BUFFER_H

#include <QObject>

/**
 * @brief Unit tests for `AudioCaptureBuffer::bytesFor()` (REQ-SW-PL-076).
 *
 * No audio device is involved: the header is pure arithmetic over
 * `QAudioFormat`, so the suite runs on CI without a sound card.
 */
class TestAudioCaptureBuffer : public QObject
{
    Q_OBJECT

private slots:
    /// 48 kHz mono float32 → 100 ms = 4800 frames × 4 B.
    void hundredMillisecondsAtFortyEightKilo();

    /// The old 1000-byte request delivered one backend period per 21 ms on Qt6;
    /// whatever the format, the request must stay above one PipeWire quantum.
    void requestStaysAboveOneBackendPeriod();

    /// A quantum is a FRAME count: at 8 kHz, 100 ms is only 800 frames, less
    /// than one 1024-frame period — the floor, not the time, must win there.
    void lowSampleRateIsFlooredInFrames();

    /// Channel count and sample size scale the byte count, never the frames.
    void bytesScaleWithChannelsAndSampleSize();

    /// An unusable format yields 0 — the caller leaves the backend default
    /// alone instead of asking for a zero-byte buffer.
    void unusableFormatAsksForNothing();
};

#endif // TEST_AUDIO_CAPTURE_BUFFER_H
