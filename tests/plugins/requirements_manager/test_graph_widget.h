#pragma once

#include <QtTest>

// GUI tests for the interactive dependency graph (DependencyGraphWidget /
// DependencyGraphScene / DependencyGraphView). QTEST_MAIN at the bottom of
// test_graph_widget.cpp provides a QApplication main; the binary runs headless
// via the offscreen platform plugin (QT_QPA_PLATFORM=offscreen, set by the
// CTest ENVIRONMENT property in CMakeLists.txt).
class TestGraphWidget : public QObject
{
    Q_OBJECT

private slots:
    // Bug A: dragging a node must move its incident edges with it.
    void edgeFollowsNodeMove();
    // Bug B: the view must refit the scene to the viewport on resize/show
    // after setRequirements() populated the graph at a small viewport size.
    void fitsViewportAfterResize();

    // REQ-SW-PL-001 (user item 6): navigation, selection, status colours.
    // Search finds a node by ID and by caption (case-insensitive); the counter
    // reports "1 / N".
    void searchFindsByIdAndCaption();
    // Enter / „Следващ" walk the matches and the hit is centred in the view.
    void searchNextCyclesAndCentres();
    // A single click selects (halo + incident edges + dimming) and must NOT
    // navigate; a double click navigates.
    void singleClickSelectsWithoutNavigating();
    void doubleClickNavigates();
    // Selecting highlights the incident edges (thicker pen, source colour) and
    // dims the rest; Escape clears.
    void selectionHighlightsIncidentEdges();
    // The status -> border colour table covers the five vocabulary statuses.
    void statusColoursAreTableDriven_data();
    void statusColoursAreTableDriven();
    // The tooltip carries id, title, status, counts and the validation issues.
    void tooltipHasStatusAndIssues();
    // A 60-node tree parsed by the real parser: >50 nodes, one exact hit.
    void largeTreeSearchesOneNode();
};
