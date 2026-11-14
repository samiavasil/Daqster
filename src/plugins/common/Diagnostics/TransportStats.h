#pragma once

// SPDX-License-Identifier: MIT
//
// Transport cost counters (REQ-SW-PL-090).
//
// Three places in the graph copy the payload on every block / frame, and until
// now nobody could say how much: the QAudioBuffer -> SampledData copy at the
// audio source boundary, the owned frame copy the Qt5 video path has to make
// (probe buffers are recycled by the backend), and the VAD concat/carry copies
// on the private audio chain. These counters change NO behaviour — they only
// make the cost measurable, so a later optimization is justified by numbers
// instead of by re-reading the call sites.
//
// One definition in the process, for the same reason as TexturePool: the
// counters live in DaqsterCommonLibrary and every consumer (nodeditor_nodes,
// private plugins, test binaries) increments the SAME atomics. A header-only
// copy per shared library would silently report a fraction of the traffic.
//
// Hot path rules: an increment is one relaxed fetch_add and allocates nothing;
// only toLogString() builds a string — call it from a log site, never from the
// copy site.

#include "DaqsterCommonExport.h"

#include <QtGlobal>
#include <QString>

#include <atomic>

namespace Daqster {

/// Cumulative copy counters, read as one consistent-enough set of values.
struct TransportSnapshot
{
    quint64 audioAllocs = 0;      ///< QAudioBuffer -> SampledData copies.
    quint64 audioBytes = 0;       ///< Bytes copied by those.
    quint64 videoAllocs = 0;      ///< Owned (Qt5) QVideoFrame copies.
    quint64 videoBytes = 0;       ///< Bytes copied by those.
    quint64 vadConcatAllocs = 0;  ///< VAD rest-buffer concat copies.
    quint64 vadConcatBytes = 0;   ///< Bytes copied by those.
    quint64 carryCopies = 0;      ///< VAD carry (partial frame) copies.
    quint64 carryBytes = 0;       ///< Bytes copied by those.
};

/// Process-wide, lock-free transport cost counters (REQ-SW-PL-090).
///
/// Thread-safe for increments from any thread; snapshot()/reset() are safe to
/// call while producers are running (values may be torn across counters, which
/// is exactly what a cumulative counter is allowed to be).
class DAQSTERCOMMON_EXPORT TransportStats
{
public:
    /// The one instance in the process (intentionally leaked — see the .cpp).
    static TransportStats &instance();

    /// Count one audio block copy of `bytes` bytes.
    void addAudioAlloc(quint64 bytes);

    /// Count one owned video frame copy of `bytes` bytes.
    void addVideoAlloc(quint64 bytes);

    /// Count one VAD concat copy of `bytes` bytes.
    void addVadConcat(quint64 bytes);

    /// Count one VAD carry copy of `bytes` bytes.
    void addCarryCopy(quint64 bytes);

    /// Read the current counters.
    TransportSnapshot snapshot() const;

    /// Zero every counter.
    void reset();

    /// One log line, Bulgarian, stable shape:
    /// `audio: N алок / M KB · video: N алок / M KB · vad-carry: N копия / M KB`
    /// (a fourth `пренос` segment is appended only when carry copies happened).
    QString toLogString() const;

private:
    TransportStats() = default;

    std::atomic<quint64> m_audioAllocs{0};
    std::atomic<quint64> m_audioBytes{0};
    std::atomic<quint64> m_videoAllocs{0};
    std::atomic<quint64> m_videoBytes{0};
    std::atomic<quint64> m_vadConcatAllocs{0};
    std::atomic<quint64> m_vadConcatBytes{0};
    std::atomic<quint64> m_carryCopies{0};
    std::atomic<quint64> m_carryBytes{0};
};

} // namespace Daqster
