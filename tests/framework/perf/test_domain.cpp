#include "test_domain.h"

#include "PerfProfiler.h"
#include "perf_log_capture.h"

using Daqster::Perf::Domain;

void DomainTest::get_returnsSameInstancePerName()
{
    Domain &a = Domain::get("video");
    Domain &b = Domain::get("video");
    QCOMPARE(&a, &b);
    QCOMPARE(QByteArray(a.name()), QByteArrayLiteral("video"));

    // A different name maps to a different instance.
    Domain &c = Domain::get("audio");
    QVERIFY(&a != &c);
    QCOMPARE(QByteArray(c.name()), QByteArrayLiteral("audio"));
}

void DomainTest::enabled_defaultsFalse()
{
    Domain &d = Domain::get("test_enabled_default");
    QVERIFY(!d.enabled());
}

void DomainTest::record_accumulatesAndFlushLogs()
{
    Domain &d = Domain::get("test_record_on");
    d.setEnabled(false);
    d.flush(); // start from a clean state

    d.setEnabled(true);
    QVERIFY(d.enabled());
    d.record("decode", 1000);
    d.record("decode", 3000);

    // avg = (1000 + 3000) / 2 = 2000 ns -> 2 us; min 1 us; max 3 us.
    // QDebug streams QByteArray quoted ("decode") and spaces items (avg= 2).
    PerfLogCapture capture;
    d.flush();
    QVERIFY(capture.contains({"stage=", "decode", "avg= 2", "min= 1", "max= 3", "count= 2"}));
}

void DomainTest::record_noopWhenDisabled()
{
    Domain &d = Domain::get("test_record_off");
    d.setEnabled(false);
    d.flush();

    d.record("decode", 1000);

    PerfLogCapture capture;
    d.flush();
    // record() was a no-op while disabled, so flush() must emit nothing.
    QCOMPARE(capture.lines().size(), 0);
}

void DomainTest::flush_resetsStats()
{
    Domain &d = Domain::get("test_flush_reset");
    d.setEnabled(true);
    d.record("decode", 1000);

    {
        PerfLogCapture first;
        d.flush();
        QVERIFY(first.contains({"stage=", "decode", "count= 1"}));
    }

    // A second flush with no new records must emit nothing (stats were reset).
    {
        PerfLogCapture second;
        d.flush();
        QCOMPARE(second.lines().size(), 0);
    }

    d.setEnabled(false);
}
