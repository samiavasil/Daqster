#pragma once

#include <QtTest>

// Test class for the pure VideoPerfBadge formatter (REQ-SW-PL-027, option B).
// Declared in a header so it can share the single demo_nodeditor_nodes_tests
// binary — see test_main.cpp. The formatter is QtCore-only, so the tests are
// deterministic (no wall-clock, no media backend).
class TestVideoPerfBadge : public QObject
{
    Q_OBJECT

private slots:
    void softwareHandleMapsToSw();
    void hardwareHandleMapsToHw();
    void fpsZeroEdgeCase();
    void nsToMsConversion();
    void enumToText();
    void unknownEnumsFallBackToNumeric();
    void noSamplesNegativeRendersZero();
    void fullFormatLayout();
};
