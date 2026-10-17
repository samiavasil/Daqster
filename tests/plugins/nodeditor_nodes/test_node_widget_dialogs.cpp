#include "test_node_widget_dialogs.h"

#include "NodeWidgetDialogs.h" // code under test

#include <QApplication>
#include <QLabel>
#include <QWidget>

void TestNodeWidgetDialogs::embeddedWidgetResolvesToActiveWindow()
{
    QWidget main;
    main.resize(200, 150);
    main.show();

    // What QGraphicsProxyWidget::setWidget() leaves behind: an offscreen
    // toplevel owned by nobody the user can see.
    QWidget embedded;
    embedded.setAttribute(Qt::WA_DontShowOnScreen, true);
    embedded.resize(80, 40);
    embedded.show();

    main.raise();
    main.activateWindow();
    QCoreApplication::processEvents();

    QWidget* active = QApplication::activeWindow();
    QVERIFY(active != nullptr);
    QVERIFY(active != &embedded);
    QVERIFY(Daqster::nodeWidgetDialogParent(&embedded) == active);
}

void TestNodeWidgetDialogs::childWidgetUsesItsTopLevelWindow()
{
    QWidget main;
    main.resize(200, 150);
    main.show();

    QLabel child(QStringLiteral("child"), &main);
    child.show();
    QCoreApplication::processEvents();

    QVERIFY(Daqster::nodeWidgetDialogParent(&child) == &main);
}

void TestNodeWidgetDialogs::detachedWidgetIsItsOwnDialogParent()
{
    QWidget floating;
    floating.resize(120, 60);
    floating.show();
    QCoreApplication::processEvents();

    QVERIFY(Daqster::nodeWidgetDialogParent(&floating) == &floating);
}

QTEST_MAIN(TestNodeWidgetDialogs)
