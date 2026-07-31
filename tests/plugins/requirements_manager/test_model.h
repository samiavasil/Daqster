#pragma once

#include <QtTest>

// Test class for RequirementsModel. Declared in a header (instead of relying
// on QTEST_GUILESS_MAIN inside the .cpp) so the three Requirements Manager
// test classes can share a single test binary - see test_main.cpp.
class TestModel : public QObject
{
    Q_OBJECT

private slots:
    void contract_sectionsMode();
    void contract_hierarchyMode();
    void hierarchy_shape();
    void hierarchy_nesting();
    void indexForId_behaviour();
    void danglingParent_isTopLevel();
    void cycle_safe();
    void requirementRole_data();
};
