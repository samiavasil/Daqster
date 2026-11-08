#pragma once

#include <QtTest>

// REQ-SW-PL-084: the IDE half of the plugin-menu capability —
// PluginMenuController installs IMenuProvider actions into a host menu bar
// (reusing existing menus, creating submenus, calling back by id) and must
// survive providers that are missing, empty or broken.
//
// Declared in a header so it can share the single node_editor_ide_tests binary
// (see test_main.cpp). Runs under QApplication with the offscreen platform.
class TestPluginMenu : public QObject
{
    Q_OBJECT

private slots:
    void actionsAppearUnderRequestedPath();
    void submenuIsCreatedOnlyOnce();
    void triggerSendsTheProviderId();
    void installingTwiceDoesNotDuplicate();
    void disabledAndInvalidEntries();
    void providerThatThrowsIsSkipped();
    void noProvidersMeansNoActions();
};
