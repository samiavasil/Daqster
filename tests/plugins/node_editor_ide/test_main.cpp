#include <QtTest>
#include <QApplication>

#include "test_layout_tree.h"
#include "test_layout_engine.h"
#include "test_layout_designer.h"
#include "test_flow_fixtures.h"
#include "test_widget_provider_fallback.h"
#include "test_widget_embed.h"
#include "test_status_bar.h"
#include "test_plugin_menu.h"
#include "test_scene_problems_panel.h"
#include "test_live_validation_problems.h"

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
    {
        TestStatusBar statusBar;
        status |= QTest::qExec(&statusBar, argc, argv);
    }
    {
        TestPluginMenu pluginMenu;
        status |= QTest::qExec(&pluginMenu, argc, argv);
    }
    {
        TestSceneProblemsPanel sceneProblemsPanel;
        status |= QTest::qExec(&sceneProblemsPanel, argc, argv);
    }
    {
        TestLiveValidationProblems liveValidationProblems;
        status |= QTest::qExec(&liveValidationProblems, argc, argv);
    }
    return status;
}