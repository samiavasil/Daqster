#include <QtTest>

#include "LayoutTree.h"
#include "test_layout_tree.h"

// REQ-SW-PL-053 Phase 2a: schema-v2 layout tree JSON round-trip and tolerant
// parsing tests. The model is pure data (QtCore only) — no widgets involved.

namespace {

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

bool docksEqual(const FlowUi::DockLayout& a, const FlowUi::DockLayout& b)
{
    return a.node == b.node
        && a.title == b.title
        && a.area == b.area
        && a.floating == b.floating
        && a.geometry.x == b.geometry.x
        && a.geometry.y == b.geometry.y
        && a.geometry.w == b.geometry.w
        && a.geometry.h == b.geometry.h
        && a.geometry.maximized == b.geometry.maximized;
}

bool sectionsEqual(const FlowUi::LayoutSection& a, const FlowUi::LayoutSection& b)
{
    if (a.version != b.version)
        return false;
    if (a.workspaces.size() != b.workspaces.size())
        return false;
    for (size_t i = 0; i < a.workspaces.size(); ++i) {
        const FlowUi::WorkspaceLayout& wa = a.workspaces[i];
        const FlowUi::WorkspaceLayout& wb = b.workspaces[i];
        if (wa.id != wb.id || wa.name != wb.name)
            return false;
        if (!nodesEqual(wa.root, wb.root))
            return false;
        if (wa.docks.size() != wb.docks.size())
            return false;
        for (size_t j = 0; j < wa.docks.size(); ++j) {
            if (!docksEqual(wa.docks[j], wb.docks[j]))
                return false;
        }
    }
    if (a.nodes.size() != b.nodes.size())
        return false;
    for (auto it = a.nodes.constBegin(); it != a.nodes.constEnd(); ++it) {
        if (!b.nodes.contains(it.key()))
            return false;
        if (it.value().autoStart != b.nodes.value(it.key()).autoStart)
            return false;
    }
    return true;
}

FlowUi::LayoutSection makeGridSection()
{
    FlowUi::LayoutSection section;
    section.version = 2;

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
    dock.floating = false;
    dock.geometry = {0, 0, 320, 600, false};
    ws.docks.push_back(dock);

    section.workspaces.push_back(ws);

    FlowUi::NodeUi source;
    source.autoStart = true;
    section.nodes.insert(0, source);
    for (QtNodes::NodeId id = 1; id <= 4; ++id)
        section.nodes.insert(id, FlowUi::NodeUi());
    return section;
}

FlowUi::LayoutSection makeSplitSection()
{
    FlowUi::LayoutSection section;
    section.version = 2;

    FlowUi::WorkspaceLayout ws;
    ws.id = 0;
    ws.name = QStringLiteral("Split");

    FlowUi::LayoutNode split;
    split.type = FlowUi::LayoutNode::Type::Split;
    split.orientation = FlowUi::LayoutNode::Orientation::Vertical;
    split.sizes = {200, 400};

    FlowUi::LayoutNode left;
    left.type = FlowUi::LayoutNode::Type::Panel;
    left.nodeId = 1;
    left.title = QStringLiteral("Left");
    split.children.push_back(left);

    FlowUi::LayoutNode right;
    right.type = FlowUi::LayoutNode::Type::Panel;
    right.nodeId = 2;
    split.children.push_back(right);

    ws.root = split;
    section.workspaces.push_back(ws);
    return section;
}

FlowUi::LayoutSection makeTabSection()
{
    FlowUi::LayoutSection section;
    section.version = 2;

    FlowUi::WorkspaceLayout ws;
    ws.id = 0;
    ws.name = QStringLiteral("Tabs");

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
    section.workspaces.push_back(ws);
    return section;
}

FlowUi::LayoutSection makeComboSection()
{
    FlowUi::LayoutSection section;
    section.version = 2;

    FlowUi::WorkspaceLayout ws;
    ws.id = 0;
    ws.name = QStringLiteral("Combo");

    // split (horizontal) → [ panel 1, tab → [ panel 2, grid 2x1 (3,4) ] ]
    FlowUi::LayoutNode split;
    split.type = FlowUi::LayoutNode::Type::Split;
    split.orientation = FlowUi::LayoutNode::Orientation::Horizontal;
    split.sizes = {300, 700};

    FlowUi::LayoutNode panel1;
    panel1.type = FlowUi::LayoutNode::Type::Panel;
    panel1.nodeId = 1;
    split.children.push_back(panel1);

    FlowUi::LayoutNode tab;
    tab.type = FlowUi::LayoutNode::Type::Tab;
    tab.currentIndex = 0;

    FlowUi::LayoutNode panel2;
    panel2.type = FlowUi::LayoutNode::Type::Panel;
    panel2.nodeId = 2;
    tab.children.push_back(panel2);

    FlowUi::LayoutNode grid;
    grid.type = FlowUi::LayoutNode::Type::Grid;
    grid.rows = 2;
    grid.cols = 1;
    grid.cells = {3u, 4u};
    tab.children.push_back(grid);

    split.children.push_back(tab);
    ws.root = split;
    section.workspaces.push_back(ws);
    return section;
}

} // namespace

void TestLayoutTree::gridRoundTrip()
{
    const FlowUi::LayoutSection section = makeGridSection();
    const FlowUi::LayoutSection parsed =
        FlowUi::LayoutSection::fromJson(section.toJson());
    QVERIFY(sectionsEqual(section, parsed));
}

void TestLayoutTree::splitRoundTrip()
{
    const FlowUi::LayoutSection section = makeSplitSection();
    const FlowUi::LayoutSection parsed =
        FlowUi::LayoutSection::fromJson(section.toJson());
    QVERIFY(sectionsEqual(section, parsed));
}

void TestLayoutTree::tabRoundTrip()
{
    const FlowUi::LayoutSection section = makeTabSection();
    const FlowUi::LayoutSection parsed =
        FlowUi::LayoutSection::fromJson(section.toJson());
    QVERIFY(sectionsEqual(section, parsed));
}

void TestLayoutTree::dockRoundTrip()
{
    const FlowUi::LayoutSection section = makeGridSection();
    QCOMPARE(static_cast<int>(section.workspaces[0].docks.size()), 1);
    const FlowUi::DockLayout& dock = section.workspaces[0].docks[0];
    QCOMPARE(dock.node, 0u);
    QCOMPARE(dock.title, QStringLiteral("Source Config"));
    QCOMPARE(dock.area, FlowUi::DockLayout::Area::Right);
    QCOMPARE(dock.floating, false);
    QCOMPARE(dock.geometry.w, 320);
    QCOMPARE(dock.geometry.h, 600);
}

void TestLayoutTree::comboRoundTrip()
{
    const FlowUi::LayoutSection section = makeComboSection();
    const FlowUi::LayoutSection parsed =
        FlowUi::LayoutSection::fromJson(section.toJson());
    QVERIFY(sectionsEqual(section, parsed));
}

void TestLayoutTree::nodesMapRoundTrip()
{
    const FlowUi::LayoutSection section = makeGridSection();
    QCOMPARE(static_cast<int>(section.nodes.size()), 5);
    QVERIFY(section.nodes.contains(0));
    QCOMPARE(section.nodes.value(0).autoStart, true);
    QCOMPARE(section.nodes.value(1).autoStart, false);
}

void TestLayoutTree::unknownTypeFallsBackToPanel()
{
    // Unknown node type → panel with no node (tolerant parse).
    const FlowUi::LayoutNode node =
        FlowUi::LayoutNode::fromJson(QJsonObject{{QStringLiteral("type"), QStringLiteral("bogus")}});
    QCOMPARE(node.type, FlowUi::LayoutNode::Type::Panel);
    QCOMPARE(node.nodeId, QtNodes::InvalidNodeId);
}

void TestLayoutTree::missingFieldsUseDefaults()
{
    // A grid with no rows/cols/cells keeps its defaults (0/0/empty).
    const FlowUi::LayoutNode grid =
        FlowUi::LayoutNode::fromJson(QJsonObject{{QStringLiteral("type"), QStringLiteral("grid")}});
    QCOMPARE(grid.type, FlowUi::LayoutNode::Type::Grid);
    QCOMPARE(grid.rows, 0);
    QCOMPARE(grid.cols, 0);
    QVERIFY(grid.cells.empty());

    // A panel with no node key → InvalidNodeId.
    const FlowUi::LayoutNode panel =
        FlowUi::LayoutNode::fromJson(QJsonObject{{QStringLiteral("type"), QStringLiteral("panel")}});
    QCOMPARE(panel.type, FlowUi::LayoutNode::Type::Panel);
    QCOMPARE(panel.nodeId, QtNodes::InvalidNodeId);

    // A dock with no area → right.
    const FlowUi::DockLayout dock = FlowUi::DockLayout::fromJson(QJsonObject{});
    QCOMPARE(dock.area, FlowUi::DockLayout::Area::Right);
    QCOMPARE(dock.floating, false);
}

void TestLayoutTree::versionThreeParsesEmpty()
{
    const FlowUi::LayoutSection parsed =
        FlowUi::LayoutSection::fromJson(QJsonObject{{QStringLiteral("version"), 3}});
    QVERIFY(parsed.workspaces.empty());
    QVERIFY(parsed.nodes.isEmpty());
}

void TestLayoutTree::versionOneParsesEmpty()
{
    // v1 flows must NOT be interpreted as layout trees — empty section.
    const FlowUi::LayoutSection parsed =
        FlowUi::LayoutSection::fromJson(QJsonObject{{QStringLiteral("version"), 1}});
    QVERIFY(parsed.workspaces.empty());
    QVERIFY(parsed.nodes.isEmpty());
}

// No QTEST_GUILESS_MAIN here: the layout test classes share one binary whose
// main lives in test_main.cpp (QApplication — the engine tests build widgets).