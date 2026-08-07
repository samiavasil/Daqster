#pragma once

#include <QtTest>

// Test class for the multi-repo merge logic of RequirementsParser
// (REQ-SW-PL-012): repoForId() / parseDirectories(). Declared in a header
// (instead of relying on QTEST_GUILESS_MAIN inside the .cpp) so the
// Requirements Manager test classes share a single test binary - see
// test_main.cpp. discoverRepoRoots() is environment-dependent (walks up from
// the application binary directory) and is deliberately NOT unit-tested.
class TestMerge : public QObject
{
    Q_OBJECT

private slots:
    void repoForId_publicPrivateOther();
    void parseDirectories_twoRoots_mergeAndRepoStamp();
    void parseDirectories_sameFileViaTwoRoots_dedup();
    void parseDirectories_stableSort();
    void parseDirectories_emptyRoots();
    void parseDirectories_dependencyHintsPreserved();
};
