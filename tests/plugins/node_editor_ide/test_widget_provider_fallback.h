#pragma once

#include <QtTest>

// REQ-SW-PL-051 regression: with more than one IWidgetProvider plugin loaded,
// ChatGraphModel must consult ALL of them, in order, and take the first widget
// any provider returns. Picking only the first provider silently dropped every
// widget the other provider owned — e.g. the AiStudio GUI plugin (a second
// provider) becoming the "first one" and returning nullptr for VideoOutput,
// which left the runtime canvas empty.
//
// Declared in a header so it can share the single node_editor_ide_tests binary
// (see test_main.cpp). Runs under QApplication with the offscreen platform.
class TestWidgetProviderFallback : public QObject
{
    Q_OBJECT

private slots:
    void secondProviderServesWhenFirstReturnsNull();
    void firstServingProviderWins();
    void cacheReturnsSameWidget();
    void allProvidersMissFallsBackToEmbeddedWidget();
    void emptyProviderListFallsBackToEmbeddedWidget();
};
