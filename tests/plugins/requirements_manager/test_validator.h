#pragma once

#include <QtTest>

// Test class for RequirementsValidator. Declared in a header (instead of
// relying on QTEST_GUILESS_MAIN inside the .cpp) so the three Requirements
// Manager test classes can share a single test binary - see test_main.cpp.
class TestValidator : public QObject
{
    Q_OBJECT

private slots:
    void cleanFixture_noIssues();
    void danglingParent_error();
    void danglingDependency_error();
    void cycle_detection();
    void missingFields_warnings();
    void selfReference_error();
    void archivedDependency_warning();
};
