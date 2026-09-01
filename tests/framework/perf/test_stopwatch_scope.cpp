#include "test_stopwatch_scope.h"

#include "PerfProfiler.h"
#include "perf_log_capture.h"

#include <QThread>

using Daqster::Perf::Domain;
using Daqster::Perf::Scope;
using Daqster::Perf::Stopwatch;

void StopwatchScopeTest::stopwatch_mark_returnsPositiveElapsed()
{
    Stopwatch sw;
    QThread::msleep(10);
    const std::int64_t elapsed = sw.mark();
    QVERIFY(elapsed > 0);
}

void StopwatchScopeTest::stopwatch_reset_restartsMeasurement()
{
    Stopwatch sw;
    QThread::msleep(10);
    sw.reset();
    QThread::msleep(10);
    const std::int64_t elapsed = sw.mark();
    QVERIFY(elapsed > 0);
}

void StopwatchScopeTest::scope_recordsElapsedOnDestruction()
{
    Domain &d = Domain::get("test_scope_on");
    d.setEnabled(true);
    d.flush();

    {
        Scope s(d, "block");
        QThread::msleep(10);
    }

    PerfLogCapture capture;
    d.flush();
    QVERIFY(capture.contains({"stage=", "block", "count= 1"}));

    d.setEnabled(false);
}

void StopwatchScopeTest::scope_noopWhenDomainDisabled()
{
    Domain &d = Domain::get("test_scope_off");
    d.setEnabled(false);
    d.flush();

    {
        Scope s(d, "block");
        QThread::msleep(10);
    }

    PerfLogCapture capture;
    d.flush();
    // The Scope was constructed with a disabled domain: no clock read, no
    // record on destruction, so flush() must emit nothing.
    QCOMPARE(capture.lines().size(), 0);
}
