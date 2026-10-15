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
#include <QSplitter>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

using QtNodes::InvalidNodeId;
using QtNodes::NodeId;

namespace {

const char* const kNodeMime = "application/x-daqster-node-id";
const int kAssignRole = Qt::UserRole + 1;

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

/// Detaches every node widget built into `root` so deleting the preview does
/// not destroy widgets owned by the graph model.
void releaseNodeWidgets(QWidget* root)
{
    if (!root)
        return;

    if (root->property("layoutNodeId").isValid())
        root->setParent(nullptr);

    const QList<QWidget*> children = root->findChildren<QWidget*>();
    for (QWidget* widget : children) {
        if (widget->property("layoutNodeId").isValid())
            widget->setParent(nullptr);
    }
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
    auto addTo = [](QToolBar* bar, const QString& text, const std::function<void()>& fn) {
        QAction* action = bar->addAction(text);
        QObject::connect(action, &QAction::triggered, bar, [fn]() { fn(); });
        return action;
    };

    // Row 1 — build the layout: containers + docks.
    QToolBar* structure = makeBar();
    addTo(structure, tr("Split H"),
          [this]() { addSplit(FlowUi::LayoutNode::Orientation::Horizontal); });
    addTo(structure, tr("Split V"),
          [this]() { addSplit(FlowUi::LayoutNode::Orientation::Vertical); });
    addTo(structure, tr("Grid 2×2"), [this]() { addGrid(); });
    addTo(structure, tr("Tab"), [this]() { addTab(); });
    structure->addSeparator();
    addTo(structure, tr("Dock ◀"), [this]() { addDock(FlowUi::DockLayout::Area::Left); });
    addTo(structure, tr("Dock ▶"), [this]() { addDock(FlowUi::DockLayout::Area::Right); });
    addTo(structure, tr("Dock ▲"), [this]() { addDock(FlowUi::DockLayout::Area::Top); });
    addTo(structure, tr("Dock ▼"), [this]() { addDock(FlowUi::DockLayout::Area::Bottom); });

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
    addTo(actions, tr("+WS"), [this]() { addWorkspace(); });
    addTo(actions, tr("−WS"), [this]() { removeWorkspace(); });
    actions->addSeparator();
    addTo(actions, tr("Clear Slot"), [this]() { removeSelected(); });
    addTo(actions, tr("Capture"), [this]() { captureCurrent(); });
    addTo(actions, tr("Undo"), [this]() { undo(); });
    addTo(actions, tr("Redo"), [this]() { redo(); });
    actions->addSeparator();
    addTo(actions, tr("Save Layout"), [this]() { Q_EMIT saveRequested(); });
    addTo(actions, tr("Exit"), [this]() { Q_EMIT closed(); });

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

    populateNode(wsItem, ws.root);
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

void LayoutDesignerPanel::populateNode(QTreeWidgetItem* parent, FlowUi::LayoutNode& node)
{
    switch (node.type) {
    case FlowUi::LayoutNode::Type::Panel: {
        const bool empty = isUnassigned(node.nodeId);
        const QString label = empty
            ? tr("Panel (empty)")
            : tr("Panel: %1 (#%2)").arg(node.title.isEmpty() ? tr("node") : node.title).arg(node.nodeId);
        addAssignableItem(parent, label,
                          [&node](NodeId id) { node.nodeId = id; }, node.nodeId);
        break;
    }
    case FlowUi::LayoutNode::Type::Split: {
        const QString label = node.orientation == FlowUi::LayoutNode::Orientation::Vertical
            ? tr("Split (vertical)")
            : tr("Split (horizontal)");
        auto* item = new QTreeWidgetItem(parent, {label});
        item->setExpanded(true);
        for (FlowUi::LayoutNode& child : node.children)
            populateNode(item, child);
        break;
    }
    case FlowUi::LayoutNode::Type::Grid: {
        auto* item = new QTreeWidgetItem(
            parent, {tr("Grid %1×%2").arg(node.rows).arg(node.cols)});
        item->setExpanded(true);

        const int cellCount = node.rows * node.cols;
        for (int index = 0; index < cellCount; ++index) {
            const int row = index / node.cols;
            const int col = index % node.cols;
            const NodeId current =
                (index < static_cast<int>(node.cells.size()) && node.cells[index].has_value())
                    ? *node.cells[index]
                    : InvalidNodeId;
            const QString label = isUnassigned(current)
                ? tr("Cell %1,%2 (empty)").arg(row + 1).arg(col + 1)
                : tr("Cell %1,%2: node #%3").arg(row + 1).arg(col + 1).arg(current);

            addAssignableItem(item, label,
                              [&node, index](NodeId id) {
                                  if (index >= static_cast<int>(node.cells.size()))
                                      node.cells.resize(index + 1, std::nullopt);
                                  if (isUnassigned(id))
                                      node.cells[index] = std::nullopt;
                                  else
                                      node.cells[index] = id;
                              },
                              current);
        }
        break;
    }
    case FlowUi::LayoutNode::Type::Tab: {
        auto* item = new QTreeWidgetItem(
            parent, {tr("Tab (%1 pages)").arg(node.children.size())});
        item->setExpanded(true);
        for (FlowUi::LayoutNode& child : node.children)
            populateNode(item, child);
        break;
    }
    }
}

void LayoutDesignerPanel::populateDocks(QTreeWidgetItem* parent, FlowUi::WorkspaceLayout& ws)
{
    auto* docksItem = new QTreeWidgetItem(parent, {tr("Docks")});
    docksItem->setExpanded(true);

    for (FlowUi::DockLayout& dock : ws.docks) {
        const bool empty = isUnassigned(dock.node);
        const QString label = empty
            ? tr("Dock %1 (empty)").arg(areaLabel(dock.area))
            : tr("Dock %1: %2 (#%3)")
                  .arg(areaLabel(dock.area))
                  .arg(dock.title.isEmpty() ? tr("node") : dock.title)
                  .arg(dock.node);
        addAssignableItem(docksItem, label,
                          [&dock](NodeId id) { dock.node = id; }, dock.node);
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
    applyMutation([this, orientation]() {
        FlowUi::WorkspaceLayout& ws = currentWorkspace();

        FlowUi::LayoutNode split;
        split.type = FlowUi::LayoutNode::Type::Split;
        split.orientation = orientation;

        const bool rootEmpty = ws.root.type == FlowUi::LayoutNode::Type::Panel
            && isUnassigned(ws.root.nodeId);
        if (!rootEmpty)
            split.children.push_back(ws.root);

        FlowUi::LayoutNode empty;
        empty.type = FlowUi::LayoutNode::Type::Panel;
        split.children.push_back(empty);
        if (split.children.size() > 1)
            split.sizes.assign(split.children.size(), 100);

        ws.root = split;
    });
}

void LayoutDesignerPanel::addGrid()
{
    applyMutation([this]() {
        FlowUi::LayoutNode grid;
        grid.type = FlowUi::LayoutNode::Type::Grid;
        grid.rows = 2;
        grid.cols = 2;
        grid.cells.assign(4, std::nullopt);
        currentWorkspace().root = grid;
    });
}

void LayoutDesignerPanel::addTab()
{
    applyMutation([this]() {
        FlowUi::WorkspaceLayout& ws = currentWorkspace();

        FlowUi::LayoutNode tab;
        tab.type = FlowUi::LayoutNode::Type::Tab;

        const bool rootEmpty = ws.root.type == FlowUi::LayoutNode::Type::Panel
            && isUnassigned(ws.root.nodeId);
        if (!rootEmpty)
            tab.children.push_back(ws.root);

        FlowUi::LayoutNode empty;
        empty.type = FlowUi::LayoutNode::Type::Panel;
        tab.children.push_back(empty);

        ws.root = tab;
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
