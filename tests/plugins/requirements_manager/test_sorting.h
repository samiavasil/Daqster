#pragma once

#include <QtTest>

// Sorting tests (REQ-SW-PL-001): the requirements tree sorts inside a section
// without moving the section rows, and the traceability table sorts by every
// column. Both proxies are QtCore-only, so this class runs in the headless
// requirements_manager_tests binary.
class TestSorting : public QObject
{
    Q_OBJECT

private slots:
    void treeSortsWithinSection_data();
    void treeSortsWithinSection();
    void treeSectionsKeepSourceOrder();
    void matrixSortsEveryColumn_data();
    void matrixSortsEveryColumn();
    void matrixDefaultOrderKeepsSourceOrder();
};
