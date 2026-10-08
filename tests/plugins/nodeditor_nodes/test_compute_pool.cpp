#include "test_compute_pool.h"

#include "Threading/ComputePool.h"

#include <QSemaphore>
#include <QThread>

#include <atomic>

using Daqster::ComputePool;

void ComputePoolTest::initTestCase()
{
    // Force a single worker thread so the latest-wins / cancel sequences are
    // deterministic. Must run before the first ComputePool::instance() call.
    qputenv("DAQSTER_COMPUTE_THREADS", "1");
}

// ── submit runs the task + metrics ───────────────────────────────────────────
void ComputePoolTest::submit_runsTask_andCountsMetrics()
{
    ComputePool &pool = ComputePool::instance();
    const QByteArray key = "metrics-key";

    std::atomic<int> ran{0};
    QVERIFY(pool.submitLatest(key, [&ran]() { ran = 1; }));

    QTRY_VERIFY_WITH_TIMEOUT(pool.completed(key) >= 1, 5000);
    QCOMPARE(ran.load(), 1);
    QCOMPARE(pool.submitted(key), quint64(1));
    QCOMPARE(pool.started(key), quint64(1));
    QCOMPARE(pool.completed(key), quint64(1));
    QCOMPARE(pool.skipped(key), quint64(0));
}

// ── latest-wins: pending replaced by newer submission ────────────────────────
//
// With one worker thread: task A blocks on the gate; B and C are submitted
// while A runs. B becomes pending, C replaces B (skipped++). After A finishes,
// only C runs — B never executes.
void ComputePoolTest::latestWins_replacesPending()
{
    ComputePool &pool = ComputePool::instance();
    const QByteArray key = "latest-wins-key";

    QSemaphore gate(0);
    std::atomic<int> ranA{0}, ranB{0}, ranC{0};

    pool.submitLatest(key, [&]() {
        ranA = 1;
        gate.acquire(); // block until the test releases
    });
    QTRY_VERIFY_WITH_TIMEOUT(pool.started(key) >= 1, 5000);

    pool.submitLatest(key, [&ranB]() { ranB = 1; });
    pool.submitLatest(key, [&ranC]() { ranC = 1; });

    // Release A; the pending C (which replaced B) must run next.
    gate.release();
    QTRY_VERIFY_WITH_TIMEOUT(pool.completed(key) >= 2, 5000);

    QCOMPARE(ranA.load(), 1);
    QCOMPARE(ranB.load(), 0); // replaced by C — never ran
    QCOMPARE(ranC.load(), 1);
    QCOMPARE(pool.submitted(key), quint64(3));
    QCOMPARE(pool.started(key), quint64(2));
    QCOMPARE(pool.completed(key), quint64(2));
    QVERIFY(pool.skipped(key) >= 1);
}

// ── cancel drops pending + prevents chaining ─────────────────────────────────
void ComputePoolTest::cancel_dropsPendingAndPreventsChaining()
{
    ComputePool &pool = ComputePool::instance();
    const QByteArray key = "cancel-key";

    QSemaphore gate(0);
    std::atomic<int> ran{0};

    pool.submitLatest(key, [&]() {
        ran++;
        gate.acquire(); // block until the test releases
    });
    QTRY_VERIFY_WITH_TIMEOUT(pool.started(key) >= 1, 5000);

    // Park a pending task, then cancel: the pending task must be dropped and
    // must NOT be chained after the running task finishes.
    pool.submitLatest(key, [&ran]() { ran++; });
    pool.cancel(key, 2000); // waits for the running task (times out — it is blocked)

    gate.release(); // let the running task finish
    QTRY_VERIFY_WITH_TIMEOUT(pool.completed(key) >= 1, 5000);

    // Give the pool a moment to (wrongly) chain the pending task if it could.
    QThread::msleep(200);
    QCOMPARE(ran.load(), 1); // only the first task ran
    QCOMPARE(pool.completed(key), quint64(1));
}

// ── submit after cancel reactivates the key ──────────────────────────────────
//
// Regression for the key-reuse case: a new node allocated at the same address
// as a destroyed one reuses the pool key. The stale `cancelled` flag must be
// cleared by the new submission, otherwise every submission is dropped forever.
void ComputePoolTest::submit_afterCancel_reactivatesKey()
{
    ComputePool &pool = ComputePool::instance();
    const QByteArray key = "reactivate-key";

    QSemaphore gate(0);
    std::atomic<int> ran{0};

    pool.submitLatest(key, [&]() {
        ran++;
        gate.acquire();
    });
    QTRY_VERIFY_WITH_TIMEOUT(pool.started(key) >= 1, 5000);

    pool.cancel(key, 2000); // marks the key cancelled
    gate.release();
    QTRY_VERIFY_WITH_TIMEOUT(pool.completed(key) >= 1, 5000);

    // A new submission for the same key must run (cancelled flag cleared).
    pool.submitLatest(key, [&ran]() { ran++; });
    QTRY_VERIFY_WITH_TIMEOUT(pool.completed(key) >= 2, 5000);
    QCOMPARE(ran.load(), 2);
}

// ── fps counts completions in the rolling 1 s window ─────────────────────────
void ComputePoolTest::fps_countsRecentCompletions()
{
    ComputePool &pool = ComputePool::instance();
    const QByteArray key = "fps-key";

    pool.submitLatest(key, []() {});
    QTRY_VERIFY_WITH_TIMEOUT(pool.completed(key) >= 1, 5000);

    // The completion is inside the rolling 1 s window right after it happened.
    QVERIFY(pool.fps(key) >= 1.0);
}

QTEST_GUILESS_MAIN(ComputePoolTest)