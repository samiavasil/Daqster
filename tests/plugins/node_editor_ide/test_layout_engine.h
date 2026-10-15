#pragma once

#include <QtTest>

// Test class for the FlowUi::LayoutEngine widget builder + capturer
// (REQ-SW-PL-053 Phase 2a). Builds real QSplitter/QGridLayout/QTabWidget/
// QDockWidget hierarchies, so the binary runs under QApplication with the
// offscreen platform plugin. Declared in a header so it can share the single
// node_editor_ide_tests binary — see test_main.cpp.
class TestLayoutEngine : public QObject
{
    Q_OBJECT

private slots:
    void gridBuildsCentralWidget();
    void buildIntoReplacesExistingCentralWidget();
    void placeholdersFillEmptyCells();
    void splitRestoresSizes();
    void tabRestoresCurrent();
    void dockBuildsInRightArea();
    void dockFloatsWhenFloating();
    void deembeddedWidgetBecomesCleanChild();
    void captureRoundTrips();
    void missingNodeSkipsCell();
};