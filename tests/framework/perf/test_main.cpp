#include <QtTest>
#include <QCoreApplication>

#include "test_rolling_stats.h"
#include "test_domain.h"
#include "test_stopwatch_scope.h"

// Shared main for the perf_profiler_tests binary. Mirrors the
// QTEST_GUILESS_MAIN model (QCoreApplication, headless) used by the
// demo_nodeditor_nodes test suite: each test class is declared in its own
// header and run through QTest::qExec from a single main().
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    int status = 0;
    {
        RollingStatsTest rollingStats;
        status |= QTest::qExec(&rollingStats, argc, argv);
    }
    {
        DomainTest domain;
        status |= QTest::qExec(&domain, argc, argv);
    }
    {
        StopwatchScopeTest stopwatchScope;
        status |= QTest::qExec(&stopwatchScope, argc, argv);
    }
    return status;
}
