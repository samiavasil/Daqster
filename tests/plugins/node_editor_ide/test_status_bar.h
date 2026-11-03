#pragma once

#include <QtTest>

// REQ-SW-PL-079: the IDE half of the status-bar capability — StatusBarController
// renders IStatusBarProvider plugins (one row per provider, their actions in a
// menu) and must survive providers that are missing, empty or broken.
//
// Declared in a header so it can share the single node_editor_ide_tests binary
// (see test_main.cpp). Runs under QApplication with the offscreen platform.
class TestStatusBar : public QObject
{
    Q_OBJECT

private slots:
    void statusBarShowsProviderText();
    void statusActionIsTriggered();
    void emptyTextHidesLabel();
    void missingProviderDoesNotCrashIde();
    void providersAreOrderedByStatusOrder();
    void duplicateStatusIdKeepsFirstRow();
    void menuIsHiddenWithoutActions();
};
