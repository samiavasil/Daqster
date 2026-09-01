#pragma once

#include <QtTest>

// Test class for DependencyGraphLayout (Sugiyama phases 2 & 3: barycenter
// crossing minimization + coordinate assignment). Declared in a header so it
// joins the shared requirements_manager_tests binary — see test_main.cpp.
// QtCore-only: no QApplication, no GUI.
class TestGraphLayout : public QObject
{
    Q_OBJECT

private slots:
    void layersPreserved();
    void crossingsReduced();
    void deterministic();
    void alignedAndCentered();
    void cycleResidualLayerStable();
    void parentEdgesDoNotBreakLayering();
};
