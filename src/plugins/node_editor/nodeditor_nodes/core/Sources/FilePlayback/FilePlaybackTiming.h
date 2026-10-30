#pragma once

#include "NodeDataTypes/SampledStreamDescriptor.h"

#include <QtGlobal>

/**
 * @brief Real-time timing for the File Playback source (REQ-SW-PL-043).
 *
 * The node reads a chunk of raw bytes per timer tick, so two quantities are
 * easy to confuse and expensive to confuse:
 *   - `chunkSize` — a BYTE budget (`QFile::read()` counts bytes);
 *   - `sampleRate` — FRAMES per second.
 *
 * Dividing bytes by the frame rate (the original implementation) is only
 * correct for 8-bit mono streams. For FLOAT32 mono (4 B/frame) it produced an
 * interval 4x longer than the audio in the chunk — 92 ms of silence scheduled
 * for 23 ms of sound — so the file played 4x too slowly AND the downstream
 * sink starved (underrun on every block, REQ-SW-PL-076).
 *
 * Everything here therefore goes through `SampledStreamDescriptor::bytesPerFrame()`
 * and never hardcodes 2 or 4.
 */
namespace FilePlaybackTiming {

/// Smallest timer interval that can still express "play the chunk now".
constexpr int kMinIntervalMs = 1;

/**
 * @brief Byte count of the whole frames that fit in a byte budget.
 *
 * A frame is `descriptor.bytesPerFrame()` bytes — one sample of every channel,
 * per the `SampledStreamDescriptor` layout convention. A chunk must never split
 * a frame, so the budget is rounded DOWN to a whole number of frames.
 *
 * @param descriptor Stream layout (sample rate + per-channel sample types).
 * @param maxChunkBytes Upper bound for one chunk, in bytes.
 * @return Bytes covered by whole frames, or 0 when the layout is unusable or
 *         the budget is smaller than a single frame.
 */
inline qint64 chunkBytesFor(const SampledStreamDescriptor &descriptor, qint64 maxChunkBytes)
{
    const int bytesPerFrame = descriptor.bytesPerFrame();
    if (bytesPerFrame <= 0 || maxChunkBytes < bytesPerFrame)
        return 0;

    return (maxChunkBytes / bytesPerFrame) * bytesPerFrame;
}

/**
 * @brief Real-time duration of one chunk, in milliseconds.
 *
 * The timer interval must match how long the bytes in the chunk actually play,
 * so it is derived from FRAMES, never from bytes:
 * `frames = chunkBytes / bytesPerFrame(descriptor)`, then
 * `intervalMs = frames * 1000 / sampleRate`. Two files with the same number of
 * frames per chunk get the same interval regardless of sample type (S16 vs
 * F32), because `bytesPerFrame` scales the byte count with the sample size.
 *
 * @param descriptor Stream layout (sample rate + per-channel sample types).
 * @param chunkBytes Size of one chunk in bytes (should be frame-aligned, see
 *        chunkBytesFor()).
 * @return Interval in ms, rounded to the nearest millisecond and clamped to
 *         kMinIntervalMs; -1 when the descriptor or the chunk is unusable
 *         (cannot happen for a playing stream — the caller must refuse to
 *         start instead).
 */
inline int chunkIntervalMs(const SampledStreamDescriptor &descriptor, qint64 chunkBytes)
{
    if (descriptor.sampleRate <= 0.0 || chunkBytes <= 0)
        return -1;

    const int bytesPerFrame = descriptor.bytesPerFrame();
    if (bytesPerFrame <= 0)
        return -1;

    const qint64 frames = chunkBytes / bytesPerFrame;
    if (frames <= 0)
        return -1;

    const double intervalMs = static_cast<double>(frames) * 1000.0 / descriptor.sampleRate;
    const int rounded = static_cast<int>(intervalMs + 0.5);
    return rounded < kMinIntervalMs ? kMinIntervalMs : rounded;
}

} // namespace FilePlaybackTiming
