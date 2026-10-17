#pragma once

#include "LayoutEngine.h"
#include "LayoutTree.h"

#include <QVector>
#include <QWidget>

#include <functional>

class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QMainWindow;
class QSpinBox;
class QToolBar;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;

/**
 * Visual Layout Designer (REQ-SW-PL-059).
 *
 * Edits a FlowUi::LayoutSection (schema v2: split/grid/tab/panel + docks) with
 * the mouse and renders it live with FlowUi::LayoutEngine::buildWorkspace().
 *
 * The panel is passive about who owns the graph: the host (NodeEditorIdeObject)
 * supplies
 *   - a Provider that returns the live widget of a node, and
 *   - the list of nodes that have a widget ("Available Widgets").
 *
 * m_section is the single source of truth. Every mutation pushes an undo
 * snapshot, rebuilds the structure tree and rebuilds the live preview.
 *
 * Container buttons act on the item SELECTED in the Structure tree (or the
 * workspace root when nothing is selected), which is what enables nesting:
 * select an empty pane/cell, press "Grid", and the grid is built in that slot.
 * "Delete" removes the selected component; widgets it held simply detach
 * (they belong to the graph, not the layout).
 *
 * Widget map:
 *   - toolbar                → create split/grid/tab/dock, delete, capture, save, undo
 *   - "Available Widgets"    → drag source (one entry per widget node)
 *   - "Structure" tree       → drop target (workspace/containers/grid cells/tabs/docks)
 *   - "Preview"              → live WYSIWYG built by LayoutEngine
 */
class LayoutDesignerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LayoutDesignerPanel(QWidget* parent = nullptr);
    ~LayoutDesignerPanel() override;

    using Provider = std::function<QWidget*(QtNodes::NodeId)>;

    /// Live widget provider handed to LayoutEngine::buildWorkspace().
    void setProvider(Provider provider);

    /// Nodes offered in the "Available Widgets" list (id, display label).
    void setAvailableNodes(const QVector<QPair<QtNodes::NodeId, QString>>& nodes);

    /// The section being edited (source of truth).
    FlowUi::LayoutSection layoutSection() const { return m_section; }
    void setLayoutSection(const FlowUi::LayoutSection& section);

    /// Rebuild preview + structure tree (e.g. after the host changed providers).
    void refresh();

    // ── Programmatic commands (also used by the toolbar) ────────────────────
    // Containers are inserted into the currently selected slot, or over the
    // workspace root when nothing is selected.
    void addSplit(FlowUi::LayoutNode::Orientation orientation);
    void addGrid(int rows = 2, int cols = 2);
    void addTab();
    void addDock(FlowUi::DockLayout::Area area);
    void captureCurrent();
    void undo();
    void redo();
    void addWorkspace();
    void removeWorkspace();

    /// Delete the selected layout component (container or panel) — or a dock —
    /// from the tree. Widgets placed inside are merely detached: they belong to
    /// the graph, so they reappear in "Available Widgets" rather than vanish.
    void deleteComponent();

    /// Select the Structure item at `path` (a chain of child indices from the
    /// workspace root; empty = root) and return true when found. Lets tests and
    /// scripts drive the selection that the toolbar acts on.
    bool selectNodeByPath(const QVector<int>& path);

    /// Number of drop targets currently offered by the Structure tree.
    int slotCount() const { return m_assigners.size(); }

    /// Assign `nodeId` to the slot at `slotIndex` — the same path the drag&drop
    /// handler uses (InvalidNodeId clears the slot).
    void assignSlot(int slotIndex, QtNodes::NodeId nodeId);

    /// MIME payload codec for the "Available Widgets" → Structure drag & drop.
    /// Both the drag source (QListWidget::mimeData) and the drop target
    /// (eventFilter) MUST go through these so the round-trip stays in sync.
    static QByteArray encodeNodeMime(QtNodes::NodeId nodeId);
    static QtNodes::NodeId decodeNodeMime(const QByteArray& payload);

Q_SIGNALS:
    /// The user pressed "Save Layout" — the host serializes layoutSection().
    void saveRequested();
    /// The user pressed "Exit Designer".
    void closed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // ── construction ────────────────────────────────────────────────────────
    void buildUi();
    QWidget* buildToolbar();

    // ── model helpers ───────────────────────────────────────────────────────
    FlowUi::WorkspaceLayout& currentWorkspace();
    void ensureWorkspace();
    void pushUndo();
    void applyMutation(const std::function<void()>& mutate);

    // ── tree building ───────────────────────────────────────────────────────
    void rebuildStructure();
    void populateNode(QTreeWidgetItem* parent,
                      FlowUi::LayoutNode& node,
                      const QVector<int>& path);
    void populateDocks(QTreeWidgetItem* parent, FlowUi::WorkspaceLayout& ws);
    QTreeWidgetItem* addAssignableItem(QTreeWidgetItem* parent,
                                       const QString& label,
                                       std::function<void(QtNodes::NodeId)> assign,
                                       QtNodes::NodeId current);

    /// The LayoutNode the toolbar currently targets (selected tree item, or
    /// null → callers fall back to the workspace root).
    FlowUi::LayoutNode* selectedNode();
    /// Insert `container` at the selection (see class docs for the rules).
    void insertContainer(FlowUi::LayoutNode container);

    // ── preview ─────────────────────────────────────────────────────────────
    void rebuildPreview();
    void clearPreview();

    // ── actions ─────────────────────────────────────────────────────────────
    void removeSelected();

    void rebuildAvailable();
    void refreshWorkspaceCombo();

    // ── state ───────────────────────────────────────────────────────────────
    FlowUi::LayoutSection m_section;
    int m_currentWorkspace = 0;
    bool m_hasSection = false;

    Provider m_provider;
    QVector<QPair<QtNodes::NodeId, QString>> m_available;

    // tree item data → index into m_assigners
    QVector<std::function<void(QtNodes::NodeId)>> m_assigners;

    QVector<FlowUi::LayoutSection> m_undoStack;
    QVector<FlowUi::LayoutSection> m_redoStack;

    QLineEdit* m_search = nullptr;
    QListWidget* m_availableList = nullptr;
    QTreeWidget* m_structure = nullptr;
    QWidget* m_previewHost = nullptr;
    QVBoxLayout* m_previewLayout = nullptr;
    QMainWindow* m_preview = nullptr;
    QComboBox* m_workspaceCombo = nullptr;
    QSpinBox* m_gridRows = nullptr;
    QSpinBox* m_gridCols = nullptr;
};
