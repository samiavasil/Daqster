#include "LayoutDesignerPanel.h"

#include <QAction>
#include <QBrush>
#include <QColor>
#include <QComboBox>
#include <QDataStream>
#include <QDropEvent>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QMimeData>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QVariantList>

#include <algorithm>

using QtNodes::InvalidNodeId;
using QtNodes::NodeId;

namespace {

const char* const kNodeMime = "application/x-daqster-node-id";
const int kAssignRole = Qt::UserRole + 1;
// Tree item data roles used by the toolbar to target the selected component:
const int kPathRole = Qt::UserRole + 2; // QVariantList of child indices from root
const int kDockRole = Qt::UserRole + 3; // int index into WorkspaceLayout::docks

/// QListWidget whose drag payload is the node id (our own mime type).
class NodeListWidget : public QListWidget
{
public:
    using QListWidget::QListWidget;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override
#else
    QMimeData* mimeData(const QList<QListWidgetItem*> items) const override
#endif
    {
        auto* mime = new QMimeData();
        if (!items.isEmpty()) {
            const quint32 nodeId = items.first()->data(Qt::UserRole).toUInt();
            mime->setData(QLatin1String(kNodeMime),
                          LayoutDesignerPanel::encodeNodeMime(nodeId));
        }
        return mime;
    }
};

QPoint dropPosition(QDropEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position().toPoint();
#else
    return event->pos();
#endif
}

QString areaLabel(FlowUi::DockLayout::Area area)
{
    switch (area) {
    case FlowUi::DockLayout::Area::Left:   return QStringLiteral("Left");
    case FlowUi::DockLayout::Area::Top:    return QStringLiteral("Top");
    case FlowUi::DockLayout::Area::Bottom: return QStringLiteral("Bottom");
    case FlowUi::DockLayout::Area::Right:  // fall through
    default:                               return QStringLiteral("Right");
    }
}

/// True when a node contributes no panel (empty slot).
bool isUnassigned(NodeId id)
{
    return id == InvalidNodeId;
}

QVariantList pathToList(const QVector<int>& path)
{
    QVariantList list;
    list.reserve(path.size());
    for (int index : path)
        list.append(index);
    return list;
}

QVector<int> pathFromList(const QVariantList& list)
{
    QVector<int> path;
    path.reserve(list.size());
    for (const QVariant& value : list)
        path.append(value.toInt());
    return path;
}

/// Resolve `path` (child indices from `root`) to a node, or null when it runs
/// off the tree. An empty path is `root` itself.
FlowUi::LayoutNode* nodeAtPath(FlowUi::LayoutNode& root, const QVector<int>& path)
{
    FlowUi::LayoutNode* node = &root;
    for (int index : path) {
        if (index < 0 || index >= static_cast<int>(node->children.size()))
            return nullptr;
        node = &node->children[index];
    }
    return node;
}

/// Keep a container usable: a split always has at least two panes (a one-sided
/// split is meaningless), a tab at least one page, and a grid exactly rows*cols
/// cells.
void ensureSlot(FlowUi::LayoutNode& node)
{
    using FlowUi::LayoutNode;
    switch (node.type) {
    case LayoutNode::Type::Split:
        while (node.children.size() < 2)
            node.children.push_back(LayoutNode::emptyPanel());
        break;
    case LayoutNode::Type::Tab:
        if (node.children.empty())
            node.children.push_back(LayoutNode::emptyPanel());
        break;
    case LayoutNode::Type::Grid: {
        const int count = std::max(1, node.rows * node.cols);
        node.children.resize(static_cast<std::size_t>(count), LayoutNode::emptyPanel());
        break;
    }
    case LayoutNode::Type::Panel:
        break;
    }
}

/// Move an existing leaf into a freshly built container's first slot.
void attachFirst(FlowUi::LayoutNode& node, const FlowUi::LayoutNode& leaf)
{
    using FlowUi::LayoutNode;
    switch (node.type) {
    case LayoutNode::Type::Split:
    case LayoutNode::Type::Tab:
        node.children.push_back(leaf);
        node.children.push_back(LayoutNode::emptyPanel());
        break;
    case LayoutNode::Type::Grid: {
        const int count = std::max(1, node.rows * node.cols);
        node.children.assign(static_cast<std::size_t>(count), LayoutNode::emptyPanel());
        node.children[0] = leaf;
        break;
    }
    case LayoutNode::Type::Panel:
        break;
    }
}

/// Remove `target` from `parent`'s subtree by pointer identity. A grid cell is
/// reset to an empty panel (the grid keeps its dimensions); a split/tab child is
/// erased. Returns true when found.
bool removeNodeFrom(FlowUi::LayoutNode& parent, const FlowUi::LayoutNode* target)
{
    for (std::size_t i = 0; i < parent.children.size(); ++i) {
        if (&parent.children[i] == target) {
            if (parent.type == FlowUi::LayoutNode::Type::Grid)
                parent.children[i] = FlowUi::LayoutNode::emptyPanel();
            else
                parent.children.erase(parent.children.begin() + static_cast<long>(i));
            return true;
        }
        if (removeNodeFrom(parent.children[i], target))
            return true;
    }
    return false;
}

/// Detaches every node widget built into `root` so deleting the preview does
/// not destroy widgets owned by the graph model.
void releaseNodeWidgets(QWidget* root)
{
    if (!root)
        return;

    // Two passes: collect the graph-owned widgets FIRST, then reparent them.
    // Reparenting a widget out of a QSplitter makes Qt destroy a QSplitterHandle
    // (a plain child QWidget that carries no layoutNodeId), so a findChildren()
    // snapshot taken before the reparent would still hold a dangling handle
    // pointer — dereferencing it in the same loop segfaults. Collecting first
    // means the only pointers ever dereferenced are the live, tagged widgets.
    QList<QWidget*> owned;
    if (root->property("layoutNodeId").isValid())
        owned.push_back(root);

    const QList<QWidget*> children = root->findChildren<QWidget*>();
    for (QWidget* widget : children) {
        if (widget->property("layoutNodeId").isValid())
            owned.push_back(widget);
    }

    for (QWidget* widget : owned)
        widget->setParent(nullptr);
}

} // namespace

LayoutDesignerPanel::LayoutDesignerPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

LayoutDesignerPanel::~LayoutDesignerPanel()
{
    clearPreview();
}

QByteArray LayoutDesignerPanel::encodeNodeMime(NodeId nodeId)
{
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream << static_cast<quint32>(nodeId);
    return payload;
}

NodeId LayoutDesignerPanel::decodeNodeMime(const QByteArray& payload)
{
    QDataStream stream(payload); // read-only
    quint32 rawId = 0;
    stream >> rawId;
    // A short/corrupt payload must not silently decode as node 0.
    if (stream.status() != QDataStream::Ok)
        return InvalidNodeId;
    return static_cast<NodeId>(rawId);
}

void LayoutDesignerPanel::setProvider(Provider provider)
{
    m_provider = std::move(provider);
}

void LayoutDesignerPanel::setAvailableNodes(const QVector<QPair<NodeId, QString>>& nodes)
{
    m_available = nodes;
    rebuildAvailable();
}

void LayoutDesignerPanel::setLayoutSection(const FlowUi::LayoutSection& section)
{
    m_section = section;
    m_section.version = 2;
    m_hasSection = !section.workspaces.empty();
    m_currentWorkspace = 0;
    m_undoStack.clear();
    m_redoStack.clear();
    ensureWorkspace();
    refreshWorkspaceCombo();
    rebuildStructure();
    rebuildPreview();
}

void LayoutDesignerPanel::refresh()
{
    rebuildAvailable();
    refreshWorkspaceCombo();
    rebuildStructure();
    rebuildPreview();
}

// ── UI construction ─────────────────────────────────────────────────────────

void LayoutDesignerPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(buildToolbar());

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left: Available Widgets (drag source)
    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    leftLayout->addWidget(new QLabel(tr("Available Widgets"), left));
    m_search = new QLineEdit(left);
    m_search->setPlaceholderText(tr("Search…"));
    m_search->setClearButtonEnabled(true);
    leftLayout->addWidget(m_search);
    m_availableList = new NodeListWidget(left);
    m_availableList->setDragEnabled(true);
    m_availableList->setDragDropMode(QAbstractItemView::DragOnly);
    m_availableList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_availableList->setToolTip(tr("Drag a widget onto a slot in the Structure tree"));
    leftLayout->addWidget(m_availableList, 1);
    splitter->addWidget(left);

    // Middle: Structure tree (drop target)
    auto* middle = new QWidget(splitter);
    auto* middleLayout = new QVBoxLayout(middle);
    middleLayout->setContentsMargins(4, 4, 4, 4);
    middleLayout->addWidget(new QLabel(tr("Structure — drop a widget onto a slot"), middle));
    m_structure = new QTreeWidget(middle);
    m_structure->setHeaderHidden(true);
    m_structure->setAcceptDrops(true);
    m_structure->viewport()->setAcceptDrops(true);
    m_structure->viewport()->installEventFilter(this);
    middleLayout->addWidget(m_structure, 1);
    splitter->addWidget(middle);

    // Right: live preview (WYSIWYG via LayoutEngine)
    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    rightLayout->addWidget(new QLabel(tr("Preview (live)"), right));
    m_previewHost = new QWidget(right);
    m_previewLayout = new QVBoxLayout(m_previewHost);
    m_previewLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(m_previewHost, 1);
    splitter->addWidget(right);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 2);
    splitter->setSizes({220, 260, 520});
    root->addWidget(splitter, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString&) {
        rebuildAvailable();
    });
}

QWidget* LayoutDesignerPanel::buildToolbar()
{
    // Two stacked rows on purpose: a QToolBar does not wrap, and at the IDE's
    // default width (1024px) a single row overflows — which silently hides the
    // last actions ("Save Layout", "Exit") behind the "»" extension button,
    // where users never find them. Splitting into a containers row and an
    // actions row keeps everything visible.
    auto* container = new QWidget(this);
    auto* rows = new QVBoxLayout(container);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(0);

    auto makeBar = [container, rows]() {
        auto* bar = new QToolBar(container);
        bar->setToolButtonStyle(Qt::ToolButtonTextOnly);
        bar->setMovable(false);
        rows->addWidget(bar);
        return bar;
    };
    auto addTo = [](QToolBar* bar, const QString& text, const std::function<void()>& fn,
                    const QString& tip = QString()) {
        QAction* action = bar->addAction(text);
        if (!tip.isEmpty())
            action->setToolTip(tip);
        QObject::connect(action, &QAction::triggered, bar, [fn]() { fn(); });
        return action;
    };

    // Row 1 — build the layout: containers + docks.
    // Container buttons act on the Structure-tree selection (or the workspace
    // root when nothing is selected) — that is how nesting works.
    QToolBar* structure = makeBar();
    addTo(structure, tr("Split H"),
          [this]() { addSplit(FlowUi::LayoutNode::Orientation::Horizontal); },
          tr("Split the selected pane (or the workspace root) into left | right"));
    addTo(structure, tr("Split V"),
          [this]() { addSplit(FlowUi::LayoutNode::Orientation::Vertical); },
          tr("Split the selected pane (or the workspace root) into top / bottom"));
    addTo(structure, tr("Tab"), [this]() { addTab(); },
          tr("Turn the selected pane (or the workspace root) into a tab group"));

    m_gridRows = new QSpinBox(structure);
    m_gridRows->setRange(1, 8);
    m_gridRows->setValue(2);
    m_gridRows->setToolTip(tr("Grid rows"));
    m_gridCols = new QSpinBox(structure);
    m_gridCols->setRange(1, 8);
    m_gridCols->setValue(2);
    m_gridCols->setToolTip(tr("Grid columns"));
    addTo(structure, tr("Grid"),
          [this]() { addGrid(m_gridRows->value(), m_gridCols->value()); },
          tr("Replace the selected pane with an R×C grid (e.g. 2×3 for six displays)"));
    structure->addWidget(m_gridRows);
    structure->addWidget(new QLabel(tr("×"), structure));
    structure->addWidget(m_gridCols);

    structure->addSeparator();
    addTo(structure, tr("Dock ◀"), [this]() { addDock(FlowUi::DockLayout::Area::Left); },
          tr("Add a left side strip (dock); drag a widget onto it in the tree"));
    addTo(structure, tr("Dock ▶"), [this]() { addDock(FlowUi::DockLayout::Area::Right); },
          tr("Add a right side strip (dock); drag a widget onto it in the tree"));
    addTo(structure, tr("Dock ▲"), [this]() { addDock(FlowUi::DockLayout::Area::Top); },
          tr("Add a top side strip (dock)"));
    addTo(structure, tr("Dock ▼"), [this]() { addDock(FlowUi::DockLayout::Area::Bottom); },
          tr("Add a bottom side strip (dock)"));

    structure->addSeparator();
    addTo(structure, tr("Delete"), [this]() { deleteComponent(); },
          tr("Delete the selected component — its widgets detach and return to Available"));

    // Row 2 — workspace, slot actions, save/exit (always visible).
    QToolBar* actions = makeBar();
    m_workspaceCombo = new QComboBox(actions);
    m_workspaceCombo->setToolTip(tr("Active workspace"));
    connect(m_workspaceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                if (index < 0 || index >= m_section.workspaces.size() || index == m_currentWorkspace)
                    return;
                m_currentWorkspace = index;
                rebuildStructure();
                rebuildPreview();
            });
    actions->addWidget(m_workspaceCombo);
    addTo(actions, tr("+WS"), [this]() { addWorkspace(); }, tr("Add workspace"));
    addTo(actions, tr("−WS"), [this]() { removeWorkspace(); }, tr("Remove workspace"));
    actions->addSeparator();
    addTo(actions, tr("Clear Slot"), [this]() { removeSelected(); },
          tr("Unassign the widget from the selected slot (the slot stays)"));
    addTo(actions, tr("Capture"), [this]() { captureCurrent(); },
          tr("Copy the current preview (including splitter sizes) back into the model"));
    addTo(actions, tr("Undo"), [this]() { undo(); });
    addTo(actions, tr("Redo"), [this]() { redo(); });
    actions->addSeparator();
    addTo(actions, tr("Save Layout"), [this]() { Q_EMIT saveRequested(); },
          tr("Write the layout as the flow's ui section (version 2)"));
    addTo(actions, tr("Exit"), [this]() { Q_EMIT closed(); }, tr("Close the Layout Designer"));

    return container;
}

// ── model helpers ───────────────────────────────────────────────────────────

FlowUi::WorkspaceLayout& LayoutDesignerPanel::currentWorkspace()
{
    return m_section.workspaces[m_currentWorkspace];
}

void LayoutDesignerPanel::ensureWorkspace()
{
    m_section.version = 2;
    if (m_section.workspaces.empty()) {
        FlowUi::WorkspaceLayout ws;
        ws.id = 0;
        ws.name = tr("Workspace 0");
        ws.root.type = FlowUi::LayoutNode::Type::Panel;
        m_section.workspaces.push_back(ws);
    }
    if (m_currentWorkspace < 0
        || m_currentWorkspace >= static_cast<int>(m_section.workspaces.size())) {
        m_currentWorkspace = 0;
    }
}

void LayoutDesignerPanel::pushUndo()
{
    m_undoStack.push_back(m_section);
    constexpr int kMaxUndo = 50;
    if (m_undoStack.size() > kMaxUndo)
        m_undoStack.removeFirst();
}

void LayoutDesignerPanel::applyMutation(const std::function<void()>& mutate)
{
    ensureWorkspace();
    pushUndo();
    mutate();
    m_redoStack.clear();
    m_section.version = 2;
    rebuildStructure();
    rebuildPreview();
}

// ── structure tree ──────────────────────────────────────────────────────────

void LayoutDesignerPanel::rebuildStructure()
{
    if (!m_structure)
        return;

    m_structure->clear();
    m_assigners.clear();

    ensureWorkspace();
    FlowUi::WorkspaceLayout& ws = currentWorkspace();

    auto* wsItem = new QTreeWidgetItem(m_structure,
                                       {tr("Workspace: %1")
                                            .arg(ws.name.isEmpty() ? QString::number(ws.id) : ws.name)});
    wsItem->setExpanded(true);
    // Selecting the workspace targets the root (empty path).
    wsItem->setData(0, kPathRole, pathToList({}));

    populateNode(wsItem, ws.root, {});
    populateDocks(wsItem, ws);

    m_structure->expandAll();
}

QTreeWidgetItem* LayoutDesignerPanel::addAssignableItem(QTreeWidgetItem* parent,
                                                        const QString& label,
                                                        std::function<void(NodeId)> assign,
                                                        NodeId current)
{
    auto* item = new QTreeWidgetItem(parent, {label});
    if (isUnassigned(current)) {
        QFont font = item->font(0);
        font.setItalic(true);
        item->setFont(0, font);
        item->setForeground(0, QBrush(QColor(150, 150, 150)));
    } else {
        item->setForeground(0, QBrush(QColor(30, 130, 30)));
    }

    if (assign) {
        m_assigners.push_back(std::move(assign));
        item->setData(0, kAssignRole, static_cast<int>(m_assigners.size() - 1));
        item->setToolTip(0, tr("Drop a widget here"));
    }
    return item;
}

void LayoutDesignerPanel::populateNode(QTreeWidgetItem* parent,
                                       FlowUi::LayoutNode& node,
                                       const QVector<int>& path)
{
    switch (node.type) {
    case FlowUi::LayoutNode::Type::Panel: {
        const bool empty = isUnassigned(node.nodeId);
        const QString label = empty
            ? tr("Panel (empty)")
            : tr("Panel: %1 (#%2)").arg(node.title.isEmpty() ? tr("node") : node.title).arg(node.nodeId);
        QTreeWidgetItem* item = addAssignableItem(parent, label,
                                                  [&node](NodeId id) { node.nodeId = id; },
                                                  node.nodeId);
        item->setData(0, kPathRole, pathToList(path));
        break;
    }
    case FlowUi::LayoutNode::Type::Split: {
        const QString label = node.orientation == FlowUi::LayoutNode::Orientation::Vertical
            ? tr("Split (vertical)")
            : tr("Split (horizontal)");
        auto* item = new QTreeWidgetItem(parent, {label});
        item->setExpanded(true);
        item->setData(0, kPathRole, pathToList(path));
        for (int i = 0; i < static_cast<int>(node.children.size()); ++i) {
            QVector<int> childPath = path;
            childPath.append(i);
            populateNode(item, node.children[i], childPath);
        }
        break;
    }
    case FlowUi::LayoutNode::Type::Grid: {
        auto* item = new QTreeWidgetItem(
            parent, {tr("Grid %1×%2").arg(node.rows).arg(node.cols)});
        item->setExpanded(true);
        item->setData(0, kPathRole, pathToList(path));

        const int cellCount = std::max(0, node.rows * node.cols);
        if (static_cast<int>(node.children.size()) < cellCount)
            node.children.resize(static_cast<std::size_t>(cellCount),
                                 FlowUi::LayoutNode::emptyPanel());

        for (int index = 0; index < cellCount; ++index) {
            const int row = index / node.cols;
            const int col = index % node.cols;
            const QString prefix = tr("Cell %1,%2").arg(row + 1).arg(col + 1);

            QVector<int> childPath = path;
            childPath.append(index);
            FlowUi::LayoutNode& child = node.children[index];

            if (child.type == FlowUi::LayoutNode::Type::Panel) {
                const bool empty = isUnassigned(child.nodeId);
                const QString label = empty ? tr("%1 (empty)").arg(prefix)
                                            : tr("%1: node #%2").arg(prefix).arg(child.nodeId);
                QTreeWidgetItem* cell = addAssignableItem(
                    item, label, [&child](NodeId id) { child.nodeId = id; }, child.nodeId);
                cell->setData(0, kPathRole, pathToList(childPath));
            } else {
                // A nested container in this cell — recurse so it stays editable.
                auto* cell = new QTreeWidgetItem(item, {prefix});
                cell->setExpanded(true);
                cell->setData(0, kPathRole, pathToList(childPath));
                populateNode(cell, child, childPath);
            }
        }
        break;
    }
    case FlowUi::LayoutNode::Type::Tab: {
        auto* item = new QTreeWidgetItem(
            parent, {tr("Tab (%1 pages)").arg(node.children.size())});
        item->setExpanded(true);
        item->setData(0, kPathRole, pathToList(path));
        for (int i = 0; i < static_cast<int>(node.children.size()); ++i) {
            QVector<int> childPath = path;
            childPath.append(i);
            populateNode(item, node.children[i], childPath);
        }
        break;
    }
    }
}

void LayoutDesignerPanel::populateDocks(QTreeWidgetItem* parent, FlowUi::WorkspaceLayout& ws)
{
    auto* docksItem = new QTreeWidgetItem(parent, {tr("Docks")});
    docksItem->setExpanded(true);

    for (int index = 0; index < static_cast<int>(ws.docks.size()); ++index) {
        FlowUi::DockLayout& dock = ws.docks[index];
        const bool empty = isUnassigned(dock.node);
        const QString label = empty
            ? tr("Dock %1 (empty)").arg(areaLabel(dock.area))
            : tr("Dock %1: %2 (#%3)")
                  .arg(areaLabel(dock.area))
                  .arg(dock.title.isEmpty() ? tr("node") : dock.title)
                  .arg(dock.node);
        QTreeWidgetItem* item = addAssignableItem(docksItem, label,
                                                  [&dock](NodeId id) { dock.node = id; },
                                                  dock.node);
        item->setData(0, kDockRole, index);
    }
}

// ── preview ─────────────────────────────────────────────────────────────────

void LayoutDesignerPanel::rebuildPreview()
{
    clearPreview();
    if (!m_provider)
        return;

    ensureWorkspace();
    const FlowUi::WorkspaceLayout& ws = currentWorkspace();
    // placeholders = true: show empty slots as dashed boxes so the structure is
    // visible before widgets are dropped in (runtime leaves them invisible).
    FlowUi::LayoutEngine::BuildOptions options;
    options.placeholders = true;
    m_preview = FlowUi::LayoutEngine::buildWorkspace(ws, m_provider, options);
    if (!m_preview)
        return;

    m_preview->menuBar()->setVisible(false);
    m_preview->setWindowFlag(Qt::Window, false);
    m_preview->setParent(m_previewHost);
    m_previewLayout->addWidget(m_preview);
    m_preview->show();
}

void LayoutDesignerPanel::clearPreview()
{
    if (!m_preview)
        return;

    // Detach graph-owned widgets first — deleting the preview must not delete
    // widgets that belong to the graph model.
    releaseNodeWidgets(m_preview);

    if (m_previewLayout)
        m_previewLayout->removeWidget(m_preview);
    m_preview->setParent(nullptr);
    delete m_preview;
    m_preview = nullptr;
}

// ── actions ─────────────────────────────────────────────────────────────────

void LayoutDesignerPanel::addSplit(FlowUi::LayoutNode::Orientation orientation)
{
    FlowUi::LayoutNode split;
    split.type = FlowUi::LayoutNode::Type::Split;
    split.orientation = orientation;
    insertContainer(std::move(split));
}

void LayoutDesignerPanel::addGrid(int rows, int cols)
{
    FlowUi::LayoutNode grid;
    grid.type = FlowUi::LayoutNode::Type::Grid;
    grid.rows = std::max(1, rows);
    grid.cols = std::max(1, cols);
    insertContainer(std::move(grid));
}

void LayoutDesignerPanel::addTab()
{
    FlowUi::LayoutNode tab;
    tab.type = FlowUi::LayoutNode::Type::Tab;
    insertContainer(std::move(tab));
}

void LayoutDesignerPanel::insertContainer(FlowUi::LayoutNode container)
{
    applyMutation([this, container = std::move(container)]() {
        FlowUi::WorkspaceLayout& ws = currentWorkspace();
        FlowUi::LayoutNode* target = selectedNode();
        if (!target)
            target = &ws.root;

        // Leaf (Panel): replace it. An assigned node is preserved by moving it
        // into the new container's first slot.
        if (target->isPanel()) {
            FlowUi::LayoutNode built = container;
            if (!isUnassigned(target->nodeId))
                attachFirst(built, *target);
            else
                ensureSlot(built);
            *target = std::move(built);
            return;
        }

        // Grid: fill the first free cell (a grid keeps its dimensions).
        if (target->type == FlowUi::LayoutNode::Type::Grid) {
            for (FlowUi::LayoutNode& child : target->children) {
                if (child.isEmptyPanel()) {
                    FlowUi::LayoutNode built = container;
                    ensureSlot(built);
                    child = std::move(built);
                    return;
                }
            }
            return; // no free cell — nothing to do
        }

        // Split / Tab: append the new container as a pane/page.
        FlowUi::LayoutNode built = container;
        ensureSlot(built);
        target->children.push_back(std::move(built));
        if (target->type == FlowUi::LayoutNode::Type::Split)
            target->sizes.assign(target->children.size(), 100);
    });
}

void LayoutDesignerPanel::addDock(FlowUi::DockLayout::Area area)
{
    applyMutation([this, area]() {
        FlowUi::DockLayout dock;
        dock.area = area;
        dock.node = InvalidNodeId;
        currentWorkspace().docks.push_back(dock);
    });
}

void LayoutDesignerPanel::captureCurrent()
{
    if (!m_preview)
        return;

    applyMutation([this]() {
        FlowUi::WorkspaceLayout& ws = currentWorkspace();
        FlowUi::WorkspaceLayout captured = FlowUi::LayoutEngine::captureWorkspace(m_preview);
        captured.id = ws.id;
        captured.name = ws.name;
        ws = captured;
    });
}

void LayoutDesignerPanel::removeSelected()
{
    const QList<QTreeWidgetItem*> selected = m_structure->selectedItems();
    if (selected.isEmpty())
        return;

    QTreeWidgetItem* item = selected.first();
    const QVariant data = item->data(0, kAssignRole);
    if (!data.isValid())
        return;

    const int index = data.toInt();
    if (index < 0 || index >= m_assigners.size())
        return;

    const std::function<void(NodeId)> assign = m_assigners[index];
    applyMutation([assign]() { assign(InvalidNodeId); });
}

FlowUi::LayoutNode* LayoutDesignerPanel::selectedNode()
{
    if (!m_structure)
        return nullptr;

    const QList<QTreeWidgetItem*> selected = m_structure->selectedItems();
    if (selected.isEmpty())
        return nullptr;

    const QVariant data = selected.first()->data(0, kPathRole);
    if (!data.isValid())
        return nullptr;

    return nodeAtPath(currentWorkspace().root, pathFromList(data.toList()));
}

void LayoutDesignerPanel::deleteComponent()
{
    if (!m_structure)
        return;

    const QList<QTreeWidgetItem*> selected = m_structure->selectedItems();
    if (selected.isEmpty())
        return;

    QTreeWidgetItem* item = selected.first();

    // A dock is removed from the workspace's docks list.
    const QVariant dockData = item->data(0, kDockRole);
    if (dockData.isValid()) {
        const int index = dockData.toInt();
        applyMutation([this, index]() {
            FlowUi::WorkspaceLayout& ws = currentWorkspace();
            if (index >= 0 && index < static_cast<int>(ws.docks.size()))
                ws.docks.erase(ws.docks.begin() + index);
        });
        return;
    }

    FlowUi::LayoutNode* target = selectedNode();
    if (!target)
        return;

    // Deleting a component only removes it from the layout: the widgets it held
    // stay in the graph and reappear in "Available Widgets".
    applyMutation([this, target]() {
        FlowUi::WorkspaceLayout& ws = currentWorkspace();
        if (target == &ws.root) {
            ws.root = FlowUi::LayoutNode::emptyPanel();
            return;
        }
        removeNodeFrom(ws.root, target);
        // Never leave an empty split/tab as the root — collapse it.
        if ((ws.root.type == FlowUi::LayoutNode::Type::Split
             || ws.root.type == FlowUi::LayoutNode::Type::Tab)
            && ws.root.children.empty()) {
            ws.root = FlowUi::LayoutNode::emptyPanel();
        }
    });
}

bool LayoutDesignerPanel::selectNodeByPath(const QVector<int>& path)
{
    if (!m_structure)
        return false;

    const QVariantList wanted = pathToList(path);
    std::function<QTreeWidgetItem*(QTreeWidgetItem*)> find =
        [&find, &wanted](QTreeWidgetItem* item) -> QTreeWidgetItem* {
        const QVariant data = item->data(0, kPathRole);
        if (data.isValid() && data.toList() == wanted)
            return item;
        for (int i = 0; i < item->childCount(); ++i) {
            if (QTreeWidgetItem* found = find(item->child(i)))
                return found;
        }
        return nullptr;
    };

    for (int i = 0; i < m_structure->topLevelItemCount(); ++i) {
        if (QTreeWidgetItem* item = find(m_structure->topLevelItem(i))) {
            m_structure->setCurrentItem(item);
            item->setSelected(true);
            return true;
        }
    }
    return false;
}

void LayoutDesignerPanel::addWorkspace()
{
    applyMutation([this]() {
        FlowUi::WorkspaceLayout ws;
        ws.id = m_section.workspaces.empty() ? 0 : m_section.workspaces.back().id + 1;
        ws.name = tr("Workspace %1").arg(ws.id);
        ws.root.type = FlowUi::LayoutNode::Type::Panel;
        m_section.workspaces.push_back(ws);
        m_currentWorkspace = static_cast<int>(m_section.workspaces.size()) - 1;
    });
    refreshWorkspaceCombo();
}

void LayoutDesignerPanel::removeWorkspace()
{
    if (m_section.workspaces.size() <= 1)
        return;

    applyMutation([this]() {
        m_section.workspaces.erase(m_section.workspaces.begin() + m_currentWorkspace);
        if (m_currentWorkspace >= static_cast<int>(m_section.workspaces.size()))
            m_currentWorkspace = static_cast<int>(m_section.workspaces.size()) - 1;
    });
    refreshWorkspaceCombo();
}

// ── undo / redo / assignment ────────────────────────────────────────────────

void LayoutDesignerPanel::undo()
{
    if (m_undoStack.isEmpty())
        return;

    m_redoStack.push_back(m_section);
    m_section = m_undoStack.takeLast();
    if (m_currentWorkspace >= static_cast<int>(m_section.workspaces.size()))
        m_currentWorkspace = qMax(0, static_cast<int>(m_section.workspaces.size()) - 1);
    ensureWorkspace();
    refreshWorkspaceCombo();
    rebuildStructure();
    rebuildPreview();
}

void LayoutDesignerPanel::redo()
{
    if (m_redoStack.isEmpty())
        return;

    m_undoStack.push_back(m_section);
    m_section = m_redoStack.takeLast();
    if (m_currentWorkspace >= static_cast<int>(m_section.workspaces.size()))
        m_currentWorkspace = qMax(0, static_cast<int>(m_section.workspaces.size()) - 1);
    ensureWorkspace();
    refreshWorkspaceCombo();
    rebuildStructure();
    rebuildPreview();
}

void LayoutDesignerPanel::assignSlot(int slotIndex, NodeId nodeId)
{
    if (slotIndex < 0 || slotIndex >= m_assigners.size())
        return;

    // Copy: applyMutation() rebuilds the tree, which clears m_assigners.
    const std::function<void(NodeId)> assign = m_assigners[slotIndex];
    applyMutation([assign, nodeId]() { assign(nodeId); });
}

// ── available widgets ───────────────────────────────────────────────────────
void LayoutDesignerPanel::rebuildAvailable()
{
    if (!m_availableList)
        return;

    m_availableList->clear();
    const QString filter = m_search ? m_search->text().trimmed() : QString();

    for (const auto& entry : m_available) {
        if (!filter.isEmpty() && !entry.second.contains(filter, Qt::CaseInsensitive))
            continue;

        auto* item = new QListWidgetItem(entry.second, m_availableList);
        item->setData(Qt::UserRole, static_cast<quint32>(entry.first));
        item->setToolTip(tr("Node #%1 — drag onto a slot").arg(entry.first));
    }
}

// ── workspace combo ─────────────────────────────────────────────────────────

void LayoutDesignerPanel::refreshWorkspaceCombo()
{
    if (!m_workspaceCombo)
        return;

    const QSignalBlocker blocker(m_workspaceCombo);
    m_workspaceCombo->clear();
    for (int i = 0; i < m_section.workspaces.size(); ++i) {
        const FlowUi::WorkspaceLayout& ws = m_section.workspaces[i];
        m_workspaceCombo->addItem(
            ws.name.isEmpty() ? tr("Workspace %1").arg(ws.id) : ws.name);
    }
    if (m_currentWorkspace >= 0 && m_currentWorkspace < m_workspaceCombo->count())
        m_workspaceCombo->setCurrentIndex(m_currentWorkspace);
}

// ── drop handling ───────────────────────────────────────────────────────────

bool LayoutDesignerPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_structure->viewport()) {
        switch (event->type()) {
        case QEvent::DragEnter:
        case QEvent::DragMove: {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            if (dropEvent->mimeData()->hasFormat(QLatin1String(kNodeMime))) {
                dropEvent->acceptProposedAction();
                return true;
            }
            break;
        }
        case QEvent::Drop: {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            if (!dropEvent->mimeData()->hasFormat(QLatin1String(kNodeMime)))
                break;

            const NodeId id = LayoutDesignerPanel::decodeNodeMime(
                dropEvent->mimeData()->data(QLatin1String(kNodeMime)));
            if (id == InvalidNodeId) {
                // Corrupt/unknown payload — accept the drop but change nothing
                // rather than clearing the target slot.
                dropEvent->acceptProposedAction();
                return true;
            }

            QTreeWidgetItem* item = m_structure->itemAt(dropPosition(dropEvent));
            while (item) {
                const QVariant data = item->data(0, kAssignRole);
                if (data.isValid()) {
                    const int index = data.toInt();
                    if (index >= 0 && index < m_assigners.size()) {
                        const std::function<void(NodeId)> assign = m_assigners[index];
                        applyMutation([assign, id]() { assign(id); });
                    }
                    break;
                }
                item = item->parent();
            }

            dropEvent->acceptProposedAction();
            return true;
        }
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}
