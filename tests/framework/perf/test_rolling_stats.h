#pragma once

#include <QtTest>

/// Unit tests for Daqster::Perf::RollingStats (REQ-SW-FW-008, AC 4).
///
/// Covers average/min/max correctness, the fixed-capacity ring overflow
/// behaviour (only the last N samples are kept) and reset() semantics.
class RollingStatsTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyStats_returnMinusOne();
    void avgMinMax_correct();
    void ringOverflow_keepsLastN();
    void reset_clearsAndReusable();
};
