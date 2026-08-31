#pragma once

#include <QtTest>

/// Unit tests for Daqster::Perf::Stopwatch and Daqster::Perf::Scope
/// (REQ-SW-FW-008, AC 3).
///
/// Stopwatch::mark()/reset() measure elapsed steady-clock time; Scope records
/// the elapsed ns into its domain on destruction (and is a no-op when the
/// domain is disabled).
class StopwatchScopeTest : public QObject
{
    Q_OBJECT

private slots:
    void stopwatch_mark_returnsPositiveElapsed();
    void stopwatch_reset_restartsMeasurement();
    void scope_recordsElapsedOnDestruction();
    void scope_noopWhenDomainDisabled();
};
