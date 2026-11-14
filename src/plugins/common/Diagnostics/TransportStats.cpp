// SPDX-License-Identifier: MIT
//
// TransportStats — process-wide copy counters (REQ-SW-PL-090), see the header
// for what the three groups of counters are for.

#include "Diagnostics/TransportStats.h"

namespace Daqster {

TransportStats &TransportStats::instance()
{
    // Intentionally leaked (same lifetime rule as TexturePool::instance()):
    // source nodes and workers may still increment while static destructors
    // run at process exit, and a destroyed counter would be a crash in the
    // teardown path of exactly the code we are measuring.
    static TransportStats *stats = new TransportStats();
    return *stats;
}

void TransportStats::addAudioAlloc(quint64 bytes)
{
    m_audioAllocs.fetch_add(1, std::memory_order_relaxed);
    m_audioBytes.fetch_add(bytes, std::memory_order_relaxed);
}

void TransportStats::addVideoAlloc(quint64 bytes)
{
    m_videoAllocs.fetch_add(1, std::memory_order_relaxed);
    m_videoBytes.fetch_add(bytes, std::memory_order_relaxed);
}

void TransportStats::addVadConcat(quint64 bytes)
{
    m_vadConcatAllocs.fetch_add(1, std::memory_order_relaxed);
    m_vadConcatBytes.fetch_add(bytes, std::memory_order_relaxed);
}

void TransportStats::addCarryCopy(quint64 bytes)
{
    m_carryCopies.fetch_add(1, std::memory_order_relaxed);
    m_carryBytes.fetch_add(bytes, std::memory_order_relaxed);
}

TransportSnapshot TransportStats::snapshot() const
{
    TransportSnapshot s;
    s.audioAllocs = m_audioAllocs.load(std::memory_order_relaxed);
    s.audioBytes = m_audioBytes.load(std::memory_order_relaxed);
    s.videoAllocs = m_videoAllocs.load(std::memory_order_relaxed);
    s.videoBytes = m_videoBytes.load(std::memory_order_relaxed);
    s.vadConcatAllocs = m_vadConcatAllocs.load(std::memory_order_relaxed);
    s.vadConcatBytes = m_vadConcatBytes.load(std::memory_order_relaxed);
    s.carryCopies = m_carryCopies.load(std::memory_order_relaxed);
    s.carryBytes = m_carryBytes.load(std::memory_order_relaxed);
    return s;
}

void TransportStats::reset()
{
    m_audioAllocs.store(0, std::memory_order_relaxed);
    m_audioBytes.store(0, std::memory_order_relaxed);
    m_videoAllocs.store(0, std::memory_order_relaxed);
    m_videoBytes.store(0, std::memory_order_relaxed);
    m_vadConcatAllocs.store(0, std::memory_order_relaxed);
    m_vadConcatBytes.store(0, std::memory_order_relaxed);
    m_carryCopies.store(0, std::memory_order_relaxed);
    m_carryBytes.store(0, std::memory_order_relaxed);
}

QString TransportStats::toLogString() const
{
    const TransportSnapshot s = snapshot();
    const quint64 audioKb = s.audioBytes / 1024u;
    const quint64 videoKb = s.videoBytes / 1024u;
    const quint64 vadKb = s.vadConcatBytes / 1024u;

    QString line = QStringLiteral("audio: %1 алок / %2 KB · video: %3 алок / %4 KB · "
                                  "vad-carry: %5 копия / %6 KB")
                       .arg(s.audioAllocs)
                       .arg(audioKb)
                       .arg(s.videoAllocs)
                       .arg(videoKb)
                       .arg(s.vadConcatAllocs)
                       .arg(vadKb);

    // Always zero until a VAD node starts reporting (the API is used by the
    // private plugin) — the extra segment would only be noise before that.
    if (s.carryCopies != 0) {
        line += QStringLiteral(" · пренос: %1 копия / %2 KB")
                    .arg(s.carryCopies)
                    .arg(s.carryBytes / 1024u);
    }
    return line;
}

} // namespace Daqster
