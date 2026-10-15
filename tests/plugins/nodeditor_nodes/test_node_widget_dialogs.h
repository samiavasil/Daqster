#pragma once

#include <QtTest>

// Regression for node widget dialogs while a node widget is embedded in the
// graph. QGraphicsProxyWidget::setWidget() makes the embedded widget its own
// *offscreen* top-level window (Qt::WA_DontShowOnScreen). A modal dialog
// parented to it grabs input but is never really shown or drivable, so the
// whole application looks frozen — the "Add video" button in VideoFileSource
// used to do exactly that. nodeWidgetDialogParent() must resolve a real parent
// instead. Runs under QApplication with the offscreen platform plugin.
class TestNodeWidgetDialogs : public QObject
{
    Q_OBJECT

private slots:
    void embeddedWidgetResolvesToActiveWindow();
    void childWidgetUsesItsTopLevelWindow();
    void detachedWidgetIsItsOwnDialogParent();
};
