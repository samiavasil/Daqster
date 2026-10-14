#include <QtTest>

#include <QGridLayout>
#include <QLayoutItem>
#include <QSplitter>
#include <QTabWidget>
#include <QDockWidget>

#include "LayoutEngine.h"
#include "test_layout_engine.h"

// REQ-SW-PL-053 Phase 2a: LayoutEngine widget builder + capturer tests.
// Runs under QApplication with QT_QPA_PLATFORM=offscreen (set in CMake).

namespace {

QWidget* makeDummy(QtNodes::NodeId id)
{
    auto* widget = new QWidget();
    widget->setWindowTitle(QStringLiteral("Node %1").arg(id));
    return widget;
}

FlowUi::LayoutEngine::NodeWidgetProvider dummyProvider()
{
    return [](QtNodes::NodeId id) -> QWidget* { return makeDummy(id); };
}

bool nodesEqual(const FlowUi::LayoutNode& a, const FlowUi::LayoutNode& b)
{
    if (a.type != b.type)
        return false;
    if (a.orientation != b.orientation)
        return false;
    if (a.rows != b.rows || a.cols != b.cols)
        return false;
    if (a.currentIndex != b.currentIndex)
        return false;
    if (a.nodeId != b.nodeId)
        return false;
    if (a.title != b.title)
        return false;
    if (a.sizes != b.sizes)
        return false;
    if (a.cells.size() != b.cells.size())
        return false;
    for (size_t i = 0; i < a.cells.size(); ++i) {
        if (a.cells[i].has_value() != b.cells[i].has_value())
            return false;
        if (a.cells[i].has_value() && *a.cells[i] != *b.cells[i])
            return false;
    }
    if (a.children.size() != b.children.size())
        return false;
    for (size_t i = 0; i < a.children.size(); ++i) {
        if (!nodesEqual(a.children[i], b.children[i]))
            return false;
    }
    return true;
}

FlowUi::WorkspaceLayout makeGridWorkspace()
{
    FlowUi::WorkspaceLayout ws;
    ws.id = 0;
    ws.name = QStringLiteral("Main");

    FlowUi::LayoutNode grid;
    grid.type = FlowUi::LayoutNode::Type::Grid;
    grid.rows = 2;
    grid.cols = 2;
    grid.cells = {1u, 2u, 3u, 4u};
    ws.root = grid;

    FlowUi::DockLayout dock;
    dock.node = 0;
    dock.title = QStringLiteral("Source Config");
    dock.area = FlowUi::DockLayout::Area::Right;
    dock.floating = true;
    dock.geometry = {10, 20, 320, 600, false};
    ws.docks.push_back(dock);
    return ws;
}

} // namespace

void TestLayoutEngine::gridBuildsCentralWidget()
{
    FlowUi::WorkspaceLayout ws;
    ws.id = 0;
    ws.name = QStringLiteral("Main");

    FlowUi::LayoutNode grid;
    grid.type = FlowUi::LayoutNode::Type::Grid;
    grid.rows = 2;
    grid.cols = 2;
    grid.cells = {1u, 2u, 3u, 4u};
    ws.root = grid;

    QMainWindow* window = FlowUi::LayoutEngine::buildWorkspace(ws, dummyProvider());
    QVERIFY(window != nullptr);

    QWidget* central = window->centralWidget();
    QVERIFY(central != nullptr);
    QCOMPARE(central->property("layoutNodeType").toString(), QStringLiteral("grid"));

    auto* layout = qobject_cast<QGridLayout*>(central->layout());
    QVERIFY(layout != nullptr);
    QCOMPARE(layout->count(), 4);

    for (int index = 0; index < 4; ++index) {
        QLayoutItem* item = layout->itemAtPosition(index / 2, index % 2);
        QVERIFY(item != nullptr);
        QWidget* cell = item->widget();
        QVERIFY(cell != nullptr);
        QCOMPARE(cell->property("layoutNodeType").toString(), QStringLiteral("panel"));
        QCOMPARE(cell->property("layoutNodeId").toUInt(), static_cast<uint>(index + 1));
    }
    delete window;
}

// REQ-SW-PL-053/REQ-SW-PL-059: RuntimeShell applies a v2 layout to its existing
// runtime window via buildInto() instead of creating a second QMainWindow.
void TestLayoutEngine::buildIntoReplacesExistingCentralWidget()
{
    QMainWindow host;
    QWidget* oldCentral = new QWidget(&host);
    host.setCentralWidget(oldCentral);

    const FlowUi::WorkspaceLayout ws = makeGridWorkspace();
    const bool built = FlowUi::LayoutEngine::buildInto(&host, ws, dummyProvider());
    QVERIFY(built);

    QWidget* central = host.centralWidget();
    QVERIFY(central != nullptr);
    QVERIFY(central != oldCentral);
    QCOMPARE(central->property("layoutNodeType").toString(), QStringLiteral("grid"));

    // The dock declared by the workspace is attached to the host window.
    QCOMPARE(host.findChildren<QDockWidget*>().size(), 1);
    QCOMPARE(host.property("layoutWorkspaceId").toInt(), 0);
}

void TestLayoutEngine::splitRestoresSizes()
{
    FlowUi::WorkspaceLayout ws;
    ws.id = 0;

    FlowUi::LayoutNode split;
    split.type = FlowUi::LayoutNode::Type::Split;
    split.orientation = FlowUi::LayoutNode::Orientation::Horizontal;
    split.sizes = {100, 200};

    FlowUi::LayoutNode left;
    left.type = FlowUi::LayoutNode::Type::Panel;
    left.nodeId = 1;
    split.children.push_back(left);

    FlowUi::LayoutNode right;
    right.type = FlowUi::LayoutNode::Type::Panel;
    right.nodeId = 2;
    split.children.push_back(right);
    ws.root = split;

    QMainWindow* window = FlowUi::LayoutEngine::buildWorkspace(ws, dummyProvider());
    QVERIFY(window != nullptr);

    auto* splitter = qobject_cast<QSplitter*>(window->centralWidget());
    QVERIFY(splitter != nullptr);
    QCOMPARE(splitter->orientation(), Qt::Horizontal);
    QCOMPARE(splitter->count(), 2);

    // QSplitter normalizes setSizes() to its current width — give the window a
    // known size so the restored sizes are deterministic.
    window->resize(300, 200);
    window->show();
    QCoreApplication::processEvents();

    const QList<int> sizes = splitter->sizes();
    QCOMPARE(sizes.size(), 2);
    // The offscreen window frame may shave a few pixels off the requested
    // width; the 1:2 ratio from setSizes({100, 200}) must be preserved.
    QVERIFY(sizes[0] + sizes[1] >= 290);
    QVERIFY(sizes[0] + sizes[1] <= 300);
    QVERIFY(qAbs(sizes[0] * 2 - sizes[1]) <= 4);

    delete window;
}

void TestLayoutEngine::tabRestoresCurrent()
{
    FlowUi::WorkspaceLayout ws;
    ws.id = 0;

    FlowUi::LayoutNode tab;
    tab.type = FlowUi::LayoutNode::Type::Tab;
    tab.currentIndex = 1;

    FlowUi::LayoutNode first;
    first.type = FlowUi::LayoutNode::Type::Panel;
    first.nodeId = 1;
    first.title = QStringLiteral("First");
    tab.children.push_back(first);

    FlowUi::LayoutNode second;
    second.type = FlowUi::LayoutNode::Type::Panel;
    second.nodeId = 2;
    second.title = QStringLiteral("Second");
    tab.children.push_back(second);

    FlowUi::LayoutNode third;
    third.type = FlowUi::LayoutNode::Type::Panel;
    third.nodeId = 3;
    tab.children.push_back(third);
    ws.root = tab;

    QMainWindow* window = FlowUi::LayoutEngine::buildWorkspace(ws, dummyProvider());
    QVERIFY(window != nullptr);

    auto* tabs = qobject_cast<QTabWidget*>(window->centralWidget());
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 3);
    QCOMPARE(tabs->currentIndex(), 1);
    QCOMPARE(tabs->tabText(0), QStringLiteral("First"));
    QCOMPARE(tabs->tabText(1), QStringLiteral("Second"));
    // Third panel has no explicit title → falls back to the widget window title.
    QCOMPARE(tabs->tabText(2), QStringLiteral("Node 3"));

    delete window;
}

void TestLayoutEngine::dockBuildsInRightArea()
{
    FlowUi::WorkspaceLayout ws;
    ws.id = 0;

    FlowUi::DockLayout dock;
    dock.node = 0;
    dock.title = QStringLiteral("Source Config");
    dock.area = FlowUi::DockLayout::Area::Right;
    dock.floating = false;
    ws.docks.push_back(dock);

    QMainWindow* window = FlowUi::LayoutEngine::buildWorkspace(ws, dummyProvider());
    QVERIFY(window != nullptr);

    const QList<QDockWidget*> docks = window->findChildren<QDockWidget*>();
    QCOMPARE(docks.size(), 1);
    QCOMPARE(window->dockWidgetArea(docks[0]), Qt::RightDockWidgetArea);
    QCOMPARE(docks[0]->isFloating(), false);
    QCOMPARE(docks[0]->property("layoutDockNode").toUInt(), 0u);
    QCOMPARE(docks[0]->property("layoutDockTitle").toString(), QStringLiteral("Source Config"));

    delete window;
}

void TestLayoutEngine::dockFloatsWhenFloating()
{
    FlowUi::WorkspaceLayout ws;
    ws.id = 0;

    FlowUi::DockLayout dock;
    dock.node = 0;
    dock.title = QStringLiteral("Source Config");
    dock.area = FlowUi::DockLayout::Area::Right;
    dock.floating = true;
    dock.geometry = {10, 20, 320, 600, false};
    ws.docks.push_back(dock);

    QMainWindow* window = FlowUi::LayoutEngine::buildWorkspace(ws, dummyProvider());
    QVERIFY(window != nullptr);

    const QList<QDockWidget*> docks = window->findChildren<QDockWidget*>();
    QCOMPARE(docks.size(), 1);
    QCOMPARE(docks[0]->isFloating(), true);

    delete window;
}

void TestLayoutEngine::captureRoundTrips()
{
    const FlowUi::WorkspaceLayout ws = makeGridWorkspace();
    QMainWindow* window = FlowUi::LayoutEngine::buildWorkspace(ws, dummyProvider());
    QVERIFY(window != nullptr);

    const FlowUi::WorkspaceLayout captured = FlowUi::LayoutEngine::captureWorkspace(window);

    QCOMPARE(captured.id, ws.id);
    QCOMPARE(captured.name, ws.name);
    QVERIFY(nodesEqual(captured.root, ws.root));

    QCOMPARE(static_cast<int>(captured.docks.size()), 1);
    const FlowUi::DockLayout& dock = captured.docks[0];
    QCOMPARE(dock.node, 0u);
    QCOMPARE(dock.title, QStringLiteral("Source Config"));
    QCOMPARE(dock.area, FlowUi::DockLayout::Area::Right);
    QCOMPARE(dock.floating, true);
    // Floating dock geometry is applied by buildWorkspace and read back live.
    QCOMPARE(dock.geometry.w, 320);
    QCOMPARE(dock.geometry.h, 600);

    delete window;
}

void TestLayoutEngine::missingNodeSkipsCell()
{
    FlowUi::WorkspaceLayout ws;
    ws.id = 0;

    FlowUi::LayoutNode grid;
    grid.type = FlowUi::LayoutNode::Type::Grid;
    grid.rows = 1;
    grid.cols = 2;
    grid.cells = {1u, 2u};
    ws.root = grid;

    // Provider cannot supply node 2 → its cell is skipped.
    QMainWindow* window = FlowUi::LayoutEngine::buildWorkspace(
        ws, [](QtNodes::NodeId id) -> QWidget* {
            if (id == 2)
                return nullptr;
            return makeDummy(id);
        });
    QVERIFY(window != nullptr);

    auto* layout = qobject_cast<QGridLayout*>(window->centralWidget()->layout());
    QVERIFY(layout != nullptr);
    QCOMPARE(layout->count(), 1);
    QVERIFY(layout->itemAtPosition(0, 0) != nullptr);
    QVERIFY(layout->itemAtPosition(0, 1) == nullptr);

    delete window;
}

// No QTEST_MAIN here: the layout test classes share one binary whose main
// lives in test_main.cpp (QApplication — this class builds real widgets).