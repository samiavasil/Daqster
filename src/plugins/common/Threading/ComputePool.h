#pragma once

// SPDX-License-Identifier: MIT
//
// Shared, process-wide compute pool (REQ-SW-PL-039).
//
// A single QThreadPool shared by ALL nodes (not per-node). Per-key
// "latest-wins" submission (frame skipping) + per-key serialization so the
// DaqDisplayNode ring-buffer contract (worker-only ring access) is preserved
// even though the pool itself is shared.
//
// Threading: submitLatest()/cancel()/metrics are GUI-thread only; tasks run on
// pool threads. The singleton is intentionally leaked (function-local static
// pointer, never deleted) — mirrors VideoGLContextManager.

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QThreadPool>
#include <QVector>
#include <functional>
#include <memory>

namespace Daqster {

// Shared, process-wide compute pool (singleton, intentionally leaked).
// Per-key "latest-wins" submission (frame skipping) + per-key serialization.
// GUI-thread only for submit/cancel; tasks run on pool threads.
class ComputePool
{
public:
    static ComputePool &instance();

    // Submit a task for `key`. If a task for this key is queued, it is
    // REPLACED (skipped++). If one is running, the new task becomes pending
    // (skipped++). Returns true if accepted.
    bool submitLatest(const QByteArray &key, std::function<void()> task);

    // Cancel all queued tasks for `key` and wait for the running one (≤ timeout).
    void cancel(const QByteArray &key, int timeoutMs = 500);

    // Metrics.
    quint64 submitted(const QByteArray &key) const;
    quint64 started(const QByteArray &key) const;
    quint64 completed(const QByteArray &key) const;
    quint64 skipped(const QByteArray &key) const;
    double fps(const QByteArray &key) const; // completed/sec over rolling 1s window

private:
    ComputePool();
    ~ComputePool();
    ComputePool(const ComputePool &) = delete;
    ComputePool &operator=(const ComputePool &) = delete;

    // Runs the queued task for `key` on a pool thread (see ComputePool.cpp).
    void runKeyed(const QByteArray &key);

    class KeyedTask;

    struct KeyState {
        std::function<void()> queued;   // not started, replaceable
        std::function<void()> pending;  // arrived while running, replaceable
        bool running = false;
        bool cancelled = false;
        quint64 submitted = 0, started = 0, completed = 0, skipped = 0;
        QVector<qint64> completionTimes; // ms timestamps for fps
    };

    QThreadPool m_pool;
    mutable QMutex m_mutex;
    QHash<QByteArray, KeyState> m_states;
};

} // namespace Daqster