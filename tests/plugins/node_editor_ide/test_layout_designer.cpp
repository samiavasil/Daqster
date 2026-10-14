#include "test_layout_designer.h"

#include "LayoutDesignerPanel.h"

#include <QtNodes/Definitions>

using QtNodes::InvalidNodeId;
using QtNodes::NodeId;

namespace {

FlowUi::LayoutSection makeGridSection()
{
    FlowUi::LayoutSection section;
    section.version = 2;

    FlowUi::WorkspaceLayout ws;
    ws.id = 0;
    ws.name = QStringLiteral("W");
    ws.root.type = FlowUi::LayoutNode::Type::Grid;
    ws.root.rows = 2;
    ws.root.cols = 2;
    ws.root.cells = {NodeId(1), NodeId(2), std::nullopt, NodeId(4)};

    FlowUi::DockLayout dock;
    dock.node = NodeId(9);
    dock.title = QStringLiteral("dock");
    dock.area = FlowUi::DockLayout::Area::Right;
    ws.docks.push_back(dock);

    section.workspaces.push_back(ws);
    return section;
}

} // namespace

void TestLayoutDesigner::emptySectionGetsOneWorkspace()
{
    LayoutDesignerPanel panel;
    panel.setLayoutSection(FlowUi::LayoutSection{});

    const FlowUi::LayoutSection out = panel.layoutSection();
    QCOMPARE(out.version, 2);
    QCOMPARE(static_cast<int>(out.workspaces.size()), 1);
    QCOMPARE(out.workspaces.front().root.type, FlowUi::LayoutNode::Type::Panel);
    QCOMPARE(out.workspaces.front().root.nodeId, InvalidNodeId);
}

void TestLayoutDesigner::setSectionRoundTrips()
{
    const FlowUi::LayoutSection section = makeGridSection();

    LayoutDesignerPanel panel;
    panel.setLayoutSection(section);

    QCOMPARE(panel.layoutSection().toJson(), section.toJson());
}

void TestLayoutDesigner::addGridCreatesEmpty2x2()
{
    LayoutDesignerPanel panel;
    panel.setLayoutSection(FlowUi::LayoutSection{});

    panel.addGrid();

    const FlowUi::LayoutNode& root = panel.layoutSection().workspaces.front().root;
    QCOMPARE(root.type, FlowUi::LayoutNode::Type::Grid);
    QCOMPARE(root.rows, 2);
    QCOMPARE(root.cols, 2);
    QCOMPARE(static_cast<int>(root.cells.size()), 4);
    for (const auto& cell : root.cells)
        QVERIFY(!cell.has_value());

    // One drop target per grid cell.
    QCOMPARE(panel.slotCount(), 4);
}

void TestLayoutDesigner::assignSlotFillsGridCell()
{
    LayoutDesignerPanel panel;
    panel.setLayoutSection(FlowUi::LayoutSection{});
    panel.addGrid();

    panel.assignSlot(2, NodeId(42));

    const FlowUi::LayoutNode& root = panel.layoutSection().workspaces.front().root;
    QVERIFY(root.cells.size() >= 3);
    QVERIFY(root.cells[2].has_value());
    QCOMPARE(*root.cells[2], NodeId(42));
    // Other cells untouched.
    QVERIFY(!root.cells[0].has_value());
}

void TestLayoutDesigner::addDockAddsDockSlot()
{
    LayoutDesignerPanel panel;
    panel.setLayoutSection(FlowUi::LayoutSection{});

    // Empty root panel = 1 slot; after adding a dock there is a second slot.
    QCOMPARE(panel.slotCount(), 1);
    panel.addDock(FlowUi::DockLayout::Area::Left);

    const FlowUi::WorkspaceLayout& ws = panel.layoutSection().workspaces.front();
    QCOMPARE(static_cast<int>(ws.docks.size()), 1);
    QCOMPARE(ws.docks.front().area, FlowUi::DockLayout::Area::Left);
    QCOMPARE(panel.slotCount(), 2);

    // Slot 1 is the dock (root slot first, then dock).
    panel.assignSlot(1, NodeId(7));
    QCOMPARE(panel.layoutSection().workspaces.front().docks.front().node, NodeId(7));
}

void TestLayoutDesigner::addSplitWrapsExistingRoot()
{
    FlowUi::LayoutSection section;
    section.workspaces.push_back(FlowUi::WorkspaceLayout{});
    section.workspaces.front().root.type = FlowUi::LayoutNode::Type::Panel;
    section.workspaces.front().root.nodeId = NodeId(5);

    LayoutDesignerPanel panel;
    panel.setLayoutSection(section);
    panel.addSplit(FlowUi::LayoutNode::Orientation::Horizontal);

    const FlowUi::LayoutNode& root = panel.layoutSection().workspaces.front().root;
    QCOMPARE(root.type, FlowUi::LayoutNode::Type::Split);
    QCOMPARE(root.orientation, FlowUi::LayoutNode::Orientation::Horizontal);
    QCOMPARE(static_cast<int>(root.children.size()), 2);
    QCOMPARE(root.children[0].nodeId, NodeId(5));
    QCOMPARE(root.children[1].nodeId, InvalidNodeId);
}

void TestLayoutDesigner::undoRestoresPreviousSection()
{
    LayoutDesignerPanel panel;
    panel.setLayoutSection(FlowUi::LayoutSection{});

    const QJsonObject before = panel.layoutSection().toJson();
    panel.addGrid();
    QCOMPARE(panel.layoutSection().workspaces.front().root.type,
             FlowUi::LayoutNode::Type::Grid);

    panel.undo();
    QCOMPARE(panel.layoutSection().toJson(), before);

    panel.redo();
    QCOMPARE(panel.layoutSection().workspaces.front().root.type,
             FlowUi::LayoutNode::Type::Grid);
}

void TestLayoutDesigner::addAndRemoveWorkspace()
{
    LayoutDesignerPanel panel;
    panel.setLayoutSection(FlowUi::LayoutSection{});
    QCOMPARE(static_cast<int>(panel.layoutSection().workspaces.size()), 1);

    panel.addWorkspace();
    QCOMPARE(static_cast<int>(panel.layoutSection().workspaces.size()), 2);

    panel.removeWorkspace();
    QCOMPARE(static_cast<int>(panel.layoutSection().workspaces.size()), 1);
}
