// SPDX-License-Identifier: MIT
//
// Shared, process-wide compute pool (REQ-SW-PL-039).
//
// Per-key "latest-wins" submission: a queued task is replaced by the newest
// submission (frame skipping), a submission arriving while a task is running
// becomes `pending` (also replaceable), and only after the running task
// finishes is the pending task chained — so per-key serialization holds even
// though the underlying QThreadPool is shared by all nodes.
//
// cancel(key) marks the key cancelled (dropping queued + pending) and waits
// for the running task (≤ timeout) so a node can be destroyed safely.

#include "ComputePool.h"

#include <QDateTime>
#include <QThread>
#include <QtGlobal>

namespace Daqster {

namespace {
// Rolling window for the fps metric (ms).
constexpr qint64 kFpsWindowMs = 1000;
} // namespace

// QRunnable that drives runKeyed() on a pool thread. Auto-deleted by the pool.
class ComputePool::KeyedTask : public QRunnable
{
public:
    KeyedTask(ComputePool *pool, QByteArray key)
        : m_pool(pool)
        , m_key(std::move(key))
    {
        setAutoDelete(true);
    }

    void run() override { m_pool->runKeyed(m_key); }

private:
    ComputePool *m_pool;
    QByteArray m_key;
};

ComputePool &ComputePool::instance()
{
    // Function-local static pointer: intentionally leaked (never destroyed) so
    // the pool outlives all nodes and no destruction-order hazard with
    // QCoreApplication teardown can occur (mirrors VideoGLContextManager).
    static ComputePool *s_instance = new ComputePool();
    return *s_instance;
}

ComputePool::ComputePool()
{
    int threads = QThread::idealThreadCount();
    const QByteArray env = qgetenv("DAQSTER_COMPUTE_THREADS");
    if (!env.isEmpty()) {
        bool ok = false;
        const int v = env.toInt(&ok);
        if (ok && v > 0)
            threads = v;
    }
    m_pool.setMaxThreadCount(qMax(1, threads));
}

ComputePool::~ComputePool()
{
    m_pool.clear();
    m_pool.waitForDone();
}

bool ComputePool::submitLatest(const QByteArray &key, std::function<void()> task)
{
    if (!task)
        return false;

    QMutexLocker locker(&m_mutex);
    KeyState &st = m_states[key];
    st.submitted++;

    // A new submission means the key is alive again — clear a stale
    // `cancelled` flag left by a previous cancel() (e.g. a new node allocated
    // at the same address as a destroyed one). Without this, the reused key
    // would silently drop every submission forever.
    st.cancelled = false;

    if (st.queued) {
        // A task is queued but not started — replace it (latest wins).
        st.queued = std::move(task);
        st.skipped++;
        return true;
    }
    if (st.running) {
        // A task is running — park the new one as replaceable pending.
        st.pending = std::move(task);
        st.skipped++;
        return true;
    }

    // Nothing queued/running — start a KeyedTask for this key.
    st.queued = std::move(task);
    m_pool.start(new KeyedTask(this, key));
    return true;
}

void ComputePool::runKeyed(const QByteArray &key)
{
    std::function<void()> fn;
    {
        QMutexLocker locker(&m_mutex);
        KeyState &st = m_states[key];

        // After cancel() nothing may run for this key (safe node destruction).
        if (st.cancelled) {
            st.queued = {};
            return;
        }

        fn = std::move(st.queued);
        st.queued = {};
        if (!fn)
            return;

        st.running = true;
        st.started++;
    }

    fn();

    {
        QMutexLocker locker(&m_mutex);
        KeyState &st = m_states[key];
        st.running = false;
        st.completed++;

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        st.completionTimes.append(now);
        while (!st.completionTimes.isEmpty()
               && st.completionTimes.first() < now - kFpsWindowMs) {
            st.completionTimes.removeFirst();
        }

        // Chain the pending task (if any) — per-key serialization. A pending
        // task parked after cancel() is dropped instead of chained.
        if (st.pending && !st.cancelled) {
            st.queued = std::move(st.pending);
            st.pending = {};
            m_pool.start(new KeyedTask(this, key));
        }
    }
}

void ComputePool::cancel(const QByteArray &key, int timeoutMs)
{
    QMutexLocker locker(&m_mutex);
    KeyState &st = m_states[key];
    st.cancelled = true;
    st.queued = {};
    st.pending = {};

    if (!st.running)
        return;

    // Wait for the running task (≤ timeout). Poll instead of a condition
    // variable so the header stays dependency-free.
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + qMax(0, timeoutMs);
    while (st.running) {
        locker.unlock();
        QThread::msleep(1);
        locker.relock();
        if (QDateTime::currentMSecsSinceEpoch() >= deadline)
            break;
    }
}

quint64 ComputePool::submitted(const QByteArray &key) const
{
    QMutexLocker locker(&m_mutex);
    const auto it = m_states.constFind(key);
    return it == m_states.constEnd() ? 0 : it.value().submitted;
}

quint64 ComputePool::started(const QByteArray &key) const
{
    QMutexLocker locker(&m_mutex);
    const auto it = m_states.constFind(key);
    return it == m_states.constEnd() ? 0 : it.value().started;
}

quint64 ComputePool::completed(const QByteArray &key) const
{
    QMutexLocker locker(&m_mutex);
    const auto it = m_states.constFind(key);
    return it == m_states.constEnd() ? 0 : it.value().completed;
}

quint64 ComputePool::skipped(const QByteArray &key) const
{
    QMutexLocker locker(&m_mutex);
    const auto it = m_states.constFind(key);
    return it == m_states.constEnd() ? 0 : it.value().skipped;
}

double ComputePool::fps(const QByteArray &key) const
{
    QMutexLocker locker(&m_mutex);
    const auto it = m_states.constFind(key);
    if (it == m_states.constEnd())
        return 0.0;
    const KeyState &st = it.value();
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - kFpsWindowMs;
    int count = 0;
    for (qint64 t : st.completionTimes) {
        if (t >= cutoff)
            count++;
    }
    return static_cast<double>(count);
}

} // namespace Daqster