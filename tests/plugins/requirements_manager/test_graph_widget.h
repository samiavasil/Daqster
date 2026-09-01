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
};
