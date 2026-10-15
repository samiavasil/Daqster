#pragma once

#include <QtTest>

// Test class for the visual Layout Designer panel (REQ-SW-PL-059). The panel is
// pure QtWidgets + the schema-v2 LayoutTree/LayoutEngine models, so it runs in
// the shared node_editor_ide_tests binary under QApplication (offscreen).
// Declared in a header so it can share the single binary — see test_main.cpp.
class TestLayoutDesigner : public QObject
{
    Q_OBJECT

private slots:
    void emptySectionGetsOneWorkspace();
    void setSectionRoundTrips();
    void addGridCreatesEmpty2x2();
    void addGridRespectsRequestedSize();
    void assignSlotFillsGridCell();
    void addDockAddsDockSlot();
    void addSplitWrapsExistingRoot();
    void nestedContainerTargetsSelectedSlot();
    void deleteRemovesComponentAndDetachesNode();
    void deleteCollapsesEmptySplitRoot();
    void undoRestoresPreviousSection();
    void addAndRemoveWorkspace();
    void nodeMimeRoundTripsNodeId();
};
