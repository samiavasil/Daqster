#pragma once

#include <QtTest>

// Test class for DependencyGraphData. Declared in a header (instead of relying
// on QTEST_GUILESS_MAIN inside the .cpp) so all Requirements Manager test
// classes share a single test binary - see test_main.cpp.
class TestGraph : public QObject
{
    Q_OBJECT

private slots:
    void nodeAndEdgeCounts();
    void danglingRecordedNotRendered();
    void cycleInputTerminates();
    void layeredLayoutInvariant();
};
