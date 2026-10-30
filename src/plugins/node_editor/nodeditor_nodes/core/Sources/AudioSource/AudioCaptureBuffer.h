#pragma once

#include "AudioCompat.h"

#include <QAudioFormat>
#include <QtGlobal>

#include <algorithm>

/**
 * @brief Sizing of the capture buffer for the AudioSource node (REQ-SW-PL-076
 * defect: on Qt6 the microphone delivered a quarter of real time).
 *
 * `MicCaptureWorker` feeds a `QIODevice` to `QAudioSource`/`QAudioInput` and has
 * to tell the backend how much audio to buffer. The two Qt versions read that
 * number in **different directions**, and the difference is not cosmetic:
 *
 *   - **Qt5** — `setBufferSize()` is a *period*: the backend pulls that many
 *     bytes each time it needs data, so 1000 bytes at 48 kHz = 5.2 ms, one
 *     callback per ~5 ms, i.e. real time.
 *   - **Qt6** — the backend reads **at most `bufferSize()` bytes per server
 *     period**, whatever the period is. With a PipeWire quantum of 1024 frames
 *     (21.3 ms at 48 kHz) a 1000-byte request delivers 1000 bytes every 21.3 ms,
 *     no matter how much audio the device produced in that time.
 *
 * Measured with a standalone probe on this machine (PipeWire, 48 kHz mono f32,
 * two seconds of capture, bytes delivered / elapsed):
 *
 * | Qt | `setBufferSize` | delivered | share of real time |
 * |----|-----------------|-----------|--------------------|
 * | 5  | 1000 B          | 192 000 B/s | 100 %, 375 callbacks/s |
 * | 6  | 1000 B          |  45 977 B/s | **24 %, one 1000 B callback per 21 ms** |
 * | 6  | 19 200 B (100 ms) | 186 368 B/s | 97 %, real time again |
 *
 * So a fixed small byte budget is a Qt5-only assumption. Everything downstream
 * starves with it: `AudioOutput` receives 5 ms of audio every 21 ms, its 200 ms
 * device buffer never fills, the sound stutters, the status says "ready" and the
 * underrun counter increments once per starvation cycle (796 in a real run).
 * The sink cannot compensate — it is handed a quarter of the audio it needs.
 *
 * The request is therefore derived from the FORMAT and bounded in TIME, with a
 * frame floor above one server period: 100 ms, and never fewer than four
 * PipeWire quanta of 1024 frames. The floor matters because a quantum is a
 * FRAME count, not a duration — 100 ms at 8 kHz is only 800 frames, i.e. less
 * than one quantum, which would bring the starvation straight back.
 */
namespace AudioCaptureBuffer {

/// How much audio the buffer asks for, in milliseconds (100 ms = one to five
/// server periods on a normal desktop: a PipeWire quantum is 21.3 ms at 48 kHz).
constexpr int kBufferMs = 100;

/// Smallest request in FRAMES — four PipeWire/PulseAudio periods of 1024
/// frames. Keeps every sample rate above one backend period (100 ms at 8 kHz is
/// 800 frames, less than a single quantum).
constexpr qint64 kMinFrames = 4 * 1024;

/**
 * @brief Bytes to pass to `QAudioSource::setBufferSize()` for @p format.
 *
 * `max(100 ms, 4096 frames)` worth of audio, in the format's frame size.
 *
 * @param format Format the capture stream is opened with
 * @return Buffer size in bytes; 0 when the format is unusable (no rate, no
 *         channels or no known sample size) — the caller then leaves the
 *         backend's default alone instead of requesting zero bytes
 */
inline qint64 bytesFor(const QAudioFormat &format)
{
    const int bytesPerSample = AudioCompat::sampleSize(format) / 8;
    const int bytesPerFrame = format.channelCount() * bytesPerSample;
    if (format.sampleRate() <= 0 || bytesPerFrame <= 0)
        return 0;

    const qint64 framesFromTime = qint64(format.sampleRate()) * kBufferMs / 1000;
    const qint64 frames = std::max<qint64>(framesFromTime, kMinFrames);
    return frames * bytesPerFrame;
}

} // namespace AudioCaptureBuffer
