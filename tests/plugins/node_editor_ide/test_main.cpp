#include <QtTest>
#include <QApplication>

#include "test_layout_tree.h"
#include "test_layout_engine.h"
#include "test_layout_designer.h"
#include "test_flow_fixtures.h"
#include "test_widget_provider_fallback.h"
#include "test_widget_embed.h"

// Shared main for the node_editor_ide layout tests. LayoutEngine builds real
// widgets (QSplitter/QGridLayout/QTabWidget/QDockWidget), so the binary runs
// under QApplication with the offscreen platform plugin (set in CMake). The
// LayoutTree tests are QtCore-only but run fine under QApplication too.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);

    int status = 0;
    {
        TestLayoutTree layoutTree;
        status |= QTest::qExec(&layoutTree, argc, argv);
    }
    {
        TestLayoutEngine layoutEngine;
        status |= QTest::qExec(&layoutEngine, argc, argv);
    }
    {
        TestLayoutDesigner layoutDesigner;
        status |= QTest::qExec(&layoutDesigner, argc, argv);
    }
    {
        TestFlowFixtures flowFixtures;
        status |= QTest::qExec(&flowFixtures, argc, argv);
    }
    {
        TestWidgetProviderFallback widgetProviderFallback;
        status |= QTest::qExec(&widgetProviderFallback, argc, argv);
    }
    {
        TestWidgetEmbed widgetEmbed;
        status |= QTest::qExec(&widgetEmbed, argc, argv);
    }
    return status;
}