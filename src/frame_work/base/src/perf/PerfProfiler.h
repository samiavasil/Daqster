#pragma once
#include <cstdint>
#include <chrono>
#include <vector>
#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <atomic>

// Compile-time opt-out: CMake defines DAQSTER_ENABLE_PERF=1/0 via PUBLIC
// compile definition. Fallback to 0 if undefined.
#ifndef DAQSTER_ENABLE_PERF
#  define DAQSTER_ENABLE_PERF 0
#endif

namespace Daqster::Perf {

// Fixed-capacity rolling statistics. add() is O(1) with no heap allocation in
// the hot path (vector reserved once at construction). avg/min/max are computed
// on demand (called from flush(), not the hot path).
class RollingStats {
public:
    explicit RollingStats(std::size_t capacity = 256);
    void add(std::int64_t value);
    std::int64_t count() const;   // samples added, capped at capacity
    std::int64_t avg() const;     // -1 if empty
    std::int64_t min() const;     // -1 if empty
    std::int64_t max() const;     // -1 if empty
    void reset();
private:
    std::vector<std::int64_t> m_ring;   // pre-allocated, fixed capacity
    std::size_t m_size = 0;             // valid samples (0..capacity)
    std::size_t m_head = 0;             // next write index
    std::int64_t m_sum = 0;             // running sum for O(1) avg
};

// A named, runtime-toggleable profiling domain. enabled()/setEnabled() use a
// relaxed atomic (safe to toggle from another thread). record() is a no-op when
// disabled; it is NOT thread-safe by itself — a domain is intended to be
// recorded from a single (usually the GUI) thread.
class Domain {
public:
    static Domain &get(const char *name);      // thread-safe get-or-create
    const char *name() const;
    bool enabled() const;                       // relaxed atomic load
    void setEnabled(bool on);                   // relaxed atomic store
    void record(const char *stage, std::int64_t ns);  // no-op when off
    void flush();                               // qCDebug(lcPerf) + reset
private:
    Domain(const char *name);
    QByteArray m_name;
    std::atomic<bool> m_enabled{false};
    QHash<QByteArray, RollingStats> m_stages;
};

// RAII timer for synchronous blocks. Zero cost when the domain is disabled
// (no clock read, no record).
class Scope {
public:
    Scope(Domain &d, const char *stage);
    ~Scope();
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
private:
    using clock = std::chrono::steady_clock;
    Domain &m_d;
    const char *m_stage;
    clock::time_point m_start{};
    bool m_active = false;
};

// Reusable stopwatch for async/event measurement. mark() returns nanoseconds
// elapsed since the previous mark()/reset()/construction.
class Stopwatch {
public:
    Stopwatch();
    std::int64_t mark();
    void reset();
private:
    using clock = std::chrono::steady_clock;
    clock::time_point m_last;
};

} // namespace Daqster::Perf

// Macros
#define PERF_CAT_IMPL(a, b) a##b
#define PERF_CAT(a, b) PERF_CAT_IMPL(a, b)

#if DAQSTER_ENABLE_PERF
#  define PERF_SCOPE(dom, stage) \
    ::Daqster::Perf::Scope PERF_CAT(perf_scope_, __LINE__)(::Daqster::Perf::Domain::get(dom), stage)
#  define PERF_ENABLED(dom) (::Daqster::Perf::Domain::get(dom).enabled())
#else
#  define PERF_SCOPE(dom, stage) ((void)0)
#  define PERF_ENABLED(dom) (false)
#endif
