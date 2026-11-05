#pragma once

#include <QtTest>

// REQ-SW-PL-082: the IDE half of "loads, marks, does not start". The panel is
// the thing the user actually acts on — an empty list must not occupy the
// canvas, a loaded scene's problems must all be listed with their node ids, and
// activating a row must tell the host which node to select.
//
// Declared in a header so it can share the single node_editor_ide_tests binary
// (see test_main.cpp). Runs under QApplication with the offscreen platform.
class TestSceneProblemsPanel : public QObject
{
    Q_OBJECT

private slots:
    void emptyProblemsHidePanel();
    void problemsFillListAndStoreIds();
    void activationEmitsNodeId();
    void problemTextIncludesHint();
    void enterKeyActivatesSelectedRow();
    void panelStartsCollapsedAndExpands();
};
