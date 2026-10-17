#include "test_layout_designer.h"

#include "LayoutDesignerPanel.h"

#include <QHash>
#include <QWidget>

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
    for (const NodeId id : {NodeId(1), NodeId(2), InvalidNodeId, NodeId(4)}) {
        FlowUi::LayoutNode cell;
        cell.type = FlowUi::LayoutNode::Type::Panel;
        cell.nodeId = id;
        ws.root.children.push_back(cell);
    }

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

    const FlowUi::LayoutSection out = panel.layoutSection();
    const FlowUi::LayoutNode& root = out.workspaces.front().root;
    QCOMPARE(root.type, FlowUi::LayoutNode::Type::Grid);
    QCOMPARE(root.rows, 2);
    QCOMPARE(root.cols, 2);
    QCOMPARE(static_cast<int>(root.children.size()), 4);
    for (const auto& cell : root.children)
        QVERIFY(cell.isEmptyPanel());

    // One drop target per grid cell.
    QCOMPARE(panel.slotCount(), 4);
}

void TestLayoutDesigner::assignSlotFillsGridCell()
{
    LayoutDesignerPanel panel;
    panel.setLayoutSection(FlowUi::LayoutSection{});
    panel.addGrid();

    panel.assignSlot(2, NodeId(42));

    const FlowUi::LayoutSection out = panel.layoutSection();
    const FlowUi::LayoutNode& root = out.workspaces.front().root;
    QVERIFY(root.children.size() >= 3);
    QCOMPARE(root.children[2].nodeId, NodeId(42));
    // Other cells untouched.
    QVERIFY(root.children[0].isEmptyPanel());
}

void TestLayoutDesigner::addGridRespectsRequestedSize()
{
    LayoutDesignerPanel panel;
    panel.setLayoutSection(FlowUi::LayoutSection{});

    panel.addGrid(2, 3);

    const FlowUi::LayoutSection out = panel.layoutSection();
    const FlowUi::LayoutNode& root = out.workspaces.front().root;
    QCOMPARE(root.type, FlowUi::LayoutNode::Type::Grid);
    QCOMPARE(root.rows, 2);
    QCOMPARE(root.cols, 3);
    QCOMPARE(static_cast<int>(root.children.size()), 6);
    // One drop target per cell.
    QCOMPARE(panel.slotCount(), 6);
}

// Container buttons act on the item selected in the Structure tree, which is
// what makes nesting possible: select an empty pane, then press "Grid".
void TestLayoutDesigner::nestedContainerTargetsSelectedSlot()
{
    LayoutDesignerPanel panel;
    panel.setLayoutSection(FlowUi::LayoutSection{});

    // Split the root into two panes: path {0} and {1}.
    panel.addSplit(FlowUi::LayoutNode::Orientation::Horizontal);

    // Select the second pane and build a 2x3 grid inside it.
    QVERIFY(panel.selectNodeByPath({1}));
    panel.addGrid(2, 3);

    const FlowUi::LayoutSection out = panel.layoutSection();
    const FlowUi::LayoutNode& root = out.workspaces.front().root;
    QCOMPARE(root.type, FlowUi::LayoutNode::Type::Split);
    QCOMPARE(static_cast<int>(root.children.size()), 2);
    QCOMPARE(root.children[0].type, FlowUi::LayoutNode::Type::Panel);
    QCOMPARE(root.children[1].type, FlowUi::LayoutNode::Type::Grid);
    QCOMPARE(root.children[1].rows, 2);
    QCOMPARE(root.children[1].cols, 3);
    QCOMPARE(static_cast<int>(root.children[1].children.size()), 6);
}

// Deleting a component removes it from the layout; its widget (owned by the
// graph) is detached, i.e. simply no longer referenced.
void TestLayoutDesigner::deleteRemovesComponentAndDetachesNode()
{
    FlowUi::LayoutSection section;
    FlowUi::WorkspaceLayout ws;
    ws.id = 0;
    ws.root.type = FlowUi::LayoutNode::Type::Split;
    ws.root.orientation = FlowUi::LayoutNode::Orientation::Horizontal;
    FlowUi::LayoutNode a;
    a.type = FlowUi::LayoutNode::Type::Panel;
    a.nodeId = 5;
    FlowUi::LayoutNode b;
    b.type = FlowUi::LayoutNode::Type::Panel;
    b.nodeId = 6;
    ws.root.children.push_back(a);
    ws.root.children.push_back(b);
    section.workspaces.push_back(ws);

    LayoutDesignerPanel panel;
    panel.setLayoutSection(section);

    QVERIFY(panel.selectNodeByPath({0}));
    panel.deleteComponent();

    const FlowUi::LayoutSection out = panel.layoutSection();
    const FlowUi::LayoutNode& root = out.workspaces.front().root;
    QCOMPARE(root.type, FlowUi::LayoutNode::Type::Split);
    QCOMPARE(static_cast<int>(root.children.size()), 1);
    // Node 5 is gone from the layout; node 6 stays.
    QCOMPARE(root.children[0].nodeId, NodeId(6));

    // And the whole thing is undoable.
    panel.undo();
    const FlowUi::LayoutSection restored = panel.layoutSection();
    QCOMPARE(static_cast<int>(restored.workspaces.front().root.children.size()), 2);
}

// Deleting the last pane of a split root collapses it back to an empty panel
// instead of leaving a degenerate one-sided split.
void TestLayoutDesigner::deleteCollapsesEmptySplitRoot()
{
    FlowUi::LayoutSection section;
    FlowUi::WorkspaceLayout ws;
    ws.id = 0;
    ws.root.type = FlowUi::LayoutNode::Type::Split;
    FlowUi::LayoutNode only;
    only.type = FlowUi::LayoutNode::Type::Panel;
    ws.root.children.push_back(only);
    section.workspaces.push_back(ws);

    LayoutDesignerPanel panel;
    panel.setLayoutSection(section);

    QVERIFY(panel.selectNodeByPath({0}));
    panel.deleteComponent();

    const FlowUi::LayoutSection out = panel.layoutSection();
    QCOMPARE(out.workspaces.front().root.type, FlowUi::LayoutNode::Type::Panel);
    QCOMPARE(out.workspaces.front().root.nodeId, InvalidNodeId);
}

void TestLayoutDesigner::addDockAddsDockSlot()
{
    LayoutDesignerPanel panel;
    panel.setLayoutSection(FlowUi::LayoutSection{});

    // Empty root panel = 1 slot; after adding a dock there is a second slot.
    QCOMPARE(panel.slotCount(), 1);
    panel.addDock(FlowUi::DockLayout::Area::Left);

    const FlowUi::WorkspaceLayout ws = panel.layoutSection().workspaces.front();
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

    const FlowUi::LayoutSection out = panel.layoutSection();
    const FlowUi::LayoutNode& root = out.workspaces.front().root;
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

// Regression (2026-10-15): the drag source wrote the node id with QDataStream
// but the drop target read it back with QByteArray::toUInt() (decimal parse),
// so every drop decoded as node 0. Both sides now share encode/decodeNodeMime.
// Regression (REQ-SW-PL-059): a splitter with a widget in each pane used to
// segfault on the second drop. LayoutDesignerPanel::clearPreview() releases the
// graph-owned widgets before deleting the preview, but LayoutDesignerPanel's
// releaseNodeWidgets() reparented widgets out of a QSplitter while still
// iterating a findChildren() snapshot: reparenting out of a splitter makes Qt
// destroy a QSplitterHandle (an untagged child), and the next snapshot entry —
// the freed handle — was dereferenced via ->property(). Both pane assignments
// exercise the clearPreview()/rebuildPreview() cycle on real widgets.
void TestLayoutDesigner::splitterRebuildWithTwoWidgetsKeepsPreview()
{
    QHash<NodeId, QWidget*> cache;
    QList<QWidget*> owned;

    LayoutDesignerPanel panel;
    panel.setProvider([&cache, &owned](NodeId id) -> QWidget* {
        if (auto it = cache.constFind(id); it != cache.constEnd())
            return it.value();
        auto* w = new QWidget;
        w->setWindowTitle(QStringLiteral("node-%1").arg(id));
        cache.insert(id, w);
        owned.push_back(w);
        return w;
    });

    panel.setLayoutSection(FlowUi::LayoutSection{});
    panel.addSplit(FlowUi::LayoutNode::Orientation::Horizontal);
    QCOMPARE(panel.slotCount(), 2);

    panel.assignSlot(0, NodeId(1));
    panel.assignSlot(1, NodeId(2));

    const FlowUi::LayoutNode& root = panel.layoutSection().workspaces.front().root;
    QCOMPARE(root.type, FlowUi::LayoutNode::Type::Split);
    QCOMPARE(static_cast<int>(root.children.size()), 2);
    QCOMPARE(root.children[0].nodeId, NodeId(1));
    QCOMPARE(root.children[1].nodeId, NodeId(2));

    panel.setProvider(nullptr);
    qDeleteAll(owned);
}

void TestLayoutDesigner::nodeMimeRoundTripsNodeId()
{
    for (const NodeId id : {NodeId(0), NodeId(1), NodeId(42), NodeId(65535), NodeId(16777216)}) {
        const QByteArray payload = LayoutDesignerPanel::encodeNodeMime(id);
        QCOMPARE(LayoutDesignerPanel::decodeNodeMime(payload), id);
    }

    // The sentinel survives, and a corrupt/short payload is rejected safely
    // (returns InvalidNodeId rather than silently assigning node 0).
    QCOMPARE(LayoutDesignerPanel::decodeNodeMime(
                 LayoutDesignerPanel::encodeNodeMime(InvalidNodeId)),
             InvalidNodeId);
    QCOMPARE(LayoutDesignerPanel::decodeNodeMime(QByteArray()), InvalidNodeId);
    QCOMPARE(LayoutDesignerPanel::decodeNodeMime(QByteArray("12")), InvalidNodeId);
}
