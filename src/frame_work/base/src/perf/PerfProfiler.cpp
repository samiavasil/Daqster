#include "PerfProfiler.h"
#include "include/LogCategories.h"

#include <QMutexLocker>

namespace Daqster::Perf {

namespace {

// Convert nanoseconds to microseconds for human-readable logging.
std::int64_t ns2us(std::int64_t ns)
{
    return ns / 1000;
}

} // namespace

// ── RollingStats ────────────────────────────────────────────────────────────

RollingStats::RollingStats(std::size_t capacity)
    : m_ring(capacity)
    , m_size(0)
    , m_head(0)
    , m_sum(0)
{
}

void RollingStats::add(std::int64_t value)
{
    if (m_ring.empty())
        return; // zero capacity — nothing to track

    if (m_size < m_ring.size()) {
        // Not yet full: append at the write head and grow.
        m_ring[m_head] = value;
        m_head = (m_head + 1) % m_ring.size();
        ++m_size;
        m_sum += value;
    } else {
        // Full ring: m_head points at the oldest sample. Overwrite it and
        // advance the head so the running sum stays O(1).
        m_sum -= m_ring[m_head];
        m_ring[m_head] = value;
        m_head = (m_head + 1) % m_ring.size();
        m_sum += value;
    }
}

std::int64_t RollingStats::count() const
{
    return static_cast<std::int64_t>(m_size);
}

std::int64_t RollingStats::avg() const
{
    if (m_size == 0)
        return -1;
    return m_sum / static_cast<std::int64_t>(m_size);
}

std::int64_t RollingStats::min() const
{
    if (m_size == 0)
        return -1;
    std::int64_t result = m_ring[0];
    for (std::size_t i = 1; i < m_size; ++i) {
        if (m_ring[i] < result)
            result = m_ring[i];
    }
    return result;
}

std::int64_t RollingStats::max() const
{
    if (m_size == 0)
        return -1;
    std::int64_t result = m_ring[0];
    for (std::size_t i = 1; i < m_size; ++i) {
        if (m_ring[i] > result)
            result = m_ring[i];
    }
    return result;
}

void RollingStats::reset()
{
    m_size = 0;
    m_head = 0;
    m_sum = 0;
}

// ── Domain ──────────────────────────────────────────────────────────────────

Domain::Domain(const char *name)
    : m_name(name)
{
}

Domain &Domain::get(const char *name)
{
    static QHash<QByteArray, Domain *> s_registry;
    static QMutex s_mutex;

    const QByteArray key(name);
    QMutexLocker locker(&s_mutex);
    auto it = s_registry.constFind(key);
    if (it != s_registry.constEnd())
        return **it;

    auto *domain = new Domain(name);
    s_registry.insert(key, domain);
    return *domain;
}

const char *Domain::name() const
{
    return m_name.constData();
}

bool Domain::enabled() const
{
    return m_enabled.load(std::memory_order_relaxed);
}

void Domain::setEnabled(bool on)
{
    m_enabled.store(on, std::memory_order_relaxed);
}

void Domain::record(const char *stage, std::int64_t ns)
{
    if (!m_enabled.load(std::memory_order_relaxed))
        return;
    m_stages[QByteArray(stage)].add(ns);
}

std::int64_t Domain::avg(const char *stage) const
{
    const auto it = m_stages.constFind(QByteArray(stage));
    return it == m_stages.constEnd() ? -1 : it->avg();
}

std::int64_t Domain::min(const char *stage) const
{
    const auto it = m_stages.constFind(QByteArray(stage));
    return it == m_stages.constEnd() ? -1 : it->min();
}

std::int64_t Domain::max(const char *stage) const
{
    const auto it = m_stages.constFind(QByteArray(stage));
    return it == m_stages.constEnd() ? -1 : it->max();
}

std::int64_t Domain::count(const char *stage) const
{
    const auto it = m_stages.constFind(QByteArray(stage));
    return it == m_stages.constEnd() ? -1 : it->count();
}

void Domain::flush()
{
    for (auto it = m_stages.begin(); it != m_stages.end(); ++it) {
        RollingStats &stats = it.value();
        if (stats.count() == 0)
            continue; // nothing recorded for this stage

        qCDebug(lcPerf) << "domain=" << m_name
                        << "stage=" << it.key()
                        << "avg=" << ns2us(stats.avg())
                        << "min=" << ns2us(stats.min())
                        << "max=" << ns2us(stats.max())
                        << "count=" << stats.count();
        stats.reset();
    }
}

// ── Scope ───────────────────────────────────────────────────────────────────

Scope::Scope(Domain &d, const char *stage)
    : m_d(d)
    , m_stage(stage)
{
    m_active = m_d.enabled();
    if (m_active)
        m_start = clock::now();
}

Scope::~Scope()
{
    if (!m_active)
        return;
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             clock::now() - m_start)
                             .count();
    m_d.record(m_stage, elapsed);
}

// ── Stopwatch ───────────────────────────────────────────────────────────────

Stopwatch::Stopwatch()
    : m_last(clock::now())
{
}

std::int64_t Stopwatch::mark()
{
    const auto now = clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             now - m_last)
                             .count();
    m_last = now;
    return elapsed;
}

void Stopwatch::reset()
{
    m_last = clock::now();
}

} // namespace Daqster::Perf
