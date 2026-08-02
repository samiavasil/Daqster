#pragma once

#include <QtTest>

// Test class for RequirementsParser. Declared in a header (instead of relying
// on QTEST_GUILESS_MAIN inside the .cpp) so the three Requirements Manager
// test classes can share a single test binary - see test_main.cpp.
class TestParser : public QObject
{
    Q_OBJECT

private slots:
    void parseDirectory_fullMetadata();
    void parseDirectory_minimalAndDashFields();
    void parseDirectory_traceabilityFields();
    void parseDirectory_emptyDir();
    void generateNextId_scansActiveAndArchive();
    void moveToArchive_movesFile();
    void moveToActive_movesFile();
    void moveRejectsWrongSection();
    void writeRequirement_roundTrip();
};
