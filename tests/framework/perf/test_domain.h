#pragma once

#include <QtTest>

/// Unit tests for Daqster::Perf::Domain (REQ-SW-FW-008, AC 2).
///
/// Covers the thread-safe get-or-create registry, the enabled()/setEnabled()
/// toggle, record() accumulation when enabled, the no-op behaviour when
/// disabled, and flush() aggregate + reset semantics (observed through the
/// daqster.perf debug output captured by PerfLogCapture).
class DomainTest : public QObject
{
    Q_OBJECT

private slots:
    void get_returnsSameInstancePerName();
    void enabled_defaultsFalse();
    void record_accumulatesAndFlushLogs();
    void record_noopWhenDisabled();
    void flush_resetsStats();
};
