#include "test_rolling_stats.h"

#include "PerfProfiler.h"

#include <cstdint>

using Daqster::Perf::RollingStats;

void RollingStatsTest::emptyStats_returnMinusOne()
{
    RollingStats s(8);
    QCOMPARE(s.count(), std::int64_t{0});
    QCOMPARE(s.avg(), std::int64_t{-1});
    QCOMPARE(s.min(), std::int64_t{-1});
    QCOMPARE(s.max(), std::int64_t{-1});
}

void RollingStatsTest::avgMinMax_correct()
{
    RollingStats s(8);
    s.add(10);
    s.add(20);
    s.add(30);

    QCOMPARE(s.count(), std::int64_t{3});
    QCOMPARE(s.avg(), std::int64_t{20});
    QCOMPARE(s.min(), std::int64_t{10});
    QCOMPARE(s.max(), std::int64_t{30});
}

void RollingStatsTest::ringOverflow_keepsLastN()
{
    RollingStats s(3);
    s.add(1);
    s.add(2);
    s.add(3);

    QCOMPARE(s.count(), std::int64_t{3});
    QCOMPARE(s.avg(), std::int64_t{2});
    QCOMPARE(s.min(), std::int64_t{1});
    QCOMPARE(s.max(), std::int64_t{3});

    // Overflow: only the last 3 samples (2, 3, 4) survive.
    s.add(4);
    QCOMPARE(s.count(), std::int64_t{3});
    QCOMPARE(s.avg(), std::int64_t{3});
    QCOMPARE(s.min(), std::int64_t{2});
    QCOMPARE(s.max(), std::int64_t{4});

    // And again: last 3 are now (3, 4, 5).
    s.add(5);
    QCOMPARE(s.count(), std::int64_t{3});
    QCOMPARE(s.avg(), std::int64_t{4});
    QCOMPARE(s.min(), std::int64_t{3});
    QCOMPARE(s.max(), std::int64_t{5});
}

void RollingStatsTest::reset_clearsAndReusable()
{
    RollingStats s(4);
    s.add(100);
    s.add(200);
    QCOMPARE(s.count(), std::int64_t{2});

    s.reset();
    QCOMPARE(s.count(), std::int64_t{0});
    QCOMPARE(s.avg(), std::int64_t{-1});
    QCOMPARE(s.min(), std::int64_t{-1});
    QCOMPARE(s.max(), std::int64_t{-1});

    // The buffer stays pre-allocated and reusable after reset.
    s.add(7);
    QCOMPARE(s.count(), std::int64_t{1});
    QCOMPARE(s.avg(), std::int64_t{7});
}
