#pragma once

#include <QtTest>

// Test class for the FlowUi::LayoutTree schema-v2 data model + JSON
// (REQ-SW-PL-053 Phase 2a). The model is QtCore-only, so these tests are
// headless. Declared in a header so it can share the single
// node_editor_ide_tests binary — see test_main.cpp.
class TestLayoutTree : public QObject
{
    Q_OBJECT

private slots:
    void gridRoundTrip();
    void splitRoundTrip();
    void tabRoundTrip();
    void dockRoundTrip();
    void comboRoundTrip();
    void nodesMapRoundTrip();
    void unknownTypeFallsBackToPanel();
    void missingFieldsUseDefaults();
    void versionThreeParsesEmpty();
    void versionOneParsesEmpty();
};