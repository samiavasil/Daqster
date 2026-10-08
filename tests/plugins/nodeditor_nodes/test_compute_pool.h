#pragma once

#include <QtTest>

/// Unit tests for the shared ComputePool (REQ-SW-PL-039): per-key latest-wins
/// submission (frame skipping), per-key serialization, cancel() semantics and
/// the per-key metrics. QTEST_GUILESS_MAIN in test_compute_pool.cpp provides a
/// QCoreApplication main — no widgets needed.
///
/// The pool runs on real worker threads, so the tests synchronize with
/// QTRY_VERIFY (event processing + timeout) and a QSemaphore gate that blocks
/// a task until the test releases it. DAQSTER_COMPUTE_THREADS=1 is forced in
/// initTestCase() so the latest-wins/cancel sequences are deterministic.
class ComputePoolTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    /// A submitted task runs on a pool thread and the metrics count it.
    void submit_runsTask_andCountsMetrics();

    /// Latest-wins: a task submitted while another runs becomes pending and is
    /// replaced by a newer submission (skipped++); the replaced task never runs.
    void latestWins_replacesPending();

    /// cancel() drops queued/pending tasks and prevents chaining after the
    /// running task finishes.
    void cancel_dropsPendingAndPreventsChaining();

    /// A new submission after cancel() reactivates the key (stale `cancelled`
    /// flag is cleared) — a reused key must not drop submissions forever.
    void submit_afterCancel_reactivatesKey();

    /// fps() counts completions in the rolling 1 s window.
    void fps_countsRecentCompletions();
};